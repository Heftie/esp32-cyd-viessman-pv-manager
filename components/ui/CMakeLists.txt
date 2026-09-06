idf_component_register(SRCS "ui.c" "screen_nav.c" "home_screen.c" "settings_screen.c" "wifi_settings_screen.c" "wifi_connect_screen.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES lvgl__lvgl espressif__esp_lvgl_port web_server
                    )
