#include "web_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <mdns.h>
#include <cJSON.h>

#include "wifi_provision.h"

static const char *TAG = "web_server";

#define WIFI_BRINGUP_STACK   4096
#define WIFI_BRINGUP_PRIO    5
#define WIFI_BRINGUP_CORE    1 // Wi-Fi driver task defaults to core 0
#define WIFI_CONNECT_TIMEOUT_MS 20000
#define WIFI_CONNECTED_BIT   (1 << 0)

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

// 0 until an SNTP sync has landed. Once set, any boot-relative
// esp_timer_get_time() timestamp converts to wall-clock epoch
// microseconds by adding this offset — see web_server_get_wall_clock().
static volatile int64_t s_boot_epoch_offset_us = 0;
static volatile bool s_time_synced = false;
static volatile web_server_time_source_t s_time_source = WEB_SERVER_TIME_UNSET;

// WiFi status for UI consumers (web_server_get_status()). Written once per
// bring-up outcome from wifi_bringup_task/ip_event_handler (WiFi task/event
// context), read from wherever a UI polls it — no lock, same convention as
// s_boot_epoch_offset_us/s_time_synced above.
static volatile web_server_wifi_mode_t s_wifi_mode = WEB_SERVER_WIFI_CONNECTING;
static char s_wifi_ssid[33] = {0};
static char s_wifi_ip[16] = {0};

static httpd_handle_t s_httpd;
static uint16_t s_http_port;
static EventGroupHandle_t s_wifi_event_group;

// Set for the duration of web_server_scan_wifi() below. ESP32 has one
// 2.4GHz radio, so a full-channel scan necessarily leaves whatever
// channel STA is connected on — the AP (or STA itself) can and does
// treat that as a dropped link, firing WIFI_EVENT_STA_DISCONNECTED right
// in the middle of the scan. Reconnecting immediately then collides with
// the still-running scan (esp_wifi_scan_start() fails outright with
// ESP_ERR_WIFI_STATE, "STA is connecting"), which used to spiral into
// repeated failures and eventually the 20s connect-timeout fallback to
// the setup AP. Deferring the reconnect until the scan itself finishes
// (see web_server_scan_wifi()) avoids that fight entirely.
static volatile bool s_scan_in_progress = false;

// --- Wi-Fi / SNTP / mDNS bring-up ------------------------------------------

static void sntp_sync_cb(struct timeval *tv)
{
    int64_t epoch_us = (int64_t)tv->tv_sec * 1000000LL + tv->tv_usec;
    s_boot_epoch_offset_us = epoch_us - esp_timer_get_time();
    s_time_synced = true;
    s_time_source = WEB_SERVER_TIME_NTP;
    ESP_LOGI(TAG, "SNTP synced");
}

static void start_sntp(void)
{
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = sntp_sync_cb;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&cfg));
}

static void start_mdns(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return;
    }
    mdns_hostname_set(CONFIG_WEB_SERVER_MDNS_HOSTNAME);
    mdns_instance_name_set("ESP32 CYD");
    mdns_service_add(NULL, "_http", "_tcp", s_http_port, NULL, 0);
    ESP_LOGI(TAG, "mDNS: http://%s.local", CONFIG_WEB_SERVER_MDNS_HOSTNAME);
}

static esp_err_t start_httpd(void);

static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id != IP_EVENT_STA_GOT_IP) {
        return;
    }
    ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "got IP: " IPSTR, IP2STR(&evt->ip_info.ip));
    snprintf(s_wifi_ip, sizeof(s_wifi_ip), IPSTR, IP2STR(&evt->ip_info.ip));
    s_wifi_mode = WEB_SERVER_WIFI_STA;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

    // Only the first connection needs to bring these up; reconnects after a
    // drop don't need a fresh mDNS/httpd instance.
    static bool started_once = false;
    if (!started_once) {
        started_once = true;
        start_sntp();
        start_mdns();
        if (start_httpd() != ESP_OK) {
            ESP_LOGE(TAG, "start_httpd failed");
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_scan_in_progress) {
            ESP_LOGI(TAG, "Wi-Fi disconnected (scan in progress), deferring reconnect");
            return;
        }
        if (s_wifi_mode == WEB_SERVER_WIFI_AP) {
            // Already fell back to the setup AP — the stored network
            // didn't work within the initial connect window, so
            // retrying it here just burns airtime the SoftAP needs for
            // its own beacons/probe responses (one radio, shared with
            // STA), which is what was making the portal itself flaky to
            // reach. Leave STA idle; the next real attempt happens after
            // new credentials are saved and the device reboots.
            return;
        }
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying");
        esp_wifi_connect();
    }
}

