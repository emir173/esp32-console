#pragma once

// TFT_eSPI configuration for the HS280S010B display module.
// Panel: 2.8-inch ST7789V, 240x320 portrait / 320x240 landscape.
#define USER_SETUP_INFO "E-OS HS280S010B ST7789V"

#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// LCD and microSD share the same SPI bus. Each device has its own CS pin.
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_MISO 14
#define TFT_CS   15
#define TFT_DC   13
#define TFT_RST  7

// GPIO16 controls an external high-side backlight switch/load switch.
// Do not power the display backlight directly from this GPIO.
#define TFT_BL 16
#define TFT_BACKLIGHT_ON HIGH

// Start conservatively for wiring/prototype reliability. Increase only after
// the display and shared SD bus pass the hardware test.
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

// The color-order and inversion settings are deliberately left at the
// TFT_eSPI ST7789 defaults. The display test makes either mismatch obvious.

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_GFXFF
