/*
 * ══════════════════════════════════════════════════════════════
 *  E-OS V2.1 — Premium Konsol Launcher
 *  PSP XMB / Nintendo Switch İlhamlı Yatay Carousel Tasarım
 * ══════════════════════════════════════════════════════════════
 *  MCU:    ESP32-S3 (Dual Core, 240 MHz)
 *  TFT:    160x128, ST7735, SPI (TFT_eSPI)
 *  OLED:   128x64, SH1106, I2C (U8g2)
 *  Flash:  16 MB  |  PSRAM: 8 MB (OPI)
 * ══════════════════════════════════════════════════════════════
 *  V2.1 Yenilikler:
 *    - PACMAN oyunu eklendi (ikon + carousel)
 *    - AYARLAR sistemi (ses açma/kapama, NVS kayıt)
 *    - Yüksek skor gösterimi (BTN_B bilgi ekranı)
 *    - playSound() sarmalayıcı fonksiyon
 * ══════════════════════════════════════════════════════════════
 */

// ============================================================
//  KÜTÜPHANE BAĞLAMALARI — Her birinin rolü aşağıda açıklanmıştır
// ============================================================
#include <TFT_eSPI.h>        // 160x128 ST7735 TFT ekran sürücüsü (SPI)
#include <SPI.h>             // Donanımsal SPI hattı (TFT + SD kart ortak veriyolu)
#include <SD.h>              // SD kart dosya sistemi — .bin oyun dosyalarını okur
#include <Wire.h>            // I2C hattı — OLED (SH1106) iletişimi için
#include <U8g2lib.h>         // OLED grafik kütüphanesi (bootloader ilerleme çubuğu)
#include <Update.h>          // OTA firmware yazma API'si — flash'a .bin aktarır
#include <Preferences.h>     // NVS (Non-Volatile Storage) — ses ayarı, son oyun, skorlar
#include <esp_ota_ops.h>     // ESP32 partition yönetimi — hızlı başlatma için boot partition seçer
#include "../hardware_config.h"  // Pin tanımları, butonlar, joystick, SPI pinleri (ortak yapılandırma)

// TFT ekran nesnesi — tüm 2D çizimler bu nesne üzerinden yapılır
TFT_eSPI tft = TFT_eSPI();
// OLED ekran nesnesi — SH1106 128x64, donanımsal I2C, reset pini yok
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// Joystick merkez değerleri — kalibrasyonda ölçülür, sıfır konumu temsil eder
// Bu değerlere göre sapma hesaplanır (analogRead - joyCenter)
int joyCenterX = 0;
int joyCenterY = 0;

// ─── RTC Bellek: ESP.restart() sonrası korunur ───
// RTC_NOINIT_ATTR: bu değişkenler reset/restart sonrası sıfırlanmaz
// Bootloader'a geçiş için "magic byte" mekanizması kullanılır
RTC_NOINIT_ATTR uint32_t bootModeMagic; // 0xDEADBEEF = bootloader
RTC_NOINIT_ATTR char bootFilename[32];  // Hangi dosya yüklenecek?

// ─── Renk düzeltme fonksiyonu (donanım RGB sıralaması) ───
// TFT_eSPI varsayılan RGB565 formatında renk üretir, ancak bu ekranın
// donanım sıralaması BGR olduğu için parametreleri ters çeviririz.
// Parametreler: r, g, b (0-255 arası klasik RGB)
// Return: 16-bit RGB565 renk kodu (TFT_eSPI fonksiyonları bunu kabul eder)
uint16_t RGB_FIX(uint8_t r, uint8_t g, uint8_t b) { return tft.color565(b, g, r); }

// ══════════════════════════════════════════════════════════════
//  OYUN BİLGİ YAPISI VE LİSTESİ
// ══════════════════════════════════════════════════════════════

// Her oyunun renk teması, dosya adı ve etiketini tutan yapı
// Bu struct bir oyunun carousel'deki tüm görsel/metadata bilgisini barındırır
struct GameInfo {
    const char* label;        // Oyun adı — carousel altında ve bilgi ekranında gösterilir
    const char* filename;     // SD karttaki .bin dosya yolu — flash'a yazılacak firmware
    uint16_t primaryColor;    // Ana tema rengi (seçili) — ikon border, başlık, accent
    uint16_t dimColor;        // Sönük rengi (seçili değil) — komşu kartlar, alt çizgi
    const char* hsKey;        // --- V2.1 EKLENTİSİ --- NVS yüksek skor anahtarı
                              // Boş string ise oyunun skor sistemi yoktur (DOOM, MODE7 vb.)
};

// --- V2.2 EKLENTİSİ --- 10 oyun + AYARLAR = 11
// Toplam carousel eleman sayısı (oyun + ayarlar sayfası dahil)
#define GAME_COUNT 11
#define SETTINGS_INDEX 10  // AYARLAR'ın carousel indeksi (son eleman)
// Not: RGB_FIX() tft başlatılmadan çağrılamaz, renkler setup sonrası atanacak
// Bu yüzden dizi global tanımlanır ama içerik initColorsAndGames() içinde doldurulur
GameInfo games[GAME_COUNT];

// ─── Global State Değişkenleri ───
// Bu blok carousel menünün tüm durumunu (state) tutar
int carouselSel = 0;         // Seçili oyun indexi (0-6)
bool menuDrawn = false;      // İlk tam çizim yapıldı mı? (frame çizimini tek seferlik yapar)
int prevSel = -1;            // Önceki seçim (partial redraw için)
uint32_t animTick = 0;       // Nabız animasyon zamanlayıcı (updatePulse iç zamanlama)
uint8_t pulseState = 0;      // Nabız kademe (0-2) — 3 fazlı parlama döngüsü
bool oledReady = false;      // OLED başlatıldı mı? (begin() öncesi çizim yapmayı engeller)
uint32_t lastOledUpdate = 0; // OLED istatistik güncelleme zamanlayıcı (1Hz)

// --- V2.1 EKLENTİSİ --- Ses açık/kapalı durumu (global, NVS'ten okunur)
bool soundEnabled = true;

// ─── Sık kullanılan renkler (setup'ta atanacak) ───
// Bu palet initColorsAndGames() içinde RGB_FIX ile donanıma uygun hale getirilir
// Burada sadece değişkenler tanımlanır; değerler setup() sonrası belirlenir
uint16_t COL_BG_DARK;        // Koyu arka plan — carousel ana zemin rengi
uint16_t COL_BG_STATUS;      // Status bar arka planı — üst bilgi şeridi
uint16_t COL_ACCENT;         // Cyan accent — vurgu rengi (logolar, progress bar)
uint16_t COL_ACCENT_DIM;     // Koyu accent — sönük vurgu (gradient kenarlar)
uint16_t COL_WHITE;          // Beyaz — metin ve parlak noktalar
uint16_t COL_GRAY;           // Gri metin — ikincil etiketler
uint16_t COL_DARK_GRAY;      // Koyu gri — pasif çizgiler, kapatma ipuçları
uint16_t COL_BLACK;          // Siyah — gölgeler, göz bebeği
uint16_t COL_SUCCESS;        // Yeşil — "yüklü", "açık", pil durumu
uint16_t COL_DANGER;         // Kırmızı — hata, "kapalı", uyarı

// ══════════════════════════════════════════════════════════════
// --- V2.1 EKLENTİSİ --- SES SARMALAYICI FONKSİYON
// ══════════════════════════════════════════════════════════════

// Tüm tone() çağrılarını saran fonksiyon — ses kapalıysa çalmaz
// Bu sayede tek bir yerden ses kontrolü yapılır; her tone() çağrısı elden geçirilmez
// Parametreler:
//   freq: buzzer frekansı (Hz) — yüksek değer =tiz ses
//   dur:  çalma süresi (ms) — süre sonunda otomatik durur
// Return: yok (void)
void playSound(uint16_t freq, uint32_t dur) {
    // Sadece ses açıksa buzzer'a sinyal gönder
    // soundEnabled NVS'ten okunur, ayarlar menüsünden değiştirilebilir
    if (soundEnabled) {
        tone(BUZZER, freq, dur);
    }
}

// ══════════════════════════════════════════════════════════════
//  RENK PALETİNİ VE OYUN LİSTESİNİ BAŞLAT
// ══════════════════════════════════════════════════════════════

// TFT başlatıldıktan sonra çağrılmalı — renk değerleri donanıma bağlı
// Bu fonksiyon hem renk paletini (COL_*) hem de games[] dizisini doldurur
// setup() içinde tft.init() sonrası çağrılır, aksi halde RGB_FIX crash eder
void initColorsAndGames() {
    // ─── Renk Paleti ───
    // Her renk RGB_FIX ile 16-bit RGB565'e çevrilir
    COL_BG_DARK    = RGB_FIX(8, 8, 20);     // Çok koyu lacivert — ana arka plan
    COL_BG_STATUS  = RGB_FIX(12, 12, 28);   // Status bar için biraz daha açık lacivert
    COL_ACCENT     = RGB_FIX(80, 180, 255); // Parlak cyan — marka vurgu rengi
    COL_ACCENT_DIM = RGB_FIX(30, 70, 100);  // Sönük cyan — kenar gradient'leri
    COL_WHITE      = RGB_FIX(255, 255, 255);
    COL_GRAY       = RGB_FIX(120, 120, 120);
    COL_DARK_GRAY  = RGB_FIX(50, 50, 50);
    COL_BLACK      = RGB_FIX(0, 0, 0);
    COL_SUCCESS    = RGB_FIX(50, 255, 80);  // Canlı yeşil — başarı/bildirim
    COL_DANGER     = RGB_FIX(255, 50, 50);  // Canlı kırmızı — hata/tehlike

    // DOOM
    games[0].label = "DOOM";
    games[0].filename = "/doom.bin";
    games[0].primaryColor = RGB_FIX(255, 100, 20);
    games[0].dimColor = RGB_FIX(80, 30, 8);
    games[0].hsKey = ""; // Doom'da skor sistemi yok

    // SNAKE
    games[1].label = "SNAKE";
    games[1].filename = "/snake.bin";
    games[1].primaryColor = RGB_FIX(50, 255, 100);
    games[1].dimColor = RGB_FIX(15, 80, 30);
    games[1].hsKey = "hs_snake";

    // FLAPPY BIRD
    games[2].label = "FLAPPY";
    games[2].filename = "/flappy.bin";
    games[2].primaryColor = RGB_FIX(255, 220, 30);
    games[2].dimColor = RGB_FIX(80, 70, 10);
    games[2].hsKey = "hs_flappy";

    // SPACE INVADERS
    games[3].label = "SPACE";
    games[3].filename = "/space.bin";
    games[3].primaryColor = RGB_FIX(220, 60, 255);
    games[3].dimColor = RGB_FIX(70, 20, 80);
    games[3].hsKey = "hs_space";

    // ARKANOID
    games[4].label = "ARKANOID";
    games[4].filename = "/arkanoid.bin";
    games[4].primaryColor = RGB_FIX(30, 210, 255);
    games[4].dimColor = RGB_FIX(10, 65, 80);
    games[4].hsKey = "hs_arkanoid";

    // --- V2.1 EKLENTİSİ --- PACMAN
    games[5].label = "PACMAN";
    games[5].filename = "/pacman.bin";
    games[5].primaryColor = RGB_FIX(255, 255, 0);
    games[5].dimColor = RGB_FIX(80, 80, 0);
    games[5].hsKey = "hs_pacman";

    // MODE7
    games[6].label = "MODE7";
    games[6].filename = "/mode7.bin";
    games[6].primaryColor = RGB_FIX(50, 50, 200);
    games[6].dimColor = RGB_FIX(20, 20, 80);
    games[6].hsKey = "";

    // 3D WIRE
    games[7].label = "3D WIRE";
    games[7].filename = "/wire3d.bin";
    games[7].primaryColor = RGB_FIX(0, 255, 0);
    games[7].dimColor = RGB_FIX(0, 80, 0);
    games[7].hsKey = "";

    // STRIKE
    games[8].label = "STRIKE";
    games[8].filename = "/strike.bin";
    games[8].primaryColor = RGB_FIX(100, 200, 255);
    games[8].dimColor = RGB_FIX(40, 80, 100);
    games[8].hsKey = "";

    // JUMP
    games[9].label = "JUMP";
    games[9].filename = "/platform.bin";
    games[9].primaryColor = RGB_FIX(255, 69, 0);
    games[9].dimColor = RGB_FIX(100, 30, 0);
    games[9].hsKey = "";

    // --- V2.1 EKLENTİSİ --- AYARLAR (Sistem uygulaması — flashlama yok!)
    games[10].label = "AYARLAR";
    games[10].filename = "/settings.bin";
    games[10].primaryColor = RGB_FIX(150, 150, 150);
    games[10].dimColor = RGB_FIX(50, 50, 50);
    games[10].hsKey = "";  // Ayarlar için skor yok
}

// ══════════════════════════════════════════════════════════════
//  ERKEN BOOTLOADER — FLASH YAZMA (DOKUNULMADI!)
// ══════════════════════════════════════════════════════════════

