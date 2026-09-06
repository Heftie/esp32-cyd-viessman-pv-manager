#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Registers every screen (home, settings, wifi_settings) and shows the
// home screen. Call once from app_main(), after the LVGL display/touch
// bring-up (lv_scr_act() must already be valid). Safe to call before
// web_server bring-up: home_screen and wifi_settings_screen read it
// through their own refresh timers via a plain static-default read
// ("Time not set", WiFi "Connecting...") that's safe to see before
// web_server_init() has run, so the screen shows up immediately rather
// than waiting behind it — its WiFi connect alone can take up to 20s
// before falling back to the setup AP.
void ui_init(void);

#ifdef __cplusplus
}
#endif
