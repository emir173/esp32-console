#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>

#include "../hardware_config.h"

TFT_eSPI tft = TFT_eSPI();

namespace {
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t SD_FREQUENCY = 20000000;

void drawCornerMarker(int x, int y, int dx, int dy) {
  tft.drawLine(x, y, x + dx * 14, y, TFT_WHITE);
  tft.drawLine(x, y, x, y + dy * 14, TFT_WHITE);
}

void drawColorBars(int y, int height) {
  const uint16_t colors[] = {
    TFT_RED, TFT_GREEN, TFT_BLUE, TFT_CYAN,
    TFT_MAGENTA, TFT_YELLOW, TFT_WHITE, TFT_BLACK
  };
  const int barWidth = tft.width() / 8;

  for (int i = 0; i < 8; ++i) {
    const int x = i * barWidth;
    const int width = (i == 7) ? tft.width() - x : barWidth;
    tft.fillRect(x, y, width, height, colors[i]);
  }
}

void printStatusLine(int y, const char* label, bool passed) {
  tft.setCursor(8, y);
  tft.setTextColor(passed ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.print(label);
  tft.print(passed ? ": PASS" : ": FAIL");
}
}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);

  // GPIO16 must drive an external backlight switch or its enable input.
  // For an always-on prototype, connect LCD pin 8 to the regulated 3.3 V
  // rail instead and leave GPIO16 disconnected.
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  pinMode(TFT_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);

  const bool geometryPassed = tft.width() == 320 && tft.height() == 240;

  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_WHITE);
  drawCornerMarker(2, 2, 1, 1);
  drawCornerMarker(tft.width() - 3, 2, -1, 1);
  drawCornerMarker(2, tft.height() - 3, 1, -1);
  drawCornerMarker(tft.width() - 3, tft.height() - 3, -1, -1);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(8, 8);
  tft.print("E-OS HS280S010B DISPLAY TEST");

  tft.setCursor(8, 30);
  tft.print("Detected geometry: ");
  tft.print(tft.width());
  tft.print("x");
  tft.print(tft.height());
  printStatusLine(50, "320x240 landscape", geometryPassed);

  drawColorBars(78, 70);

  // Release the display before selecting the SD card on the shared bus.
  digitalWrite(TFT_CS, HIGH);
  const bool sdPassed = SD.begin(SD_CS, SPI, SD_FREQUENCY);
  printStatusLine(166, "microSD", sdPassed);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(8, 194);
  tft.print("Check: red green blue, all four corners,");
  tft.setCursor(8, 212);
  tft.print("no clipping, correct orientation and SD PASS.");

  Serial.print("Display geometry: ");
  Serial.print(tft.width());
  Serial.print('x');
  Serial.println(tft.height());
  Serial.print("microSD: ");
  Serial.println(sdPassed ? "PASS" : "FAIL");
}

void loop() {
  delay(1000);
}
