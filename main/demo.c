#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

#include <esp_system.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_check.h>

#include <lvgl.h>
#include <esp_lvgl_port.h>

#include "lcd.h"
#include "touch.h"
#include "hardware.h"
#include "rgb_led.h"
#include "web_server.h"
#include "ui.h"

static const char *TAG="demo";

void app_main(void)
{
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_handle_t lcd_panel;
    esp_lcd_touch_handle_t tp;
    lvgl_port_touch_cfg_t touch_cfg;
    lv_display_t *lvgl_display = NULL;

    const lcd_config_t lcd_cfg = {
        .spi_host = LCD_SPI_HOST,
        .spi_clk_gpio = LCD_SPI_CLK,
        .spi_mosi_gpio = LCD_SPI_MOSI,
        .spi_miso_gpio = LCD_SPI_MISO,
        .dc_gpio = LCD_DC,
        .cs_gpio = LCD_CS,
        .reset_gpio = LCD_RESET,
        .backlight_gpio = LCD_BACKLIGHT,
        .backlight_ledc_channel = LCD_BACKLIGHT_LEDC_CH,
        .pixel_clock_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .bits_per_pixel = LCD_BITS_PIXEL,
        .h_res = LCD_H_RES,
        .v_res = LCD_V_RES,
        .draw_buf_lines = LCD_BUF_LINES,
        .double_buffer = LCD_DOUBLE_BUFFER,
        .mirror_x = LCD_MIRROR_X,
        .mirror_y = LCD_MIRROR_Y,
    };

    const touch_config_t touch_hw_cfg = {
        .spi_host = TOUCH_SPI,
        .bus_already_initialized = TOUCH_SPI_SHARED_WITH_LCD,
        .spi_clk_gpio = TOUCH_SPI_CLK,
        .spi_mosi_gpio = TOUCH_SPI_MOSI,
        .spi_miso_gpio = TOUCH_SPI_MISO,
        .clock_hz = TOUCH_CLOCK_HZ,
        .cs_gpio = TOUCH_CS,
        .dc_gpio = TOUCH_DC,
        .rst_gpio = TOUCH_RST,
        .irq_gpio = TOUCH_IRQ,
        .h_res = LCD_H_RES,
        .v_res = LCD_V_RES,
        .mirror_x = TOUCH_MIRROR_X,
        .mirror_y = TOUCH_MIRROR_Y,
        .x_res_min = TOUCH_X_RES_MIN,
        .x_res_max = TOUCH_X_RES_MAX,
        .y_res_min = TOUCH_Y_RES_MIN,
        .y_res_max = TOUCH_Y_RES_MAX,
    };

    ESP_ERROR_CHECK(lcd_display_brightness_init(&lcd_cfg));

    ESP_ERROR_CHECK(app_lcd_init(&lcd_cfg, &lcd_io, &lcd_panel));
    lvgl_display = app_lvgl_init(&lcd_cfg, lcd_io, lcd_panel);
    if (lvgl_display == NULL)
    {
        ESP_LOGI(TAG, "fatal error in app_lvgl_init");
        esp_restart();
    }

    ESP_ERROR_CHECK(touch_init(&touch_hw_cfg, &tp));
    touch_cfg.disp = lvgl_display;
    touch_cfg.handle = tp;
    touch_cfg.scale.x = 0;
    touch_cfg.scale.y = 0;
    lvgl_port_add_touch(&touch_cfg);

    ESP_ERROR_CHECK(lcd_display_brightness_set(75));
    ESP_ERROR_CHECK(lcd_display_rotate(lvgl_display, LV_DISPLAY_ROTATION_90));

    // Shown as early as possible, before any of the slower/fallible
    // hardware bring-up below — web_server's WiFi negotiation alone can
    // take up to 20s falling back to the setup AP. web_server is polled
    // through wifi_settings_screen's own refresh timer via a plain
    // static-default read that's safe to see before web_server_init()
    // has even run, so the screen just shows "Connecting..." until it
    // catches up.
    ui_init();

    const rgb_led_config_t rgb_led_cfg = {
        .red_gpio = RGB_LED_RED,
        .green_gpio = RGB_LED_GREEN,
        .blue_gpio = RGB_LED_BLUE,
        .active_low = RGB_LED_ACTIVE_LOW,
    };
    if (rgb_led_init(&rgb_led_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "rgb_led_init failed");
    }

    const web_server_config_t web_cfg = {
        .http_port = 0, // default 80
    };
    if (web_server_init(&web_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "web_server_init failed");
    }

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
