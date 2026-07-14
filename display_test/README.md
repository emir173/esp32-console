# HS280S010B display bring-up

This sketch verifies the modified console hardware before the launcher and
games are resized from 160x128 to 320x240.

## Display wiring

| HS280S010B | ESP32-S3 |
|---|---:|
| VCC | Regulated 3.3 V |
| GND | GND |
| LCD CS | GPIO15 |
| LCD RST | GPIO7 |
| LCD DC | GPIO13 |
| LCD MOSI | GPIO11 |
| LCD SCK | GPIO12 |
| LCD MISO | GPIO14 |
| LED | Regulated 3.3 V, or external high-side switch controlled by GPIO16 |

Touch pins 10-14 are intentionally left disconnected.

## microSD wiring

| HS280S010B J4 | ESP32-S3 |
|---|---:|
| SD CS | GPIO10 |
| SD MISO | GPIO14 |
| SD MOSI | GPIO11 |
| SD SCK | GPIO12 |

The LCD and microSD share the SPI bus and use separate chip-select pins.

## I2C wiring

GPIO41 and GPIO42 are reserved for the OLED I2C bus and are not shared with
the display:

| OLED | ESP32-S3 |
|---|---:|
| SDA | GPIO41 |
| SCL | GPIO42 |

## Expected result

With TFT_eSPI configured using the repository `User_Setup.h`, the screen must
show a 320x240 landscape test image with visible corner markers, correctly
ordered red/green/blue color bars, and `microSD: PASS` when a FAT32 card is
inserted.

The HS280S010B is a 3.3 V device. Do not connect its VCC, logic signals, or
backlight supply to 5 V.
