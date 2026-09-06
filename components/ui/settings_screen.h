#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// A flat menu of settings destinations — currently just WiFi
// (wifi_settings_screen.c). Reached from the home screen's Settings
// button. More entries can be added here the same way as this project
// grows (each its own screen, one button here to reach it).
void settings_screen_create(void);

#ifdef __cplusplus
}
#endif
