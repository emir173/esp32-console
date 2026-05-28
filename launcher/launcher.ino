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

#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_ota_ops.h>

TFT_eSPI tft = TFT_eSPI();
U8G2_SH1106_128X64_NONAME_F_SW_I2C oled(U8G2_R0, 9, 8, U8X8_PIN_NONE);

// ─── SPI HATTI PIN TANIMLARI ───
#define SPI_SCK  12
#define SPI_MOSI 11
#define SPI_MISO 42

#define TFT_CS   15
#define SD_CS    10
#define JOY_X    1
#define JOY_Y    2
#define JOY_SW   18
#define BTN_A    3   
#define BTN_B    21
#define BTN_C    4  
#define BTN_D    6    
#define BUZZER   5   

int joyCenterX = 0;
int joyCenterY = 0;

// ─── RTC Bellek: ESP.restart() sonrası korunur ───
RTC_NOINIT_ATTR uint32_t bootModeMagic; // 0xDEADBEEF = bootloader
RTC_NOINIT_ATTR char bootFilename[32];  // Hangi dosya yüklenecek?

// ─── Renk düzeltme fonksiyonu (donanım RGB sıralaması) ───
uint16_t RGB_FIX(uint8_t r, uint8_t g, uint8_t b) { return tft.color565(b, g, r); }

// ══════════════════════════════════════════════════════════════
//  OYUN BİLGİ YAPISI VE LİSTESİ
// ══════════════════════════════════════════════════════════════

// Her oyunun renk teması, dosya adı ve etiketini tutan yapı
struct GameInfo {
    const char* label;        // Oyun adı
    const char* filename;     // SD karttaki .bin dosya yolu
    uint16_t primaryColor;    // Ana tema rengi (seçili)
    uint16_t dimColor;        // Sönük rengi (seçili değil)
    const char* hsKey;        // --- V2.1 EKLENTİSİ --- NVS yüksek skor anahtarı
};

// --- V2.1 EKLENTİSİ --- 5 oyun + PACMAN + AYARLAR = 7
#define GAME_COUNT 7
#define SETTINGS_INDEX 6  // AYARLAR'ın carousel indeksi (son eleman)
// Not: RGB_FIX() tft başlatılmadan çağrılamaz, renkler setup sonrası atanacak
GameInfo games[GAME_COUNT];

// ─── Global State Değişkenleri ───
int carouselSel = 0;         // Seçili oyun indexi (0-6)
bool menuDrawn = false;      // İlk tam çizim yapıldı mı?
int prevSel = -1;            // Önceki seçim (partial redraw için)
uint32_t animTick = 0;       // Nabız animasyon zamanlayıcı
uint8_t pulseState = 0;      // Nabız kademe (0-2)
bool oledReady = false;      // OLED başlatıldı mı?
uint32_t lastOledUpdate = 0; // OLED istatistik güncelleme zamanlayıcı (1Hz)

// --- V2.1 EKLENTİSİ --- Ses açık/kapalı durumu (global, NVS'ten okunur)
bool soundEnabled = true;

// ─── Sık kullanılan renkler (setup'ta atanacak) ───
uint16_t COL_BG_DARK;        // Koyu arka plan
uint16_t COL_BG_STATUS;      // Status bar arka planı
uint16_t COL_ACCENT;         // Cyan accent
uint16_t COL_ACCENT_DIM;     // Koyu accent
uint16_t COL_WHITE;          // Beyaz
uint16_t COL_GRAY;           // Gri metin
uint16_t COL_DARK_GRAY;      // Koyu gri
uint16_t COL_BLACK;          // Siyah
uint16_t COL_SUCCESS;        // Yeşil
uint16_t COL_DANGER;         // Kırmızı

// ══════════════════════════════════════════════════════════════
// --- V2.1 EKLENTİSİ --- SES SARMALAYICI FONKSİYON
// ══════════════════════════════════════════════════════════════

// Tüm tone() çağrılarını saran fonksiyon — ses kapalıysa çalmaz
void playSound(uint16_t freq, uint32_t dur) {
    if (soundEnabled) {
        tone(BUZZER, freq, dur);
    }
}

// ══════════════════════════════════════════════════════════════
//  RENK PALETİNİ VE OYUN LİSTESİNİ BAŞLAT
// ══════════════════════════════════════════════════════════════

// TFT başlatıldıktan sonra çağrılmalı — renk değerleri donanıma bağlı
void initColorsAndGames() {
    COL_BG_DARK    = RGB_FIX(8, 8, 20);
    COL_BG_STATUS  = RGB_FIX(12, 12, 28);
    COL_ACCENT     = RGB_FIX(80, 180, 255);
    COL_ACCENT_DIM = RGB_FIX(30, 70, 100);
    COL_WHITE      = RGB_FIX(255, 255, 255);
    COL_GRAY       = RGB_FIX(120, 120, 120);
    COL_DARK_GRAY  = RGB_FIX(50, 50, 50);
    COL_BLACK      = RGB_FIX(0, 0, 0);
    COL_SUCCESS    = RGB_FIX(50, 255, 80);
    COL_DANGER     = RGB_FIX(255, 50, 50);

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

    // --- V2.1 EKLENTİSİ --- AYARLAR (Sistem uygulaması — flashlama yok!)
    games[6].label = "AYARLAR";
    games[6].filename = "/settings.bin";
    games[6].primaryColor = RGB_FIX(150, 150, 150);
    games[6].dimColor = RGB_FIX(50, 50, 50);
    games[6].hsKey = "";  // Ayarlar için skor yok
}

// ══════════════════════════════════════════════════════════════
//  ERKEN BOOTLOADER — FLASH YAZMA (DOKUNULMADI!)
// ══════════════════════════════════════════════════════════════

