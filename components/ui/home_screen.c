#include "home_screen.h"

#include <stdio.h>
#include <time.h>

#include <lvgl.h>
#include <esp_lvgl_port.h>

#include "screen_nav.h"
#include "web_server.h"

static lv_obj_t *s_clock_label;
static lv_timer_t *s_clock_refresh_timer;
static bool s_home_screen_active;

static void home_clock_refresh_timer_cb(lv_timer_t *timer)
{
    if (!s_home_screen_active) {
        return;
    }

    // Sized well above what the format string can ever actually need (max
    // 19 chars + " UTC" + NUL) — GCC's -Wformat-truncation can't tell
    // tm_year/tm_mday/etc. are bounded and assumes worst-case %d width
    // for a small buffer, which trips -Werror.
    char clock_buf[64];
    time_t now;
    if (web_server_get_wall_clock(&now)) {
        struct tm tm_utc;
        gmtime_r(&now, &tm_utc);
        snprintf(clock_buf, sizeof(clock_buf), "%02d/%02d/%04d %02d:%02d:%02d UTC",
                 tm_utc.tm_mday, tm_utc.tm_mon + 1, tm_utc.tm_year + 1900,
                 tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
    } else {
        snprintf(clock_buf, sizeof(clock_buf), "Time not set");
    }

    lvgl_port_lock(0);
    lv_label_set_text(s_clock_label, clock_buf);
    lvgl_port_unlock();
}

static void home_settings_cb(lv_event_t *e)
{
    s_home_screen_active = false;
    screen_push("settings");
}

// Rebuilds from scratch each time it's shown (matching the rest of this
// component's single-active-screen pattern).
void home_screen_create(void)
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
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(header, 8, 0);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Home");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    lv_obj_t *btn_settings = lv_button_create(header);
    lv_obj_set_size(btn_settings, 84, 32);
    lv_obj_add_event_cb(btn_settings, home_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_settings = lv_label_create(btn_settings);
    lv_label_set_text(lbl_settings, "Settings");
    lv_obj_center(lbl_settings);

    s_clock_label = lv_label_create(scr);
    lv_label_set_text(s_clock_label, "Time not set");
    lv_obj_set_style_text_color(s_clock_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_pad_hor(s_clock_label, 8, 0);

    s_home_screen_active = true;

    lvgl_port_unlock();

    if (s_clock_refresh_timer == NULL) {
        s_clock_refresh_timer = lv_timer_create(home_clock_refresh_timer_cb, 1000, NULL);
    }
}