// SD karttan hedef .bin dosyasını OTA ile flash'a yazan bootloader
// Bu fonksiyonun mantığı AYNEN korunmuştur — sadece OLED mesajları var
//
// Akış:
//   1. OLED'de "BOOTLOADER" mesajı göster
//   2. SD karttan .bin dosyasını aç
//   3. İlk 4 byte'ı kontrol et — ESP32 firmware magic byte 0xE9 olmalı
//   4. Update.begin() ile flash yazma modunu başlat
//   5. 4KB bloklar halinde SD → flash aktar (DMA uyumlu internal RAM)
//   6. Her saniye OLED'de ilerleme yüzdesi ve çubuk göster
//   7. Başarı: NVS'e kaydet + ESP.restart() → yeni oyuna geç
//      Hata:   kullanıcı BTN_B'ye basana kadar bekle
//
// Parametre: filename — SD karttaki .bin dosya yolu (örn: "/snake.bin")
// Return: yok — başarı durumunda ESP.restart() ile çıkılır
void runEarlyBootloader(const char* filename) {
    // OLED'i aç ve bootloader başlangıç ekranını çiz
    oled.begin();
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(0, 12, "BOOTLOADER V8.3");
    oled.drawStr(0, 24, "Oyun Yukleniyor...");
    oled.sendBuffer();

    // .bin dosyasını SD karttan okuma modunda aç
    File binFile = SD.open(filename, FILE_READ);
    if (!binFile) {
        // Dosya bulunamadı — hata ekranı göster ve BTN_B beklenip geri dön
        oled.clearBuffer();
        oled.drawStr(0, 20, "HATA: DOSYA YOK!");
        oled.sendBuffer();
        while (digitalRead(BTN_B)) delay(50); // B basılana kadar bekle
        return;
    }

    size_t fileSize = binFile.size();
    // ESP32 firmware magic byte kontrolu — gecersiz .bin dosyalarini reddet
    // Geçerli bir ESP32 .bin dosyası en az 4 byte olmalı (header)
    if (fileSize < 4) {
        binFile.close();
        oled.clearBuffer();
        oled.drawStr(0, 20, "HATA: GECERSIZ DOSYA!");
        oled.sendBuffer();
        while (digitalRead(BTN_B)) delay(50);
        return;
    }
    // İlk 4 byte'ı oku — ESP32 firmware imzası 0xE9 ile başlar
    uint8_t headerCheck[4];
    binFile.read(headerCheck, 4);
    if (headerCheck[0] != 0xE9) {
        // Magic byte eşleşmedi — bu bir ESP32 firmware dosyası değil
        binFile.close();
        oled.clearBuffer();
        oled.drawStr(0, 20, "HATA: GECERSIZ BIN!");
        oled.sendBuffer();
        while (digitalRead(BTN_B)) delay(50);
        return;
    }
    // Başa sar — yazma işlemi dosyanın en başından başlamalı
    binFile.seek(0);
    // Update.begin() flash yazma modunu açar; fileSize kadar yer ayrılır
    if (!Update.begin(fileSize)) {
        // Flash'ta yeterli boş alan yok — yazma reddedildi
        binFile.close();
        oled.clearBuffer();
        oled.drawStr(0, 20, "HATA: FLASH DOLU!");
        oled.sendBuffer();
        while (digitalRead(BTN_B)) delay(50);
        return;
    }

    // PSRAM darboğazını aşmak için donanımsal DMA uyumlu İç RAM kullanıyoruz (4KB)
    // 4096 byte = standart flash sector boyutuna optimal hizalama
    uint8_t buf[4096];
    size_t written = 0;
    bool flashError = false;
    uint32_t lastDraw = 0; // OLED ekran güncelleme zamanlayıcı (1Hz)

    // SD → Flash ana aktarım döngüsü
    while (binFile.available()) {
        // 4KB blok oku
        int bytesRead = binFile.read(buf, sizeof(buf));
        if (bytesRead <= 0) break; // Okuma hatası

        // Okunan byte'ları flash'a yaz — yazılan miktar eşleşmezse hata
        if (Update.write(buf, bytesRead) != (size_t)bytesRead) {
            flashError = true;
            break;
        }
        written += bytesRead;

        // Saniyede bir kez (veya tamamlandığında) OLED ilerleme göstergesi güncelle
        uint32_t now = millis();
        if (now - lastDraw > 1000 || written == fileSize) {
            lastDraw = now;
            // Yüzde hesapla: (yazılan / toplam) * 100
            int pct = (written * 100) / fileSize;
            oled.clearBuffer();
            oled.setFont(u8g2_font_6x10_tr);
            oled.drawStr(0, 12, "FLASHING...");
            char progStr[24];
            // "45%  230K/512K" formatında ilerleme satırı
            snprintf(progStr, sizeof(progStr), "%d%%  %dK/%dK", pct, (int)(written/1024), (int)(fileSize/1024));
            oled.drawStr(0, 28, progStr);
            // Dış çerçeve (128px genişlik)
            oled.drawFrame(0, 40, 128, 14);
            // İç dolgu çubuğu — yüzdeye göre ölçeklenir (124px max)
            int barW = (124 * pct) / 100;
            if (barW > 0) oled.drawBox(2, 42, barW, 10);
            oled.sendBuffer();
        }
    }
    binFile.close();

    // Aktarım sırasında hata oluştu — flash yazma yarıda kesildi
    if (flashError) {
        oled.clearBuffer();
        oled.drawStr(0, 20, "FLASH YAZMA HATASI!");
        oled.sendBuffer();
        while (digitalRead(BTN_B)) delay(50);
        return;
    }

    // Update.end(true) yazmayı bitirip doğrulama (hash/checksum) yapar
    if (Update.end(true)) {
        // Başarılı — doğrulandı
        oled.clearBuffer();
        oled.setFont(u8g2_font_6x10_tr);
        oled.drawStr(15, 20, "YUKLEME BASARILI!");
        oled.drawStr(5, 40, "Oyun baslatiliyor...");
        // Dolu ilerleme çubuğu — %100 bitti
        oled.drawFrame(0, 50, 128, 10);
        oled.drawBox(2, 52, 124, 6);
        oled.sendBuffer();
        
        // --- V2.2: Yükleme Başarılı ekranı 1 saniye kalsın ve temizlensin ---
        if (soundEnabled) tone(BUZZER, 1500, 100); // Başarı sesi (sadece ses açıkken)
        delay(1000); 
        oled.clearBuffer();
        oled.sendBuffer();
        noTone(BUZZER);
        // ----------------------------------------------------------------
        
        // NVS'e yeni oyunu kaydet — sonraki açılışta "hızlı başlatma" için
        Preferences prefs;
        prefs.begin("os", false); // false = oku/yaz modu
        prefs.putString("last_game", filename);
        prefs.end();
        
        ESP.restart(); // Yeni oyuna geçiş yap — bu satırdan sonra kod çalışmaz
    } else {
        // Doğrulama başarısız — flash yazma bozuk olabilir
        oled.clearBuffer();
        oled.drawStr(0, 20, "DOGRULAMA HATASI!");
        oled.sendBuffer();
        while (digitalRead(BTN_B)) delay(50);
    }
}

// ══════════════════════════════════════════════════════════════
//  YARDIMCI ÇİZİM FONKSİYONLARI
// ══════════════════════════════════════════════════════════════

// Dikey gradient dikdörtgen — yukarıdan aşağıya renk geçişi (basitleştirilmiş)
// Performans için 2 piksel adımlarla çizilir
// Parametreler: x,y (sol-üst köşe), w,h (boyut), colTop (üst renk), colBot (alt renk)
// Return: yok — doğrudan TFT'ye çizer
// Not: Tam interpolasyon yerine basit 2 bölge (üst/alt) kullanılır → hız
void drawVerticalGradient(int x, int y, int w, int h, uint16_t colTop, uint16_t colBot) {
    // colTop ve colBot arasında satır satır interpolasyon
    // Basit yaklaşım: üst yarı colTop, alt yarı colBot, orta geçiş
    int halfH = h / 2;
    // 2 piksel adımlı çizim — aynı satırı iki kez çizerek yarıya düşürür
    for (int row = 0; row < h; row += 2) {
        uint16_t c;
        if (row < halfH) {
            c = colTop;
        } else {
            c = colBot;
        }
        // Her yatay çizgiyi 2 piksel yüksekliğinde çiftle (performans)
        tft.drawFastHLine(x, y + row, w, c);
        tft.drawFastHLine(x, y + row + 1, w, c);
    }
}

// Üst durum çubuğu çizimi — E-OS başlığı, SD ikonu ve accent çizgi
// 14px yüksekliğinde, ekranın en üstünde sabit duran bilgi şeridi
// İçerik: play üçgeni + "E-OS" yazısı, ses ikonu, pil ikonu, alt accent çizgi
// Parametre: yok (hepsi global/sabit değerlerle çizilir)
// Return: yok
void drawStatusBar() {
    // Status bar arka planı — 14px yüksek, 160px geniş (tam ekran genişliği)
    tft.fillRect(0, 0, 160, 14, COL_BG_STATUS);
    
    // Sol: Küçük play üçgeni + E-OS yazısı
    // Play triangle (4x5px) — marka logosu (sol üst köşe)
    tft.fillTriangle(4, 3, 4, 9, 8, 6, COL_ACCENT);
    
    // E-OS marka yazısı — üçgenin sağında
    tft.setTextSize(1);
    tft.setTextColor(COL_WHITE, COL_BG_STATUS); // Arka plan rengi vererek flicker engelle
    tft.setCursor(12, 3);
    tft.print("E-OS");
    
    // Pil placeholder ikonu — sağ üst köşede (150,4)
    // Dış çerçeve 7x6 + nub 2px
    tft.drawRect(150, 4, 7, 6, COL_GRAY);
    tft.drawFastVLine(157, 6, 2, COL_GRAY);    // Pil ucu
    tft.fillRect(151, 5, 5, 4, COL_SUCCESS); // Dolu pil — yeşil
    
    // --- V2.1 EKLENTİSİ --- Ses durumu ikonu (status bar'da)
    // Hoparlör ikonu + ses açık/kapalı durum göstergesi
    int sx = 138; // Hoparlör X pozisyonu — pil ikonunun solunda
    // Hoparlör Gövdesi (daha küçük) — 3 dikey çizgi ile koni etkisi
    tft.fillRect(sx, 6, 2, 2, COL_WHITE);
    tft.drawFastVLine(sx + 2, 5, 4, COL_WHITE);
    tft.drawFastVLine(sx + 3, 4, 6, COL_WHITE);
    
    if (soundEnabled) {
        // Ses açık — Dalgalar (yeşil, hoparlörün sağında)
        tft.drawFastVLine(sx + 5, 5, 4, COL_SUCCESS);
        tft.drawFastVLine(sx + 7, 4, 6, COL_SUCCESS);
    } else {
        // Ses kapalı — Küçük X işareti (kırmızı)
        tft.drawLine(sx + 5, 4, sx + 9, 9, COL_DANGER);
        tft.drawLine(sx + 9, 4, sx + 5, 9, COL_DANGER);
    }
    
    // Alt accent çizgi — ortadan kenarlara gradient efekti
    // 3 katmanlı: dış (koyu), orta (normal), iç (parlak) → merkezde parlaklık
    tft.drawFastHLine(0, 14, 160, COL_ACCENT_DIM);
    tft.drawFastHLine(30, 14, 100, COL_ACCENT);
    tft.drawFastHLine(55, 14, 50, RGB_FIX(120, 210, 255));
}

// Sayfa gösterge noktaları — seçili oyunu belirten noktalar
// Ekranın alt orta kısmında, her oyun için bir nokta
// Seçili nokta dolu beyaz, diğerleri boş koyu gri halka
// Parametre: selected — seçili oyun indeksi (0..GAME_COUNT-1)
// Return: yok
void drawPageDots(int selected) {
    int dotY = 98; // Noktaların Y konumu — oyun adı çizgisinin altı
    // Her nokta 4px + aralarında 6px boşluk = 10px adım
    int totalWidth = GAME_COUNT * 4 + (GAME_COUNT - 1) * 6; // Her dot 4px, arası 6px
    // Yatay olarak ortala: (ekran genişliği - toplam genişlik) / 2
    int startX = (160 - totalWidth) / 2;
    
    // Önce eski noktaları temizle — arka plan rengine boyayarak
    tft.fillRect(0, dotY - 2, 160, 8, COL_BG_DARK);
    
    // Her noktayı sırayla çiz
    for (int i = 0; i < GAME_COUNT; i++) {
        int cx = startX + i * 10 + 2; // X: adım 10px, merkez için +2
        int cy = dotY + 2;
        if (i == selected) {
            // Seçili oyun — dolu beyaz daire
            tft.fillCircle(cx, cy, 2, COL_WHITE);
        } else {
            // Diğerleri — boş koyu gri halka
            tft.drawCircle(cx, cy, 2, COL_DARK_GRAY);
        }
    }
}

// Alt kontrol ipuçları çizimi — kullanıcıya butonların ne işe yaradığını gösterir
// Ekranın en altında (y=114), ince ayraç çizgisinin altında
// "A:Baslat" solda, "B:Bilgi" sağda
// Parametre: yok
// Return: yok
void drawControlHints() {
    int hintY = 114; // İpucu satırı Y konumu — ekranın en altı
    // Üst ince ayraç çizgisi — koyu gri, kenarlardan 10px içeride
    tft.drawFastHLine(10, hintY - 2, 140, COL_DARK_GRAY);
    
    // Sol ipucu: A tuşu = Başlat
    tft.setTextSize(1);
    tft.setTextColor(COL_GRAY, COL_BG_DARK);  // Tuş etiketi gri
    tft.setCursor(8, hintY);
    tft.print("A:");
    tft.setTextColor(COL_WHITE, COL_BG_DARK); // İşlevi beyaz
    tft.print("Baslat");
    
    // Sağ ipucu: B tuşu = Bilgi
    tft.setTextColor(COL_GRAY, COL_BG_DARK);
    tft.setCursor(108, hintY);
    tft.print("B:");
    tft.setTextColor(COL_WHITE, COL_BG_DARK);
    tft.print("Bilgi");
}

// ══════════════════════════════════════════════════════════════
//  BÜYÜK İKONLAR (48x48 piksel) — Carousel merkez kartı
// ══════════════════════════════════════════════════════════════

