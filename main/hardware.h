#pragma once

#include "board_config.h"

// LCD SPI pins/host and RGB LED wiring are identical across all supported
// boards; only resolution, mirroring, backlight/touch pins and the touch
// bus topology differ per board (set below).

#if defined(BOARD_CYD_3248S035R)

// ESP32-3248S035R (3.5", ST7796 SPI panel, resistive XPT2046 touch).
// Pin assignments and orientation/color settings below are flash-tested on
// physical hardware (see ESP32-CYD-3.5-Pinout.md for sourcing details).

#define LCD_H_RES          320
#define LCD_V_RES          480

#define LCD_MIRROR_X       (true)
#define LCD_MIRROR_Y       (false)

#define LCD_BACKLIGHT      (gpio_num_t) GPIO_NUM_27
#define LCD_BACKLIGHT_LEDC_CH  (1)

#define TOUCH_X_RES_MIN 0
#define TOUCH_X_RES_MAX 320
#define TOUCH_Y_RES_MIN 0
#define TOUCH_Y_RES_MAX 480

// Touch (XPT2046) shares the LCD's SPI bus/pins on this board (CLK/MOSI/MISO
// are the LCD_SPI_* pins below) — only CS and IRQ are dedicated touch pins.
// touch_config_t.bus_already_initialized is set for this board in demo.c,
// so components/touch never calls spi_bus_initialize() itself here — the
// lcd component already brought up LCD_SPI_HOST before touch_init() runs.
#define TOUCH_SPI_SHARED_WITH_LCD 1
#define TOUCH_CLOCK_HZ (1 * 1000 * 1000) // XPT2046 SPI clock; matches the driver's own ESP_LCD_TOUCH_SPI_CLOCK_HZ
#define TOUCH_SPI      LCD_SPI_HOST
// Unused (bus_already_initialized is set instead) — only defined so
// demo.c can build one unconditional touch_config_t literal regardless
// of which board is selected.
#define TOUCH_SPI_CLK  (gpio_num_t) GPIO_NUM_NC
#define TOUCH_SPI_MOSI (gpio_num_t) GPIO_NUM_NC
#define TOUCH_SPI_MISO (gpio_num_t) GPIO_NUM_NC
#define TOUCH_CS       (gpio_num_t) GPIO_NUM_33
#define TOUCH_DC       (gpio_num_t) GPIO_NUM_NC
#define TOUCH_RST      (gpio_num_t) GPIO_NUM_NC
#define TOUCH_IRQ      (gpio_num_t) GPIO_NUM_NC /* GPIO_NUM_36, XPT driver is working better (for me) without IRQ */

#define TOUCH_MIRROR_X (true)
#define TOUCH_MIRROR_Y (true)

#elif defined(BOARD_CYD_2432S028R_ILI9341) || defined(BOARD_CYD_2432S028R_ST7789)

// ESP32-2432S028R (2.8", resistive XPT2046 touch on its own SPI bus).
// ILI9341 = older micro-USB-only boards; ST7789 = newer boards with an
// added USB-C connector. See ESP32-CYD-Pinout.md.

#define LCD_H_RES          240
#define LCD_V_RES          320

#if defined(BOARD_CYD_2432S028R_ILI9341)
#define LCD_MIRROR_X       (true)
#define LCD_MIRROR_Y       (false)
#else
#define LCD_MIRROR_X       (false)
#define LCD_MIRROR_Y       (false)
#endif

#define LCD_BACKLIGHT      (gpio_num_t) GPIO_NUM_21
#define LCD_BACKLIGHT_LEDC_CH  (1)

#define TOUCH_X_RES_MIN 0
#define TOUCH_X_RES_MAX 240
#define TOUCH_Y_RES_MIN 0
#define TOUCH_Y_RES_MAX 320

// Touch (XPT2046) runs on its own, fully independent SPI bus on this board
// (unlike the 3.5" board, which shares the LCD's bus) — demo.c leaves
// touch_config_t.bus_already_initialized false, so components/touch calls
// spi_bus_initialize() for TOUCH_SPI itself.
#define TOUCH_SPI_SHARED_WITH_LCD 0
#define TOUCH_CLOCK_HZ (1 * 1000 * 1000) // XPT2046 SPI clock; matches the driver's own ESP_LCD_TOUCH_SPI_CLOCK_HZ
#define TOUCH_SPI      SPI3_HOST
#define TOUCH_SPI_CLK  (gpio_num_t) GPIO_NUM_25
#define TOUCH_SPI_MOSI (gpio_num_t) GPIO_NUM_32
#define TOUCH_SPI_MISO (gpio_num_t) GPIO_NUM_39
#define TOUCH_CS       (gpio_num_t) GPIO_NUM_33
#define TOUCH_DC       (gpio_num_t) GPIO_NUM_NC
#define TOUCH_RST      (gpio_num_t) GPIO_NUM_NC
#define TOUCH_IRQ      (gpio_num_t) GPIO_NUM_NC /* GPIO_NUM_36, XPT driver is working better (for me) without IRQ */

#define TOUCH_MIRROR_X (true)
#define TOUCH_MIRROR_Y (false)

#else
#error "No board selected — uncomment exactly one BOARD_CYD_* define in components/board_config/include/board_config.h"
#endif

#define LCD_BITS_PIXEL     16
#define LCD_BUF_LINES      30
#define LCD_DOUBLE_BUFFER  1

#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_CMD_BITS       (8)
#define LCD_PARAM_BITS     (8)
#define LCD_SPI_HOST       SPI2_HOST
#define LCD_SPI_CLK        (gpio_num_t) GPIO_NUM_14
#define LCD_SPI_MOSI       (gpio_num_t) GPIO_NUM_13
#define LCD_SPI_MISO       (gpio_num_t) GPIO_NUM_12
#define LCD_DC             (gpio_num_t) GPIO_NUM_2
#define LCD_CS             (gpio_num_t) GPIO_NUM_15
#define LCD_RESET          (gpio_num_t) GPIO_NUM_4
#define LCD_BUSY           (gpio_num_t) GPIO_NUM_NC

// Onboard RGB LED (active LOW). Red is GPIO 4, which this project also
// labels LCD_RESET above — but GPIO 4 isn't actually wired to a display
// reset line (the panel resets via EN at power-on plus the software reset
// command in esp_lcd_panel_init(), see ESP32-CYD-Pinout.md sections 1.1/1.3).
// So driving it as the red channel here is safe and doesn't affect the display.
#define RGB_LED_RED        (gpio_num_t) GPIO_NUM_4
#define RGB_LED_GREEN      (gpio_num_t) GPIO_NUM_16
#define RGB_LED_BLUE       (gpio_num_t) GPIO_NUM_17
#define RGB_LED_ACTIVE_LOW (true)