// Boot flow: load credentials from NVS (see wifi_provision.c) and try
// STA with a bounded wait. No stored credentials, or no connection within
// that window, falls back to a SoftAP + captive portal so the network can
// be (re)configured from a phone — no rebuild/reflash, no credentials in
// any file this repo tracks or even builds from.
static void wifi_bringup_task(void *arg)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();
    // AP netif is created lazily by wifi_provision_start_ap() only if we
    // actually fall back to it — creating it here unconditionally races
    // wifi_provision.c's own call and asserts (duplicate netif key).

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL));

    char ssid[33] = {0};
    char pass[65] = {0};
    if (wifi_provision_load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        ESP_LOGI(TAG, "found stored credentials for \"%s\", connecting", ssid);
        strncpy(s_wifi_ssid, ssid, sizeof(s_wifi_ssid) - 1);

        wifi_config_t sta_cfg = { 0 };
        strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1);
        strncpy((char *)sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password) - 1);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                                pdFALSE, pdTRUE, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
        if (bits & WIFI_CONNECTED_BIT) {
            vTaskDelete(NULL);
            return; // SNTP/mDNS/httpd already kicked off from ip_event_handler
        }

        ESP_LOGW(TAG, "could not connect to \"%s\" within %d ms, falling back to setup AP",
                 ssid, WIFI_CONNECT_TIMEOUT_MS);
        esp_wifi_stop();
    } else {
        ESP_LOGW(TAG, "no stored WiFi credentials, starting setup AP");
    }

    char ap_ssid[33] = {0};
    char ap_ip[16] = {0};
    if (wifi_provision_start_ap(ap_ssid, sizeof(ap_ssid), ap_ip, sizeof(ap_ip)) != ESP_OK) {
        ESP_LOGE(TAG, "wifi_provision_start_ap failed");
    } else {
        strncpy(s_wifi_ssid, ap_ssid, sizeof(s_wifi_ssid) - 1);
        strncpy(s_wifi_ip, ap_ip, sizeof(s_wifi_ip) - 1);
        s_wifi_mode = WEB_SERVER_WIFI_AP;
    }
    vTaskDelete(NULL);
}

// --- HTTP handlers -----------------------------------------------------

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    // Without this, a browser is free to cache this page indefinitely —
    // it's a single static response with no Last-Modified/ETag for it to
    // revalidate against, and this HTML is the entire app (CSS + JS
    // inlined, no separate asset files). A tab left open across a
    // firmware update just keeps running whatever JS it already loaded,
    // same as any page would; this at least makes a *fresh* load/reload
    // always get the current version instead of a browser-cached one.
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
}

// GET /api/time — the device's own wall clock (UTC epoch seconds) and
// whether it's actually synced yet, from the device's own clock rather
// than the browser's (which could be skewed from it, especially before
// this board's first NTP sync).
static esp_err_t api_time_handler(httpd_req_t *req)
{
    time_t now;
    bool synced = web_server_get_wall_clock(&now);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "synced", synced);
    if (synced) {
        cJSON_AddNumberToObject(root, "epoch", (double)now);
    } else {
        cJSON_AddNullToObject(root, "epoch");
    }

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t start_httpd(void)
{
    static const httpd_uri_t routes[] = {
        { .uri = "/", .method = HTTP_GET, .handler = root_handler },
        { .uri = "/api/time", .method = HTTP_GET, .handler = api_time_handler },
    };

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = s_http_port;
    config.stack_size = 6144;
    config.core_id = WIFI_BRINGUP_CORE;

    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        return err;
    }

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        if (httpd_register_uri_handler(s_httpd, &routes[i]) != ESP_OK) {
            ESP_LOGE(TAG, "failed to register route: %s", routes[i].uri);
        }
    }

    ESP_LOGI(TAG, "httpd started on port %u", s_http_port);
    return ESP_OK;
}

