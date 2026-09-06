#include "wifi_connect_screen.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <lvgl.h>
#include <esp_lvgl_port.h>

#include "screen_nav.h"
#include "web_server.h"

// --- List view: scan results ------------------------------------------

static lv_obj_t *s_list_view;
static lv_obj_t *s_list_container;
static lv_obj_t *s_list_status_label;

// Populated by render_scan_results(); network_row_clicked_cb's user_data
// is the row's index into these, rather than a pointer into scan-result
// memory that's long gone by the time the row is actually tapped.
static char s_row_ssid[WEB_SERVER_WIFI_SCAN_MAX][33];

// --- Connect view: password entry + on-screen keyboard -----------------

static lv_obj_t *s_connect_view;
static lv_obj_t *s_connect_ssid_label; // shown for a list-picked network
static lv_obj_t *s_connect_ssid_ta;    // shown for manual entry
static lv_obj_t *s_connect_pass_ta;
static lv_obj_t *s_connect_status_label;
static lv_obj_t *s_connect_btn;
static lv_obj_t *s_keyboard;

static bool s_screen_active;
static bool s_manual_entry;

// --- List view -----------------------------------------------------------

static void show_connect_view(const char *ssid, bool manual)
{
    s_manual_entry = manual;

    lv_obj_add_flag(s_list_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_connect_view, LV_OBJ_FLAG_HIDDEN);

    if (manual) {
        lv_obj_clear_flag(s_connect_ssid_ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_connect_ssid_label, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(s_connect_ssid_ta, "");
    } else {
        lv_obj_add_flag(s_connect_ssid_ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_connect_ssid_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_connect_ssid_label, ssid);
    }

    lv_textarea_set_text(s_connect_pass_ta, "");
    lv_label_set_text(s_connect_status_label, "");
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_connect_btn, LV_OBJ_FLAG_CLICKABLE); // re-enable after a prior failed attempt
}

static void network_row_clicked_cb(lv_event_t *e)
{
    size_t idx = (size_t)(uintptr_t)lv_event_get_user_data(e);
    show_connect_view(s_row_ssid[idx], false);
}

static void manual_row_clicked_cb(lv_event_t *e)
{
    show_connect_view("", true);
}

static void render_scan_results(const web_server_wifi_scan_result_t *results, size_t n)
{
    lv_obj_clean(s_list_container);

    if (n == 0) {
        lv_obj_t *lbl = lv_label_create(s_list_container);
        lv_label_set_text(lbl, "No networks found nearby.");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    }

    if (n > WEB_SERVER_WIFI_SCAN_MAX) {
        n = WEB_SERVER_WIFI_SCAN_MAX;
    }

    for (size_t i = 0; i < n; i++) {
        strncpy(s_row_ssid[i], results[i].ssid, sizeof(s_row_ssid[i]) - 1);
        s_row_ssid[i][sizeof(s_row_ssid[i]) - 1] = '\0';

        lv_obj_t *row = lv_obj_create(s_list_container);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, lv_pct(100), 40);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1c1c1c), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(row, network_row_clicked_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, s_row_ssid[i]);
        lv_obj_set_style_text_color(name, lv_color_white(), 0);
        lv_obj_set_flex_grow(name, 1);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

        char sig_buf[16];
        snprintf(sig_buf, sizeof(sig_buf), "%s%d dBm", results[i].secure ? "*" : "", (int)results[i].rssi);
        lv_obj_t *sig = lv_label_create(row);
        lv_label_set_text(sig, sig_buf);
        lv_obj_set_style_text_color(sig, lv_color_hex(0x00e08a), 0);
    }
}

static void wifi_scan_task(void *arg)
{
    web_server_wifi_scan_result_t results[WEB_SERVER_WIFI_SCAN_MAX];
    size_t n = web_server_scan_wifi(results, WEB_SERVER_WIFI_SCAN_MAX);

    lvgl_port_lock(0);
    if (s_screen_active) {
        render_scan_results(results, n);
        lv_label_set_text(s_list_status_label, "");
    }
    lvgl_port_unlock();

    vTaskDelete(NULL);
}

