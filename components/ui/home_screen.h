#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Default screen: a UTC clock (from web_server_get_wall_clock();
// "Time not set" until NTP lands) and a Settings button. A minimal
// starting point — wire up your own status/content here as this project
// grows.
void home_screen_create(void);

#ifdef __cplusplus
}
#endif
