# ESP32 Cheap Yellow Display (CYD) — ESP32-3248S035R Pinout Reference

Board: **ESP32-3248S035R** (3.5", 320x480, ST7796 SPI panel, resistive XPT2046 touch)

> ✅ **Flash-tested on physical hardware** (2026-09-04) — pin assignments,
> mirroring and color order below are confirmed working, not just sourced
> from community docs. Pin data was originally cross-referenced from three
> independent sources
> ([espboards.dev](https://www.espboards.dev/esp32/cyd-esp32-3248s035/),
> [esp3d.io](https://esp3d.io/esp3d-tft/version_1x/hardware/esp32/sunton-35-3248/),
> [homeding.github.io](https://homeding.github.io/boards/esp32/panel-3248S035.htm))
> that agreed with each other, against a fourth
> ([androidcrypto.github.io](https://androidcrypto.github.io/ESP32-Boards/esp32_cyd_st7796_3_5_inches))
> that disagreed on backlight pin (21 vs 27) and touch bus wiring (separate vs
> shared) — the majority sources turned out correct. If you're on a different
> hardware revision, the mirror/color settings in §5 are the first things to
> re-check.

---

## 1. Display (ST7796, SPI — same physical bus as the 2.8" board)

| Function | GPIO |
|---|---|
| SCLK | GPIO 14 |
| MOSI | GPIO 13 |
| MISO | GPIO 12 |
| CS | GPIO 15 |
| DC | GPIO 2 |
| RST | Not wired (EN pin resets at power-on + software reset in `esp_lcd_panel_init()`) — same situation as the 2.8" board, see its doc §1.1 |
| Backlight | **GPIO 27** (differs from the 2.8" board's GPIO 21) |

Resolution: 320x480 (vs. 240x320 on the 2.8" board).

## 2. Resistive Touch (XPT2046)

**Shares the display's SPI bus** (SCLK/MOSI/MISO = GPIO 14/13/12 above) — only
CS and IRQ are dedicated touch pins. This differs structurally from the 2.8"
board, where touch runs on a fully separate SPI bus (GPIO 25/32/39). Because
of this, `touch_init()` on this branch does **not** call
`spi_bus_initialize()` — it reuses the bus `app_lcd_init()` already brought up.

| Function | GPIO |
|---|---|
| CS | GPIO 33 |
| IRQ | GPIO 36 (left as `GPIO_NUM_NC` in this project — same reasoning as the 2.8" board) |

## 3. RGB LED (onboard, active LOW)

| Color | GPIO |
|---|---|
| Red | GPIO 4 |
| Green | GPIO 16 |
| Blue | GPIO 17 |

Same pins as the 2.8" board. GPIO 4 is also labeled `LCD_RESET` in
`hardware.h` — harmless for the same reason as on the 2.8" board (no real
reset line is wired to that GPIO).

## 4. This Project's Actual Pin Config

Source of truth: [`main/hardware.h`](main/hardware.h).

```c
// Display (esp_lcd_st7796, SPI2_HOST)
#define LCD_SPI_CLK   GPIO_NUM_14
#define LCD_SPI_MOSI  GPIO_NUM_13
#define LCD_SPI_MISO  GPIO_NUM_12
#define LCD_DC        GPIO_NUM_2
#define LCD_CS        GPIO_NUM_15
#define LCD_RESET     GPIO_NUM_4    // inert, see note above
#define LCD_BACKLIGHT GPIO_NUM_27   // PWM via LEDC

// Touch (esp_lcd_touch_xpt2046) — shares LCD_SPI_HOST, only CS/IRQ dedicated
#define TOUCH_SPI  LCD_SPI_HOST
#define TOUCH_CS   GPIO_NUM_33
#define TOUCH_IRQ  GPIO_NUM_NC
```

**UART link (project-specific, not board-imposed):** `UART_LINK_TXD` moved
from GPIO 27 (used on the 2.8" board) to **GPIO 21**, since GPIO 27 is now
claimed by the backlight on this board. `UART_LINK_RXD` stays GPIO 22.

## 5. Orientation & Color — Confirmed On Hardware

These required correction from the initial community-sourced guesses; values
below are what actually worked when flash-tested on a real board:

- **`LCD_MIRROR_X` / `LCD_MIRROR_Y`** = `true` / `false`.
- **`TOUCH_MIRROR_X` / `TOUCH_MIRROR_Y`** = `true` / `true`.
- **RGB color order** (`rgb_ele_order` in `lcd.c`) = `LCD_RGB_ELEMENT_ORDER_BGR`
  (not `RGB` — colors came out with red/blue swapped, e.g. the demo's blue
  theme button rendered yellow, until this was set).

If you're on a different board/revision and these don't match, the demo's
touch-test screen (draws a dot where you touch, with an on-screen X/Y
readout) and its default-blue Clear button make both issues immediately
visible — flip one flag at a time and reflash to zero in on the right
combination.

---

*Compiled from community pinout pages (see links above), cross-checked
against this repo's `main/hardware.h`, `main/lcd.c`, and `main/touch.c`, then
flash-verified on physical ESP32-3248S035R hardware on 2026-09-04.*
