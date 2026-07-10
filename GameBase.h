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
#include <esp_timer.h>

// ============================================================
//  NOTE_* — Standart müzik notaları (Hz, A4=440 akort)
//  playSound(NOTE_A5, 80) gibi okunabilir çağrılar için.
//  Sadece gerçek nota frekansları; efektler (100, 400 vb.) burada DEĞİL.
// ============================================================
// ════════════════════════════════════════════════════════════════
//  E-OS SOUND PALETTE v2.0  (165 Hz - 784 Hz bandi)
// ════════════════════════════════════════════════════════════════
//  ALTIN KURALLAR:
//   1) Tum ses efektleri [165 Hz - 784 Hz] bandinda olmalidir.
//      Alt sinir 165 Hz (Mi3): piezo'nun ton kaybetmeden urettigi
//      en dusuk musical nota. 150 Hz altinda piezo "tik tik" gibi
//      metronom vurur, tonu kaybeder.
//      Ust sinir 784 Hz (Sol5): kulagi tirmalamayan en parlak nota.
//      880+ Hz keskin/rahatsiz edici, 1000+ Hz kesinlikle YASAK.
//
//   2) SURE MUHENDISLIGI (psychoacoustics — vol kontrolumuz yok):
//        165-220 Hz -> 80-150 ms  (olum, ciddi olaylar)
//        247-330 Hz -> 50-100 ms  (darbe, patlama)
//        349-440 Hz -> 25-60 ms   (ates, menu, aksiyon)
//        523-659 Hz -> 20-50 ms   (coin, item, kucuk odul)
//        784 Hz     -> 15-40 ms   (sadece zafer/level-up doruk notasi)
//      Yuksek frekanslar (650-784 Hz) ASLA 40 ms'i gecmemeli.
//      100 ms'den uzun efektler SADECE olum/boss olaylari icin.
//
//   3) ZAFER TRIAD (Do-Mi-Sol yukselen majör arpej):
//        playSound(523, 50);   // Do5
//        delay(60);            // = playSound suresi + 10 ms (kritik!)
//        playSound(659, 50);   // Mi5
//        delay(60);
//        playSound(784, 40);   // Sol5 (tavan — en kisa, en parlak)
//      Aralardaki delay() her zaman playSound() suresinden 10 ms
//      uzun olmalidir ki notalar birbirine girmesin.
//
//   4) STANDART UI SESLERI (tutarlilik — tum modüllerde ayni):
//        Menu baslat (A ile oyun ac) : 659 Hz / 50 ms  (Mi5)
//        Restart (GameOver -> basla) : 587 Hz / 50 ms  (Re5)
//        Pause -> Devam et           : 587 Hz / 40 ms  (Re5)
//        Pause acma                  : 400 Hz / 50 ms  (Sol4)
//        OS'a don (BTN_B)            : 400 Hz / 50 ms  (Sol4)
//
//   5) YASAKLI FREKANSLAR:
//        >784 Hz      -> 784'e cek veya KALDIR
//        880-1319 Hz  -> 523-659'a cek
//        2500+ Hz     -> tamamen KALDIR (kulak katili)
//        <165 Hz      -> 165'e cek (piezo tonu kaybeder)
// ════════════════════════════════════════════════════════════════
//  BASS / TOK KATMANI  (165-330 Hz) — "Karanlik, Ciddi, Tehdit"
// ════════════════════════════════════════════════════════════════
//    E3 = 165 Hz  | Olum sesi (en alcak ton = en ciddi olay)
//    G3 = 196 Hz  | Hasar alma, can kaybi
//    A3 = 220 Hz  | Patlama, dusman yok etme (tok darbe)
//    B3 = 247 Hz  | Kalkan kirilma, agir carpisma
//    C4 = 262 Hz  | Gecis sesi (menu kapanma, pause)
//    E4 = 330 Hz  | Dusman ezme (stomp), lazer atesi
// ----------------------------------------------------------------
//  ORTA KATMAN  (349-523 Hz) — "Notr, Bilgilendirici"
// ----------------------------------------------------------------
//    F4 = 349 Hz  | Menu navigasyonu (yukari/asagi)
//    G4 = 392 Hz  | Buton onay, secim
//    A4 = 440 Hz  | Ates etme (pew!), standart aksiyon
//    C5 = 523 Hz  | Item toplama, kucuk odul
// ----------------------------------------------------------------
//  PARLAK KATMAN  (587-784 Hz) — "Nese, Basari, Odul"
// ----------------------------------------------------------------
//    D5 = 587 Hz  | Ziplama, restart, kucuk basari
//    E5 = 659 Hz  | Altin/coin toplama (ding!), menu baslat
//    G5 = 784 Hz  | Zafer fanfare tavani, level-up, skor ding
// ════════════════════════════════════════════════════════════════