// DOOM İkonu — Stilize kafatası (48x48)
// Kompozisyon: Yuvarlak kafatası + 2 göz (parlayan kırmızı) + burun + alt dişler + 3 alev
// Parametreler: x,y — ikonun sol-üst köşesi (48x48 alan)
// Return: yok
void drawBigIconDoom(int x, int y) {
    // Renk paleti — kemik, koyu turuncu, parlayan turuncu, kırmızı göz, gölge
    uint16_t bone  = RGB_FIX(255, 200, 150);
    uint16_t dark  = RGB_FIX(180, 80, 20);
    uint16_t glow  = RGB_FIX(255, 100, 20);
    uint16_t eye   = RGB_FIX(255, 40, 0);
    uint16_t shadow = RGB_FIX(100, 40, 10);
    
    // Kafatası gövdesi — yuvarlatılmış dikdörtgen, üst highlight ile
    tft.fillRoundRect(x + 10, y + 4, 28, 26, 8, bone);
    tft.fillRoundRect(x + 12, y + 6, 24, 22, 6, RGB_FIX(240, 190, 140));
    // Göz yuvaları — koyu arka plan
    tft.fillRoundRect(x + 14, y + 12, 8, 8, 3, RGB_FIX(30, 5, 0));
    tft.fillRoundRect(x + 26, y + 12, 8, 8, 3, RGB_FIX(30, 5, 0));
    // Kırmızı göz bebekleri
    tft.fillCircle(x + 17, y + 15, 2, eye);
    tft.fillCircle(x + 29, y + 15, 2, eye);
    // Göz parıltısı (1px beyazmsı nokta)
    tft.drawPixel(x + 16, y + 14, glow);
    tft.drawPixel(x + 28, y + 14, glow);
    // Burun (üçgen)
    tft.fillTriangle(x + 22, y + 20, x + 25, y + 20, x + 23, y + 24, RGB_FIX(30, 5, 0));
    // Üst dudak gölgesi
    tft.fillRect(x + 14, y + 27, 20, 4, shadow);
    // Alt dişler — 5 adet kemik rengi dikdörtgen
    for (int i = 0; i < 5; i++) {
        tft.fillRect(x + 15 + i * 4, y + 27, 3, 5, bone);
    }
    // Kaş çizgileri (koyu)
    tft.drawLine(x + 20, y + 5, x + 16, y + 11, dark);
    tft.drawLine(x + 30, y + 6, x + 33, y + 11, dark);
    // Alt alevler — 3 üçgen (sol, orta, sağ)
    tft.fillTriangle(x + 8, y + 47, x + 12, y + 34, x + 16, y + 47, RGB_FIX(255, 60, 0));
    tft.fillTriangle(x + 20, y + 47, x + 24, y + 32, x + 28, y + 47, RGB_FIX(255, 80, 0));
    tft.fillTriangle(x + 32, y + 47, x + 36, y + 35, x + 40, y + 47, RGB_FIX(255, 60, 0));
    // Alev uçlarında sarımsı parıltı
    tft.drawPixel(x + 12, y + 36, RGB_FIX(255, 200, 50));
    tft.drawPixel(x + 24, y + 34, RGB_FIX(255, 220, 50));
    tft.drawPixel(x + 36, y + 37, RGB_FIX(255, 200, 50));
}

// SNAKE İkonu — Kıvrımlı yılan (48x48)
// Kompozisyon: S-kıvrımlı yılan gövdesi + baş + gözler + dil + 2 elma
// Parametreler: x,y — ikonun sol-üst köşesi
// Return: yok
void drawBigIconSnake(int x, int y) {
    // Yılan renk paleti — body, açık vurgu, koyu gölge, baş, elma
    uint16_t body    = RGB_FIX(40, 200, 80);
    uint16_t bodyLt  = RGB_FIX(80, 255, 120);
    uint16_t bodyDk  = RGB_FIX(20, 140, 50);
    uint16_t head    = RGB_FIX(60, 255, 100);
    uint16_t apple   = RGB_FIX(255, 50, 40);
    
    // S kıvrımı — 4 segment: alt yatay → sağ dikey → üst yatay → sol dikey → baş
    tft.fillRoundRect(x + 6, y + 34, 24, 6, 2, body);     // Alt yatay
    tft.drawFastHLine(x + 6, y + 35, 24, bodyLt);          // Highlight
    tft.fillRoundRect(x + 26, y + 20, 6, 18, 2, body);    // Sağ dikey
    tft.drawFastVLine(x + 27, y + 20, 18, bodyLt);         // Highlight
    tft.fillRoundRect(x + 12, y + 16, 18, 6, 2, body);    // Üst yatay
    tft.drawFastHLine(x + 12, y + 17, 18, bodyLt);         // Highlight
    tft.fillRoundRect(x + 10, y + 6, 6, 14, 2, body);     // Sol dikey
    tft.drawFastVLine(x + 11, y + 6, 14, bodyLt);          // Highlight
    // Yılan başı — yuvarlatılmış, hafif yukarıda
    tft.fillRoundRect(x + 8, y + 2, 10, 8, 3, head);
    // Gözler — beyaz yuva + siyah bebek
    tft.fillCircle(x + 11, y + 5, 1, COL_WHITE);
    tft.drawPixel(x + 12, y + 5, COL_BLACK);
    tft.fillCircle(x + 15, y + 5, 1, COL_WHITE);
    tft.drawPixel(x + 16, y + 5, COL_BLACK);
    // Kırmızı çatal dil — yılanın sağında
    tft.drawFastHLine(x + 18, y + 6, 3, RGB_FIX(255, 80, 80));
    tft.drawPixel(x + 21, y + 5, RGB_FIX(255, 80, 80));
    tft.drawPixel(x + 21, y + 7, RGB_FIX(255, 80, 80));
    // Kuyruk ucu (koyu, sol altta)
    tft.fillRect(x + 4, y + 36, 4, 4, bodyDk);
    tft.fillRect(x + 2, y + 38, 3, 2, bodyDk);
    // Elmalar — üst sağda ve alt solda büyük, parlak vurgulu
    tft.fillCircle(x + 38, y + 12, 4, apple);
    tft.fillCircle(x + 38, y + 11, 4, RGB_FIX(255, 70, 50));
    tft.drawPixel(x + 36, y + 10, RGB_FIX(255, 150, 140)); // Parıltı
    tft.fillRect(x + 38, y + 6, 2, 3, RGB_FIX(80, 180, 40)); // Sap
    tft.fillCircle(x + 36, y + 40, 3, apple);
    tft.drawPixel(x + 35, y + 39, RGB_FIX(255, 150, 140));
}

// FLAPPY BIRD İkonu — Tatlı kuş + borular (48x48)
// Kompozisyon: 4 yeşil boru (köşelerde) + sarı kuş (ortada)
// Parametreler: x,y — ikonun sol-üst köşesi
// Return: yok
void drawBigIconFlappy(int x, int y) {
    // Renk paleti — sarı gövde, kanat, gagalar, boru yeşili
    uint16_t bodyCol  = RGB_FIX(255, 220, 40);
    uint16_t wingCol  = RGB_FIX(255, 170, 20);
    uint16_t beakCol  = RGB_FIX(255, 100, 30);
    uint16_t pipeCol  = RGB_FIX(80, 200, 60);
    uint16_t pipeLt   = RGB_FIX(120, 240, 100);
    
    // Sol üst boru — dik + üstte kapak
    tft.fillRect(x + 2, y + 0, 10, 18, pipeCol);
    tft.fillRect(x + 0, y + 16, 14, 4, pipeCol);
    tft.drawFastVLine(x + 3, y + 0, 18, pipeLt); // Highlight
    // Sol alt boru
    tft.fillRect(x + 0, y + 36, 14, 4, pipeCol);
    tft.fillRect(x + 2, y + 38, 10, 10, pipeCol);
    tft.drawFastVLine(x + 3, y + 38, 10, pipeLt);
    // Sağ üst boru
    tft.fillRect(x + 36, y + 0, 10, 10, pipeCol);
    tft.fillRect(x + 34, y + 8, 14, 4, pipeCol);
    tft.drawFastVLine(x + 37, y + 0, 10, pipeLt);
    // Sağ alt boru
    tft.fillRect(x + 34, y + 40, 14, 4, pipeCol);
    tft.fillRect(x + 36, y + 42, 10, 6, pipeCol);
    tft.drawFastVLine(x + 37, y + 42, 6, pipeLt);
    // Kuş gövdesi — sarı daire, üst highlight, alt karın
    tft.fillCircle(x + 24, y + 24, 8, bodyCol);
    tft.fillCircle(x + 24, y + 23, 8, RGB_FIX(255, 230, 60));
    tft.fillCircle(x + 25, y + 27, 5, RGB_FIX(255, 240, 180));
    // Kanat — yuvarlatılmış dikdörtgen + highlight
    tft.fillRoundRect(x + 14, y + 22, 10, 6, 2, wingCol);
    tft.drawFastHLine(x + 15, y + 23, 8, RGB_FIX(255, 190, 50));
    // Göz — beyaz yuva + siyah bebek + parıltı
    tft.fillCircle(x + 28, y + 21, 4, COL_WHITE);
    tft.fillCircle(x + 29, y + 21, 2, COL_BLACK);
    tft.drawPixel(x + 28, y + 20, COL_WHITE);
    // Gaga — iki katmanlı (üst parlak, alt koyu)
    tft.fillRoundRect(x + 31, y + 24, 8, 4, 1, beakCol);
    tft.fillRoundRect(x + 31, y + 26, 8, 3, 1, RGB_FIX(220, 80, 20));
    // Küçük üst kanat detayı
    tft.fillRect(x + 15, y + 20, 3, 2, wingCol);
}

// SPACE INVADERS İkonu — Klasik alien + gemi (48x48)
// Kompozisyon: Yıldız arka planı + mor alien (üstte) + mavi gemi (altta) + lazer
// Parametreler: x,y — ikonun sol-üst köşesi
// Return: yok
void drawBigIconSpace(int x, int y) {
    // Renk paleti — alien mor, gemi mavi, yıldız, lazer sarı
    uint16_t alien   = RGB_FIX(220, 60, 255);
    uint16_t alienLt = RGB_FIX(255, 120, 255);
    uint16_t ship    = RGB_FIX(100, 220, 255);
    uint16_t shipDk  = RGB_FIX(60, 140, 180);
    uint16_t star    = RGB_FIX(200, 200, 255);
    uint16_t laser   = RGB_FIX(255, 255, 80);
    
    // Yıldız arka planı — 7 dağınık piksel
    tft.drawPixel(x + 5, y + 3, star);
    tft.drawPixel(x + 42, y + 8, star);
    tft.drawPixel(x + 15, y + 42, star);
    tft.drawPixel(x + 40, y + 38, star);
    tft.drawPixel(x + 2, y + 28, star);
    tft.drawPixel(x + 45, y + 22, star);
    tft.drawPixel(x + 30, y + 2, star);
    // Alien gövdesi — katmanlı dikdörtgenlerle piksel sanatı
    tft.fillRect(x + 16, y + 4, 16, 4, alien);   // Anten üstü
    tft.fillRect(x + 12, y + 8, 24, 4, alien);   // Baş geniş
    tft.fillRect(x + 10, y + 10, 28, 4, alien);  // Gövde
    tft.fillRect(x + 10, y + 14, 28, 4, alien);  // Alt gövde
    // Gözler — siyah yuva + mor parıltı
    tft.fillRect(x + 14, y + 10, 4, 4, COL_BLACK);
    tft.fillRect(x + 30, y + 10, 4, 4, COL_BLACK);
    tft.drawPixel(x + 14, y + 10, alienLt);
    tft.drawPixel(x + 30, y + 10, alienLt);
    // Antenler — yanlardan yukarı
    tft.fillRect(x + 12, y + 2, 2, 4, alien);
    tft.fillRect(x + 34, y + 2, 2, 4, alien);
    tft.drawPixel(x + 11, y + 1, alienLt);
    tft.drawPixel(x + 35, y + 1, alienLt);
    // Bacaklar — 4 küçük dikdörtgen
    tft.fillRect(x + 10, y + 18, 4, 4, alien);
    tft.fillRect(x + 18, y + 18, 4, 2, alien);
    tft.fillRect(x + 26, y + 18, 4, 2, alien);
    tft.fillRect(x + 34, y + 18, 4, 4, alien);
    // Lazer ışınları — alien'den aşağı sarı çizgiler
    tft.drawFastVLine(x + 20, y + 22, 6, laser);
    tft.drawFastVLine(x + 28, y + 23, 5, laser);
    // Oyuncu gemisi — alt orta, üçgenimsi
    tft.fillRect(x + 16, y + 38, 16, 4, ship);   // Ana gövde
    tft.fillRect(x + 20, y + 36, 8, 2, ship);    // Üst taret
    tft.fillRect(x + 22, y + 34, 4, 2, shipDk);  // Namlu
    tft.drawPixel(x + 24, y + 33, ship);         // Namlu ucu
    // Geminin ateş ettiği yeşil lazer — yukarı
    tft.drawFastVLine(x + 24, y + 28, 5, RGB_FIX(80, 255, 80));
}

