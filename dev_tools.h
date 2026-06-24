#pragma once
// ============================================================
//  E-OS — Gelistirici Araclari (Dev Tools)
//  FPS HUD + Screenshot (BMP → SD Kart)
//
//  Kullanim:
//    1. Oyun .ino dosyasinin basina:  #include "../dev_tools.h"
//    2. setup() icinde SD baslat:     initDevTools(tft);
//       (SPI.begin sonrasi, tft.init ONCE)
//    3. loop() sonunda HUD ciz:       drawDevHUD(canvas);
//    4. Screenshot: pushSprite ONCESI checkScreenshot(canvas);
//
//  Renk modlari (oyuna gore set edin):
//    setScreenshotMode(SCR_BGR_SWAP);   // RGB() makrosu (wireframe3d, galacticstrike, mode7, platformer)
//    setScreenshotMode(SCR_RGB_SWAP);   // TFT_* sabitleri (snake, arkanoid, flappy, pacman, spaceInvaders)
//    setScreenshotMode(SCR_BGR_NOSWAP); // RGB_FIX() + direkt fb (doom)
//
//  NOT: Screenshot sonrasi oyun donabilir (SPI mutex sorunu).
//       Screenshot SD kartta kalici. Manuel reset atin.
// ============================================================

// Pin guard — oyun kendi pinlerini tanimladiysa tekrar include etme
#ifndef TFT_CS
  #include "../hardware_config.h"
#endif

#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>

// ============ Screenshot Renk Modlari ============
#define SCR_BGR_SWAP    0  // RGB() makrosu + sprite (byte swap + BGR565)
#define SCR_RGB_SWAP    1  // TFT_* sabitleri + sprite (byte swap + RGB565)
#define SCR_BGR_NOSWAP  2  // RGB_FIX() + direkt fb (swap yok + BGR565)

static int _scr_color_mode = SCR_BGR_SWAP;

void setScreenshotMode(int mode) {
    _scr_color_mode = mode;
}

// ============ FPS Sayaci ============
static uint32_t _fps_lastMs = 0;
static uint16_t _fps_count = 0;
static uint16_t _fps_value = 0;

void updateFPS() {
    _fps_count++;
    uint32_t now = millis();
    if (now - _fps_lastMs >= 1000) {
        _fps_value = _fps_count;
        _fps_count = 0;
        _fps_lastMs = now;
    }
}

// ============ Dev HUD ============
void drawDevHUD(TFT_eSprite &spr) {
    updateFPS();
    spr.setTextSize(1);
    spr.setTextColor(TFT_WHITE);
    spr.setCursor(2, 2);
    spr.printf("FPS:%d", _fps_value);
}

// ============ Screenshot ============
static bool _sd_ready = false;
static int _screenshot_num = 0;
static TFT_eSPI *_tft = nullptr;

void devToolsTick() {
    // No-op
}

void initDevTools(TFT_eSPI &tft) {
    _tft = &tft;

    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    uint32_t t0 = millis();
    _sd_ready = SD.begin(SD_CS, SPI, 4000000);
    if (millis() - t0 > 2000) _sd_ready = false;

    if (_sd_ready) {
        if (!SD.exists("/screenshots")) SD.mkdir("/screenshots");
        while (_screenshot_num < 999) {
            char fname[40];
            sprintf(fname, "/screenshots/SCR_%03d.bmp", _screenshot_num + 1);
            if (!SD.exists(fname)) break;
            _screenshot_num++;
        }
    }
}

