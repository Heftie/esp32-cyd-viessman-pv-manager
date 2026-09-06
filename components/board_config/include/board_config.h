#pragma once

// Select exactly one target board by uncommenting its #define below, then
// rebuild with `idf.py fullclean && idf.py build` — switching boards changes
// which panel-driver component gets linked in (see components/lcd/idf_component.yml),
// so a plain incremental build can leave stale config/objects around.
//
// This define gates pin assignments, resolution and mirroring in
// main/hardware.h, and the panel driver selection in components/lcd/lcd.c.
// Every other component (components/touch included) only ever sees the
// per-board values main/demo.c reads out of hardware.h and passes in via
// config structs — this is the one header that reaches board identity
// itself, kept in its own component so components/lcd can depend on it
// too without depending on all of `main`.

// ESP32-2432S028R, 2.8" 240x320 — newer boards with both micro-USB and
// USB-C connectors, ST7789 panel driver. Default.
#define BOARD_CYD_2432S028R_ST7789

// ESP32-2432S028R, 2.8" 240x320 — older boards with micro-USB only,
// ILI9341 panel driver.
// #define BOARD_CYD_2432S028R_ILI9341

// ESP32-3248S035R, 3.5" 320x480, ST7796 panel driver. Resistive touch
// (XPT2046) shares the LCD's SPI bus on this board.
// #define BOARD_CYD_3248S035R