// ARKANOID İkonu — Tuğlalar + top + paddle (48x48)
// Kompozisyon: 5x4 tuğla dizisi (renkli) + top + alt paddle
// Parametreler: x,y — ikonun sol-üst köşesi
// Return: yok
void drawBigIconArkanoid(int x, int y) {
    // Paddle ve top renkleri
    uint16_t paddle = RGB_FIX(30, 210, 255);
    uint16_t padLt  = RGB_FIX(100, 230, 255);
    uint16_t ball   = COL_WHITE;
    
    // Tuğla renkleri — 5 satır, her satır farklı renk (gökkuşağı)
    uint16_t brickColors[] = {
        RGB_FIX(255, 60, 40),   // Kırmızı
        RGB_FIX(255, 160, 30),  // Turuncu
        RGB_FIX(255, 230, 40),  // Sarı
        RGB_FIX(60, 220, 60),   // Yeşil
        RGB_FIX(60, 140, 255),  // Mavi
    };
    
    // Tuğla dizisi — 5 satır x 4 sütun, her tuğla 10x5 piksel
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            int bx = x + 4 + col * 11;  // X: 4 başlangıç + 11px adım
            int by = y + 2 + row * 6;   // Y: 2 başlangıç + 6px adım
            tft.fillRect(bx, by, 10, 5, brickColors[row]); // Dolu tuğla
            tft.drawRect(bx, by, 10, 5, RGB_FIX(40, 40, 40)); // Kenarlık
            tft.drawFastHLine(bx + 1, by + 1, 8, RGB_FIX(255, 255, 200)); // Üst highlight
        }
    }
    
    // 3 tuğla "yok edilmiş" — arka plan rengine boyayarak boşluk yarat
    tft.fillRect(x + 4, y + 2, 10, 5, COL_BG_DARK);
    tft.fillRect(x + 26, y + 8, 10, 5, COL_BG_DARK);
    tft.fillRect(x + 37, y + 2, 10, 5, COL_BG_DARK);
    // Top — ortada beyaz daire + parıltı
    tft.fillCircle(x + 24, y + 34, 3, ball);
    tft.drawPixel(x + 23, y + 33, RGB_FIX(200, 200, 255));
    // Top izi — koyu gri piksellerle hareket efekti
    tft.drawPixel(x + 20, y + 36, COL_DARK_GRAY);
    tft.drawPixel(x + 17, y + 38, COL_DARK_GRAY);
    // Paddle — altta yuvarlatılmış dikdörtgen + highlight
    tft.fillRoundRect(x + 12, y + 42, 24, 4, 2, paddle);
    tft.drawFastHLine(x + 14, y + 42, 20, padLt);
}

// --- V2.1 EKLENTİSİ --- PACMAN İkonu (48x48)
// Kompozisyon: Mavi labirent çerçeve + sarı Pac-Man (ağız açık) + yemler + güç hapı + kırmızı hayalet
// Parametreler: x,y — ikonun sol-üst köşesi
// Return: yok
void drawBigIconPacman(int x, int y) {
    // Renk paleti — sarı Pac-Man, krem yem, kırmızı hayalet, mavi labirent
    uint16_t pacCol  = RGB_FIX(255, 255, 0);
    uint16_t pacDk   = RGB_FIX(200, 200, 0);
    uint16_t dotCol  = RGB_FIX(255, 200, 150);
    uint16_t ghostB  = RGB_FIX(255, 80, 80);   // Kırmızı hayalet
    uint16_t ghostEye = COL_WHITE;
    uint16_t mazeCol = RGB_FIX(30, 30, 200);
    
    // Labirent duvarları (arka plan) — çerçeve + 4 yatay duvar
    tft.drawFastHLine(x + 0, y + 0, 48, mazeCol);   // Üst kenar
    tft.drawFastHLine(x + 0, y + 47, 48, mazeCol);  // Alt kenar
    tft.drawFastVLine(x + 0, y + 0, 48, mazeCol);   // Sol kenar
    tft.drawFastVLine(x + 47, y + 0, 48, mazeCol);  // Sağ kenar
    tft.drawFastHLine(x + 8, y + 10, 14, mazeCol);  // Üst-sol iç duvar
    tft.drawFastHLine(x + 28, y + 10, 14, mazeCol); // Üst-sağ iç duvar
    tft.drawFastHLine(x + 8, y + 38, 14, mazeCol);  // Alt-sol iç duvar
    tft.drawFastHLine(x + 28, y + 38, 14, mazeCol); // Alt-sağ iç duvar
    
    // Pac-Man (ağız açık, sağa bakıyor) — büyük sarı daire
    tft.fillCircle(x + 16, y + 24, 10, pacCol);
    tft.fillCircle(x + 16, y + 23, 10, RGB_FIX(255, 255, 40)); // Üst highlight
    // Ağız (üçgen kesim) — arka plan rengiyle "ısırık" efekti
    tft.fillTriangle(x + 18, y + 24, x + 28, y + 20, x + 28, y + 28, RGB_FIX(12, 12, 20));
    // Göz — siyah yuva + beyaz parıltı
    tft.fillCircle(x + 16, y + 19, 2, COL_BLACK);
    tft.drawPixel(x + 15, y + 18, COL_WHITE);
    
    // Yem noktaları (Pac-Man'ın önünde) — 3 adet, sağa doğru küçülen
    tft.fillCircle(x + 32, y + 24, 2, dotCol);
    tft.fillCircle(x + 39, y + 24, 1, dotCol);
    tft.fillCircle(x + 44, y + 24, 1, dotCol);
    
    // Güç hapı (büyük nokta) — sağ üstte, daha büyük daire
    tft.fillCircle(x + 40, y + 12, 3, dotCol);
    
    // Kırmızı hayalet (sağ altta) — yuvarlatılmış üst + dikdörtgen alt
    tft.fillRoundRect(x + 32, y + 32, 12, 10, 4, ghostB);
    tft.fillRect(x + 32, y + 38, 12, 6, ghostB);
    // Hayalet alt dalgası — 2 kesik (三层 dişli alt kenar efekti)
    tft.fillRect(x + 32, y + 42, 3, 2, RGB_FIX(12, 12, 20));
    tft.fillRect(x + 38, y + 42, 3, 2, RGB_FIX(12, 12, 20));
    // Hayalet gözleri — beyaz yuva + mavi bebek (bakan yön)
    tft.fillCircle(x + 36, y + 36, 2, ghostEye);
    tft.fillCircle(x + 40, y + 36, 2, ghostEye);
    tft.fillCircle(x + 37, y + 36, 1, RGB_FIX(30, 30, 200));
    tft.fillCircle(x + 41, y + 36, 1, RGB_FIX(30, 30, 200));
}

// --- MODE7 ---
// MODE7 İkonu (48x48) — Basit pseudo-3D yarış sahnesi
// Kompozisyon: Mavi gökyüzü (üçgen) + gri yol (dikdörtgen) + kırmızı araba (merkezde)
// Parametreler: x,y — ikonun sol-üst köşesi
// Return: yok
void drawBigIconMode7(int x, int y) {
    tft.fillRect(x+10, y+24, 28, 14, RGB_FIX(100, 100, 100)); // Road (gri yol)
    tft.fillTriangle(x+24, y+4, x+10, y+24, x+38, y+24, RGB_FIX(50, 50, 200)); // Sky (mavi gökyüzü, perspektif)
    tft.fillRect(x+20, y+20, 8, 8, RGB_FIX(255, 0, 0)); // Car (kırmızı araba)
}
// MODE7 Küçük İkon (20x20) — Komşu kartlarda kullanılır
// Parametreler: x,y (sol-üst), col (oyunun tema rengi — gri yol yerine)
// Return: yok
void drawSmallIconMode7(int x, int y, uint16_t col) {
    tft.fillRect(x+5, y+10, 10, 6, col);                   // Yol (tema rengi)
    tft.fillRect(x+8, y+8, 4, 4, RGB_FIX(255, 0, 0));      // Araba (sabit kırmızı)
}

// --- 3D WIRE ---
// 3D WIRE İkonu (48x48) — İki kesişen küp (wire-frame / tel kafes)
// Kompozisyon: Ön küp + arka küp + 4 köşegen bağlantı çizgisi (3D derinlik etkisi)
// Parametreler: x,y — ikonun sol-üst köşesi
// Return: yok
void drawBigIconWire3D(int x, int y) {
    // Ön küp (parlak yeşil)
    tft.drawRect(x+12, y+12, 16, 16, RGB_FIX(0, 255, 0));
    // Arka küp (koyu yeşil — derinlik hissi)
    tft.drawRect(x+20, y+20, 16, 16, RGB_FIX(0, 200, 0));
    // Köşegen bağlantılar — iki küpü birleştiren 4 çizgi
    tft.drawLine(x+12, y+12, x+20, y+20, RGB_FIX(0, 150, 0));
    tft.drawLine(x+28, y+12, x+36, y+20, RGB_FIX(0, 150, 0));
    tft.drawLine(x+12, y+28, x+20, y+36, RGB_FIX(0, 150, 0));
    tft.drawLine(x+28, y+28, x+36, y+36, RGB_FIX(0, 150, 0));
}
// 3D WIRE Küçük İkon (20x20) — Basit 2 kesişen kare
// Parametreler: x,y (sol-üst), col (tema rengi)
// Return: yok
void drawSmallIconWire3D(int x, int y, uint16_t col) {
    tft.drawRect(x+4, y+4, 8, 8, col);    // Ön kare
    tft.drawRect(x+8, y+8, 8, 8, col);    // Arka kare (offset)
    tft.drawLine(x+4, y+4, x+8, y+8, col); // Tek köşegen (basitleştirilmiş)
}

// --- STRIKE ---
// STRIKE İkonu (48x48) — Fırlatılan ok/huni şekli
// Kompozisyon: Üçgen başlık (mavi) + turuncu sap
// Parametreler: x,y — ikonun sol-üst köşesi
// Return: yok
void drawBigIconStrike(int x, int y) {
    // Ok başı — mavi üçgen (yukarıdan aşağı)
    tft.fillTriangle(x+24, y+8, x+14, y+38, x+34, y+38, RGB_FIX(100, 200, 255));
    // Sap — turuncu dikdörtgen
    tft.fillRect(x+22, y+38, 4, 6, RGB_FIX(255, 100, 0));
}
// STRIKE Küçük İkon (20x20) — Basit üçgen ok başı
// Parametreler: x,y (sol-üst), col (tema rengi)
// Return: yok
void drawSmallIconStrike(int x, int y, uint16_t col) {
    tft.fillTriangle(x+10, y+4, x+5, y+16, x+15, y+16, col);
}

// --- JUMP ---
// JUMP İkonu (48x48) — Platform oyunu sahnesi
// Kompozisyon: Yeşil zemin (alt) + turuncu oyuncu (üstünde)
// Parametreler: x,y — ikonun sol-üst köşesi
// Return: yok
void drawBigIconJump(int x, int y) {
    tft.fillRect(x+8, y+32, 32, 8, RGB_FIX(0, 200, 0));   // Ground (yeşil zemin)
    tft.fillRect(x+16, y+16, 10, 14, RGB_FIX(255, 69, 0)); // Player (turuncu)
}
// JUMP Küçük İkon (20x20) — Basit zemin + oyuncu
// Parametreler: x,y (sol-üst), col (tema rengi — oyuncu rengi)
// Return: yok
void drawSmallIconJump(int x, int y, uint16_t col) {
    tft.fillRect(x+4, y+14, 12, 4, RGB_FIX(0, 200, 0)); // Zemin (sabit yeşil)
    tft.fillRect(x+8, y+6, 4, 6, col);                   // Oyuncu (tema rengi)
}

// --- V2.1 EKLENTİSİ --- AYARLAR İkonu — Dişli çark (48x48)
// Kompozisyon: Büyük dişli (3 katmanlı daire) + 8 diş + merkez delik + alt "CONFIG" yazısı
// Parametreler: x,y — ikonun sol-üst köşesi
// Return: yok
void drawBigIconSettings(int x, int y) {
    // Dişli renk paleti — dış gri, iç daha koyu, merkez en koyu
    uint16_t gearOuter = RGB_FIX(150, 150, 150);
    uint16_t gearInner = RGB_FIX(100, 100, 100);
    uint16_t gearCenter = RGB_FIX(60, 60, 60);
    uint16_t accent = COL_ACCENT;
    
    // Dişlinin merkezi — kartın orta-alt noktası
    int cx = x + 24;
    int cy = y + 22;
    
    // Ana dişli gövdesi (büyük daire) — 3 katmanlı (dış/iç/merkez)
    tft.fillCircle(cx, cy, 14, gearOuter);
    tft.fillCircle(cx, cy, 11, gearInner);
    tft.fillCircle(cx, cy, 6, gearCenter);
    // Merkez delik — arka plan rengi (dişlinin boşluğu)
    tft.fillCircle(cx, cy, 3, RGB_FIX(12, 12, 20));
    
    // Dişli dişleri (8 yön — dikdörtgenlerle)
    // 4 ana yön + 4 çapraz = toplam 8 diş
    // Üst
    tft.fillRect(cx - 3, cy - 17, 6, 6, gearOuter);
    // Alt
    tft.fillRect(cx - 3, cy + 11, 6, 6, gearOuter);
    // Sol
    tft.fillRect(cx - 17, cy - 3, 6, 6, gearOuter);
    // Sağ
    tft.fillRect(cx + 11, cy - 3, 6, 6, gearOuter);
    // Çapraz üst-sol
    tft.fillRect(cx - 13, cy - 13, 5, 5, gearOuter);
    // Çapraz üst-sağ
    tft.fillRect(cx + 8, cy - 13, 5, 5, gearOuter);
    // Çapraz alt-sol
    tft.fillRect(cx - 13, cy + 8, 5, 5, gearOuter);
    // Çapraz alt-sağ
    tft.fillRect(cx + 8, cy + 8, 5, 5, gearOuter);
    
    // Merkez accent vurgusu — cyan halka (marka rengi)
    tft.drawCircle(cx, cy, 3, accent);
    
    // Alt yazı: küçük "CONFIG" etiketi — dişlinin altında
    tft.setTextSize(1);
    tft.setTextColor(gearOuter, RGB_FIX(12, 12, 20)); // Arka planla flicker engelle
    tft.setCursor(x + 6, y + 40); // 36px genislik, 48px karta ortalandi
    tft.print("CONFIG");
}

// ══════════════════════════════════════════════════════════════
//  KÜÇÜK İKONLAR (20x20 piksel) — Carousel komşu kartlar
// ══════════════════════════════════════════════════════════════

