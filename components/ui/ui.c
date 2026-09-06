#include "ui.h"

#include "home_screen.h"
#include "screen_nav.h"
#include "settings_screen.h"
#include "wifi_connect_screen.h"
#include "wifi_settings_screen.h"

void ui_init(void)
{
    screen_register("home", home_screen_create);
    screen_register("settings", settings_screen_create);
    screen_register("wifi_settings", wifi_settings_screen_create);
    screen_register("wifi_connect", wifi_connect_screen_create);
    screen_activate(SCREEN_HOME);
}