// --- BASS KATMANI (Tok, ciddi, karanlik) 165-330 Hz ---
constexpr uint16_t NOTE_E3 = 165;   // Olum, en ciddi olay
constexpr uint16_t NOTE_G3 = 196;   // Hasar, can kaybi
constexpr uint16_t NOTE_A3 = 220;   // Patlama, yikim
constexpr uint16_t NOTE_B3 = 247;   // Agir darbe
constexpr uint16_t NOTE_C4 = 262;   // Gecis, menu kapanis
constexpr uint16_t NOTE_E4 = 330;   // Dusman ezme, stomp

// --- ORTA KATMAN (Notr, bilgilendirici) 349-523 Hz ---
constexpr uint16_t NOTE_F4 = 349;   // Menu navigasyonu
constexpr uint16_t NOTE_G4 = 392;   // Buton onay, secim
constexpr uint16_t NOTE_A4 = 440;   // Ates etme, standart aksiyon
constexpr uint16_t NOTE_C5 = 523;   // Item toplama, kucuk odul

// --- PARLAK KATMAN (Nese, odul, basari) 587-784 Hz ---
constexpr uint16_t NOTE_D5 = 587;   // Ziplama, restart
constexpr uint16_t NOTE_E5 = 659;   // Altin/coin (ding!), menu baslat
constexpr uint16_t NOTE_G5 = 784;   // Zafer, level-up (TAVAN NOTA)

// ════════════════════════════════════════════════════════════════
//  BUZZER SES ALTYAPISI — LEDC + yazilimsal VOLUME (v3.2)
// ════════════════════════════════════════════════════════════════
//  Neden LEDC? Arduino `tone()` sabit ~%50 duty kullanir, volume
//  kontrolu YOK. Pasif buzzer'da ses seviyesi PWM duty-cycle ile
//  ayarlanabilir: dusuk duty = kisik ses. Bu yuzden tum buzzer
//  cikisi tek merkezden LEDC ile yonetilir.
//
//  Onemli: `tone()`/`noTone()` her cagirida pini ledcAttach/Detach
//  eder ve ayri bir task'ta calisir → bizim kalici ledcAttach'imizle
//  CAKISIR. Bu yuzden projede raw tone()/noTone() KULLANILMAZ; yerine
//  osBuzzerPlay / osBuzzerTone / osBuzzerOff kullanilir.
//
//  Ses seviyesi 3 kademe (NVS "snd_vol"): 0=Kapali, 1=Kisik, 2=Yuksek.
//  Ayri "sound_en" (bool) mute bayragi da korunur (oyunlar onu okur);
//  SETTINGS ikisini senkron tutar (OFF→sound_en=false).
// ════════════════════════════════════════════════════════════════

static const uint8_t  OS_BUZZER_RES = 10;    // 10-bit — core tone() ile ayni
static const uint16_t OS_DUTY_LOW   = 256;   // %25 duty (kisik) = -3dB, HIGH'a en yakin
                                             // TEMIZ tini. Pasif buzzer + DAC yok →
                                             // her kisma tiniyi bozar; %25 = temiz tini +
                                             // A/B'de fark edilir kisilma dengesi (secildi).
                                             // Daha kisik istenirse: 154(%15, -6.9dB) ama
                                             // doom vb. darbeli sesler cilizlasir.
static const uint16_t OS_DUTY_HIGH  = 512;   // ~%50 duty  (yuksek = eski ses = tone())

static uint8_t            osSoundVolume = 2;         // 0/1/2 (ilk tonda NVS'ten yuklenir)
static bool               osAudioReady  = false;
static esp_timer_handle_t osToneTimer   = nullptr;

// Suredolunca cagrilir → buzzer'i sustur (non-blocking oto-durdurma)
static void osToneStopCb(void *) { ledcWrite(BUZZER, 0); }