// DOOM Küçük İkon — Basit kafatası silüeti
// 20x20 piksel — komşu kartlarda gösterilir, basitleştirilmiş tasarım
// Parametreler: x,y (sol-üst), col (oyunun tema rengi — genelde sönük ton)
// Return: yok
void drawSmallIconDoom(int x, int y, uint16_t col) {
    // Kafatası silüeti — yuvarlatılmış dikdörtgen
    tft.fillRoundRect(x + 4, y + 2, 12, 11, 4, col);
    // Göz yuvaları — siyah
    tft.fillRect(x + 6, y + 6, 3, 3, COL_BLACK);
    tft.fillRect(x + 11, y + 6, 3, 3, COL_BLACK);
    // Burun — siyah kare
    tft.fillRect(x + 9, y + 10, 2, 2, COL_BLACK);
    // Üst dudak — tema rengi çizgi
    tft.fillRect(x + 5, y + 13, 10, 2, col);
    // Dişler — 3 adet kemik rengi dikdörtgen
    for (int i = 0; i < 3; i++) {
        tft.fillRect(x + 6 + i * 3, y + 13, 2, 3, RGB_FIX(200, 180, 140));
    }
    // Alt alevler — 3 turuncu nokta
    tft.fillRect(x + 3, y + 17, 2, 2, RGB_FIX(255, 80, 0));
    tft.fillRect(x + 9, y + 16, 2, 3, RGB_FIX(255, 80, 0));
    tft.fillRect(x + 15, y + 17, 2, 2, RGB_FIX(255, 80, 0));
}

// SNAKE Küçük İkon — Basit yılan
// 20x20 — kıvrımlı gövde + baş + elma
// Parametreler: x,y (sol-üst), col (tema rengi)
// Return: yok
void drawSmallIconSnake(int x, int y, uint16_t col) {
    // S kıvrımı — 3 dikdörtgenle basitleştirilmiş
    tft.fillRect(x + 3, y + 14, 10, 3, col);  // Alt yatay
    tft.fillRect(x + 11, y + 8, 3, 9, col);   // Sağ dikey
    tft.fillRect(x + 5, y + 6, 8, 3, col);    // Üst yatay
    tft.fillRect(x + 4, y + 2, 4, 6, col);    // Sol dikey (baş bölümü)
    // Göz — beyaz nokta
    tft.drawPixel(x + 5, y + 3, COL_WHITE);
    // Elma — kırmızı daire (sağ üstte)
    tft.fillCircle(x + 16, y + 4, 2, RGB_FIX(255, 50, 40));
}

// FLAPPY Küçük İkon — Basit kuş
// 20x20 — sarı kuş + 2 boru
// Parametreler: x,y (sol-üst), col (tema rengi — sarı ton)
// Return: yok
void drawSmallIconFlappy(int x, int y, uint16_t col) {
    // Kuş gövdesi — daire + üst highlight
    tft.fillCircle(x + 8, y + 10, 5, col);
    tft.fillCircle(x + 8, y + 9, 5, RGB_FIX(255, 230, 60));
    // Kanat — alt dikdörtgen
    tft.fillRect(x + 3, y + 9, 5, 3, RGB_FIX(255, 170, 20));
    // Göz — beyaz yuva + siyah bebek
    tft.fillCircle(x + 11, y + 8, 2, COL_WHITE);
    tft.drawPixel(x + 12, y + 8, COL_BLACK);
    // Gaga — turuncu
    tft.fillRect(x + 14, y + 10, 4, 2, RGB_FIX(255, 100, 30));
    // Borular — 2 yeşil dikdörtgen (üst ve alt)
    tft.fillRect(x + 1, y + 0, 4, 5, RGB_FIX(80, 200, 60));
    tft.fillRect(x + 1, y + 16, 4, 4, RGB_FIX(80, 200, 60));
}

// SPACE Küçük İkon — Basit alien
// 20x20 — katmanlı dikdörtgenlerle piksel sanatı alien
// Parametreler: x,y (sol-üst), col (tema rengi — mor)
// Return: yok
void drawSmallIconSpace(int x, int y, uint16_t col) {
    // Alien gövdesi — 3 katmanlı (üst dar, orta geniş, alt orta)
    tft.fillRect(x + 6, y + 2, 8, 3, col);   // Üst anten başı
    tft.fillRect(x + 4, y + 5, 12, 3, col);  // Orta gövde
    tft.fillRect(x + 3, y + 6, 14, 3, col);  // Alt gövde (en geniş)
    // Gözler — siyah yuvalar
    tft.fillRect(x + 5, y + 6, 2, 2, COL_BLACK);
    tft.fillRect(x + 13, y + 6, 2, 2, COL_BLACK);
    // Bacaklar — 2 kısa dikdörtgen
    tft.fillRect(x + 4, y + 9, 2, 2, col);
    tft.fillRect(x + 14, y + 9, 2, 2, col);
    // Oyuncu gemisi — altta mavi
    tft.fillRect(x + 7, y + 15, 6, 2, RGB_FIX(100, 220, 255));
    tft.fillRect(x + 9, y + 14, 2, 1, RGB_FIX(100, 220, 255));
}

// ARKANOID Küçük İkon — Tuğla + top + paddle
// 20x20 — 6 tuğla + top + alt paddle
// Parametreler: x,y (sol-üst), col (tema rengi — paddle rengi)
// Return: yok
void drawSmallIconArkanoid(int x, int y, uint16_t col) {
    // Üst tuğla satırı — 3 farklı renk (kırmızı, yeşil, mavi)
    tft.fillRect(x + 2, y + 2, 5, 3, RGB_FIX(255, 60, 40));
    tft.fillRect(x + 8, y + 2, 5, 3, RGB_FIX(60, 220, 60));
    tft.fillRect(x + 14, y + 2, 5, 3, RGB_FIX(60, 140, 255));
    // Alt tuğla satırı — sarı + turuncu + kırmızı
    tft.fillRect(x + 2, y + 6, 5, 3, RGB_FIX(255, 230, 40));
    tft.fillRect(x + 8, y + 6, 5, 3, RGB_FIX(255, 160, 30));
    tft.fillRect(x + 14, y + 6, 5, 3, RGB_FIX(255, 60, 40));
    // Top — ortada beyaz daire
    tft.fillCircle(x + 10, y + 13, 2, COL_WHITE);
    // Paddle — altta tema rengi çizgi
    tft.fillRect(x + 5, y + 17, 10, 2, col);
}

// --- V2.1 EKLENTİSİ --- PACMAN Küçük İkon (20x20)
// Basitleştirilmiş Pac-Man + yem
// Parametreler: x,y (sol-üst), col (tema rengi — sarı)
// Return: yok
void drawSmallIconPacman(int x, int y, uint16_t col) {
    // Pac-Man gövdesi — daire
    tft.fillCircle(x + 7, y + 10, 6, col);
    // Ağız — üçgen kesik (arka plan rengiyle)
    tft.fillTriangle(x + 9, y + 10, x + 15, y + 7, x + 15, y + 13, RGB_FIX(8, 8, 16));
    // Göz — siyah nokta
    tft.drawPixel(x + 7, y + 7, COL_BLACK);
    // Yem noktaları — sağda krem nokta
    tft.fillCircle(x + 16, y + 10, 1, RGB_FIX(255, 200, 150));
}

// --- V2.1 EKLENTİSİ --- AYARLAR Küçük İkon — Mini dişli (20x20)
// Basitleştirilmiş dişli — 3 katmanlı daire + 4 diş
// Parametreler: x,y (sol-üst), col (tema rengi — gri)
// Return: yok
void drawSmallIconSettings(int x, int y, uint16_t col) {
    // Dişli merkezi — kartın tam ortası
    int cx = x + 10;
    int cy = y + 10;
    // 3 katmanlı daire — dış, iç, merkez deliği
    tft.fillCircle(cx, cy, 6, col);
    tft.fillCircle(cx, cy, 4, RGB_FIX(80, 80, 80));
    tft.fillCircle(cx, cy, 2, RGB_FIX(12, 12, 20));
    // Dişler — 4 ana yön (üst, alt, sol, sağ)
    tft.fillRect(cx - 2, cy - 8, 4, 3, col);   // Üst diş
    tft.fillRect(cx - 2, cy + 5, 4, 3, col);   // Alt diş
    tft.fillRect(cx - 8, cy - 2, 3, 4, col);   // Sol diş
    tft.fillRect(cx + 5, cy - 2, 3, 4, col);   // Sağ diş
}

// ══════════════════════════════════════════════════════════════
//  İKON ÇİZİM YÖNLENDİRİCİLER (index'e göre doğru ikonu çağırır)
// ══════════════════════════════════════════════════════════════

// Büyük ikon çizimi — carousel merkez kartı için
// Bu fonksiyon bir "dispatcher" (yönlendirici): verilen index'e göre
// ilgili oyunun 48x48 ikon çizim fonksiyonunu çağırır
// Parametreler: index (oyun indeksi 0-10), x,y (sol-üst köşe)
// Return: yok
void drawBigIcon(int index, int x, int y) {
    switch (index) {
        case 0: drawBigIconDoom(x, y);     break;
        case 1: drawBigIconSnake(x, y);    break;
        case 2: drawBigIconFlappy(x, y);   break;
        case 3: drawBigIconSpace(x, y);    break;
        case 4: drawBigIconArkanoid(x, y); break;
        case 5: drawBigIconPacman(x, y);   break;  // --- V2.1 ---
        case 6: drawBigIconMode7(x, y);    break;
        case 7: drawBigIconWire3D(x, y);   break;
        case 8: drawBigIconStrike(x, y);   break;
        case 9: drawBigIconJump(x, y);     break;
        case 10: drawBigIconSettings(x, y); break;
    }
}

// Küçük ikon çizimi — carousel komşu kartlar için
// drawBigIcon'un 20x20 versiyonu — yan kartlarda kullanılır
// Parametreler: index (oyun indeksi), x,y (sol-üst), col (tema rengi — sönük)
// Return: yok
void drawSmallIcon(int index, int x, int y, uint16_t col) {
    switch (index) {
        case 0: drawSmallIconDoom(x, y, col);     break;
        case 1: drawSmallIconSnake(x, y, col);    break;
        case 2: drawSmallIconFlappy(x, y, col);   break;
        case 3: drawSmallIconSpace(x, y, col);    break;
        case 4: drawSmallIconArkanoid(x, y, col); break;
        case 5: drawSmallIconPacman(x, y, col);   break;  // --- V2.1 ---
        case 6: drawSmallIconMode7(x, y, col);    break;
        case 7: drawSmallIconWire3D(x, y, col);   break;
        case 8: drawSmallIconStrike(x, y, col);   break;
        case 9: drawSmallIconJump(x, y, col);     break;
        case 10: drawSmallIconSettings(x, y, col); break;
    }
}

// ══════════════════════════════════════════════════════════════
//  ANA CAROUSEL ÇİZİM FONKSİYONU
// ══════════════════════════════════════════════════════════════

// Carousel'in sabit bölgelerini çizer (status bar, hints — bir kez çağrılır)
// Arka plan 3 yatay bölgeye ayrılır (koyu-orta-koyu) — basit gradient etkisi
// Parametre: yok
// Return: yok
void drawCarouselFrame() {
    // Arka plan gradient (basitleştirilmiş — 3 bölge)
    // Üst bölge: status bar altı (y=0-42) — en açık lacivert
    tft.fillRect(0, 0, 160, 42, RGB_FIX(10, 10, 24));
    // Orta bölge: ana kart alanı (y=42-85) — standart arka plan
    tft.fillRect(0, 42, 160, 43, COL_BG_DARK);
    // Alt bölge: ipuçları (y=85-128) — en koyu lacivert
    tft.fillRect(0, 85, 160, 43, RGB_FIX(5, 5, 14));
    
    // Sabit UI elemanlarını çiz — status bar (üst) + control hints (alt)
    drawStatusBar();
    drawControlHints();
}

// Seçili oyunun merkez kartını çizer (48x48 ikon + çerçeve)
// Kart, ekranın tam ortasında konumlanır — 3 katmanlı çerçeve (sönük/parlak/iç)
// Parametre: sel — seçili oyun indeksi
// Return: yok
void drawCenterCard(int sel) {
    // Kart konumu — ekranın ortasında (160px genişlik için)
    int cx = 54;  // (160 - 52) / 2 — yatay ortalama
    int cy = 18;  // Üst boşluk (status bar'ın altında)
    int cardW = 52;
    int cardH = 56;
    
    // Seçili oyunun tema rengi — border için
    uint16_t borderCol = games[sel].primaryColor;
    
    // Çok katmanlı çerçeve — derinlik etkisi
    // 1. Dış temizleme bandı (arka plan rengi) — eski kartı sil
    tft.fillRect(cx - 2, cy - 1, cardW + 4, cardH + 2, COL_BG_DARK);
    // 2. Sönük dış çerçeve (gölge)
    tft.drawRect(cx - 1, cy - 1, cardW + 2, cardH + 2, games[sel].dimColor);
    // 3. Parlak ana çerçeve (oyun rengi)
    tft.drawRect(cx, cy, cardW, cardH, borderCol);
    // 4. İç çerçeve (parlak, daha kalın border efekti)
    tft.drawRect(cx + 1, cy + 1, cardW - 2, cardH - 2, borderCol);
    // 5. İç dolgu — koyu arka plan (ikon buraya çizilir)
    tft.fillRect(cx + 2, cy + 2, cardW - 4, cardH - 4, RGB_FIX(12, 12, 20));
    // Büyük ikonu çiz — kartın içinde, 2px iç boşlukla
    drawBigIcon(sel, cx + 2, cy + 4);
}