// BMP pixel donusumu — renk moduna gore
static void _writeBmpPixels(File &f, uint16_t *buf, uint16_t w, uint16_t h) {
    uint32_t rowSize = ((w * 3) + 3) & ~3;
    uint8_t rowBuf[480 + 4];

    for (int y = h - 1; y >= 0; y--) {
        int idx = 0;
        for (int x = 0; x < w; x++) {
            uint16_t pixel = buf[y * w + x];

            uint8_t r5, g6, b5;

            if (_scr_color_mode == SCR_BGR_NOSWAP) {
                // RGB_FIX() + direkt fb: BGR565, byte swap YOK
                b5 = (pixel >> 11) & 0x1F;
                g6 = (pixel >> 5) & 0x3F;
                r5 = pixel & 0x1F;
            } else {
                // Sprite: drawPixel byte swap yapmis, geri al
                pixel = (pixel << 8) | (pixel >> 8);

                if (_scr_color_mode == SCR_BGR_SWAP) {
                    // RGB() makrosu: BGR565 (B=ust, R=alt)
                    b5 = (pixel >> 11) & 0x1F;
                    g6 = (pixel >> 5) & 0x3F;
                    r5 = pixel & 0x1F;
                } else {
                    // TFT_* sabitleri: RGB565 (R=ust, B=alt)
                    r5 = (pixel >> 11) & 0x1F;
                    g6 = (pixel >> 5) & 0x3F;
                    b5 = pixel & 0x1F;
                }
            }

            // BMP BGR: B, G, R
            rowBuf[idx++] = (b5 << 3) | (b5 >> 2);
            rowBuf[idx++] = (g6 << 2) | (g6 >> 4);
            rowBuf[idx++] = (r5 << 3) | (r5 >> 2);
        }
        while (idx < (int)rowSize) rowBuf[idx++] = 0;
        f.write(rowBuf, rowSize);
    }
}

// BMP header yaz
static void _writeBmpHeader(File &f, uint16_t w, uint16_t h) {
    uint32_t rowSize = ((w * 3) + 3) & ~3;
    uint32_t imgSize = rowSize * h;
    uint32_t fileSize = 54 + imgSize;

    uint8_t bmpFileHdr[14] = {
        'B', 'M',
        (uint8_t)(fileSize),       (uint8_t)(fileSize >> 8),
        (uint8_t)(fileSize >> 16), (uint8_t)(fileSize >> 24),
        0, 0, 0, 0,
        54, 0, 0, 0
    };
    f.write(bmpFileHdr, 14);

    uint8_t bmpInfoHdr[40] = {0};
    bmpInfoHdr[0] = 40;
    bmpInfoHdr[4]  = (uint8_t)(w);
    bmpInfoHdr[5]  = (uint8_t)(w >> 8);
    bmpInfoHdr[6]  = (uint8_t)(w >> 16);
    bmpInfoHdr[7]  = (uint8_t)(w >> 24);
    bmpInfoHdr[8]  = (uint8_t)(h);
    bmpInfoHdr[9]  = (uint8_t)(h >> 8);
    bmpInfoHdr[10] = (uint8_t)(h >> 16);
    bmpInfoHdr[11] = (uint8_t)(h >> 24);
    bmpInfoHdr[12] = 1;
    bmpInfoHdr[14] = 24;
    bmpInfoHdr[20] = (uint8_t)(imgSize);
    bmpInfoHdr[21] = (uint8_t)(imgSize >> 8);
    bmpInfoHdr[22] = (uint8_t)(imgSize >> 16);
    bmpInfoHdr[23] = (uint8_t)(imgSize >> 24);
    f.write(bmpInfoHdr, 40);
}

// SPI reset + SD re-init (screenshot oncesi)
static bool _resetSPIAndSD() {
    Serial.println("[SD] Full SPI reset...");
    Serial.flush();
    SPI.end();
    delay(20);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
    delay(20);
    Serial.println("[SD] SPI reset done.");
    Serial.flush();

    Serial.println("[SD] Re-init SD...");
    Serial.flush();
    SD.end();
    delay(10);
    {
        uint32_t t0 = millis();
        _sd_ready = SD.begin(SD_CS, SPI, 4000000);
        if (millis() - t0 > 3000) _sd_ready = false;
    }
    if (!_sd_ready) {
        Serial.println("[SD] SD.begin failed!");
        return false;
    }
    Serial.println("[SD] SD re-init OK.");
    return true;
}

// Screenshot al — TFT_eSprite icin (sprite oyunlari)
bool takeScreenshot(TFT_eSprite &spr) {
    return false; // Sistem kapatildi
}

// Screenshot al — uint16_t* framebuffer icin (doom)
bool takeScreenshotFB(uint16_t *buf, int w, int h) {
    return false; // Sistem kapatildi
}

// BTN_D screenshot kontrolu — pushSprite ONCESI cagir
bool checkScreenshot(TFT_eSprite &spr) {
    // Screenshot işimiz bittiği için sistem kapatıldı (Yanlışlıkla basıp dondurmayı önler)
    return false; 
}

// BTN_D screenshot kontrolu — framebuffer icin (doom)
bool checkScreenshotFB(uint16_t *buf, int w, int h) {
    // Screenshot işimiz bittiği için sistem kapatıldı
    return false; 
}