// Aktif ses seviyesine karsilik gelen duty (0 = kapali)
inline uint16_t osCurrentDuty() {
    if (osSoundVolume == 1) return OS_DUTY_LOW;
    if (osSoundVolume >= 2) return OS_DUTY_HIGH;
    return 0;
}

// ------------------------------------------------------------
//  osAudioInit — LEDC + oto-durdurma timer'ini bir kez hazirla
//  Lazy: ilk ses cagrisinda otomatik calisir; osInitBuzzer da cagirir.
//  NVS'ten "snd_vol" seviyesini yukler.
// ------------------------------------------------------------
inline void osAudioInit() {
    if (osAudioReady) return;
    Preferences prefs;
    prefs.begin("os", true);
    osSoundVolume = prefs.getUChar("snd_vol", 2);
    prefs.end();

    ledcAttach(BUZZER, 2000, OS_BUZZER_RES);   // frekans ton basina degistirilir

    esp_timer_create_args_t args = {};
    args.callback = &osToneStopCb;
    args.name     = "ostone";
    esp_timer_create(&args, &osToneTimer);

    osAudioReady = true;
}

// ------------------------------------------------------------
//  osBuzzerPlay — Non-blocking ton, dur ms sonra otomatik susar.
//  osPlaySound'un motoru. Aktif volume duty'siyle calar.
// ------------------------------------------------------------
inline void osBuzzerPlay(uint16_t freq, uint32_t dur) {
    osAudioInit();
    if (osSoundVolume == 0 || freq == 0) { ledcWrite(BUZZER, 0); return; }
    ledcWriteTone(BUZZER, freq);          // frekansi ayarla (duty'yi %50 yapar)
    ledcWrite(BUZZER, osCurrentDuty());   // duty'yi volume icin override et
    if (osToneTimer) {
        esp_timer_stop(osToneTimer);      // onceki tonu iptal et (yeniden baslat)
        esp_timer_start_once(osToneTimer, (uint64_t)dur * 1000ULL);
    }
}

// ------------------------------------------------------------
//  osBuzzerTone — Surekli ton (oto-durdurma YOK). Cagiran osBuzzerOff
//  ile durdurur. Orn. mode7 motor sesi (her karede frekans guncellenir).
// ------------------------------------------------------------
inline void osBuzzerTone(uint16_t freq) {
    osAudioInit();
    if (osSoundVolume == 0 || freq == 0) { ledcWrite(BUZZER, 0); return; }
    if (osToneTimer) esp_timer_stop(osToneTimer);   // surekli ton: oto-stop iptal
    ledcWriteTone(BUZZER, freq);
    ledcWrite(BUZZER, osCurrentDuty());
}

// ------------------------------------------------------------
//  osBuzzerOff — Buzzer'i aninda sustur (raw noTone(BUZZER) yerine)
// ------------------------------------------------------------
inline void osBuzzerOff() {
    if (!osAudioReady) return;            // hic ses calmadiysa zaten sessiz
    if (osToneTimer) esp_timer_stop(osToneTimer);
    ledcWrite(BUZZER, 0);
}

// ------------------------------------------------------------
//  osLoadVolume / osSaveVolume / osSetVolume — ses seviyesi (0/1/2)
//  osSetVolume: calisma aninda uygula (launcher SETTINGS canli onizleme).
// ------------------------------------------------------------
inline uint8_t osLoadVolume(uint8_t defaultVal = 2) {
    Preferences prefs;
    prefs.begin("os", true);
    uint8_t v = prefs.getUChar("snd_vol", defaultVal);
    prefs.end();
    return v;
}
inline void osSaveVolume(uint8_t vol) {
    Preferences prefs;
    prefs.begin("os", false);
    prefs.putUChar("snd_vol", vol);
    prefs.end();
}
inline void osSetVolume(uint8_t vol) { osSoundVolume = vol; }