// Sol veya sağ komşu kartı çizer (küçük ikon)
// Carousel'in yan taraflarındaki küçük kartlar — seçili oyunun komşuları
// Parametreler: sel (mevcut seçili), side (-1 = sol, +1 = sağ)
// Return: yok
// Not: Modular aritmetik ile carousel sarmalanır (son elemandan ilkine geçiş)
void drawSideCard(int sel, int side) {
    // Komşu index — modülüs ile sarmala (dairesel carousel)
    int index = (sel + side + GAME_COUNT) % GAME_COUNT;
    
    // Küçük kart boyutları — merkez karttan yarım boyutta
    int cardW = 24;
    int cardH = 28;
    int cx, cy;
    
    // Side'a göre X konumu belirle
    if (side == -1) {
        // Sol kart — ekranın sol kenarına yakın
        cx = 6;
        cy = 32;
    } else {
        // Sağ kart — ekranın sağ kenarına yakın
        cx = 130;
        cy = 32;
    }
    
    // Komşu kartın tema rengi — sönük ton (seçili değil)
    uint16_t borderCol = games[index].dimColor;
    
    // Kart çizimi — temizlik + çerçeve + iç dolgu
    tft.fillRect(cx - 1, cy - 1, cardW + 2, cardH + 2, COL_BG_DARK);
    tft.drawRect(cx, cy, cardW, cardH, borderCol);
    tft.fillRect(cx + 1, cy + 1, cardW - 2, cardH - 2, RGB_FIX(8, 8, 16));
    // Küçük ikonu çiz — sönük renkle (komşu old. için vurgusuz)
    drawSmallIcon(index, cx + 2, cy + 4, borderCol);
}

// Oyun adını çizer (merkez kartın altında)
// Oyun adı + altında iki katmanlı accent çizgi (sönük + parlak)
// Parametre: sel — seçili oyun indeksi
// Return: yok
void drawGameLabel(int sel) {
    // Etiket alanını temizle — arka plan rengine boya
    tft.fillRect(0, 78, 160, 16, COL_BG_DARK);
    
    // Oyun adını al ve uzunluğunu hesapla
    const char* name = games[sel].label;
    int nameLen = strlen(name);
    // Font boyutu 1 = her karakter 6px genişlik
    int namePixelW = nameLen * 6;
    // Yatay olarak ortalı başlangıç X'i
    int nameX = (160 - namePixelW) / 2;
    
    // Oyun adını yaz — tema rengiyle
    tft.setTextSize(1);
    tft.setTextColor(games[sel].primaryColor, COL_BG_DARK); // Flicker engelle
    tft.setCursor(nameX, 80);
    tft.print(name);
    
    // Alt accent çizgi — iki katmanlı (sönük dış + parlak iç)
    int lineW = namePixelW + 16; // İsimden biraz daha geniş
    int lineX = (160 - lineW) / 2;
    tft.drawFastHLine(lineX, 90, lineW, games[sel].dimColor);       // Sönük dış çizgi
    tft.drawFastHLine(lineX + 4, 90, lineW - 8, games[sel].primaryColor); // Parlak iç çizgi
}

// Tüm carousel'i çizer
// PSP/PS3 tarzı dönen menü — seçili oyun ortada büyük, yanlar küçük
// fullRedraw: true ise tüm menüyü (frame dahil) yeniden çiz
//             false ise sadece kartları/etiketi/noktaları güncelle (partial redraw)
// Parametre: fullRedraw — tam çizim mi, kısmi çizim mi?
// Return: yok
void drawCarousel(bool fullRedraw) {
    if (fullRedraw) {
        // Tam çizim — sabit frame'i (arka plan, status bar, hints) çiz
        drawCarouselFrame();
        menuDrawn = true; // İlk çizim yapıldı işareti
    }
    
    // Dinamik bölümleri her zaman çiz (seçim değişince güncellenir)
    drawCenterCard(carouselSel);        // Merkez kart (büyük ikon)
    drawSideCard(carouselSel, -1);      // Sol komşu kart
    drawSideCard(carouselSel, +1);      // Sağ komşu kart
    drawGameLabel(carouselSel);         // Oyun adı + alt çizgi
    drawPageDots(carouselSel);          // Sayfa noktaları
}

// ══════════════════════════════════════════════════════════════
//  NABIZ ANİMASYONU — Seçili kartın border'ı hafifçe parlar/söner
// ══════════════════════════════════════════════════════════════

// updatePulse — seçili merkez kartın border rengini 3 fazda döngüsel değiştirir
// Faz 0: Oyunun ana rengi
// Faz 1: Oyun renginin karanlık tonu (RGB565 bileşenleri ölçeklenir)
// Faz 2: Beyaz — en parlak vurgu
// 400ms'de bir faz değişir → yavaş,premium hisli nabız efekti
// Parametre: yok (global carouselSel, animTick, pulseState kullanır)
// Return: yok
void updatePulse() {
    uint32_t now = millis();
    // 400ms geçmediyse çık — frame limiting'in bir parçası
    if (now - animTick < 400) return;
    animTick = now;
    
    // 3 fazlı döngü — 0 → 1 → 2 → 0 ...
    pulseState = (pulseState + 1) % 3;
    
    // Merkez kart konumu (drawCenterCard ile aynı)
    int cx = 54;
    int cy = 18;
    int cardW = 52;
    int cardH = 56;
    
    // Faza göre border rengini seç
    uint16_t borderCol;
    switch (pulseState) {
        case 0:
            // Faz 0 — oyunun ana rengi
            borderCol = games[carouselSel].primaryColor;
            break;
        case 1:
            // Faz 1 — ana rengin karanlık tonu
            // RGB565 bit alanlarını çıkarıp ölçekle:
            //   R: 5 bit (0-31) → *8 ile 0-248 aralığına
            //   G: 6 bit (0-63) → *4 ile 0-252 aralığına
            //   B: 5 bit (0-31) → *8 ile 0-248 aralığına
            borderCol = RGB_FIX(
                        ((games[carouselSel].primaryColor >> 11) & 0x1F) * 8,
                        ((games[carouselSel].primaryColor >> 5) & 0x3F) * 4,
                        (games[carouselSel].primaryColor & 0x1F) * 8
                    );
            break;
        default:
            // Faz 2 — beyaz (en parlak vurgu)
            borderCol = COL_WHITE;
            break;
    }
    
    // Sadece border çizgilerini güncelle — kartın içeriği dokunulmaz
    tft.drawRect(cx, cy, cardW, cardH, borderCol);
    tft.drawRect(cx + 1, cy + 1, cardW - 2, cardH - 2, borderCol);
}

// ══════════════════════════════════════════════════════════════
//  BOOT ANİMASYONU — Premium giriş ekranı
// ══════════════════════════════════════════════════════════════

// playBootJingle — kısa açılış melodisi (4 notalık yükselen akor)
// Notalar: A5 (880) → C6 (1047) → E6 (1319) → G6 (1568)
// Ses kapalıysa playSound() sayesinde sessiz kalır
// Parametre: yok
// Return: yok
void playBootJingle() {
    playSound(880, 80);    // A5
    delay(100);
    playSound(1047, 80);   // C6
    delay(100);
    playSound(1319, 80);   // E6
    delay(100);
    playSound(1568, 150);  // G6 (uzun)
    delay(170);
    noTone(BUZZER);        // Buzzer'ı kapat (güvenlik)
}

// drawBootAnimation — premium açılış sekansı
// Akış:
//   1. Siyah ekran + lacivert gradient arka plan
//   2. "E-OS" yazısı karakter karakter belirir (önce sönük, sonra beyaz)
//   3. "v2.1" versiyon etiketi
//   4. Genişleyen accent çizgi (orta noktadan dışa yayılır)
//   5. 4 segmentli ilerleme çubuğu (her segment farklı nota ile)
//   6. Fade-out: koyulaşan ekranlar → siyah
// Parametre: yok
// Return: yok
void drawBootAnimation() {
    // Siyah başlangıç — ekranı temizle
    tft.fillScreen(COL_BLACK);
    
    // İki katmanlı lacivert arka plan (üst açık, alt koyu)
    tft.fillRect(0, 0, 160, 64, RGB_FIX(6, 6, 18));
    tft.fillRect(0, 64, 160, 64, RGB_FIX(3, 3, 10));
    
    // ─── "E-OS" logosu — karakter karakter belirir ───
    tft.setTextSize(2); // 2x font — büyük logo
    const char* logoText = "E-OS";
    int logoX = 40; // Tam ortaya hizalandı
    int logoY = 30;
    
    // 4 karakteri sırayla çiz — önce sönük, sonra beyaz vurgu
    for (int i = 0; i < 4; i++) {
        // Sönük alt katman (geçici gölge)
        tft.setTextColor(COL_ACCENT_DIM);
        tft.setCursor(logoX + i * 20, logoY); // Her karakter 20px aralıkla
        tft.print(logoText[i]);
        delay(40);
        
        // Beyaz renk vurgusuyla E-OS yazdır — parça parça parlama
        tft.setTextColor(COL_WHITE);
        tft.setCursor(logoX + i * 20, logoY);
        tft.print(logoText[i]);
        delay(50);
    }
    
    delay(60);
    
    // ─── Versiyon etiketi "v2.1" ───
    tft.setTextSize(1);
    tft.setTextColor(COL_GRAY);
    tft.setCursor(70, logoY + 20); // Logonun altında, ortalanmış
    tft.print("v2.1");
    
    // ─── Genişleyen accent çizgi ───
    // Orta noktadan (x=80) dışa doğru 60px genişleyene kadar çiz
    for (int w = 0; w <= 60; w += 4) {
        int lx = 80 - w;
        // Sönük dış çizgi (geniş)
        tft.drawFastHLine(lx, logoY + 30, w * 2, COL_ACCENT_DIM);
        // Parlak iç çizgi (dar — merkezde yoğun)
        tft.drawFastHLine(80 - w/2, logoY + 30, w, COL_ACCENT);
        delay(8); // Yavaşça genişleme animasyonu
    }
    
    // ─── İlerleme çubuğu (4 segmentli) ───
    int barX = 25;
    int barY = logoY + 40;
    int barW = 110;
    int barH = 8;
    
    // Dış çerçeve — yuvarlatılmış
    tft.drawRoundRect(barX, barY, barW, barH, 3, COL_ACCENT_DIM);
    
    int segments = 4; // 4 segment = 4 nota
    int segW = (barW - 4) / segments; // Her segmentin genişliği
    
    // Her segmenti sırayla doldur — her biri bir nota ile eş zamanlı
    for (int s = 0; s < segments; s++) {
        // Segmentin müzik notası ve süresi (boot jingle ile aynı)
        int notes[] = {880, 1047, 1319, 1568};
        int durs[]  = {80, 80, 80, 150};
        playSound(notes[s], durs[s]);
        
        // Segment içini piksel piksel doldur — gradient parlaklık
        for (int px = 0; px < segW; px++) {
            int fillX = barX + 2 + s * segW + px;
            // Parlaklık piksel ilerledikçe artar (120 → 255)
            uint8_t bright = 120 + (px * 135 / segW);
            tft.drawFastVLine(fillX, barY + 2, barH - 4, RGB_FIX(30, bright, 255));
            delay(3); // Yavaş dolma efekti
        }
        delay(50); // Segmentler arası küçük duraklama
    }
    noTone(BUZZER); // Buzzer kapat
    
    delay(150);
    
    // ─── Fade-out — 3 kademeli koyulaşma ───
    tft.fillRect(0, 0, 160, 128, RGB_FIX(4, 4, 12));
    delay(60);
    tft.fillRect(0, 0, 160, 128, RGB_FIX(2, 2, 6));
    delay(60);
    tft.fillScreen(COL_BLACK); // Tam siyah
    delay(40);
}

// ══════════════════════════════════════════════════════════════
//  OYUN BAŞLATMA ANİMASYONU
// ══════════════════════════════════════════════════════════════

// drawLaunchAnimation — oyun başlatılırken gösterilen geçiş sekansı
// Akış:
//   1. Kısa ses (2 notalık jingle)
//   2. Karttan ekran boyutuna genişleyen renk patlaması (oyun rengi)
//   3. Tam ekran renk → koyu arka plan
//   4. Oyun adı + accent çizgi
//   5. İlerleme çubuğu:
//      - fastBoot=true:  yeşil "HIZLI BASLAT" (oyun zaten yüklü)
//      - fastBoot=false: accent "Flash yazilacak" (flash gerekli)
// Parametreler: sel (oyun indeksi), fastBoot (hızlı başlatma mı?)
// Return: yok
void drawLaunchAnimation(int sel, bool fastBoot) {
    // Oyunun tema rengi — animasyon boyunca kullanılır
    uint16_t gameCol = games[sel].primaryColor;
    
    // Kısa açılış sesi — 2 notalık yükselen jingle
    playSound(1200, 60);
    delay(80);
    playSound(1600, 60);
    delay(80);
    noTone(BUZZER);
    
    // ─── Renk patlaması — karttan ekran boyutuna genişleme ───
    // Merkez kart koordinatları (drawCenterCard ile aynı)
    int cx = 54, cy = 18, cw = 52, ch = 56;
    
    // 1. adım: kartı biraz büyüt
    tft.fillRect(cx - 10, cy - 5, cw + 20, ch + 10, gameCol);
    delay(40);
    // 2. adım: daha da büyüt
    tft.fillRect(cx - 30, cy - 15, cw + 60, ch + 30, gameCol);
    delay(40);
    // 3. adım: tüm satırı kapla
    tft.fillRect(0, cy - 30, 160, ch + 60, gameCol);
    delay(40);
    // 4. adım: tüm ekranı doldur
    tft.fillScreen(gameCol);
    delay(60);
    
    // Koyu arka plan — içerik buraya çizilecek
    tft.fillScreen(RGB_FIX(5, 5, 12));
    
    // ─── Oyun adı (büyük font) ───
    tft.setTextSize(2); // 2x = 12px/karakter
    int nameLen = strlen(games[sel].label);
    int nameX = (160 - nameLen * 12) / 2; // Yatay ortala
    tft.setTextColor(gameCol); // Tema rengi
    tft.setCursor(nameX, 25);
    tft.print(games[sel].label);
    
    // Alt accent çizgi — oyun rengi, ortalanmış
    tft.drawFastHLine(20, 48, 120, gameCol);
    
    if (fastBoot) {
        // ─── HIZLI BAŞLATMA — oyun zaten flash'ta yüklü ───
        tft.setTextSize(1);
        tft.setTextColor(COL_SUCCESS); // Yeşil — hızlı/success
        tft.setCursor(18, 60);
        tft.print(">> HIZLI BASLAT! <<");
        
        tft.setTextColor(COL_GRAY);
        tft.setCursor(20, 78);
        tft.print("Oyun zaten yuklu...");
        
        // Yeşil ilerleme çubuğu — hızlı dolar (4px adım, 5ms)
        tft.drawRect(25, 95, 110, 8, COL_SUCCESS);
        for (int p = 0; p < 106; p += 4) {
            tft.fillRect(27, 97, p, 4, COL_SUCCESS);
            delay(5);
        }
        
        // Başarı sesi
        playSound(1500, 100);
        delay(200);
        noTone(BUZZER);
    } else {
        // ─── NORMAL BAŞLATMA — flash yazma gerekecek ───
        tft.setTextSize(1);
        tft.setTextColor(COL_WHITE);
        tft.setCursor(12, 60);
        tft.print("Oyun hazirlaniyor...");
        
        tft.setTextColor(COL_GRAY);
        tft.setCursor(16, 78);
        tft.print("Flash yazilacak");
        
        // Accent ilerleme çubuğu — daha yavaş dolar (2px adım, 8ms)
        tft.drawRect(25, 95, 110, 8, COL_ACCENT_DIM);
        for (int p = 0; p < 106; p += 2) {
            tft.fillRect(27, 97, p, 4, COL_ACCENT);
            delay(8);
        }
        delay(200);
    }
}

