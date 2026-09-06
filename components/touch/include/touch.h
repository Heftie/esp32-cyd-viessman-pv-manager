#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_lcd_touch.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // SPI bus the touch controller sits on. On some boards this is the
    // same host the LCD is already on (touch shares the LCD's SPI
    // bus/pins) — set bus_already_initialized in that case and this
    // component skips its own spi_bus_initialize(), leaving
    // spi_clk_gpio/spi_mosi_gpio/spi_miso_gpio unused. On boards where
    // touch has its own dedicated bus, leave bus_already_initialized
    // false and fill in those three pins.
    spi_host_device_t spi_host;
    bool bus_already_initialized;
    gpio_num_t spi_clk_gpio;
    gpio_num_t spi_mosi_gpio;
    gpio_num_t spi_miso_gpio;

    int clock_hz;
    gpio_num_t cs_gpio;
    gpio_num_t dc_gpio;
    gpio_num_t rst_gpio;
    gpio_num_t irq_gpio;

    // Target display resolution the raw touch reading is scaled onto
    int h_res;
    int v_res;
    bool mirror_x;
    bool mirror_y;

    // Raw controller reading range, mapped onto [0, h_res) / [0, v_res)
    uint16_t x_res_min;
    uint16_t x_res_max;
    uint16_t y_res_min;
    uint16_t y_res_max;
} touch_config_t;

// Brings up the XPT2046 touch controller over SPI. Registers a
// process_coordinates callback that rescales raw XPT2046 readings (per
// config->x_res_*/y_res_*) onto config->h_res/v_res.
esp_err_t touch_init(const touch_config_t *config, esp_lcd_touch_handle_t *tp);

#ifdef __cplusplus
}
#endif
