#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// On-device equivalent of the WiFi setup portal. Scans for nearby
// networks (web_server_scan_wifi()) and shows them as a tappable list,
// strongest signal first, or lets you enter an SSID by hand via an
// "Enter manually" row (for a hidden network, same reasoning as the
// portal's free-text SSID field). Selecting either leads to a password
// entry with an on-screen keyboard — leave it blank for an open network
// — and "Connect" calls web_server_connect_wifi() to save it and reboot,
// same as submitting the portal form. Reached from the WiFi settings
// screen's "Connect to WiFi" button.
void wifi_connect_screen_create(void);

#ifdef __cplusplus
}
#endif
