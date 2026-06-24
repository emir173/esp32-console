#pragma once
// ============================================================
//  E-OS GameBase — Ortak oyun fonksiyonları
//
//  Bu dosya yeni oyunlar için ortak boilerplate kodu sağlar.
//  Mevcut oyunlar kendi returnToOS()/playSound() fonksiyonlarını
//  korur. Yeni oyunlar bu fonksiyonları kullanabilir.
//
//  Kullanım:
//    #include "../hardware_config.h"
//    #include "../dev_tools.h"
//    #include "../GameBase.h"
//
//    void returnToOS() { osReturnToOS(tft); }
//    void playSound(uint16_t f, uint32_t d) { osPlaySound(f, d, soundEnabled); }
//
//  NOT: TFT_eSPI kütüphanesi ve hardware_config.h önce include edilmeli.
// ============================================================

#include <TFT_eSPI.h>
#include <esp_ota_ops.h>
#include <Preferences.h>

// ============================================================
//  osReturnToOS — OS Launcher'a dönüş
//  Ekranı temizler, mesaj gösterir, OTA partition'a geçip restart eder.
//  tft: Oyunun TFT_eSPI nesnesi
// ============================================================
inline void osReturnToOS(TFT_eSPI &tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 60);
    tft.print("Ana Menuye Donuluyor...");
    delay(500);
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }
    ESP.restart();
}

// ============================================================
//  osPlaySound — Buzzer üzerinden ton üretir
//  ESP32'nin tone() fonksiyonu süre parametresini destekler.
//  freq: Frekans (Hz), dur: Süre (ms), enabled: Ses açık mı
// ============================================================
inline void osPlaySound(uint16_t freq, uint32_t dur, bool enabled) {
    if (enabled) {
        tone(BUZZER, freq, dur);
    }
}

// ============================================================
//  osPlaySoundManual — Manuel süre kontrolü (ESP32 timer bug workaround)
//  Bazı ESP32 sürümlerinde tone() süre parametresi çalışmıyor.
//  Bu versiyon manuel millis() tabanlı süre kontrolü yapar.
//  soundEndTime: Global uint32_t değişken (oyun tanımlamalı)
// ============================================================
inline void osPlaySoundManual(uint16_t freq, uint32_t dur, bool enabled, uint32_t &soundEndTime) {
    if (enabled) {
        tone(BUZZER, freq);
        soundEndTime = millis() + dur;
    }
}

// ============================================================
//  osUpdateSound — Manuel süre kontrolü için güncelleme
//  loop() içinde çağrılmalı. Süre dolunca buzzer'ı kapatır.
//  soundEndTime: osPlaySoundManual ile set edilen zaman
// ============================================================
inline void osUpdateSound(uint32_t &soundEndTime) {
    if (soundEndTime > 0 && millis() > soundEndTime) {
        noTone(BUZZER);
        digitalWrite(BUZZER, LOW);
        soundEndTime = 0;
    }
}

// ============================================================
//  osInitButtons — Buton pinlerini INPUT_PULLUP olarak ayarla
//  Tüm oyunlarda aynı 5 buton + joystick switch
// ============================================================
inline void osInitButtons() {
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP);
    pinMode(BTN_D, INPUT_PULLUP);
    pinMode(JOY_SW, INPUT_PULLUP);
}

// ============================================================
//  osInitBuzzer — Buzzer pinini çıkış olarak ayarla
// ============================================================
inline void osInitBuzzer() {
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
}

// ============================================================
//  osOLEDOff — OLED ekranı hızlıca kapat (açılışta flicker önler)
//  I2C üzerinden SH1106 Display OFF komutu (0xAE) gönderir.
//  Wire.begin() öncesi çağrılmamalı.
// ============================================================
inline void osOLEDOff() {
    Wire.beginTransmission(OLED_I2C_ADDR);
    Wire.write(0x00);
    Wire.write(0xAE);  // Display OFF
    Wire.endTransmission();
}

// ============================================================
//  osCalibrateJoystick — Joystick merkezini kalibre et
//  numSamples örnek alıp ortalama değer döndürür.
//  centerX, centerY: Referans değişkenler (oyun tanımlamalı)
// ============================================================
inline void osCalibrateJoystick(int &centerX, int &centerY, int numSamples = 10) {
    long sumX = 0, sumY = 0;
    for (int i = 0; i < numSamples; i++) {
        sumX += analogRead(JOY_X);
        sumY += analogRead(JOY_Y);
        delay(2);
    }
    centerX = sumX / numSamples;
    centerY = sumY / numSamples;
}

// ============================================================
//  osLoadHighScore — NVS'ten yüksek skoru oku
//  key: "hs_snake", "hs_pacman" gibi oyun-specific anahtar
//  defaultVal: Kayıt yoksa dönecek değer
// ============================================================
inline int osLoadHighScore(const char *key, int defaultVal = 0) {
    Preferences prefs;
    prefs.begin("os", true);
    int val = prefs.getInt(key, defaultVal);
    prefs.end();
    return val;
}

// ============================================================
//  osSaveHighScore — NVS'e yüksek skoru kaydet
//  key: "hs_snake", "hs_pacman" gibi oyun-specific anahtar
//  val: Kaydedilecek skor
// ============================================================
inline void osSaveHighScore(const char *key, int val) {
    Preferences prefs;
    prefs.begin("os", false);
    prefs.putInt(key, val);
    prefs.end();
}

// ============================================================
//  osLoadSoundSetting — NVS'ten ses ayarını oku
//  defaultVal: Kayıt yoksa dönecek değer (true = açık)
// ============================================================
inline bool osLoadSoundSetting(bool defaultVal = true) {
    Preferences prefs;
    prefs.begin("os", true);
    bool val = prefs.getBool("sound_en", defaultVal);
    prefs.end();
    return val;
}

// ============================================================
//  osSaveSoundSetting — NVS'e ses ayarını kaydet
//  enabled: true = açık, false = kapalı
// ============================================================
inline void osSaveSoundSetting(bool enabled) {
    Preferences prefs;
    prefs.begin("os", false);
    prefs.putBool("sound_en", enabled);
    prefs.end();
}
