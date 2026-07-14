#pragma once

// SPI Pinleri
#define SPI_SCK  12
#define SPI_MOSI 11
#define SPI_MISO 14

// Chip Select Pinleri
#define TFT_CS   15
#define SD_CS    10

// HS280S010B TFT control pins
#define TFT_DC   13
#define TFT_RST  7
#define TFT_BL   16  // External backlight switch enable; never source LED current directly

// Joystick Pinleri
#define JOY_X    1
#define JOY_Y    2
#define JOY_SW   18

// Butonlar (fiziksel elmas duzen, saat yonu: A=ust, B=sag, C=alt, D=sol)
//   A=GPIO3 (ust), B=GPIO21 (sag), C=GPIO6 (alt), D=GPIO4 (sol)
#define BTN_A    3
#define BTN_B    21
#define BTN_C    6
#define BTN_D    4

// I2C Pinleri (OLED)
#define I2C_SDA  41
#define I2C_SCL  42
#define OLED_I2C_ADDR 0x3C

// Diğer Donanımlar
#define BUZZER   5