// SD karttan hedef .bin dosyasını OTA ile flash'a yazan bootloader
// Bu fonksiyonun mantığı AYNEN korunmuştur — sadece OLED mesajları var
void runEarlyBootloader(const char* filename) {
    oled.begin();
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(0, 12, "BOOTLOADER V8.3");
    oled.drawStr(0, 24, "Oyun Yukleniyor...");
    oled.sendBuffer();

    File binFile = SD.open(filename, FILE_READ);
    if (!binFile) {
        oled.clearBuffer();
        oled.drawStr(0, 20, "HATA: DOSYA YOK!");
        oled.sendBuffer();
        while (digitalRead(BTN_B)) delay(50);
        return;
    }

    size_t fileSize = binFile.size();
    if (!Update.begin(fileSize)) {
        binFile.close();
        oled.clearBuffer();
        oled.drawStr(0, 20, "HATA: FLASH DOLU!");
        oled.sendBuffer();
        while (digitalRead(BTN_B)) delay(50);
        return;
    }

    // PSRAM darboğazını aşmak için donanımsal DMA uyumlu İç RAM kullanıyoruz (4KB)
    uint8_t buf[4096];
    size_t written = 0;
    bool flashError = false;
    uint32_t lastDraw = 0;

    while (binFile.available()) {
        int bytesRead = binFile.read(buf, sizeof(buf));
        if (bytesRead <= 0) break;

        if (Update.write(buf, bytesRead) != (size_t)bytesRead) {
            flashError = true;
            break;
        }
        written += bytesRead;

        uint32_t now = millis();
        if (now - lastDraw > 1000 || written == fileSize) {
            lastDraw = now;
            int pct = (written * 100) / fileSize;
            oled.clearBuffer();
            oled.setFont(u8g2_font_6x10_tr);
            oled.drawStr(0, 12, "FLASHING...");
            char progStr[24];
            sprintf(progStr, "%d%%  %dK/%dK", pct, (int)(written/1024), (int)(fileSize/1024));
            oled.drawStr(0, 28, progStr);
            oled.drawFrame(0, 40, 128, 14);
            int barW = (124 * pct) / 100;
            if (barW > 0) oled.drawBox(2, 42, barW, 10);
            oled.sendBuffer();
        }
    }
    binFile.close();

    if (flashError) {
        oled.clearBuffer();
        oled.drawStr(0, 20, "FLASH YAZMA HATASI!");
        oled.sendBuffer();
        while (digitalRead(BTN_B)) delay(50);
        return;
    }

    if (Update.end(true)) {
        oled.clearBuffer();
        oled.setFont(u8g2_font_6x10_tr);
        oled.drawStr(15, 20, "YUKLEME BASARILI!");
        oled.drawStr(5, 40, "Oyun baslatiliyor...");
        oled.drawFrame(0, 50, 128, 10);
        oled.drawBox(2, 52, 124, 6);
        oled.sendBuffer();
        
        // --- V2.2: Yükleme Başarılı ekranı 1 saniye kalsın ve temizlensin ---
        if (soundEnabled) tone(BUZZER, 1500, 100);
        delay(1000); 
        oled.clearBuffer();
        oled.sendBuffer();
        noTone(BUZZER);
        // ----------------------------------------------------------------
        
        // NVS'e yeni oyunu kaydet
        Preferences prefs;
        prefs.begin("os", false); 
        prefs.putString("last_game", filename);
        prefs.end();
        
        ESP.restart(); // Yeni oyuna geçiş yap
    } else {
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
void drawVerticalGradient(int x, int y, int w, int h, uint16_t colTop, uint16_t colBot) {
    // colTop ve colBot arasında satır satır interpolasyon
    // Basit yaklaşım: üst yarı colTop, alt yarı colBot, orta geçiş
    int halfH = h / 2;
    for (int row = 0; row < h; row += 2) {
        uint16_t c;
        if (row < halfH) {
            c = colTop;
        } else {
            c = colBot;
        }
        tft.drawFastHLine(x, y + row, w, c);
        tft.drawFastHLine(x, y + row + 1, w, c);
    }
}

// Üst durum çubuğu çizimi — E-OS başlığı, SD ikonu ve accent çizgi
void drawStatusBar() {
    // Status bar arka planı
    tft.fillRect(0, 0, 160, 14, COL_BG_STATUS);
    
    // Sol: Küçük play üçgeni + E-OS yazısı
    // Play triangle (4x5px)
    tft.fillTriangle(4, 3, 4, 9, 8, 6, COL_ACCENT);
    
    tft.setTextSize(1);
    tft.setTextColor(COL_WHITE, COL_BG_STATUS);
    tft.setCursor(12, 3);
    tft.print("E-OS");
    
    // Pil placeholder ikonu
    tft.drawRect(150, 4, 7, 6, COL_GRAY);
    tft.drawFastVLine(157, 6, 2, COL_GRAY);
    tft.fillRect(151, 5, 5, 4, COL_SUCCESS); // Dolu pil
    
    // --- V2.1 EKLENTİSİ --- Ses durumu ikonu (status bar'da)
    int sx = 138; // Hoparlör X pozisyonu
    // Hoparlör Gövdesi (daha küçük)
    tft.fillRect(sx, 6, 2, 2, COL_WHITE);
    tft.drawFastVLine(sx + 2, 5, 4, COL_WHITE);
    tft.drawFastVLine(sx + 3, 4, 6, COL_WHITE);
    
    if (soundEnabled) {
        // Ses açık — Dalgalar
        tft.drawFastVLine(sx + 5, 5, 4, COL_SUCCESS);
        tft.drawFastVLine(sx + 7, 4, 6, COL_SUCCESS);
    } else {
        // Ses kapalı — Küçük X işareti
        tft.drawLine(sx + 5, 4, sx + 9, 9, COL_DANGER);
        tft.drawLine(sx + 9, 4, sx + 5, 9, COL_DANGER);
    }
    
    // Alt accent çizgi — ortadan kenarlara gradient efekti
    tft.drawFastHLine(0, 14, 160, COL_ACCENT_DIM);
    tft.drawFastHLine(30, 14, 100, COL_ACCENT);
    tft.drawFastHLine(55, 14, 50, RGB_FIX(120, 210, 255));
}

// Sayfa gösterge noktaları — seçili oyunu belirten noktalar
void drawPageDots(int selected) {
    int dotY = 98;
    int totalWidth = GAME_COUNT * 4 + (GAME_COUNT - 1) * 6; // Her dot 4px, arası 6px
    int startX = (160 - totalWidth) / 2;
    
    // Önce eski noktaları temizle
    tft.fillRect(0, dotY - 2, 160, 8, COL_BG_DARK);
    
    for (int i = 0; i < GAME_COUNT; i++) {
        int cx = startX + i * 10 + 2;
        int cy = dotY + 2;
        if (i == selected) {
            tft.fillCircle(cx, cy, 2, COL_WHITE);
        } else {
            tft.drawCircle(cx, cy, 2, COL_DARK_GRAY);
        }
    }
}

// Alt kontrol ipuçları çizimi
void drawControlHints() {
    int hintY = 114;
    // Üst ince çizgi
    tft.drawFastHLine(10, hintY - 2, 140, COL_DARK_GRAY);
    
    tft.setTextSize(1);
    tft.setTextColor(COL_GRAY, COL_BG_DARK);
    tft.setCursor(8, hintY);
    tft.print("A:");
    tft.setTextColor(COL_WHITE, COL_BG_DARK);
    tft.print("Baslat");
    
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
void drawBigIconDoom(int x, int y) {
    uint16_t bone  = RGB_FIX(255, 200, 150);
    uint16_t dark  = RGB_FIX(180, 80, 20);
    uint16_t glow  = RGB_FIX(255, 100, 20);
    uint16_t eye   = RGB_FIX(255, 40, 0);
    uint16_t shadow = RGB_FIX(100, 40, 10);
    
    tft.fillRoundRect(x + 10, y + 4, 28, 26, 8, bone);
    tft.fillRoundRect(x + 12, y + 6, 24, 22, 6, RGB_FIX(240, 190, 140));
    tft.fillRoundRect(x + 14, y + 12, 8, 8, 3, RGB_FIX(30, 5, 0));
    tft.fillRoundRect(x + 26, y + 12, 8, 8, 3, RGB_FIX(30, 5, 0));
    tft.fillCircle(x + 17, y + 15, 2, eye);
    tft.fillCircle(x + 29, y + 15, 2, eye);
    tft.drawPixel(x + 16, y + 14, glow);
    tft.drawPixel(x + 28, y + 14, glow);
    tft.fillTriangle(x + 22, y + 20, x + 25, y + 20, x + 23, y + 24, RGB_FIX(30, 5, 0));
    tft.fillRect(x + 14, y + 27, 20, 4, shadow);
    for (int i = 0; i < 5; i++) {
        tft.fillRect(x + 15 + i * 4, y + 27, 3, 5, bone);
    }
    tft.drawLine(x + 20, y + 5, x + 16, y + 11, dark);
    tft.drawLine(x + 30, y + 6, x + 33, y + 11, dark);
    tft.fillTriangle(x + 8, y + 47, x + 12, y + 34, x + 16, y + 47, RGB_FIX(255, 60, 0));
    tft.fillTriangle(x + 20, y + 47, x + 24, y + 32, x + 28, y + 47, RGB_FIX(255, 80, 0));
    tft.fillTriangle(x + 32, y + 47, x + 36, y + 35, x + 40, y + 47, RGB_FIX(255, 60, 0));
    tft.drawPixel(x + 12, y + 36, RGB_FIX(255, 200, 50));
    tft.drawPixel(x + 24, y + 34, RGB_FIX(255, 220, 50));
    tft.drawPixel(x + 36, y + 37, RGB_FIX(255, 200, 50));
}

// SNAKE İkonu — Kıvrımlı yılan (48x48)
void drawBigIconSnake(int x, int y) {
    uint16_t body    = RGB_FIX(40, 200, 80);
    uint16_t bodyLt  = RGB_FIX(80, 255, 120);
    uint16_t bodyDk  = RGB_FIX(20, 140, 50);
    uint16_t head    = RGB_FIX(60, 255, 100);
    uint16_t apple   = RGB_FIX(255, 50, 40);
    
    tft.fillRoundRect(x + 6, y + 34, 24, 6, 2, body);
    tft.drawFastHLine(x + 6, y + 35, 24, bodyLt);
    tft.fillRoundRect(x + 26, y + 20, 6, 18, 2, body);
    tft.drawFastVLine(x + 27, y + 20, 18, bodyLt);
    tft.fillRoundRect(x + 12, y + 16, 18, 6, 2, body);
    tft.drawFastHLine(x + 12, y + 17, 18, bodyLt);
    tft.fillRoundRect(x + 10, y + 6, 6, 14, 2, body);
    tft.drawFastVLine(x + 11, y + 6, 14, bodyLt);
    tft.fillRoundRect(x + 8, y + 2, 10, 8, 3, head);
    tft.fillCircle(x + 11, y + 5, 1, COL_WHITE);
    tft.drawPixel(x + 12, y + 5, COL_BLACK);
    tft.fillCircle(x + 15, y + 5, 1, COL_WHITE);
    tft.drawPixel(x + 16, y + 5, COL_BLACK);
    tft.drawFastHLine(x + 18, y + 6, 3, RGB_FIX(255, 80, 80));
    tft.drawPixel(x + 21, y + 5, RGB_FIX(255, 80, 80));
    tft.drawPixel(x + 21, y + 7, RGB_FIX(255, 80, 80));
    tft.fillRect(x + 4, y + 36, 4, 4, bodyDk);
    tft.fillRect(x + 2, y + 38, 3, 2, bodyDk);
    tft.fillCircle(x + 38, y + 12, 4, apple);
    tft.fillCircle(x + 38, y + 11, 4, RGB_FIX(255, 70, 50));
    tft.drawPixel(x + 36, y + 10, RGB_FIX(255, 150, 140));
    tft.fillRect(x + 38, y + 6, 2, 3, RGB_FIX(80, 180, 40));
    tft.fillCircle(x + 36, y + 40, 3, apple);
    tft.drawPixel(x + 35, y + 39, RGB_FIX(255, 150, 140));
}

// FLAPPY BIRD İkonu — Tatlı kuş + borular (48x48)
void drawBigIconFlappy(int x, int y) {
    uint16_t bodyCol  = RGB_FIX(255, 220, 40);
    uint16_t wingCol  = RGB_FIX(255, 170, 20);
    uint16_t beakCol  = RGB_FIX(255, 100, 30);
    uint16_t pipeCol  = RGB_FIX(80, 200, 60);
    uint16_t pipeLt   = RGB_FIX(120, 240, 100);
    
    tft.fillRect(x + 2, y + 0, 10, 18, pipeCol);
    tft.fillRect(x + 0, y + 16, 14, 4, pipeCol);
    tft.drawFastVLine(x + 3, y + 0, 18, pipeLt);
    tft.fillRect(x + 0, y + 36, 14, 4, pipeCol);
    tft.fillRect(x + 2, y + 38, 10, 10, pipeCol);
    tft.drawFastVLine(x + 3, y + 38, 10, pipeLt);
    tft.fillRect(x + 36, y + 0, 10, 10, pipeCol);
    tft.fillRect(x + 34, y + 8, 14, 4, pipeCol);
    tft.drawFastVLine(x + 37, y + 0, 10, pipeLt);
    tft.fillRect(x + 34, y + 40, 14, 4, pipeCol);
    tft.fillRect(x + 36, y + 42, 10, 6, pipeCol);
    tft.drawFastVLine(x + 37, y + 42, 6, pipeLt);
    tft.fillCircle(x + 24, y + 24, 8, bodyCol);
    tft.fillCircle(x + 24, y + 23, 8, RGB_FIX(255, 230, 60));
    tft.fillCircle(x + 25, y + 27, 5, RGB_FIX(255, 240, 180));
    tft.fillRoundRect(x + 14, y + 22, 10, 6, 2, wingCol);
    tft.drawFastHLine(x + 15, y + 23, 8, RGB_FIX(255, 190, 50));
    tft.fillCircle(x + 28, y + 21, 4, COL_WHITE);
    tft.fillCircle(x + 29, y + 21, 2, COL_BLACK);
    tft.drawPixel(x + 28, y + 20, COL_WHITE);
    tft.fillRoundRect(x + 31, y + 24, 8, 4, 1, beakCol);
    tft.fillRoundRect(x + 31, y + 26, 8, 3, 1, RGB_FIX(220, 80, 20));
    tft.fillRect(x + 15, y + 20, 3, 2, wingCol);
}

// SPACE INVADERS İkonu — Klasik alien + gemi (48x48)
void drawBigIconSpace(int x, int y) {
    uint16_t alien   = RGB_FIX(220, 60, 255);
    uint16_t alienLt = RGB_FIX(255, 120, 255);
    uint16_t ship    = RGB_FIX(100, 220, 255);
    uint16_t shipDk  = RGB_FIX(60, 140, 180);
    uint16_t star    = RGB_FIX(200, 200, 255);
    uint16_t laser   = RGB_FIX(255, 255, 80);
    
    tft.drawPixel(x + 5, y + 3, star);
    tft.drawPixel(x + 42, y + 8, star);
    tft.drawPixel(x + 15, y + 42, star);
    tft.drawPixel(x + 40, y + 38, star);
    tft.drawPixel(x + 2, y + 28, star);
    tft.drawPixel(x + 45, y + 22, star);
    tft.drawPixel(x + 30, y + 2, star);
    tft.fillRect(x + 16, y + 4, 16, 4, alien);
    tft.fillRect(x + 12, y + 8, 24, 4, alien);
    tft.fillRect(x + 10, y + 10, 28, 4, alien);
    tft.fillRect(x + 10, y + 14, 28, 4, alien);
    tft.fillRect(x + 14, y + 10, 4, 4, COL_BLACK);
    tft.fillRect(x + 30, y + 10, 4, 4, COL_BLACK);
    tft.drawPixel(x + 14, y + 10, alienLt);
    tft.drawPixel(x + 30, y + 10, alienLt);
    tft.fillRect(x + 12, y + 2, 2, 4, alien);
    tft.fillRect(x + 34, y + 2, 2, 4, alien);
    tft.drawPixel(x + 11, y + 1, alienLt);
    tft.drawPixel(x + 35, y + 1, alienLt);
    tft.fillRect(x + 10, y + 18, 4, 4, alien);
    tft.fillRect(x + 18, y + 18, 4, 2, alien);
    tft.fillRect(x + 26, y + 18, 4, 2, alien);
    tft.fillRect(x + 34, y + 18, 4, 4, alien);
    tft.drawFastVLine(x + 20, y + 22, 6, laser);
    tft.drawFastVLine(x + 28, y + 23, 5, laser);
    tft.fillRect(x + 16, y + 38, 16, 4, ship);
    tft.fillRect(x + 20, y + 36, 8, 2, ship);
    tft.fillRect(x + 22, y + 34, 4, 2, shipDk);
    tft.drawPixel(x + 24, y + 33, ship);
    tft.drawFastVLine(x + 24, y + 28, 5, RGB_FIX(80, 255, 80));
}

// ARKANOID İkonu — Tuğlalar + top + paddle (48x48)
void drawBigIconArkanoid(int x, int y) {
    uint16_t paddle = RGB_FIX(30, 210, 255);
    uint16_t padLt  = RGB_FIX(100, 230, 255);
    uint16_t ball   = COL_WHITE;
    
    uint16_t brickColors[] = {
        RGB_FIX(255, 60, 40),
        RGB_FIX(255, 160, 30),
        RGB_FIX(255, 230, 40),
        RGB_FIX(60, 220, 60),
        RGB_FIX(60, 140, 255),
    };
    
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            int bx = x + 4 + col * 11;
            int by = y + 2 + row * 6;
            tft.fillRect(bx, by, 10, 5, brickColors[row]);
            tft.drawRect(bx, by, 10, 5, RGB_FIX(40, 40, 40));
            tft.drawFastHLine(bx + 1, by + 1, 8, RGB_FIX(255, 255, 200));
        }
    }
    
    tft.fillRect(x + 4, y + 2, 10, 5, COL_BG_DARK);
    tft.fillRect(x + 26, y + 8, 10, 5, COL_BG_DARK);
    tft.fillRect(x + 37, y + 2, 10, 5, COL_BG_DARK);
    tft.fillCircle(x + 24, y + 34, 3, ball);
    tft.drawPixel(x + 23, y + 33, RGB_FIX(200, 200, 255));
    tft.drawPixel(x + 20, y + 36, COL_DARK_GRAY);
    tft.drawPixel(x + 17, y + 38, COL_DARK_GRAY);
    tft.fillRoundRect(x + 12, y + 42, 24, 4, 2, paddle);
    tft.drawFastHLine(x + 14, y + 42, 20, padLt);
}

// --- V2.1 EKLENTİSİ --- PACMAN İkonu (48x48)
void drawBigIconPacman(int x, int y) {
    uint16_t pacCol  = RGB_FIX(255, 255, 0);
    uint16_t pacDk   = RGB_FIX(200, 200, 0);
    uint16_t dotCol  = RGB_FIX(255, 200, 150);
    uint16_t ghostB  = RGB_FIX(255, 80, 80);   // Kırmızı hayalet
    uint16_t ghostEye = COL_WHITE;
    uint16_t mazeCol = RGB_FIX(30, 30, 200);
    
    // Labirent duvarları (arka plan)
    tft.drawFastHLine(x + 0, y + 0, 48, mazeCol);
    tft.drawFastHLine(x + 0, y + 47, 48, mazeCol);
    tft.drawFastVLine(x + 0, y + 0, 48, mazeCol);
    tft.drawFastVLine(x + 47, y + 0, 48, mazeCol);
    tft.drawFastHLine(x + 8, y + 10, 14, mazeCol);
    tft.drawFastHLine(x + 28, y + 10, 14, mazeCol);
    tft.drawFastHLine(x + 8, y + 38, 14, mazeCol);
    tft.drawFastHLine(x + 28, y + 38, 14, mazeCol);
    
    // Pac-Man (ağız açık, sağa bakıyor)
    tft.fillCircle(x + 16, y + 24, 10, pacCol);
    tft.fillCircle(x + 16, y + 23, 10, RGB_FIX(255, 255, 40)); // Üst highlight
    // Ağız (üçgen kesim)
    tft.fillTriangle(x + 18, y + 24, x + 28, y + 20, x + 28, y + 28, RGB_FIX(12, 12, 20));
    // Göz
    tft.fillCircle(x + 16, y + 19, 2, COL_BLACK);
    tft.drawPixel(x + 15, y + 18, COL_WHITE);
    
    // Yem noktaları (Pac-Man'ın önünde)
    tft.fillCircle(x + 32, y + 24, 2, dotCol);
    tft.fillCircle(x + 39, y + 24, 1, dotCol);
    tft.fillCircle(x + 44, y + 24, 1, dotCol);
    
    // Güç hapı (büyük nokta)
    tft.fillCircle(x + 40, y + 12, 3, dotCol);
    
    // Kırmızı hayalet (sağ altta)
    tft.fillRoundRect(x + 32, y + 32, 12, 10, 4, ghostB);
    tft.fillRect(x + 32, y + 38, 12, 6, ghostB);
    // Hayalet alt dalgası
    tft.fillRect(x + 32, y + 42, 3, 2, RGB_FIX(12, 12, 20));
    tft.fillRect(x + 38, y + 42, 3, 2, RGB_FIX(12, 12, 20));
    // Hayalet gözleri
    tft.fillCircle(x + 36, y + 36, 2, ghostEye);
    tft.fillCircle(x + 40, y + 36, 2, ghostEye);
    tft.fillCircle(x + 37, y + 36, 1, RGB_FIX(30, 30, 200));
    tft.fillCircle(x + 41, y + 36, 1, RGB_FIX(30, 30, 200));
}

// --- V2.1 EKLENTİSİ --- AYARLAR İkonu — Dişli çark (48x48)
void drawBigIconSettings(int x, int y) {
    uint16_t gearOuter = RGB_FIX(150, 150, 150);
    uint16_t gearInner = RGB_FIX(100, 100, 100);
    uint16_t gearCenter = RGB_FIX(60, 60, 60);
    uint16_t accent = COL_ACCENT;
    
    int cx = x + 24;
    int cy = y + 22;
    
    // Ana dişli gövdesi (büyük daire)
    tft.fillCircle(cx, cy, 14, gearOuter);
    tft.fillCircle(cx, cy, 11, gearInner);
    tft.fillCircle(cx, cy, 6, gearCenter);
    // Merkez delik
    tft.fillCircle(cx, cy, 3, RGB_FIX(12, 12, 20));
    
    // Dişli dişleri (8 yön — dikdörtgenlerle)
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
    
    // Merkez accent vurgusu
    tft.drawCircle(cx, cy, 3, accent);
    
    // Alt yazı: küçük "SET" etiketi
    tft.setTextSize(1);
    tft.setTextColor(gearOuter, RGB_FIX(12, 12, 20));
    tft.setCursor(x + 6, y + 40); // 36px genislik, 48px karta ortalandi
    tft.print("CONFIG");
}

// ══════════════════════════════════════════════════════════════
//  KÜÇÜK İKONLAR (20x20 piksel) — Carousel komşu kartlar
// ══════════════════════════════════════════════════════════════

// DOOM Küçük İkon — Basit kafatası silüeti
void drawSmallIconDoom(int x, int y, uint16_t col) {
    tft.fillRoundRect(x + 4, y + 2, 12, 11, 4, col);
    tft.fillRect(x + 6, y + 6, 3, 3, COL_BLACK);
    tft.fillRect(x + 11, y + 6, 3, 3, COL_BLACK);
    tft.fillRect(x + 9, y + 10, 2, 2, COL_BLACK);
    tft.fillRect(x + 5, y + 13, 10, 2, col);
    for (int i = 0; i < 3; i++) {
        tft.fillRect(x + 6 + i * 3, y + 13, 2, 3, RGB_FIX(200, 180, 140));
    }
    tft.fillRect(x + 3, y + 17, 2, 2, RGB_FIX(255, 80, 0));
    tft.fillRect(x + 9, y + 16, 2, 3, RGB_FIX(255, 80, 0));
    tft.fillRect(x + 15, y + 17, 2, 2, RGB_FIX(255, 80, 0));
}

// SNAKE Küçük İkon — Basit yılan
void drawSmallIconSnake(int x, int y, uint16_t col) {
    tft.fillRect(x + 3, y + 14, 10, 3, col);
    tft.fillRect(x + 11, y + 8, 3, 9, col);
    tft.fillRect(x + 5, y + 6, 8, 3, col);
    tft.fillRect(x + 4, y + 2, 4, 6, col);
    tft.drawPixel(x + 5, y + 3, COL_WHITE);
    tft.fillCircle(x + 16, y + 4, 2, RGB_FIX(255, 50, 40));
}

// FLAPPY Küçük İkon — Basit kuş
void drawSmallIconFlappy(int x, int y, uint16_t col) {
    tft.fillCircle(x + 8, y + 10, 5, col);
    tft.fillCircle(x + 8, y + 9, 5, RGB_FIX(255, 230, 60));
    tft.fillRect(x + 3, y + 9, 5, 3, RGB_FIX(255, 170, 20));
    tft.fillCircle(x + 11, y + 8, 2, COL_WHITE);
    tft.drawPixel(x + 12, y + 8, COL_BLACK);
    tft.fillRect(x + 14, y + 10, 4, 2, RGB_FIX(255, 100, 30));
    tft.fillRect(x + 1, y + 0, 4, 5, RGB_FIX(80, 200, 60));
    tft.fillRect(x + 1, y + 16, 4, 4, RGB_FIX(80, 200, 60));
}

// SPACE Küçük İkon — Basit alien
void drawSmallIconSpace(int x, int y, uint16_t col) {
    tft.fillRect(x + 6, y + 2, 8, 3, col);
    tft.fillRect(x + 4, y + 5, 12, 3, col);
    tft.fillRect(x + 3, y + 6, 14, 3, col);
    tft.fillRect(x + 5, y + 6, 2, 2, COL_BLACK);
    tft.fillRect(x + 13, y + 6, 2, 2, COL_BLACK);
    tft.fillRect(x + 4, y + 9, 2, 2, col);
    tft.fillRect(x + 14, y + 9, 2, 2, col);
    tft.fillRect(x + 7, y + 15, 6, 2, RGB_FIX(100, 220, 255));
    tft.fillRect(x + 9, y + 14, 2, 1, RGB_FIX(100, 220, 255));
}

// ARKANOID Küçük İkon — Tuğla + top + paddle
void drawSmallIconArkanoid(int x, int y, uint16_t col) {
    tft.fillRect(x + 2, y + 2, 5, 3, RGB_FIX(255, 60, 40));
    tft.fillRect(x + 8, y + 2, 5, 3, RGB_FIX(60, 220, 60));
    tft.fillRect(x + 14, y + 2, 5, 3, RGB_FIX(60, 140, 255));
    tft.fillRect(x + 2, y + 6, 5, 3, RGB_FIX(255, 230, 40));
    tft.fillRect(x + 8, y + 6, 5, 3, RGB_FIX(255, 160, 30));
    tft.fillRect(x + 14, y + 6, 5, 3, RGB_FIX(255, 60, 40));
    tft.fillCircle(x + 10, y + 13, 2, COL_WHITE);
    tft.fillRect(x + 5, y + 17, 10, 2, col);
}

// --- V2.1 EKLENTİSİ --- PACMAN Küçük İkon (20x20)
void drawSmallIconPacman(int x, int y, uint16_t col) {
    // Pac-Man gövdesi
    tft.fillCircle(x + 7, y + 10, 6, col);
    // Ağız
    tft.fillTriangle(x + 9, y + 10, x + 15, y + 7, x + 15, y + 13, RGB_FIX(8, 8, 16));
    // Göz
    tft.drawPixel(x + 7, y + 7, COL_BLACK);
    // Yem noktaları
    tft.fillCircle(x + 16, y + 10, 1, RGB_FIX(255, 200, 150));
}

// --- V2.1 EKLENTİSİ --- AYARLAR Küçük İkon — Mini dişli (20x20)
void drawSmallIconSettings(int x, int y, uint16_t col) {
    int cx = x + 10;
    int cy = y + 10;
    tft.fillCircle(cx, cy, 6, col);
    tft.fillCircle(cx, cy, 4, RGB_FIX(80, 80, 80));
    tft.fillCircle(cx, cy, 2, RGB_FIX(12, 12, 20));
    // Dişler
    tft.fillRect(cx - 2, cy - 8, 4, 3, col);
    tft.fillRect(cx - 2, cy + 5, 4, 3, col);
    tft.fillRect(cx - 8, cy - 2, 3, 4, col);
    tft.fillRect(cx + 5, cy - 2, 3, 4, col);
}

// ══════════════════════════════════════════════════════════════
//  İKON ÇİZİM YÖNLENDİRİCİLER (index'e göre doğru ikonu çağırır)
// ══════════════════════════════════════════════════════════════

// Büyük ikon çizimi — carousel merkez kartı için
void drawBigIcon(int index, int x, int y) {
    switch (index) {
        case 0: drawBigIconDoom(x, y);     break;
        case 1: drawBigIconSnake(x, y);    break;
        case 2: drawBigIconFlappy(x, y);   break;
        case 3: drawBigIconSpace(x, y);    break;
        case 4: drawBigIconArkanoid(x, y); break;
        case 5: drawBigIconPacman(x, y);   break;  // --- V2.1 ---
        case 6: drawBigIconSettings(x, y); break;  // --- V2.1 ---
    }
}

// Küçük ikon çizimi — carousel komşu kartlar için
void drawSmallIcon(int index, int x, int y, uint16_t col) {
    switch (index) {
        case 0: drawSmallIconDoom(x, y, col);     break;
        case 1: drawSmallIconSnake(x, y, col);    break;
        case 2: drawSmallIconFlappy(x, y, col);   break;
        case 3: drawSmallIconSpace(x, y, col);    break;
        case 4: drawSmallIconArkanoid(x, y, col); break;
        case 5: drawSmallIconPacman(x, y, col);   break;  // --- V2.1 ---
        case 6: drawSmallIconSettings(x, y, col); break;  // --- V2.1 ---
    }
}

// ══════════════════════════════════════════════════════════════
//  ANA CAROUSEL ÇİZİM FONKSİYONU
// ══════════════════════════════════════════════════════════════

// Carousel'in sabit bölgelerini çizer (status bar, hints — bir kez çağrılır)
void drawCarouselFrame() {
    // Arka plan gradient (basitleştirilmiş — 3 bölge)
    tft.fillRect(0, 0, 160, 42, RGB_FIX(10, 10, 24));
    tft.fillRect(0, 42, 160, 43, COL_BG_DARK);
    tft.fillRect(0, 85, 160, 43, RGB_FIX(5, 5, 14));
    
    drawStatusBar();
    drawControlHints();
}

// Seçili oyunun merkez kartını çizer (48x48 ikon + çerçeve)
void drawCenterCard(int sel) {
    int cx = 54;  // (160 - 52) / 2
    int cy = 18;
    int cardW = 52;
    int cardH = 56;
    
    uint16_t borderCol = games[sel].primaryColor;
    
    tft.fillRect(cx - 2, cy - 1, cardW + 4, cardH + 2, COL_BG_DARK);
    tft.drawRect(cx - 1, cy - 1, cardW + 2, cardH + 2, games[sel].dimColor);
    tft.drawRect(cx, cy, cardW, cardH, borderCol);
    tft.drawRect(cx + 1, cy + 1, cardW - 2, cardH - 2, borderCol);
    tft.fillRect(cx + 2, cy + 2, cardW - 4, cardH - 4, RGB_FIX(12, 12, 20));
    drawBigIcon(sel, cx + 2, cy + 4);
}

// Sol veya sağ komşu kartı çizer (küçük ikon)
void drawSideCard(int sel, int side) {
    int index = (sel + side + GAME_COUNT) % GAME_COUNT;
    
    int cardW = 24;
    int cardH = 28;
    int cx, cy;
    
    if (side == -1) {
        cx = 6;
        cy = 32;
    } else {
        cx = 130;
        cy = 32;
    }
    
    uint16_t borderCol = games[index].dimColor;
    
    tft.fillRect(cx - 1, cy - 1, cardW + 2, cardH + 2, COL_BG_DARK);
    tft.drawRect(cx, cy, cardW, cardH, borderCol);
    tft.fillRect(cx + 1, cy + 1, cardW - 2, cardH - 2, RGB_FIX(8, 8, 16));
    drawSmallIcon(index, cx + 2, cy + 4, borderCol);
}

// Oyun adını çizer (merkez kartın altında)
void drawGameLabel(int sel) {
    tft.fillRect(0, 78, 160, 16, COL_BG_DARK);
    
    const char* name = games[sel].label;
    int nameLen = strlen(name);
    int namePixelW = nameLen * 6;
    int nameX = (160 - namePixelW) / 2;
    
    tft.setTextSize(1);
    tft.setTextColor(games[sel].primaryColor, COL_BG_DARK);
    tft.setCursor(nameX, 80);
    tft.print(name);
    
    int lineW = namePixelW + 16;
    int lineX = (160 - lineW) / 2;
    tft.drawFastHLine(lineX, 90, lineW, games[sel].dimColor);
    tft.drawFastHLine(lineX + 4, 90, lineW - 8, games[sel].primaryColor);
}

// Tüm carousel'i çizer
void drawCarousel(bool fullRedraw) {
    if (fullRedraw) {
        drawCarouselFrame();
        menuDrawn = true;
    }
    
    drawCenterCard(carouselSel);
    drawSideCard(carouselSel, -1);
    drawSideCard(carouselSel, +1);
    drawGameLabel(carouselSel);
    drawPageDots(carouselSel);
}

// ══════════════════════════════════════════════════════════════
//  NABIZ ANİMASYONU — Seçili kartın border'ı hafifçe parlar/söner
// ══════════════════════════════════════════════════════════════

void updatePulse() {
    uint32_t now = millis();
    if (now - animTick < 400) return;
    animTick = now;
    
    pulseState = (pulseState + 1) % 3;
    
    int cx = 54;
    int cy = 18;
    int cardW = 52;
    int cardH = 56;
    
    uint16_t borderCol;
    switch (pulseState) {
        case 0: borderCol = games[carouselSel].primaryColor; break;
        case 1: borderCol = RGB_FIX(
                    ((games[carouselSel].primaryColor >> 11) & 0x1F) * 8,
                    ((games[carouselSel].primaryColor >> 5) & 0x3F) * 4,
                    (games[carouselSel].primaryColor & 0x1F) * 8
                ); break;
        default: borderCol = COL_WHITE; break;
    }
    
    tft.drawRect(cx, cy, cardW, cardH, borderCol);
    tft.drawRect(cx + 1, cy + 1, cardW - 2, cardH - 2, borderCol);
}

// ══════════════════════════════════════════════════════════════
//  BOOT ANİMASYONU — Premium giriş ekranı
// ══════════════════════════════════════════════════════════════

void playBootJingle() {
    playSound(880, 80);
    delay(100);
    playSound(1047, 80);
    delay(100);
    playSound(1319, 80);
    delay(100);
    playSound(1568, 150);
    delay(170);
    noTone(BUZZER);
}

void drawBootAnimation() {
    tft.fillScreen(COL_BLACK);
    
    tft.fillRect(0, 0, 160, 64, RGB_FIX(6, 6, 18));
    tft.fillRect(0, 64, 160, 64, RGB_FIX(3, 3, 10));
    
    tft.setTextSize(2);
    const char* logoText = "E-OS";
    int logoX = 40; // Tam ortaya hizalandı
    int logoY = 30;
    
    for (int i = 0; i < 4; i++) {
        tft.setTextColor(COL_ACCENT_DIM);
        tft.setCursor(logoX + i * 20, logoY);
        tft.print(logoText[i]);
        delay(40);
        
        // Beyaz renk vurgusuyla E-OS yazdır
        tft.setTextColor(COL_WHITE);
        tft.setCursor(logoX + i * 20, logoY);
        tft.print(logoText[i]);
        delay(50);
    }
    
    delay(60);
    
    tft.setTextSize(1);
    tft.setTextColor(COL_GRAY);
    tft.setCursor(70, logoY + 20);
    tft.print("v2.1");
    
    for (int w = 0; w <= 60; w += 4) {
        int lx = 80 - w;
        tft.drawFastHLine(lx, logoY + 30, w * 2, COL_ACCENT_DIM);
        tft.drawFastHLine(80 - w/2, logoY + 30, w, COL_ACCENT);
        delay(8);
    }
    
    int barX = 25;
    int barY = logoY + 40;
    int barW = 110;
    int barH = 8;
    
    tft.drawRoundRect(barX, barY, barW, barH, 3, COL_ACCENT_DIM);
    
    int segments = 4;
    int segW = (barW - 4) / segments;
    
    for (int s = 0; s < segments; s++) {
        int notes[] = {880, 1047, 1319, 1568};
        int durs[]  = {80, 80, 80, 150};
        playSound(notes[s], durs[s]);
        
        for (int px = 0; px < segW; px++) {
            int fillX = barX + 2 + s * segW + px;
            uint8_t bright = 120 + (px * 135 / segW);
            tft.drawFastVLine(fillX, barY + 2, barH - 4, RGB_FIX(30, bright, 255));
            delay(3);
        }
        delay(50);
    }
    noTone(BUZZER);
    
    delay(150);
    
    tft.fillRect(0, 0, 160, 128, RGB_FIX(4, 4, 12));
    delay(60);
    tft.fillRect(0, 0, 160, 128, RGB_FIX(2, 2, 6));
    delay(60);
    tft.fillScreen(COL_BLACK);
    delay(40);
}

// ══════════════════════════════════════════════════════════════
//  OYUN BAŞLATMA ANİMASYONU
// ══════════════════════════════════════════════════════════════

void drawLaunchAnimation(int sel, bool fastBoot) {
    uint16_t gameCol = games[sel].primaryColor;
    
    playSound(1200, 60);
    delay(80);
    playSound(1600, 60);
    delay(80);
    noTone(BUZZER);
    
    int cx = 54, cy = 18, cw = 52, ch = 56;
    
    tft.fillRect(cx - 10, cy - 5, cw + 20, ch + 10, gameCol);
    delay(40);
    tft.fillRect(cx - 30, cy - 15, cw + 60, ch + 30, gameCol);
    delay(40);
    tft.fillRect(0, cy - 30, 160, ch + 60, gameCol);
    delay(40);
    tft.fillScreen(gameCol);
    delay(60);
    
    tft.fillScreen(RGB_FIX(5, 5, 12));
    
    tft.setTextSize(2);
    int nameLen = strlen(games[sel].label);
    int nameX = (160 - nameLen * 12) / 2;
    tft.setTextColor(gameCol);
    tft.setCursor(nameX, 25);
    tft.print(games[sel].label);
    
    tft.drawFastHLine(20, 48, 120, gameCol);
    
    if (fastBoot) {
        tft.setTextSize(1);
        tft.setTextColor(COL_SUCCESS);
        tft.setCursor(18, 60);
        tft.print(">> HIZLI BASLAT! <<");
        
        tft.setTextColor(COL_GRAY);
        tft.setCursor(20, 78);
        tft.print("Oyun zaten yuklu...");
        
        tft.drawRect(25, 95, 110, 8, COL_SUCCESS);
        for (int p = 0; p < 106; p += 4) {
            tft.fillRect(27, 97, p, 4, COL_SUCCESS);
            delay(5);
        }
        
        playSound(1500, 100);
        delay(200);
        noTone(BUZZER);
    } else {
        tft.setTextSize(1);
        tft.setTextColor(COL_WHITE);
        tft.setCursor(12, 60);
        tft.print("Oyun hazirlaniyor...");
        
        tft.setTextColor(COL_GRAY);
        tft.setCursor(16, 78);
        tft.print("Flash yazilacak");
        
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
void updateOLED(int sel) {
    if (!oledReady) return;
    
    // OLED'i tamamen kapatiyoruz (TFT'yi kopya etmesin, guc tasarrufu)
    oled.setPowerSave(1);
}

// ══════════════════════════════════════════════════════════════
// --- V2.1 EKLENTİSİ --- AYARLAR MENÜSÜ (TFT POPUP)
// ══════════════════════════════════════════════════════════════

// Ayarlar popup'ı — Ses açma/kapama, NVS kayıt
void openSettingsMenu() {
    playSound(1000, 40);
    delay(60);
    
    bool localSound = soundEnabled;
    bool settingsOpen = true;
    
    while (settingsOpen) {
        // Popup arka planı
        tft.fillRect(10, 20, 140, 88, RGB_FIX(12, 12, 30));
        tft.drawRect(10, 20, 140, 88, games[SETTINGS_INDEX].primaryColor);
        tft.drawRect(11, 21, 138, 86, RGB_FIX(80, 80, 80));
        
        // Başlık
        tft.setTextSize(1);
        tft.setTextColor(COL_WHITE);
        tft.setCursor(46, 28);
        tft.print("AYARLAR");
        tft.drawFastHLine(20, 38, 120, RGB_FIX(80, 80, 80));
        
        // Ses durumu satırı — vurgulu gösterim
        tft.fillRect(18, 44, 124, 18, RGB_FIX(25, 25, 50)); // Seçili satır arka planı
        tft.drawRect(18, 44, 124, 18, COL_ACCENT);
        
        tft.setTextColor(COL_GRAY);
        tft.setCursor(24, 49);
        tft.print("Ses:");
        
        if (localSound) {
            tft.setTextColor(COL_SUCCESS);
            tft.setCursor(60, 49);
            tft.print("< ACIK  >");
        } else {
            tft.setTextColor(COL_DANGER);
            tft.setCursor(60, 49);
            tft.print("< KAPALI>");
        }
        
        // Kaydet bilgisi
        tft.setTextColor(COL_GRAY);
        tft.setCursor(16, 72);
        tft.print("<> Degistir  A:Kaydet");
        
        tft.setTextColor(COL_DARK_GRAY);
        tft.setCursor(30, 86);
        tft.print("B: Iptal");
        
        // Versiyon bilgisi
        tft.setTextColor(COL_WHITE);
        tft.setTextSize(1);
        tft.setCursor(45, 110);
        tft.print("E-OS v2.1");
        
        // Kullanıcı girişi bekleme döngüsü
        bool inputReceived = false;
        while (!inputReceived) {
            // Joystick X ile ses aç/kapat
            int jx = analogRead(JOY_X) - joyCenterX;
            if (abs(jx) > 500) {
                localSound = !localSound;
                playSound(600, 20); // Sadece ses açıkken duyulur
                inputReceived = true;
                delay(200); // Debounce
            }
            
            // BTN_A ile kaydet ve çık
            if (!digitalRead(BTN_A)) {
                delay(50);
                if (!digitalRead(BTN_A)) {
                    soundEnabled = localSound;
                    // NVS'e kaydet
                    Preferences prefs;
                    prefs.begin("os", false);
                    prefs.putBool("sound_en", soundEnabled);
                    prefs.end();
                    
                    playSound(1200, 50);
                    delay(60);
                    playSound(1500, 80);
                    delay(100);
                    noTone(BUZZER);
                    
                    settingsOpen = false;
                    inputReceived = true;
                }
            }
            
            // BTN_B ile iptal et ve çık
            if (!digitalRead(BTN_B)) {
                delay(50);
                if (!digitalRead(BTN_B)) {
                    playSound(400, 40);
                    delay(80);
                    noTone(BUZZER);
                    settingsOpen = false;
                    inputReceived = true;
                }
            }
            
            delay(30);
        }
    }
    
    // Menüyü yeniden çiz
    drawCarousel(true);
}

// ══════════════════════════════════════════════════════════════
//  SETUP — Başlatma ve Boot Akışı
// ══════════════════════════════════════════════════════════════

void setup() {
    Serial.begin(115200); 
    
    // Boot döngüsü koruması: bootMode'u hemen oku ve sıfırla
    bool enterBootloader = (bootModeMagic == 0xDEADBEEF);
    char flashTarget[32];
    if (enterBootloader) {
        strcpy(flashTarget, bootFilename);
    }
    bootModeMagic = 0;
    
    // Buton ve buzzer pinleri
    pinMode(BTN_A, INPUT_PULLUP); pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP); pinMode(BTN_D, INPUT_PULLUP);
    pinMode(BUZZER, OUTPUT);
    
    // SPI CS pinleri — yüksek = deaktif
    pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH); 
    pinMode(SD_CS, OUTPUT);  digitalWrite(SD_CS, HIGH);  
    
    // SPI hattını başlat
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1); delay(50);
    bool sdReady = SD.begin(SD_CS, SPI, 10000000); // 10 MHz güvenli hız
    
    // SD kart yoksa hata ekranı göster ve dur
    if (!sdReady) {
        tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
        tft.setTextColor(RGB_FIX(255,80,0)); tft.setTextSize(1);
        tft.setCursor(10, 55); tft.print("SD KART YOK!");
        tft.setCursor(10, 70); tft.print("Karti takip resetleyin.");
        while(1) delay(100);
    }
    
    // Bootloader modunda mıyız? (RTC magic kontrolü)
    if (enterBootloader && sdReady) {
        runEarlyBootloader(flashTarget);
    }
    
    // TFT başlatma
    tft.init(); tft.setRotation(1); tft.setSwapBytes(true); tft.fillScreen(TFT_BLACK);
    pinMode(JOY_SW, INPUT_PULLUP);
    
    // Renk paletini ve oyun listesini başlat (TFT hazır olmalı!)
    initColorsAndGames();
    
    // --- V2.1 EKLENTİSİ --- NVS'ten ses ayarını oku
    {
        Preferences prefs;
        prefs.begin("os", true);
        soundEnabled = prefs.getBool("sound_en", true); // Varsayılan: açık
        prefs.end();
    }
    
    // Joystick merkez kalibrasyonu
    tft.setTextColor(COL_ACCENT); tft.setTextSize(1);
    tft.setCursor(30, 58); tft.print("Kalibrasyon...");
    long sumX = 0, sumY = 0;
    for (int i = 0; i < 10; i++) { sumX += analogRead(JOY_X); sumY += analogRead(JOY_Y); delay(2); }
    joyCenterX = sumX / 10; joyCenterY = sumY / 10;
    
    // ═══ PREMİUM BOOT ANİMASYONU ═══
    drawBootAnimation();
    
    // OLED başlatma
    oled.begin();
    oledReady = true;
    
    // İlk carousel çizimi
    drawCarousel(true);
    lastOledUpdate = 0;
    updateOLED(carouselSel);
    
    // Animasyon zamanlayıcısını başlat
    animTick = millis();
}