// Safe to call either from an already-locked context (screen creation)
// or from a plain LVGL click-event callback (which runs on the LVGL task
// itself, same as this project's other screens' direct label updates —
// see e.g. wifi_settings_screen.c's forget-button handler). Never call
// this from another task without holding lvgl_port_lock() first.
static void start_scan(void)
{
    lv_obj_clean(s_list_container);
    lv_label_set_text(s_list_status_label, "Scanning...");
    xTaskCreate(wifi_scan_task, "wifi_scan_ui", 4096, NULL, 5, NULL);
}

static void rescan_cb(lv_event_t *e)
{
    start_scan();
}

static void list_back_cb(lv_event_t *e)
{
    s_screen_active = false;
    screen_pop();
}

// --- Connect view --------------------------------------------------------

static void connect_view_back_cb(lv_event_t *e)
{
    lv_obj_add_flag(s_connect_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_list_view, LV_OBJ_FLAG_HIDDEN);
}

static void do_submit(void)
{
    const char *ssid = s_manual_entry ? lv_textarea_get_text(s_connect_ssid_ta)
                                       : lv_label_get_text(s_connect_ssid_label);
    if (ssid == NULL || ssid[0] == '\0') {
        lv_label_set_text(s_connect_status_label, "Enter a network name");
        return;
    }

    const char *pass = lv_textarea_get_text(s_connect_pass_ta);

    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN); // submitting closes it regardless of which field had focus
    lv_obj_clear_flag(s_connect_btn, LV_OBJ_FLAG_CLICKABLE);

    if (web_server_connect_wifi(ssid, pass) == ESP_OK) {
        lv_label_set_text(s_connect_status_label, "Connecting... rebooting");
    } else {
        lv_label_set_text(s_connect_status_label, "Failed to save credentials");
        lv_obj_add_flag(s_connect_btn, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void connect_btn_cb(lv_event_t *e)
{
    do_submit();
}

// Standard LVGL textarea<->keyboard binding: focusing a textarea routes
// the keyboard to it and shows the keyboard; leaving it (however that
// happens) hides the keyboard again. Enter on the password field submits
// directly, same as tapping Connect.
static void textarea_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(s_keyboard, ta);
        lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_READY) {
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
        if (ta == s_connect_pass_ta) {
            do_submit();
        }
    }
}

// --- Screen ----------------------------------------------------------------