// ══════════════════════════════════════════════════════════════
//  OLED EKRAN GÜNCELLEMESİ
// ══════════════════════════════════════════════════════════════

// OLED menüde kapalı — güç tasarrufu (V2.0.1 kararı)
// updateOLED — menü modunda OLED'i uyku moduna alır
// Menüde TFT ekran yeterli; OLED sadece oyun başlatma/bootloader sırasında aktif
// Bu sayede ~%15-20 güç tasarrufu sağlanır
// Parametre: sel — seçili oyun (mevcut implementasyonda kullanılmaz, ileride istatistik için)
// Return: yok
void updateOLED(int sel) {
    // OLED başlatılmadıysa hiçbir şey yapma
    if (!oledReady) return;
    
    // OLED'i tamamen kapatiyoruz (TFT'yi kopya etmesin, guc tasarrufu)
    // setPowerSave(1) = uyku modu (display off, ~0.1mA)
    oled.setPowerSave(1);
}

// ══════════════════════════════════════════════════════════════
// --- V2.1 EKLENTİSİ --- AYARLAR MENÜSÜ (TFT POPUP)
// ══════════════════════════════════════════════════════════════

// Ayarlar popup'ı — Ses açma/kapama, NVS kayıt
// openSettingsMenu — AYARLAR seçiliyken BTN_A'ya basınca açılan popup menü
//
// Akış:
//   1. Popup arka planı + başlık çiz
//   2. Ses durumu satırını göster (AÇIK/KAPALI)
//   3. Kullanıcı girişi bekle:
//      - Joystick X (<>): sesi aç/kapat (toggle)
//      - BTN_A: kaydet + NVS'e yaz + çık
//      - BTN_B: iptal + çıkar (değişiklik kaybolur)
//   4. Çıkışta carousel'i yeniden çiz
//
// Parametre: yok (global soundEnabled okur/yazar)
// Return: yok — çıkışta drawCarousel(true) çağrılır
void openSettingsMenu() {
    // Açılış sesi ve kısa gecikme
    playSound(1000, 40);
    delay(60);
    
    // Lokal kopya — kullanıcı iptal ederse değişiklik kaybolur
    bool localSound = soundEnabled;
    bool settingsOpen = true;
    
    // Ana popup döngüsü — settingsOpen false olana kadar dön
    while (settingsOpen) {
        // ─── Popup arka planı — 140x88 lacivert kutu ───
        tft.fillRect(10, 20, 140, 88, RGB_FIX(12, 12, 30));
        // Dış çerçeve — ayarlar tema rengi (gri)
        tft.drawRect(10, 20, 140, 88, games[SETTINGS_INDEX].primaryColor);
        // İç çerçeve — koyu gri vurgu
        tft.drawRect(11, 21, 138, 86, RGB_FIX(80, 80, 80));
        
        // Başlık — "AYARLAR" ortalanmış
        tft.setTextSize(1);
        tft.setTextColor(COL_WHITE);
        tft.setCursor(46, 28);
        tft.print("AYARLAR");
        // Başlık altı ayraç çizgisi
        tft.drawFastHLine(20, 38, 120, RGB_FIX(80, 80, 80));
        
        // ─── Ses durumu satırı — vurgulu gösterim ───
        // Seçili satır arka planı — accent vurgulu
        tft.fillRect(18, 44, 124, 18, RGB_FIX(25, 25, 50));
        tft.drawRect(18, 44, 124, 18, COL_ACCENT);
        
        // "Ses:" etiketi — gri
        tft.setTextColor(COL_GRAY);
        tft.setCursor(24, 49);
        tft.print("Ses:");
        
        // Duruma göre renk ve metin
        if (localSound) {
            // Açık — yeşil
            tft.setTextColor(COL_SUCCESS);
            tft.setCursor(60, 49);
            tft.print("< ACIK  >");
        } else {
            // Kapalı — kırmızı
            tft.setTextColor(COL_DANGER);
            tft.setCursor(60, 49);
            tft.print("< KAPALI>");
        }
        
        // ─── Kontrol ipuçları ───
        tft.setTextColor(COL_GRAY);
        tft.setCursor(16, 72);
        tft.print("<> Degistir  A:Kaydet");
        
        tft.setTextColor(COL_DARK_GRAY);
        tft.setCursor(30, 86);
        tft.print("B: Iptal");
        
        // Versiyon bilgisi — altta
        tft.setTextColor(COL_WHITE);
        tft.setTextSize(1);
        tft.setCursor(45, 110);
        tft.print("E-OS v2.1");
        
        // ─── Kullanıcı girişi bekleme döngüsü ───
        // inputReceived true olana kadar bu döngüden çıkma
        bool inputReceived = false;
        while (!inputReceived) {
            // Joystick X ile ses aç/kapat
            // joyCenterX kalibrasyon değeri çıkarılır, sapma >500 = hareket
            int jx = analogRead(JOY_X) - joyCenterX;
            if (abs(jx) > 500) {
                // Toggle — sesi tersine çevir
                localSound = !localSound;
                playSound(600, 20); // Sadece ses açıkken duyulur (test amaçlı)
                inputReceived = true;
                delay(200); // Debounce — tekrar tetiklenmeyi engelle
            }
            
            // BTN_A ile kaydet ve çık
            // İki aşamalı okuma — debounce (örnek: 50ms sonra tekrar kontrol)
            if (!digitalRead(BTN_A)) {
                delay(50);
                if (!digitalRead(BTN_A)) {
                    // Lokal ses değerini global'e aktar
                    soundEnabled = localSound;
                    // NVS'e kaydet — kalıcı hale getir
                    Preferences prefs;
                    prefs.begin("os", false); // false = oku/yaz modu
                    prefs.putBool("sound_en", soundEnabled);
                    prefs.end();
                    
                    // Başarı sesi — 2 notalık yükselen jingle
                    playSound(1200, 50);
                    delay(60);
                    playSound(1500, 80);
                    delay(100);
                    noTone(BUZZER);
                    
                    // Popup'ı kapat
                    settingsOpen = false;
                    inputReceived = true;
                }
            }
            
            // BTN_B ile iptal et ve çık
            if (!digitalRead(BTN_B)) {
                delay(50);
                if (!digitalRead(BTN_B)) {
                    // İptal sesi — tek alçak nota
                    playSound(400, 40);
                    delay(80);
                    noTone(BUZZER);
                    // localSound kaybolur — global soundEnabled dokunulmaz
                    settingsOpen = false;
                    inputReceived = true;
                }
            }
            
            // Kısa polling gecikmesi — CPU'yu yormamak için
            delay(30);
        }
    }
    
    // Popup kapandı — menüyü tamamen yeniden çiz
    drawCarousel(true);
}

// ══════════════════════════════════════════════════════════════
//  SETUP — Başlatma ve Boot Akışı
// ══════════════════════════════════════════════════════════════

// setup — Arduino giriş noktası; tüm donanım başlatma ve boot akışı burada
// Akış sırası:
//   1. Seri port + I2C başlat
//   2. Bootloader magic kontrolü (RTC'den) — flashlama moduna geç?
//   3. Pin modlarını ayarla (butonlar, buzzer, SPI CS)
//   4. SPI + SD kart başlat
//   5. Bootloader moduysa: runEarlyBootloader() çağır
//   6. TFT başlat + renk paleti + oyun listesi doldur
//   7. NVS'ten ses ayarını oku
//   8. Joystick kalibrasyonu
//   9. Boot animasyonu
//  10. OLED başlat + ilk carousel çizimi
// Parametre: yok
// Return: yok (Arduino setup imzası)
void setup() {
    // ─── BÖLÜM 1: Seri port + I2C hattı ───
    Serial.begin(115200);       // Debug için seri port
    Wire.begin(8, 9);           // I2C: SDA=8, SCL=9 (OLED için)
    Wire.setClock(400000);      // 400kHz hız — OLED tazelemesini hızlandırır
    
    // ─── BÖLÜM 2: Bootloader mod kontrolü (RTC magic byte) ───
    // Boot döngüsü koruması: bootMode'u hemen oku ve sıfırla
    // 0xDEADBEEF = bootloader moduna gir (flash yazma yapılacak)
    bool enterBootloader = (bootModeMagic == 0xDEADBEEF);
    char flashTarget[32];
    if (enterBootloader) {
        // Flashlanacak dosya adını RTC'den lokal kopyaya al
        strncpy(flashTarget, bootFilename, sizeof(flashTarget) - 1);
        flashTarget[sizeof(flashTarget) - 1] = '\0'; // Null-termination garantisi
    }
    bootModeMagic = 0; // Magic'i sıfırla — bir sonraki restart'ta tekrar girmesin
    
    // ─── BÖLÜM 3: Pin modları — butonlar, buzzer, SPI CS ───
    // Buton ve buzzer pinleri
    pinMode(BTN_A, INPUT_PULLUP); pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP); pinMode(BTN_D, INPUT_PULLUP);
    pinMode(BUZZER, OUTPUT);
    
    // SPI CS pinleri — yüksek = deaktif
    // Birden fazla SPI cihazı (TFT + SD) var, başlangıçta ikisi de pasif
    pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH); 
    pinMode(SD_CS, OUTPUT);  digitalWrite(SD_CS, HIGH);  
    
    // ─── BÖLÜM 4: SPI hattı + SD kart başlatma ───
    // SPI hattını başlat
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1); delay(50);
    bool sdReady = SD.begin(SD_CS, SPI, 10000000); // 10 MHz güvenli hız
    
    // SD kart yoksa hata ekranı göster ve dur
    // Bootloader'ın dosyayı okuyabilmesi için SD şart
    if (!sdReady) {
        tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
        tft.setTextColor(RGB_FIX(255,80,0)); tft.setTextSize(1);
        tft.setCursor(10, 55); tft.print("SD KART YOK!");
        tft.setCursor(10, 70); tft.print("Karti takip resetleyin.");
        while(1) delay(100); // Sonsuz döngü — reset beklenir
    }
    
    // ─── BÖLÜM 5: Bootloader modu — flash yazma ───
    // Bootloader modunda mıyız? (RTC magic kontrolü)
    if (enterBootloader && sdReady) {
        runEarlyBootloader(flashTarget);
        // Bu fonksiyon ya ESP.restart() ile çıkar ya da return ile normale döner
    }
    
    // ─── BÖLÜM 6: TFT başlatma + joystick switch pini ───
    tft.init(); tft.setRotation(1); tft.setSwapBytes(true); tft.fillScreen(TFT_BLACK);
    pinMode(JOY_SW, INPUT_PULLUP); // Joystick switch butonu (basılı 0)
    
    // Renk paletini ve oyun listesini başlat (TFT hazır olmalı!)
    // RGB_FIX() artık güvenle çağrılabilir
    initColorsAndGames();
    
    // ─── BÖLÜM 7: NVS'ten ses ayarını oku (V2.1) ───
    // Blok kapsamı — prefs nesnesi bu blok bitince yok olur
    {
        Preferences prefs;
        prefs.begin("os", true); // true = salt-oku modu
        // Varsayılan: true (ses açık) — ilk açılışta NVS boşsa
        soundEnabled = prefs.getBool("sound_en", true);
        prefs.end();
    }
    
    // ─── BÖLÜM 8: Joystick merkez kalibrasyonu ───
    // 10 örnek alıp ortalama → "sıfır" konumu belirle
    // Bu sayede joystick sapması = analogRead - joyCenter
    tft.setTextColor(COL_ACCENT); tft.setTextSize(1);
    tft.setCursor(30, 58); tft.print("Kalibrasyon...");
    long sumX = 0, sumY = 0;
    // 10 okuma yapıp topla, 2ms aralıkla
    for (int i = 0; i < 10; i++) { sumX += analogRead(JOY_X); sumY += analogRead(JOY_Y); delay(2); }
    joyCenterX = sumX / 10; joyCenterY = sumY / 10; // Ortalama = merkez
    
    // ═══ BÖLÜM 9: PREMİUM BOOT ANİMASYONU ═══
    drawBootAnimation();
    
    // ─── BÖLÜM 10: OLED başlat + ilk carousel çizimi ───
    // OLED başlatma
    oled.begin();
    oledReady = true; // Artık OLED çizim fonksiyonları güvenle çağrılabilir
    
    // İlk carousel çizimi — fullRedraw=true (frame dahil)
    drawCarousel(true);
    lastOledUpdate = 0; // OLED güncellemeyi tetikle
    updateOLED(carouselSel); // OLED'i uyku moduna al (güç tasarrufu)
    
    // Animasyon zamanlayıcısını başlat — updatePulse ilk çağrısında hemen tetiklenmesin
    animTick = millis();
}