// ============================================================
//  osReturnToOS — OS Launcher'a dönüş
//  Ekranı temizler, mesaj gösterir, OTA partition'a geçip restart eder.
//  tft: Oyunun TFT_eSPI nesnesi
// ============================================================
inline void osReturnToOS(TFT_eSPI &tft, bool playExitSound = false) {
    if (playExitSound) osBuzzerPlay(400, 50);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(26, 60);
    tft.print("Returning to OS...");
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
        osBuzzerPlay(freq, dur);   // LEDC + volume (oto-durdurma)
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
        osBuzzerTone(freq);   // LEDC + volume, surekli — osUpdateSound durdurur
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
        osBuzzerOff();
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
    osAudioInit();   // LEDC pinini bagla + oto-durdurma timer'i + NVS volume
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

// ============================================================
//  ORTAK EKRAN OVERLAY'LERI (v3.1)
//  Onemli: TFT_eSprite, TFT_eSPI'den turer. Bu yuzden bu
//  fonksiyonlar hem oyunun `tft` nesnesini hem de `canvas`/`fb`
//  gibi TFT_eSprite framebuffer'larini kabul eder.
//  Kullanim: oyun karesini ciz -> overlay cagir -> pushSprite.
//  Tum metinler Ingilizce (E-OS EN yerellestirme).
// ============================================================

// ------------------------------------------------------------
//  osDrawPause — Ortak PAUSE kutusu (160x128 yerlesim)
//  gfx:    tft veya TFT_eSprite
//  accent: kutu kenari + baslik rengi (oyun temasina gore)
// ------------------------------------------------------------
inline void osDrawPause(TFT_eSPI &gfx, uint16_t accent = TFT_YELLOW) {
    gfx.fillRoundRect(25, 34, 110, 60, 5, TFT_BLACK);
    gfx.drawRoundRect(25, 34, 110, 60, 5, accent);

    // Kutu ekran ortasinda (genislik 160) → merkez x = 80
    gfx.setTextSize(2);
    gfx.setTextColor(accent);
    gfx.setCursor(80 - gfx.textWidth("PAUSE") / 2, 41);
    gfx.print("PAUSE");

    // Iki menu satiri ortak sol-x'te hizali; blok ortalanir (uzun satira gore)
    gfx.setTextSize(1);
    int16_t menuX = 80 - gfx.textWidth("[A] Continue") / 2;
    gfx.setTextColor(TFT_WHITE);
    gfx.setCursor(menuX, 66);
    gfx.print("[A] Continue");
    gfx.setCursor(menuX, 79);
    gfx.print("[B] OS Menu");
}

// ------------------------------------------------------------
//  OsStat — Game-over ekraninda gosterilecek tek bir satir.
//  Her oyun kendi satirlarini verir; overlay bunlari iki-nokta
//  (":") hizali bir sutunda ortalar. value bir metindir; sayi da
//  ("3090") serbest ifade de ("Floor 19") olabilir.
// ------------------------------------------------------------
struct OsStat {
    const char *label;       // orn. "Floor", "Score", "Best"
    const char *value;       // orn. "7", "3090", "Floor 19"
    uint16_t    labelColor;  // etiket rengi (normal: TFT_LIGHTGREY)
    uint16_t    valueColor;  // deger rengi  (vurgu icin farkli)
};

// ------------------------------------------------------------
//  osDrawGameOver — Ortak GAME OVER / WIN ekrani (veri-gudumlu)
//  gfx:    tft veya TFT_eSprite
//  win:    true = kazandi (yesil cerceve), false = kaybetti (kirmizi)
//  stats:  gosterilecek satir dizisi (her oyun kendi verisini verir)
//  count:  satir sayisi (0 = skorsuz, orn. DOOM sadece "GAME OVER")
//  badge:  satirlarin altinda ortalanan vurgu metni (orn. "NEW BEST!"),
//          gerekmiyorsa nullptr
//  Cerceve + ortalanmis baslik + iki-nokta hizali tablo + ortalanmis
//  buton cubugunu OS cizer; icerik oyundan gelir → tum oyunlar ayni
//  gorunumde ama esnek satir sayisiyla.
// ------------------------------------------------------------
inline void osDrawGameOver(TFT_eSPI &gfx, bool win,
                           const OsStat *stats, uint8_t count,
                           const char *badge = nullptr,
                           uint16_t badgeColor = TFT_MAGENTA) {
    uint16_t frame = win ? TFT_GREEN : TFT_RED;
    gfx.fillRoundRect(15, 6, 130, 120, 5, TFT_BLACK);
    gfx.drawRoundRect(15, 6, 130, 120, 5, frame);
    gfx.drawRoundRect(16, 7, 128, 118, 4, frame);

    // Baslik ekran ortasinda (merkez x = 80)
    const char *title = win ? "YOU WIN!" : "GAME OVER";
    gfx.setTextSize(2);
    gfx.setTextColor(frame);
    gfx.setCursor(80 - gfx.textWidth(title) / 2, 16);
    gfx.print(title);

    // Satir tablosu: iki-nokta sutunu hizali, blok yatayda ortali,
    // dikeyde baslik ile buton cubugu arasina otomatik yerlesir.
    gfx.setTextSize(1);
    if (count > 0) {
        int16_t maxLabelW = 0, maxValueW = 0;
        for (uint8_t i = 0; i < count; i++) {
            int16_t lw = gfx.textWidth(stats[i].label);
            int16_t vw = gfx.textWidth(stats[i].value);
            if (lw > maxLabelW) maxLabelW = lw;
            if (vw > maxValueW) maxValueW = vw;
        }
        const int16_t colonW = gfx.textWidth(":");
        const int16_t gap = 6;
        int16_t blockW = maxLabelW + gap + colonW + gap + maxValueW;
        int16_t leftX  = 80 - blockW / 2;
        int16_t colonX = leftX + maxLabelW + gap;
        int16_t valueX = colonX + colonW + gap;

        int16_t rowH = 15;
        int16_t areaTop = 40;
        int16_t areaBot = badge ? 88 : 100;   // rozet varsa yer birak
        // Satirlar alana sigmiyorsa (orn. 4 satir + rozet) araligi daralt,
        // yoksa son satir rozetin ustune tasar
        if (count * rowH > areaBot - areaTop) rowH = (areaBot - areaTop) / count;
        int16_t startY  = areaTop + ((areaBot - areaTop) - count * rowH) / 2;
        if (startY < areaTop) startY = areaTop;

        for (uint8_t i = 0; i < count; i++) {
            int16_t y = startY + i * rowH;
            gfx.setTextColor(stats[i].labelColor);
            gfx.setCursor(leftX, y);  gfx.print(stats[i].label);
            gfx.setCursor(colonX, y); gfx.print(":");
            gfx.setTextColor(stats[i].valueColor);
            gfx.setCursor(valueX, y); gfx.print(stats[i].value);
        }
    }

    // Vurgu rozeti (orn. NEW BEST!) — satirlarin altinda ortali
    if (badge) {
        gfx.setTextColor(badgeColor);
        gfx.setCursor(80 - gfx.textWidth(badge) / 2, 92);
        gfx.print(badge);
    }

    // Butonlar ortak sol-x'te hizali; blok ortalanir (uzun satira gore)
    gfx.setTextColor(TFT_LIGHTGREY);
    int16_t menuX = 80 - gfx.textWidth("[A] Play Again") / 2;
    gfx.setCursor(menuX, 104);
    gfx.print("[A] Play Again");
    gfx.setCursor(menuX, 114);
    gfx.print("[B] OS Menu");
}

// ------------------------------------------------------------
//  osDrawGameOver — Kisayol overload'u (basit skor/rekor oyunlari)
//  score < 0  → skorsuz (sadece "GAME OVER" + butonlar, orn. DOOM)
//  best  >= 0 → "Best" satiri; score >= best ise "NEW BEST!" rozeti
//  Icten yukaridaki veri-gudumlu surumu cagirir → ayni hizali gorunum.
// ------------------------------------------------------------
inline void osDrawGameOver(TFT_eSPI &gfx, bool win, long score = -1, long best = -1) {
    if (score < 0) {                      // skorsuz oyunlar
        osDrawGameOver(gfx, win, (const OsStat *)nullptr, 0);
        return;
    }
    char sbuf[16], bbuf[16];
    snprintf(sbuf, sizeof(sbuf), "%ld", score);
    snprintf(bbuf, sizeof(bbuf), "%ld", best);
    OsStat rows[2] = {
        { "Score", sbuf, TFT_WHITE, TFT_YELLOW },
        { "Best",  bbuf, TFT_WHITE, TFT_GREEN  },
    };
    bool newBest = (best >= 0 && score >= best && score > 0);
    osDrawGameOver(gfx, win, rows, best >= 0 ? 2 : 1,
                   newBest ? "NEW BEST!" : nullptr);
}
