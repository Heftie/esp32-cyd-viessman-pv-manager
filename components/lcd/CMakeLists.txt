idf_component_register(SRCS "lcd.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_spi esp_lcd lvgl__lvgl espressif__esp_lvgl_port
                    PRIV_REQUIRES board_config espressif__esp_lcd_ili9341 espressif__esp_lcd_st7796
                    )