void wifi_connect_screen_create(void)
{
    lvgl_port_lock(0);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, lv_pct(100), 34);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(header, 8, 0);
    lv_obj_set_style_pad_gap(header, 10, 0);

    lv_obj_t *btn_back = lv_button_create(header);
    lv_obj_set_size(btn_back, 68, 32);
    lv_obj_add_event_cb(btn_back, list_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Connect to WiFi");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // --- list_view: rescan row + scrollable network list ---

    s_list_view = lv_obj_create(body);
    lv_obj_remove_style_all(s_list_view);
    lv_obj_set_size(s_list_view, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(s_list_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_list_view, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_list_view, 10, 0);
    lv_obj_set_style_pad_gap(s_list_view, 8, 0);

    lv_obj_t *rescan_row = lv_obj_create(s_list_view);
    lv_obj_remove_style_all(rescan_row);
    lv_obj_set_size(rescan_row, lv_pct(100), 20);
    lv_obj_set_flex_flow(rescan_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rescan_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_list_status_label = lv_label_create(rescan_row);
    lv_label_set_text(s_list_status_label, "Scanning...");
    lv_obj_set_style_text_color(s_list_status_label, lv_color_hex(0x888888), 0);

    lv_obj_t *rescan_btn = lv_button_create(rescan_row);
    lv_obj_set_size(rescan_btn, 74, 26);
    lv_obj_add_event_cb(rescan_btn, rescan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rescan_label = lv_label_create(rescan_btn);
    lv_label_set_text(rescan_label, "Rescan");
    lv_obj_center(rescan_label);

    // A persistent row, not part of the scan-result list that
    // render_scan_results() clears/rebuilds each scan — manual entry
    // stays available immediately, without waiting for a scan to finish.
    lv_obj_t *manual_row = lv_obj_create(s_list_view);
    lv_obj_remove_style_all(manual_row);
    lv_obj_set_size(manual_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(manual_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(manual_row, 1, 0);
    lv_obj_set_style_border_color(manual_row, lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(manual_row, 6, 0);
    lv_obj_add_event_cb(manual_row, manual_row_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *manual_label = lv_label_create(manual_row);
    lv_label_set_text(manual_label, "Enter network manually");
    lv_obj_set_style_text_color(manual_label, lv_color_hex(0x00e08a), 0);
    lv_obj_center(manual_label);

    s_list_container = lv_obj_create(s_list_view);
    lv_obj_remove_style_all(s_list_container);
    lv_obj_set_width(s_list_container, lv_pct(100));
    lv_obj_set_flex_grow(s_list_container, 1);
    lv_obj_set_flex_flow(s_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(s_list_container, 6, 0);

    // --- connect_view: SSID (label or manual textarea) + password + keyboard ---

    s_connect_view = lv_obj_create(body);
    lv_obj_remove_style_all(s_connect_view);
    lv_obj_set_size(s_connect_view, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(s_connect_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_connect_view, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_connect_view, 10, 0);
    lv_obj_set_style_pad_gap(s_connect_view, 6, 0);
    lv_obj_add_flag(s_connect_view, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *connect_header = lv_obj_create(s_connect_view);
    lv_obj_remove_style_all(connect_header);
    lv_obj_set_size(connect_header, lv_pct(100), 28);
    lv_obj_set_flex_flow(connect_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(connect_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(connect_header, 10, 0);

    lv_obj_t *connect_back_btn = lv_button_create(connect_header);
    lv_obj_set_size(connect_back_btn, 68, 26);
    lv_obj_add_event_cb(connect_back_btn, connect_view_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *connect_back_label = lv_label_create(connect_back_btn);
    lv_label_set_text(connect_back_label, "Back");
    lv_obj_center(connect_back_label);

    s_connect_ssid_label = lv_label_create(s_connect_view);
    lv_obj_set_style_text_color(s_connect_ssid_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_connect_ssid_label, &lv_font_montserrat_20, 0);

    s_connect_ssid_ta = lv_textarea_create(s_connect_view);
    lv_textarea_set_one_line(s_connect_ssid_ta, true);
    lv_textarea_set_placeholder_text(s_connect_ssid_ta, "Network name (SSID)");
    lv_textarea_set_max_length(s_connect_ssid_ta, 32);
    lv_obj_add_event_cb(s_connect_ssid_ta, textarea_event_cb, LV_EVENT_ALL, NULL);

    s_connect_pass_ta = lv_textarea_create(s_connect_view);
    lv_textarea_set_one_line(s_connect_pass_ta, true);
    lv_textarea_set_password_mode(s_connect_pass_ta, true);
    lv_textarea_set_placeholder_text(s_connect_pass_ta, "Password (blank if open)");
    lv_textarea_set_max_length(s_connect_pass_ta, 64);
    lv_obj_add_event_cb(s_connect_pass_ta, textarea_event_cb, LV_EVENT_ALL, NULL);

    s_connect_btn = lv_button_create(s_connect_view);
    lv_obj_set_size(s_connect_btn, lv_pct(100), 36);
    lv_obj_add_event_cb(s_connect_btn, connect_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *connect_btn_label = lv_label_create(s_connect_btn);
    lv_label_set_text(connect_btn_label, "Connect");
    lv_obj_center(connect_btn_label);

    s_connect_status_label = lv_label_create(s_connect_view);
    lv_obj_set_style_text_color(s_connect_status_label, lv_color_hex(0x888888), 0);

    s_keyboard = lv_keyboard_create(s_connect_view);
    lv_obj_set_width(s_keyboard, lv_pct(100));
    lv_obj_set_flex_grow(s_keyboard, 1);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);

    s_screen_active = true;

    lvgl_port_unlock();

    start_scan();
}