// ══════════════════════════════════════════════════════════════
//  LOOP — Carousel Navigasyonu ve Oyun Başlatma
// ══════════════════════════════════════════════════════════════

void loop() {
    uint32_t current_ms = millis();
    
    // Joystick X ekseni okuma (yatay carousel navigasyonu)
    int jx_m = analogRead(JOY_X) - joyCenterX;
    
    static uint32_t lastMove = 0;
    bool selChanged = false;
    
    // ─── Yatay navigasyon (Joystick X) ───
    if (abs(jx_m) > 500 && (current_ms - lastMove > 220)) {
        if (jx_m > 0) {
            carouselSel++;
            if (carouselSel >= GAME_COUNT) carouselSel = 0;
        } else {
            carouselSel--;
            if (carouselSel < 0) carouselSel = GAME_COUNT - 1;
        }
        lastMove = current_ms;
        selChanged = true;
        
        playSound(800, 25);
    }
    
    // Sadece X ekseni ile gezilecek, Y eksenini menude iptal ettik
    
    // ─── İlk çizim kontrolü ───
    if (!menuDrawn) {
        drawCarousel(true);
        lastOledUpdate = 0;
    }
    
    // ─── Seçim değiştiyse sadece değişen bölgeleri yeniden çiz ───
    if (selChanged) {
        drawCarousel(false);
        lastOledUpdate = 0;
        prevSel = carouselSel;
    }
    
    // ─── Nabız animasyonu (idle durumda) ───
    if (!selChanged) {
        updatePulse();
    }
    
    // ─── OLED güç tasarrufu ───
    updateOLED(carouselSel);
    
    // ═══ BTN_A: Oyun Başlat / Ayarlar Aç ═══
    if (!digitalRead(BTN_A)) {
        delay(50); // Debounce
        if (!digitalRead(BTN_A)) {
            
            // --- V2.1 EKLENTİSİ --- AYARLAR seçiliyse flashlama yapma!
            if (carouselSel == SETTINGS_INDEX) {
                openSettingsMenu();
                // openSettingsMenu() içinde carousel yeniden çiziliyor
            } else {
                // Normal oyun başlatma rutini
                const char* targetFilename = games[carouselSel].filename;
                
                // NVS Cache kontrolü — aynı oyun zaten yüklü mü?
                Preferences prefs;
                prefs.begin("os", true); 
                char lastGame[32] = "";
                prefs.getString("last_game", lastGame, sizeof(lastGame));
                prefs.end();
                
                bool isCached = (strcmp(lastGame, targetFilename) == 0);
                
                // Başlatma animasyonu göster
                drawLaunchAnimation(carouselSel, isCached);
                
                // OLED'de de bilgi göster
                if (oledReady) {
                    oled.setPowerSave(0); // Uyku modundan cikar
                    oled.clearBuffer();
                    oled.setFont(u8g2_font_7x14B_tr);
                    oled.drawStr(10, 20, games[carouselSel].label);
                    oled.setFont(u8g2_font_6x10_tr);
                    if (isCached) {
                        oled.drawStr(0, 40, "Hizli Baslatma!");
                    } else {
                        oled.drawStr(0, 40, "Flash yaziliyor...");
                    }
                    oled.sendBuffer();
                }
                
                if (isCached) {
                    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
                    if (target) {
                        esp_ota_set_boot_partition(target);
                    }
                    ESP.restart();
                } else {
                    bootModeMagic = 0xDEADBEEF;
                    strcpy(bootFilename, targetFilename);
                    ESP.restart();
                }
            }
        }
    }
    
    // ═══ BTN_B: Bilgi Ekranı + Yüksek Skor ═══
    if (!digitalRead(BTN_B)) {
        delay(50);
        if (!digitalRead(BTN_B)) {
            playSound(600, 30);
            
            // Basit bilgi popup'ı
            tft.fillRect(10, 18, 140, 92, RGB_FIX(15, 15, 35));
            tft.drawRect(10, 18, 140, 92, games[carouselSel].primaryColor);
            tft.drawRect(11, 19, 138, 90, games[carouselSel].dimColor);
            
            tft.setTextSize(1);
            tft.setTextColor(games[carouselSel].primaryColor);
            tft.setCursor(16, 24);
            tft.print(games[carouselSel].label);
            
            tft.drawFastHLine(16, 34, 120, games[carouselSel].dimColor);
            
            tft.setTextColor(COL_GRAY);
            tft.setCursor(16, 40);
            tft.print("Dosya:");
            tft.setTextColor(COL_WHITE);
            tft.setCursor(16, 50);
            tft.print(games[carouselSel].filename);
            
            // NVS durumunu göster
            Preferences prefs;
            prefs.begin("os", true);
            char lastGame[32] = "";
            prefs.getString("last_game", lastGame, sizeof(lastGame));
            
            tft.setCursor(16, 64);
            
            // --- V2.2: Ayarlar sayfası bilgisi güncellemesi ---
            if (carouselSel == SETTINGS_INDEX) {
                tft.setTextColor(COL_SUCCESS);
                tft.print("Durum: Sistem Uyg.");
            } else {
                if (strcmp(lastGame, games[carouselSel].filename) == 0) {
                    tft.setTextColor(COL_SUCCESS);
                    tft.print("Durum: Yuklu");
                } else {
                    tft.setTextColor(RGB_FIX(255, 200, 60));
                    tft.print("Durum: Flash gerekli");
                }
            }
            
            // --- V2.1 EKLENTİSİ --- Yüksek Skor gösterimi
            tft.setCursor(16, 78);
            if (carouselSel != SETTINGS_INDEX && strlen(games[carouselSel].hsKey) > 0) {
                int32_t highScore = prefs.getInt(games[carouselSel].hsKey, 0);
                tft.setTextColor(RGB_FIX(255, 220, 80));
                char hsLine[28];
                sprintf(hsLine, "En Yuksek Skor: %d", (int)highScore);
                tft.print(hsLine);
            } else {
                tft.setTextColor(COL_DARK_GRAY);
                tft.print("Skor: -");
            }
            
            prefs.end();
            
            tft.setTextColor(COL_DARK_GRAY);
            tft.setCursor(32, 100);
            tft.print("[B] ile kapat");
            
            // B bırakılana kadar bekle
            while (!digitalRead(BTN_B)) delay(30);
            // B tekrar basılana kadar bekle
            while (digitalRead(BTN_B)) delay(30);
            delay(100);
            
            // Menüyü yeniden çiz
            drawCarousel(true);
        }
    }
    
    delay(20); // Loop hızı (~50 FPS hedef)
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
