#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t http_port; // 0 = default (80)
} web_server_config_t;

typedef enum {
    WEB_SERVER_WIFI_CONNECTING = 0, // bring-up hasn't landed on STA or AP yet
    WEB_SERVER_WIFI_STA,            // joined the stored network
    WEB_SERVER_WIFI_AP,             // stored network unreachable/absent — running the setup AP
} web_server_wifi_mode_t;

typedef enum {
    WEB_SERVER_TIME_UNSET = 0, // no wall clock yet — this board has no
                                // battery-backed RTC, so this is the
                                // state on every boot until SNTP lands
    WEB_SERVER_TIME_NTP,       // set by sntp_sync_cb()
} web_server_time_source_t;

typedef struct {
    web_server_wifi_mode_t wifi_mode;
    char ssid[33]; // STA: the network joined; AP: the SoftAP's own SSID
    char ip[16];   // dotted-quad, empty until known
    bool time_synced;
    web_server_time_source_t time_source;
} web_server_status_t;

// Kicks off Wi-Fi station bring-up (credentials from NVS via
// wifi_provision.h, falling back to a SoftAP + captive portal — see
// wifi_provision_start_ap()), SNTP wall-clock sync, mDNS, and the HTTP
// server, all from a task pinned to core 1 (the Wi-Fi driver's own task
// defaults to core 0). Returns once that task is created — it does not
// block waiting for the network to come up, so a missing/wrong network
// doesn't hold up the rest of app_main. Serves:
//
//   GET  /          embedded status page (index.html): device wall clock
//   GET  /api/time  { synced, epoch } — device wall clock
esp_err_t web_server_init(const web_server_config_t *config);

// Snapshot of current WiFi/time state, for UI use (e.g. a settings
// screen). Cheap and safe to call from any task/timer; the underlying
// fields are set once per bring-up outcome from the WiFi event/bring-up
// task and read here without a lock.
void web_server_get_status(web_server_status_t *out);

// Erases stored WiFi credentials (see wifi_provision.h) and reboots. The
// next boot finds none and falls straight into the SoftAP setup flow.
// Intended for an on-device "forget this network" UI action.
void web_server_forget_wifi(void);

// Forces a fresh SNTP sync attempt right now, instead of waiting for the
// client's own poll interval. No-op-ish if there's no route out (SoftAP
// mode, or STA with no internet): the attempt just times out silently,
// same as it would at boot, and the status stays whatever it already was.
void web_server_sync_ntp_now(void);

// Current wall-clock time (UTC seconds since 1970), from the last SNTP
// sync — see web_server_status_t's time_source. Returns false if it
// hasn't landed yet.
bool web_server_get_wall_clock(time_t *out_epoch_utc);

#define WEB_SERVER_WIFI_SCAN_MAX 20

typedef struct {
    char ssid[33];
    int8_t rssi;
    bool secure;
} web_server_wifi_scan_result_t;

// Scans for nearby WiFi networks — blocks for the scan's duration
// (roughly 1-3s), so call this from a background task, never from the
// LVGL task (it would freeze the UI for that long). Works whether
// currently connected as STA, running the setup AP, or both. Results are
// deduped by SSID (strongest reading wins) and sorted strongest-first.
// Returns the number written to `out`, capped at max_out and
// WEB_SERVER_WIFI_SCAN_MAX. Intended for an on-device "connect to WiFi"
// UI screen — the setup portal's own equivalent (GET /api/scan) doesn't
// go through this, it calls wifi_provision_scan() directly.
size_t web_server_scan_wifi(web_server_wifi_scan_result_t *out, size_t max_out);

// Saves ssid/pass as the stored network (the same NVS entry the setup
// portal's form writes) and reboots shortly after returning — the next
// boot tries it via the normal wifi_provision_load() path. pass may be ""
// for an open network. Intended for an on-device "connect to WiFi" UI
// screen, as the local equivalent of submitting the portal form.
esp_err_t web_server_connect_wifi(const char *ssid, const char *pass);

#ifdef __cplusplus
}
#endif