esp_err_t web_server_init(const web_server_config_t *config)
{
    s_http_port = (config != NULL && config->http_port != 0) ? config->http_port : 80;

    // nvs_flash_init() is safe to call again from wifi_bringup_task below:
    // it's a no-op once the default partition is already initialized.
    ESP_ERROR_CHECK(nvs_flash_init());

    BaseType_t ok = xTaskCreatePinnedToCore(wifi_bringup_task, "web_bringup",
                                             WIFI_BRINGUP_STACK, NULL, WIFI_BRINGUP_PRIO,
                                             NULL, WIFI_BRINGUP_CORE);
    return (ok == pdPASS) ? ESP_OK : ESP_FAIL;
}

void web_server_get_status(web_server_status_t *out)
{
    out->wifi_mode = s_wifi_mode;
    strncpy(out->ssid, s_wifi_ssid, sizeof(out->ssid) - 1);
    out->ssid[sizeof(out->ssid) - 1] = '\0';
    strncpy(out->ip, s_wifi_ip, sizeof(out->ip) - 1);
    out->ip[sizeof(out->ip) - 1] = '\0';
    out->time_synced = s_time_synced;
    out->time_source = s_time_source;
}

void web_server_sync_ntp_now(void)
{
    // esp_netif_sntp_start() restarts the client if it's already running
    // (it was started once from start_sntp() after the first STA
    // connect), forcing a fresh attempt right now instead of waiting for
    // its own poll interval. sntp_sync_cb() fires the same way it would
    // on any other sync.
    esp_netif_sntp_start();
}

bool web_server_get_wall_clock(time_t *out_epoch_utc)
{
    if (!s_time_synced) {
        return false;
    }
    int64_t epoch_us = esp_timer_get_time() + s_boot_epoch_offset_us;
    *out_epoch_utc = (time_t)(epoch_us / 1000000);
    return true;
}

static void forget_wifi_task(void *arg)
{
    // Give the caller's UI time to show feedback ("Forgetting...") before
    // esp_restart() tears the whole board down.
    vTaskDelay(pdMS_TO_TICKS(600));
    wifi_provision_clear();
    esp_restart();
}

void web_server_forget_wifi(void)
{
    xTaskCreate(forget_wifi_task, "wifi_forget", 2048, NULL, 5, NULL);
}

size_t web_server_scan_wifi(web_server_wifi_scan_result_t *out, size_t max_out)
{
    bool was_sta = (s_wifi_mode == WEB_SERVER_WIFI_STA);

    s_scan_in_progress = true;
    wifi_provision_scan_result_t raw[WIFI_PROVISION_SCAN_MAX];
    if (max_out > WIFI_PROVISION_SCAN_MAX) {
        max_out = WIFI_PROVISION_SCAN_MAX;
    }
    size_t n = wifi_provision_scan(raw, max_out);
    s_scan_in_progress = false;

    if (was_sta) {
        // The scan very likely knocked us off our own AP for its
        // duration (see s_scan_in_progress's comment above) — our own
        // auto-reconnect sat that out, so force one clean attempt now
        // that the scan itself is done rather than leaving it to notice
        // on its own.
        esp_wifi_connect();
    }

    for (size_t i = 0; i < n; i++) {
        strncpy(out[i].ssid, raw[i].ssid, sizeof(out[i].ssid) - 1);
        out[i].ssid[sizeof(out[i].ssid) - 1] = '\0';
        out[i].rssi = raw[i].rssi;
        out[i].secure = raw[i].secure;
    }
    return n;
}

static void connect_wifi_task(void *arg)
{
    // Same reasoning as forget_wifi_task above — give the caller's UI
    // time to show feedback before esp_restart() tears the board down.
    vTaskDelay(pdMS_TO_TICKS(600));
    esp_restart();
}

esp_err_t web_server_connect_wifi(const char *ssid, const char *pass)
{
    esp_err_t err = wifi_provision_save(ssid, pass);
    if (err != ESP_OK) {
        return err;
    }
    xTaskCreate(connect_wifi_task, "wifi_connect_reboot", 2048, NULL, 5, NULL);
    return ESP_OK;
}