// ══════════════════════════════════════════════════════════════
//  LOOP — Carousel Navigasyonu ve Oyun Başlatma
// ══════════════════════════════════════════════════════════════

// loop — Arduino ana döngüsü; sürekli çalışır
// Akış (her iterasyonda):
//   1. Joystick X oku → yatay navigasyon (220ms debounce)
//   2. Seçim değiştiyse partial redraw
//   3. Nabız animasyonu (idle durumda)
//   4. OLED güç tasarrufu
//   5. BTN_A: oyun başlat / ayarlar aç
//   6. BTN_B: bilgi ekranı + yüksek skor
//   7. Frame limiting (20ms = ~50FPS)
// Parametre: yok
// Return: yok (Arduino loop imzası)
void loop() {
    // Mevcut zaman — millis() tabanlı zamanlama için
    uint32_t current_ms = millis();
    
    // Joystick X ekseni okuma (yatay carousel navigasyonu)
    // joyCenterX kalibrasyon değerini çıkar → sapma değeri
    int jx_m = analogRead(JOY_X) - joyCenterX;
    
    // static — fonksiyon çağrıları arasında değeri korunur
    static uint32_t lastMove = 0; // Son navigasyon hareketi zamanı
    bool selChanged = false;      // Bu iterasyonda seçim değişti mi?
    
    // ─── Yatay navigasyon (Joystick X) ───
    // Sapma > 500 = hareket var VE en az 220ms geçmiş = debounce
    // 220ms: bir seferde tek adım — sürekli basılı tutarsa yavaşça kayar
    if (abs(jx_m) > 500 && (current_ms - lastMove > 220)) {
        if (jx_m > 0) {
            // Sağa — sonraki oyun (sarma ile)
            carouselSel++;
            if (carouselSel >= GAME_COUNT) carouselSel = 0;
        } else {
            // Sola — önceki oyun (sarma ile)
            carouselSel--;
            if (carouselSel < 0) carouselSel = GAME_COUNT - 1;
        }
        lastMove = current_ms; // Debounce zamanlayıcıyı sıfırla
        selChanged = true;
        
        // Navigasyon sesi — kısa tıklama
        playSound(800, 25);
    }
    
    // Sadece X ekseni ile gezilecek, Y eksenini menude iptal ettik
    // (Y ekseni menüde işlevsiz — oyunlar içinde kullanılır)
    
    // ─── İlk çizim kontrolü ───
    // Eğer menü hiç çizilmediyse (örn. settings popup sonrası), tam çiz
    if (!menuDrawn) {
        drawCarousel(true);
        lastOledUpdate = 0;
    }
    
    // ─── Seçim değiştiyse sadece değişen bölgeleri yeniden çiz ───
    // Partial redraw — sadece kartlar/etiket/noktalar güncellenir, frame değil
    if (selChanged) {
        drawCarousel(false);
        lastOledUpdate = 0;
        prevSel = carouselSel;
    }
    
    // ─── Nabız animasyonu (idle durumda) ───
    // Sadece seçim değişmediğinde — hareket sırasında nabız kesilmesin
    if (!selChanged) {
        updatePulse();
    }
    
    // ─── OLED güç tasarrufu ───
    // Menüde OLED'i uyku modunda tut
    updateOLED(carouselSel);
    
    // ═══ BTN_A: Oyun Başlat / Ayarlar Aç ═══
    // A butonu basılı mı? (INPUT_PULLUP → basılı = LOW = 0)
    if (!digitalRead(BTN_A)) {
        delay(50); // Debounce — 50ms sonra tekrar kontrol
        if (!digitalRead(BTN_A)) {
            
            // --- V2.1 EKLENTİSİ --- AYARLAR seçiliyse flashlama yapma!
            // AYARLAR bir oyun değil, sistem uygulaması — flashlanmaz
            if (carouselSel == SETTINGS_INDEX) {
                openSettingsMenu();
                // openSettingsMenu() içinde carousel yeniden çiziliyor
            } else {
                // Normal oyun başlatma rutini
                const char* targetFilename = games[carouselSel].filename;
                
                // NVS Cache kontrolü — aynı oyun zaten yüklü mü?
                // "last_game" NVS anahtarı son flashlanan oyunu tutar
                Preferences prefs;
                prefs.begin("os", true);  // true = salt-oku
                char lastGame[32] = "";
                prefs.getString("last_game", lastGame, sizeof(lastGame));
                prefs.end();
                
                // Eğer son yüklenen oyun = hedef oyun → hızlı başlat
                bool isCached = (strcmp(lastGame, targetFilename) == 0);
                
                // Başlatma animasyonu göster
                // fastBoot=true: yeşil hızlı başlatma
                // fastBoot=false: accent flash yazma
                drawLaunchAnimation(carouselSel, isCached);
                
                // OLED'de de bilgi göster — uyku modundan çıkar
                if (oledReady) {
                    oled.setPowerSave(0); // Uyku modundan cikar
                    oled.clearBuffer();
                    oled.setFont(u8g2_font_7x14B_tr); // Kalın font — oyun adı
                    oled.drawStr(10, 20, games[carouselSel].label);
                    oled.setFont(u8g2_font_6x10_tr); // Normal font — durum
                    if (isCached) {
                        oled.drawStr(0, 40, "Hizli Baslatma!");
                    } else {
                        oled.drawStr(0, 40, "Flash yaziliyor...");
                    }
                    oled.sendBuffer();
                }
                
                if (isCached) {
                    // ─── HIZLI BAŞLATMA — flash zaten bu oyuna ayarlı ───
                    // Sadece boot partition'ı aktif et ve restart
                    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
                    if (target) {
                        esp_ota_set_boot_partition(target); // Bu partition'dan boot et
                    }
                    ESP.restart(); // Restart → oyun başlar
                } else {
                    // ─── NORMAL BAŞLATMA — flash yazma gerekli ───
                    // RTC'ye magic + dosya adı yaz, sonra restart
                    // Restart sonrası setup() bootloader moduna girecek
                    bootModeMagic = 0xDEADBEEF; // Magic byte = bootloader modu
                    strncpy(bootFilename, targetFilename, sizeof(bootFilename) - 1);
                    bootFilename[sizeof(bootFilename) - 1] = '\0'; // Null-termination
                    ESP.restart(); // Restart → bootloader başlar → flash yazar → restart → oyun
                }
            }
        }
    }
    
    // ═══ BTN_B: Bilgi Ekranı + Yüksek Skor ═══
    // B butonu — seçili oyun hakkında bilgi popup'ı
    if (!digitalRead(BTN_B)) {
        delay(50); // Debounce
        if (!digitalRead(BTN_B)) {
            playSound(600, 30); // Açılış sesi
            
            // Basit bilgi popup'ı — 140x92 lacivert kutu
            tft.fillRect(10, 18, 140, 92, RGB_FIX(15, 15, 35));
            // İki katmanlı çerçeve — tema rengi + sönük
            tft.drawRect(10, 18, 140, 92, games[carouselSel].primaryColor);
            tft.drawRect(11, 19, 138, 90, games[carouselSel].dimColor);
            
            // Oyun adı — popup başlığı
            tft.setTextSize(1);
            tft.setTextColor(games[carouselSel].primaryColor);
            tft.setCursor(16, 24);
            tft.print(games[carouselSel].label);
            
            // Başlık altı ayraç çizgisi
            tft.drawFastHLine(16, 34, 120, games[carouselSel].dimColor);
            
            // Dosya yolu bilgisi
            tft.setTextColor(COL_GRAY);
            tft.setCursor(16, 40);
            tft.print("Dosya:");
            tft.setTextColor(COL_WHITE);
            tft.setCursor(16, 50);
            tft.print(games[carouselSel].filename);
            
            // NVS durumunu göster — oyun yüklü mü, flash gerekli mi?
            Preferences prefs;
            prefs.begin("os", true); // Salt-oku
            char lastGame[32] = "";
            prefs.getString("last_game", lastGame, sizeof(lastGame));
            
            tft.setCursor(16, 64);
            
            // --- V2.2: Ayarlar sayfası bilgisi güncellemesi ---
            if (carouselSel == SETTINGS_INDEX) {
                // AYARLAR — sistem uygulaması, flashlama yok
                tft.setTextColor(COL_SUCCESS);
                tft.print("Durum: Sistem Uyg.");
            } else {
                // Normal oyun — NVS'te kayıtlı son oyunla karşılaştır
                if (strcmp(lastGame, games[carouselSel].filename) == 0) {
                    // Bu oyun zaten yüklü — hızlı başlatma mümkün
                    tft.setTextColor(COL_SUCCESS);
                    tft.print("Durum: Yuklu");
                } else {
                    // Başka oyun yüklü — flash yazma gerekli
                    tft.setTextColor(RGB_FIX(255, 200, 60)); // Turuncu — uyarı
                    tft.print("Durum: Flash gerekli");
                }
            }
            
            // --- V2.1 EKLENTİSİ --- Yüksek Skor gösterimi
            // NVS'ten oyunun skor anahtarıyla değeri oku
            tft.setCursor(16, 78);
            if (carouselSel != SETTINGS_INDEX && strlen(games[carouselSel].hsKey) > 0) {
                // Skor anahtarı boş değil — oyunun skoru var
                int32_t highScore = prefs.getInt(games[carouselSel].hsKey, 0); // Varsayılan: 0
                tft.setTextColor(RGB_FIX(255, 220, 80)); // Altın sarısı
                char hsLine[32];
                // "En Yuksek Skor: 1234" formatında satır
                snprintf(hsLine, sizeof(hsLine), "En Yuksek Skor: %d", (int)highScore);
                tft.print(hsLine);
            } else {
                // Skor sistemi yok (DOOM, MODE7, vb.) veya AYARLAR
                tft.setTextColor(COL_DARK_GRAY);
                tft.print("Skor: -");
            }
            
            prefs.end(); // NVS'i kapat
            
            // Kapatma ipucu — altta koyu gri
            tft.setTextColor(COL_DARK_GRAY);
            tft.setCursor(32, 100);
            tft.print("[B] ile kapat");
            
            // Buton bekleme protokolü:
            // 1. B hala basılıyken bekle (kullanıcı basılı tutar)
            while (!digitalRead(BTN_B)) delay(30);
            // 2. B tekrar basılana kadar bekle (kapatma için)
            while (digitalRead(BTN_B)) delay(30);
            delay(100); // Ekstra debounce
            
            // Menüyü yeniden çiz — popup kapatıldı
            drawCarousel(true);
        }
    }
    
    // Frame limiting — millis() tabanlı (delay() yerine)
    // 20ms = ~50 FPS — daha hızlı olursa CPU boşuna çalışır, ekran flicker olabilir
    // delay() yerine millis() farkı kullanır → daha doğru zamanlama
    static uint32_t lastFrame = 0; // static — değer korunur
    uint32_t frameTime = millis() - lastFrame;
    if (frameTime < 20) {
        // 20ms dolmadı — kalan süreyi delay ile bekle
        delay(20 - frameTime);
    }
    lastFrame = millis(); // Bu frame'in zamanını kaydet
}

/*
 * ══════════════════════════════════════════════════════════════
 *  DEĞİŞİKLİK ÖZETİ — V2.0 → V2.1
 * ══════════════════════════════════════════════════════════════
 * 
 * [YENİ] PACMAN oyunu — ikon, renk, carousel desteği
 * [YENİ] AYARLAR sistemi — dişli ikon, ses aç/kapat, NVS kayıt
 * [YENİ] playSound() — tone() sarmalayıcı, ses kapalıyken sessiz
 * [YENİ] openSettingsMenu() — popup menü, joystick/buton kontrolü
 * [YENİ] Yüksek skor gösterimi — BTN_B bilgi ekranında NVS okuma
 * [YENİ] GameInfo.hsKey — Her oyun için NVS skor anahtarı
 * [YENİ] soundEnabled global — NVS'ten boot'ta okunur
 * [YENİ] Status bar ses ikonu — ses açık/kapalı göstergesi
 * [GÜN] GAME_COUNT 5→7, SETTINGS_INDEX=6
 * [GÜN] Tüm tone() → playSound() (bootloader hariç)
 * [GÜN] Boot animasyonu "v2.1" versiyonu
 * [GÜN] drawPageDots() genişletildi (7 nokta)
 * [GÜN] BTN_A → AYARLAR kontrolü eklendi (flashlama engeli)
 * [GÜN] BTN_B → Yüksek skor satırı eklendi
 * 
 * KORUNAN:
 * - Pin tanımları, RGB_FIX(), RTC değişkenleri
 * - runEarlyBootloader() — Aynen
 * - NVS cache sistemi — Aynen  
 * - SPI izolasyonu — Aynen
 * - esp_ota_set_boot_partition() mantığı — Aynen
 * - OLED güç tasarrufu (setPowerSave) — Aynen
 * - Sadece X ekseni navigasyon — Aynen
 * - Mevcut 5 oyun ikonu — Aynen
 * 
 * ══════════════════════════════════════════════════════════════
 */
