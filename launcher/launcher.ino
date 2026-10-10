#pragma GCC optimize ("O3")
#pragma GCC optimize ("unroll-loops")
/*
 * ══════════════════════════════════════════════════════════════
 *  E-OS V3.0 — Premium Konsol Launcher
 *  PSP XMB / Nintendo Switch İlhamlı Yatay Carousel Tasarım
 * ══════════════════════════════════════════════════════════════
 *  MCU:    ESP32-S3 (Dual Core, 240 MHz)
 *  TFT:    160x128, ST7735, SPI (TFT_eSPI)
 *  OLED:   128x64, SH1106, I2C (U8g2)
 *  Flash:  16 MB  |  PSRAM: 8 MB (OPI)
 * ══════════════════════════════════════════════════════════════
 *  İçerik: 13 oyun + SISTEM satırı (AYARLAR, FLIGHT) = 15 öğe
 *
 *  Sürüm geçmişi:
 *    V2.1 - AYARLAR sistemi, yüksek skor, playSound() sarmalayıcı
 *    V3.0 - 2 boyutlu kategori/satır mimarisi (OYUNLAR + SISTEM),
 *           zaman bazlı (millis) cubic ease-out carousel animasyonları
 * ══════════════════════════════════════════════════════════════
 */

// ============================================================
//  KÜTÜPHANE BAĞLAMALARI — Her birinin rolü aşağıda açıklanmıştır
// ============================================================
#include "icons_data.h"
#include <TFT_eSPI.h>        // 160x128 ST7735 TFT ekran sürücüsü (SPI)
#include <SPI.h>             // Donanımsal SPI hattı (TFT + SD kart ortak veriyolu)
#include <SD.h>              // SD kart dosya sistemi — .bin oyun dosyalarını okur
#include <Wire.h>            // I2C hattı — OLED (SH1106) iletişimi için
#include <U8g2lib.h>         // OLED grafik kütüphanesi (bootloader ilerleme çubuğu)
#include <Update.h>          // OTA firmware yazma API'si — flash'a .bin aktarır
#include <Preferences.h>     // NVS (Non-Volatile Storage) — ses ayarı, son oyun, skorlar
#include <esp_ota_ops.h>     // ESP32 partition yönetimi — hızlı başlatma için boot partition seçer
#include "../hardware_config.h"  // Pin tanımları, butonlar, joystick, SPI pinleri (ortak yapılandırma)
#include "Input.h"
#include "../GameBase.h"         // NOTE_* müzik notaları + osPlaySound ortak API
#include "../dev_tools.h"        // USB Screenshot ve Dev Tools

// TFT ekran nesnesi — tüm 2D çizimler bu nesne üzerinden yapılır
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite fb = TFT_eSprite(&tft); // Sanal Tuval (FrameBuffer) V3.0
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

// --- V3.4 EKLENTISI --- 15 oyun + AYARLAR + FLIGHT + TOOLS + DRAW = 19
// Toplam carousel eleman sayisi (oyun + sistem araclari dahil)
#define GAME_COUNT 19
#define SETTINGS_INDEX 15  // AYARLAR'in carousel indeksi
#define FLIGHT_INDEX 16    // FLIGHT'in carousel indeksi
#define TOOLS_INDEX 17     // --- V3.1 --- TOOLS (Kronometre/Geri Sayim/Metronom) — launcher-ici, flash yok
#define DRAW_INDEX 18      // --- V3.2 --- DRAW (Piksel cizim, SD'ye BMP) — launcher-ici, flash yok
// Not: RGB_FIX() tft başlatılmadan çağrılamaz, renkler setup sonrası atanacak
// Bu yüzden dizi global tanımlanır ama içerik initColorsAndGames() içinde doldurulur
GameInfo games[GAME_COUNT];

// --- V3.0 EKLENTISI --- Kategori ve Satir Mimarisi
#define ROW_COUNT 2
struct Category {
    const char* label;     // "OYUNLAR", "SISTEM"
    uint8_t startIndex;    // 0 veya 10
    uint8_t count;         // 10 veya 3
    uint8_t cursor;        // Satir ici secim
};

Category rows[ROW_COUNT] = {
    { "GAMES", 0,  15, 0 },   // games[0..14]
    { "APPS",  15, 4,  0 }    // games[15..18] (SETTINGS, FLIGHT, TOOLS, DRAW)
};

// ─── Global State Değişkenleri ───
// Bu blok carousel menünün tüm durumunu (state) tutar
// V3.0: 2 Boyutlu State
uint8_t activeRow = 0;       // Secili satir (0 = Oyunlar, 1 = Sistem)
int16_t scrollY = 0;         // Mevcut dikey kaydirma (piksel)

// --- FPS Sayaci Icin ---
unsigned long lastFpsTime = 0;
int frameCount = 0;
int currentFps = 0;
int16_t scrollYTarget = 0;   // Hedef dikey kaydirma (piksel)
bool rowAnimating = false;   // Gecis animasyonu calisiyor mu?
int16_t scrollYStart = 0;    // Animasyon baslangic scrollY degeri
uint32_t rowStartTime = 0;   // Animasyon baslangic zamani
int16_t scrollX = 0;         // Yatay kaydirma animasyonu (PS5 tarzi)
int16_t scrollXTarget = 0;   // Hedef yatay kaydirma
bool slideAnimating = false; // Yatay animasyon aktif mi?
int slideDirection = 0;      // -1=sol, +1=sag (cursor yonu)
int slideDistance = 0;       // Animasyon mesafesi (57px simetrik, merkez-merkeze)
int slideStartX = 0;         // Animasyon baslangic scrollX degeri
uint32_t slideStartTime = 0; // Animasyon baslangic zamani

int carouselSel = 0;         // (GERIYE DONUK UYUMLULUK) Secili oyun indexi (0-12)
bool oledReady = false;      // OLED başlatıldı mı?
bool sdReady = false;
bool framebufferReady = false;
LauncherInput menuInput;

// All intervals below are shorter than INT32_MAX milliseconds.
bool deadlineReached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

uint32_t deadlineRemaining(uint32_t now, uint32_t deadline) {
    return deadlineReached(now, deadline) ? 0 : deadline - now;
}

// --- V2.1 EKLENTİSİ --- Ses açık/kapalı durumu (global, NVS'ten okunur)
bool soundEnabled = true;
bool showFpsEnabled = false; // --- V2.4 EKLENTİSİ --- FPS Göstergesi

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
    if (soundEnabled) {
        static uint32_t lastToneTime = 0;
        static uint32_t lastDur = 0;
        if (millis() - lastToneTime >= lastDur) {
            osBuzzerPlay(freq, dur);   // LEDC + volume (oto-durdurma)
            lastToneTime = millis();
            lastDur = dur;
        }
    }
}

// delay() YASAK (oyun döngüsünde) — millis()+yield() tabanlı bekleme.
// Modal/terminal akışlarda (ayarlar popup, açılış animasyonu) delay() yerine kullanılır.
static inline void yieldWait(uint32_t ms) {
    uint32_t t0 = millis();
    while (millis() - t0 < ms) yield();
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
    COL_BG_DARK    = RGB_FIX(10, 10, 26);   // Tek parça pürüzsüz koyu lacivert (Eski tona daha yakın)
    COL_BG_STATUS  = RGB_FIX(14, 14, 30);   // Status bar için uyumlu açık lacivert
    COL_ACCENT     = RGB_FIX(255, 255, 255); // Saf beyaz — premium monokrom vurgu (PS5/Apple TV)
    COL_ACCENT_DIM = RGB_FIX(90, 90, 90);    // Sönük gri — kenar gradient'leri
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
    games[6].hsKey = "hs_mode7";

    // 3D WIRE
    games[7].label = "3D WIRE";
    games[7].filename = "/wire3d.bin";
    games[7].primaryColor = RGB_FIX(0, 255, 0);
    games[7].dimColor = RGB_FIX(0, 80, 0);
    games[7].hsKey = "hs_wireframe3d";

    // STRIKE
    games[8].label = "STRIKE";
    games[8].filename = "/strike.bin";
    games[8].primaryColor = RGB_FIX(100, 200, 255);
    games[8].dimColor = RGB_FIX(40, 80, 100);
    games[8].hsKey = "hs_strike";

    // PLATFORMER
    games[9].label = "PLATFORMER";
    games[9].filename = "/platform.bin";
    games[9].primaryColor = RGB_FIX(70, 200, 90); // Acik yesil cim rengi
    games[9].dimColor = RGB_FIX(45, 140, 55);
    games[9].hsKey = "hs_platformer";

    // TETRIS
    games[10].label = "TETRIS";
    games[10].filename = "/tetris.bin";
    games[10].primaryColor = RGB_FIX(0, 200, 200);
    games[10].dimColor = RGB_FIX(0, 80, 80);
    games[10].hsKey = "hs_tetris";

    // DUNGEON
    games[11].label = "DUNGEON";
    games[11].filename = "/dungeon.bin";
    games[11].primaryColor = RGB_FIX(150, 50, 150);
    games[11].dimColor = RGB_FIX(50, 20, 50);
    games[11].hsKey = "hs_dungeon";

    // TOWER DEFENSE
    games[12].label = "TOWER DEF";
    games[12].filename = "/towerdef.bin";
    games[12].primaryColor = RGB_FIX(100, 255, 100);
    games[12].dimColor = RGB_FIX(40, 100, 40);
    games[12].hsKey = "hs_towerdef";

    // --- V3.3 EKLENTISI --- 2048 (kayan karo bulmacasi)
    games[13].label = "2048";
    games[13].filename = "/2048.bin";
    games[13].primaryColor = RGB_FIX(237, 194, 46);  // Altin sarisi - 2048 karosu
    games[13].dimColor = RGB_FIX(85, 70, 18);
    games[13].hsKey = "hs_2048";

    // --- V3.4 EKLENTISI --- RHYTHM (4 seritli ritim oyunu)
    games[14].label = "RHYTHM";
    games[14].filename = "/rhythm.bin";
    games[14].primaryColor = RGB_FIX(255, 60, 100);  // Neon pembe - arcade ritim temasi
    games[14].dimColor = RGB_FIX(85, 20, 35);
    games[14].hsKey = "hs_rhythm";

    // --- V2.1 EKLENTİSİ --- AYARLAR (Sistem uygulaması — flashlama yok!)
    games[15].label = "SETTINGS";
    games[15].filename = "/settings.bin";
    games[15].primaryColor = RGB_FIX(150, 150, 150);
    games[15].dimColor = RGB_FIX(50, 50, 50);
    games[15].hsKey = "";  // Ayarlar için skor yok

    // --- V2.4 EKLENTISI --- FLIGHT (Ucus takip sistemi)
    games[16].label = "FLIGHT";
    games[16].filename = "/flight.bin";
    games[16].primaryColor = RGB_FIX(255, 140, 0);   // Turuncu - Havacilik/Radar temasi
    games[16].dimColor = RGB_FIX(100, 50, 0);
    games[16].hsKey = "";  // Flight icin skor yok

    // --- V3.1 EKLENTISI --- TOOLS (Kronometre/Geri Sayim/Metronom — launcher-ici popup, flash yok)
    games[17].label = "TOOLS";
    games[17].filename = "/tools.bin";               // kullanilmaz (TOOLS_INDEX ozel islenir), sadece placeholder
    games[17].primaryColor = RGB_FIX(0, 210, 180);   // Teal - arac/zaman temasi
    games[17].dimColor = RGB_FIX(0, 75, 65);
    games[17].hsKey = "";  // Tools icin skor yok

    // --- V3.2 EKLENTISI --- DRAW (Piksel cizim, SD'ye BMP — launcher-ici, flash yok)
    games[18].label = "DRAW";
    games[18].filename = "/draw.bin";                // kullanilmaz (DRAW_INDEX ozel islenir), sadece placeholder
    games[18].primaryColor = RGB_FIX(210, 90, 235);  // Mor/magenta - sanat temasi
    games[18].dimColor = RGB_FIX(75, 30, 90);
    games[18].hsKey = "";  // Draw icin skor yok
}

// ══════════════════════════════════════════════════════════════
//  ERKEN BOOTLOADER — FLASH YAZMA
// ══════════════════════════════════════════════════════════════

// SD karttan hedef .bin dosyasını OTA ile flash'a yazan bootloader
//
// Akış:
//   1. OLED'de "BOOTLOADER" mesajı göster
//   2. SD karttan .bin dosyasını aç
//   3. Header, dosya boyutu ve hedef OTA partition kontrolü
//   4. Update.begin() ile flash yazma modunu başlat
//   5. 16KB bloklarla dosyanın tamamını SD → flash aktar
//   6. Her saniye OLED'de ilerleme yüzdesi ve çubuk göster
//   7. Başarı: NVS'e kaydet + ESP.restart() → yeni oyuna geç
//      Hata:   hata mesajından sonra launcher menüsüne dön
//
// Parametre: filename — SD karttaki .bin dosya yolu (örn: "/snake.bin")
// Return: yok — başarı durumunda ESP.restart() ile çıkılır
// Cache identity includes the actual OTA image, not just its filename.
const esp_partition_t* gamePartition() {
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                   ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
}

bool clearGameCache() {
    Preferences prefs;
    if (!prefs.begin("os", false)) return false;
    bool ok = !prefs.isKey("last_game") || prefs.remove("last_game");
    prefs.remove("game_sha");
    prefs.remove("game_part");
    prefs.end();
    return ok;
}

bool cachedGameMatches(const char* filename) {
    const esp_partition_t* part = gamePartition();
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!part || !running || part->address == running->address) return false;
    Preferences prefs;
    if (!prefs.begin("os", true)) return false;
    uint8_t savedHash[32];
    bool matches = prefs.getString("last_game", "") == String(filename)
                && prefs.getUInt("game_part", 0) == part->address
                && prefs.getBytesLength("game_sha") == sizeof(savedHash)
                && prefs.getBytes("game_sha", savedHash, sizeof(savedHash)) == sizeof(savedHash);
    prefs.end();
    if (!matches) return false;
    uint8_t imageHash[32];
    return esp_partition_get_sha256(part, imageHash) == ESP_OK
        && memcmp(savedHash, imageHash, sizeof(imageHash)) == 0;
}

bool saveGameCache(const char* filename) {
    const esp_partition_t* part = gamePartition();
    uint8_t imageHash[32];
    if (!part || esp_partition_get_sha256(part, imageHash) != ESP_OK) return false;
    Preferences prefs;
    if (!prefs.begin("os", false)) return false;
    // Publish the filename last. A reset or failed write leaves no usable cache.
    bool ok = prefs.putBytes("game_sha", imageHash, sizeof(imageHash)) == sizeof(imageHash)
           && prefs.putUInt("game_part", part->address) == sizeof(uint32_t)
           && prefs.putString("last_game", filename) == strlen(filename);
    if (!ok) prefs.remove("last_game");
    prefs.end();
    return ok;
}

void bootError(const char* message) {
    oled.clearBuffer();
    oled.drawStr(2, 24, message);
    oled.drawStr(2, 44, "Returning to menu...");
    oled.sendBuffer();
    delay(1500);
}

void runEarlyBootloader(const char* filename) {
    oled.begin();
    oled.setPowerSave(0);
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(19, 24, "Loading game...");
    oled.sendBuffer();

    // OTA_0 contains the launcher; never write it from an unexpected slot.
    const esp_partition_t* target = gamePartition();
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    if (!target || !running || !next
        || running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0
        || next->address != target->address) {
        bootError("PARTITION ERROR!");
        return;
    }
    File binFile = SD.open(filename, FILE_READ);
    if (!binFile || binFile.isDirectory()) {
        binFile.close();
        bootError("ERROR: NO FILE!");
        return;
    }
    size_t fileSize = binFile.size();
    uint8_t headerCheck[24];
    if (fileSize < sizeof(headerCheck) || fileSize > target->size
        || binFile.read(headerCheck, sizeof(headerCheck)) != sizeof(headerCheck)
        || headerCheck[0] != 0xE9 || !binFile.seek(0)) {
        binFile.close();
        bootError("ERROR: INVALID BIN!");
        return;
    }
    // Invalidate before any erase/write, even if power is lost mid-transfer.
    if (!clearGameCache()) {
        binFile.close();
        bootError("NVS WRITE ERROR!");
        return;
    }
    if (!Update.begin(fileSize)) {
        binFile.close();
        bootError("FLASH BEGIN ERROR!");
        return;
    }
    static uint8_t buf[16384];
    size_t written = 0;
    bool flashError = false;
    uint32_t lastDraw = 0;
    while (written < fileSize) {
        size_t toRead = min(sizeof(buf), fileSize - written);
        int bytesRead = binFile.read(buf, toRead);
        if (bytesRead <= 0 || (size_t)bytesRead > toRead
            || Update.write(buf, bytesRead) != (size_t)bytesRead) {
            flashError = true;
            break;
        }
        written += bytesRead;
        uint32_t now = millis();
        if (now - lastDraw > 1000 || written == fileSize) {
            lastDraw = now;
            int pct = (written * 100) / fileSize;
            char progStr[24];
            snprintf(progStr, sizeof(progStr), "%d%%  %dK/%dK", pct,
                     (int)(written / 1024), (int)(fileSize / 1024));
            oled.clearBuffer();
            oled.drawStr(31, 12, "FLASHING...");
            oled.drawStr(19, 28, progStr);
            oled.drawFrame(0, 40, 128, 14);
            oled.drawBox(2, 42, 124 * pct / 100, 10);
            oled.sendBuffer();
        }
        yield();
    }
    binFile.close();
    if (flashError || written != fileSize) {
        Update.abort();
        bootError("FLASH READ/WRITE ERR!");
        return;
    }
    // Exact length required. end(true) would accept a premature SD EOF.
    if (!Update.end()) {
        Update.abort();
        bootError("VERIFY ERROR!");
        return;
    }
    if (!saveGameCache(filename)) Serial.println("Game loaded; fast-boot cache unavailable.");
    oled.clearBuffer();
    oled.drawStr(19, 20, "UPLOAD SUCCESS!");
    oled.drawStr(16, 40, "Starting game...");
    oled.sendBuffer();
    if (soundEnabled) osBuzzerPlay(NOTE_E5, 50);
    delay(250);
    oled.clearBuffer();
    oled.sendBuffer();
    osBuzzerOff();
    ESP.restart();
}

// ══════════════════════════════════════════════════════════════
//  YARDIMCI ÇİZİM FONKSİYONLARI
// ══════════════════════════════════════════════════════════════

// Üst durum çubuğu çizimi — E-OS başlığı, SD ikonu ve accent çizgi
// 14px yüksekliğinde, ekranın en üstünde sabit duran bilgi şeridi
// İçerik: play üçgeni + "E-OS" yazısı, ses ikonu, pil ikonu, alt accent çizgi
// Parametre: yok (hepsi global/sabit değerlerle çizilir)
// Return: yok
void drawStatusBar() {
    // Status bar arka planı — 14px yüksek, 160px geniş (tam ekran genişliği)
    fb.fillRect(0, 0, 160, 14, COL_BG_STATUS);

    // Sol: ► üçgen + kategori adı (E-OS yazısı kaldırıldı, PS5/Apple TV tarzı)
    // Play triangle (4x7px) — Kusursuz piksel dizilimi (manuel cizim)
    fb.drawFastVLine(4, 3, 7, COL_ACCENT);
    fb.drawFastVLine(5, 4, 5, COL_ACCENT);
    fb.drawFastVLine(6, 5, 3, COL_ACCENT);
    fb.drawPixel(7, 6, COL_ACCENT);

    // Kategori adı — üçgenin hemen yanında (OYUNLAR / SISTEM)
    fb.setTextSize(1);
    fb.setTextColor(COL_WHITE, COL_BG_STATUS);
    fb.setCursor(12, 3);
    fb.print(rows[activeRow].label);

    // Pil ikonu (basit çizim) — sağ üst köşede (150,4)
    // Dış çerçeve 7x6 + nub 2px
    fb.drawRect(150, 4, 7, 6, COL_GRAY);
    fb.drawFastVLine(157, 6, 2, COL_GRAY);    // Pil ucu
    fb.fillRect(151, 5, 5, 4, COL_SUCCESS); // Dolu pil — yeşil

    // --- V2.1 EKLENTİSİ --- Ses durumu ikonu (status bar'da)
    // Hoparlör ikonu + ses açık/kapalı durum göstergesi
    int sx = 138; // Hoparlör X pozisyonu — pil ikonunun solunda
    // Hoparlör Gövdesi (daha küçük) — 3 dikey çizgi ile koni etkisi
    fb.fillRect(sx, 6, 2, 2, COL_WHITE);
    fb.drawFastVLine(sx + 2, 5, 4, COL_WHITE);
    fb.drawFastVLine(sx + 3, 4, 6, COL_WHITE);

    if (soundEnabled) {
        // Ses açık — dalga sayısı kademeyi gösterir: LOW = 1 dalga, HIGH = 2 dalga
        fb.drawFastVLine(sx + 5, 5, 4, COL_SUCCESS);
        if (osSoundVolume >= 2) fb.drawFastVLine(sx + 7, 4, 6, COL_SUCCESS);
    } else {
        // Ses kapalı — Küçük X işareti (kırmızı)
        fb.drawLine(sx + 5, 4, sx + 9, 9, COL_DANGER);
        fb.drawLine(sx + 9, 4, sx + 5, 9, COL_DANGER);
    }
    // FPS Sayaci (Tam Ortada)
    if (showFpsEnabled && currentFps >= 0) {
        fb.setTextSize(1);
        fb.setTextColor(RGB_FIX(150, 150, 150), COL_BG_STATUS);
        fb.setCursor(64, 3); // Merkeze yakin
        fb.print(currentFps);
        fb.print(" FPS");
    }


    // Alt ilerleme çizgisi — segmentli scrollbar (Gemini/PS5 tarzı)
    // 160px çizgi toplam oyun sayısına bölünür, aktif segment parlak, diğerleri sönük
    int segCount = rows[activeRow].count;
    int segW = 160 / segCount;
    int startX = (160 - (segCount * segW)) / 2;
    int segCur = rows[activeRow].cursor;
    for (int i = 0; i < segCount; i++) {
        int x = startX + i * segW;
        fb.drawFastHLine(x, 14, segW - 1, COL_ACCENT_DIM);  // 1px boşluk
    }
    // Aktif segmenti parlak yap
    int ax = startX + segCur * segW;
    fb.drawFastHLine(ax, 14, segW - 1, COL_ACCENT);
}

// ══════════════════════════════════════════════════════════════
//  BÜYÜK İKONLAR (48x48 piksel) — Carousel merkez kartı
// ══════════════════════════════════════════════════════════════

// --- V3.1 --- TOOLS ikonu prosedurel cizilir (icons_data.h'de asset yok).
// Stilize kronometre: ust dugme + govde halkasi + saat/dakika ibresi + merkez gobek.
// 64x64 alan icin (buyuk kart)
void drawToolsIconBig(int x, int y) {
    uint16_t c   = games[TOOLS_INDEX].primaryColor;
    uint16_t dim = games[TOOLS_INDEX].dimColor;
    int cx0 = x + 32, cy0 = y + 37;   // kadran merkezi
    // Ust dugme + sap
    fb.fillRect(cx0 - 5, y + 6, 10, 5, c);
    fb.fillRect(cx0 - 2, y + 11, 4, 4, c);
    // Sag baslat dugmesi (kucuk cikinti)
    fb.fillRect(x + 49, y + 14, 7, 4, c);
    // Govde halkasi (2px kalin)
    fb.drawCircle(cx0, cy0, 22, c);
    fb.drawCircle(cx0, cy0, 21, c);
    fb.drawCircle(cx0, cy0, 20, dim);
    // 12/3/6/9 tik isaretleri
    fb.drawFastVLine(cx0, cy0 - 21, 4, c);
    fb.drawFastVLine(cx0, cy0 + 18, 4, c);
    fb.drawFastHLine(cx0 - 21, cy0, 4, c);
    fb.drawFastHLine(cx0 + 18, cy0, 4, c);
    // Ibreler (beyaz) + merkez gobek
    fb.drawLine(cx0, cy0, cx0, cy0 - 15, COL_WHITE);       // dakika (yukari)
    fb.drawLine(cx0, cy0, cx0 + 10, cy0 - 8, COL_WHITE);   // saat (sag-yukari)
    fb.fillCircle(cx0, cy0, 2, c);
}

// 32x32 alan icin (yan kart / kucuk ikon)
void drawToolsIconSmall(int x, int y) {
    uint16_t c   = games[TOOLS_INDEX].primaryColor;
    uint16_t dim = games[TOOLS_INDEX].dimColor;
    int cx0 = x + 16, cy0 = y + 18;
    fb.fillRect(cx0 - 2, y + 2, 5, 3, c);   // ust dugme
    fb.drawCircle(cx0, cy0, 11, c);
    fb.drawCircle(cx0, cy0, 10, dim);
    fb.drawLine(cx0, cy0, cx0, cy0 - 8, COL_WHITE);
    fb.drawLine(cx0, cy0, cx0 + 5, cy0 - 5, COL_WHITE);
    fb.fillCircle(cx0, cy0, 1, c);
}

// --- V3.2 --- DRAW ikonu prosedurel: piksel-kalp motifi + beyaz imlec (pixel-art editoru hissi)
void drawDrawIconBig(int x, int y) {
    uint16_t red = RGB_FIX(255, 70, 70);
    static const uint8_t heart[5][5] = {
        {0,1,0,1,0}, {1,1,1,1,1}, {1,1,1,1,1}, {0,1,1,1,0}, {0,0,1,0,0}
    };
    int cell = 8, gx = x + 12, gy = y + 12;
    fb.fillRect(gx - 2, gy - 2, 5 * cell + 3, 5 * cell + 3, RGB_FIX(12, 12, 22)); // izgara zemin
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 5; c++)
            if (heart[r][c]) fb.fillRect(gx + c * cell, gy + r * cell, cell - 1, cell - 1, red);
    fb.drawRect(gx - 1, gy - 1, cell + 1, cell + 1, COL_WHITE); // sol-ust hucrede imlec
}

void drawDrawIconSmall(int x, int y) {
    uint16_t red = RGB_FIX(255, 70, 70);
    static const uint8_t h[3][3] = { {1,0,1}, {1,1,1}, {0,1,0} };
    int cell = 6, gx = x + 7, gy = y + 8;
    fb.fillRect(gx - 1, gy - 1, 3 * cell + 1, 3 * cell + 1, RGB_FIX(12, 12, 22));
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            if (h[r][c]) fb.fillRect(gx + c * cell, gy + r * cell, cell - 1, cell - 1, red);
}

// --- V3.3 --- 2048 ikonu prosedurel: 2x2 karo izgarasi "2 0 4 8" (klasik 2048 gorunumu)
void draw2048IconBig(int x, int y) {
    const char digits[4] = { '2', '0', '4', '8' };
    uint16_t cols[4] = {
        RGB_FIX(238, 228, 218),   // 2  - bej
        RGB_FIX(237, 194, 46),    // 0  - altin (tema rengi)
        RGB_FIX(245, 149, 99),    // 4  - turuncu
        RGB_FIX(246, 94, 59)      // 8  - kizil turuncu
    };
    uint16_t txts[4] = { RGB_FIX(119, 110, 101), COL_WHITE, COL_WHITE, COL_WHITE };
    int ts = 28, gap = 3;
    int gx = x + (64 - (2 * ts + gap)) / 2;
    int gy = y + (64 - (2 * ts + gap)) / 2;
    fb.setTextSize(2);
    for (int i = 0; i < 4; i++) {
        int tx = gx + (i % 2) * (ts + gap);
        int ty = gy + (i / 2) * (ts + gap);
        fb.fillRoundRect(tx, ty, ts, ts, 4, cols[i]);
        fb.setTextColor(txts[i]);
        fb.setCursor(tx + (ts - 12) / 2 + 1, ty + (ts - 14) / 2);
        fb.print(digits[i]);
    }
}

void draw2048IconSmall(int x, int y) {
    const char digits[4] = { '2', '0', '4', '8' };
    uint16_t cols[4] = {
        RGB_FIX(238, 228, 218), RGB_FIX(237, 194, 46),
        RGB_FIX(245, 149, 99),  RGB_FIX(246, 94, 59)
    };
    uint16_t txts[4] = { RGB_FIX(119, 110, 101), COL_WHITE, COL_WHITE, COL_WHITE };
    int ts = 14, gap = 2;
    int gx = x + (32 - (2 * ts + gap)) / 2;
    int gy = y + (32 - (2 * ts + gap)) / 2;
    fb.setTextSize(1);
    for (int i = 0; i < 4; i++) {
        int tx = gx + (i % 2) * (ts + gap);
        int ty = gy + (i / 2) * (ts + gap);
        fb.fillRoundRect(tx, ty, ts, ts, 3, cols[i]);
        fb.setTextColor(txts[i]);
        fb.setCursor(tx + (ts - 5) / 2, ty + (ts - 7) / 2);
        fb.print(digits[i]);
    }
}

// --- V3.4 --- RHYTHM ikonu prosedurel: 4 serit + dusen neon notalar + vurus cizgisi
void drawRhythmIconBig(int x, int y) {
    uint16_t laneCols[4] = {
        RGB_FIX(255, 60, 80),    // kirmizi-pembe
        RGB_FIX(60, 200, 255),   // cyan
        RGB_FIX(100, 255, 80),   // yesil
        RGB_FIX(255, 200, 40)    // altin
    };
    const int noteY[4] = { 10, 24, 4, 32 };   // farkli yuksekliklerde dusen notalar
    for (int i = 0; i < 4; i++) {
        int lx = x + 8 + i * 16;
        fb.drawFastVLine(lx, y + 2, 50, RGB_FIX(40, 40, 70));
        fb.fillRoundRect(lx - 6, y + noteY[i], 12, 7, 2, laneCols[i]);
    }
    // Vurus cizgisi + serit halkalari
    fb.fillRect(x + 2, y + 54, 60, 3, COL_WHITE);
    for (int i = 0; i < 4; i++) {
        fb.drawCircle(x + 8 + i * 16, y + 55, 4, laneCols[i]);
    }
}

void drawRhythmIconSmall(int x, int y) {
    uint16_t laneCols[4] = {
        RGB_FIX(255, 60, 80), RGB_FIX(60, 200, 255),
        RGB_FIX(100, 255, 80), RGB_FIX(255, 200, 40)
    };
    const int noteY[4] = { 5, 12, 2, 16 };
    for (int i = 0; i < 4; i++) {
        int lx = x + 4 + i * 8;
        fb.drawFastVLine(lx, y + 1, 25, RGB_FIX(40, 40, 70));
        fb.fillRoundRect(lx - 3, y + noteY[i], 7, 4, 1, laneCols[i]);
    }
    fb.fillRect(x + 1, y + 27, 30, 2, COL_WHITE);
}

// DOOM İkonu — Stilize kafatası (48x48)
// Kompozisyon: Yuvarlak kafatası + 2 göz (parlayan kırmızı) + burun + alt dişler + 3 alev
// Parametreler: x,y — ikonun sol-üst köşesi (48x48 alan)
// Return: yok
void drawBigIcon(int index, int x, int y) {
    switch (index) {
        case 0: fb.pushImage(x, y, 64, 64, (uint16_t*)iconDoomBig); break;
        case 1: fb.pushImage(x, y, 64, 64, (uint16_t*)iconSnakeBig); break;
        case 2: fb.pushImage(x, y, 64, 64, (uint16_t*)iconFlappyBig); break;
        case 3: fb.pushImage(x, y, 64, 64, (uint16_t*)iconSpaceBig); break;
        case 4: fb.pushImage(x, y, 64, 64, (uint16_t*)iconArkanoidBig); break;
        case 5: fb.pushImage(x, y, 64, 64, (uint16_t*)iconPacmanBig); break;
        case 6: fb.pushImage(x, y, 64, 64, (uint16_t*)iconMode7Big); break;
        case 7: fb.pushImage(x, y, 64, 64, (uint16_t*)iconWire3DBig); break;
        case 8: fb.pushImage(x, y, 64, 64, (uint16_t*)iconStrikeBig); break;
        case 9: fb.pushImage(x, y, 64, 64, (uint16_t*)iconPlatformBig); break;
        case 10: fb.pushImage(x, y, 64, 64, (uint16_t*)iconTetrisBig); break;
        case 11: fb.pushImage(x, y, 64, 64, (uint16_t*)iconDungeonBig); break;
        case 12: fb.pushImage(x, y, 64, 64, (uint16_t*)iconTowerDefenseBig); break;
        case 13: draw2048IconBig(x, y); break;    // --- V3.3 --- prosedurel 2048 karolari
        case 14: drawRhythmIconBig(x, y); break;  // --- V3.4 --- prosedurel ritim seritleri
        case 15: fb.pushImage(x, y, 64, 64, (uint16_t*)iconSettingsBig); break;
        case 16: fb.pushImage(x, y, 64, 64, (uint16_t*)iconFlightBig); break;
        case 17: drawToolsIconBig(x, y); break;   // --- V3.1 --- prosedurel kronometre (asset yok)
        case 18: drawDrawIconBig(x, y); break;    // --- V3.2 --- prosedurel piksel-kalp
    }
}

void drawSmallIcon(int index, int x, int y) {
    switch (index) {
        case 0: fb.pushImage(x, y, 32, 32, (uint16_t*)iconDoomSmall); break;
        case 1: fb.pushImage(x, y, 32, 32, (uint16_t*)iconSnakeSmall); break;
        case 2: fb.pushImage(x, y, 32, 32, (uint16_t*)iconFlappySmall); break;
        case 3: fb.pushImage(x, y, 32, 32, (uint16_t*)iconSpaceSmall); break;
        case 4: fb.pushImage(x, y, 32, 32, (uint16_t*)iconArkanoidSmall); break;
        case 5: fb.pushImage(x, y, 32, 32, (uint16_t*)iconPacmanSmall); break;
        case 6: fb.pushImage(x, y, 32, 32, (uint16_t*)iconMode7Small); break;
        case 7: fb.pushImage(x, y, 32, 32, (uint16_t*)iconWire3DSmall); break;
        case 8: fb.pushImage(x, y, 32, 32, (uint16_t*)iconStrikeSmall); break;
        case 9: fb.pushImage(x, y, 32, 32, (uint16_t*)iconPlatformSmall); break;
        case 10: fb.pushImage(x, y, 32, 32, (uint16_t*)iconTetrisSmall); break;
        case 11: fb.pushImage(x, y, 32, 32, (uint16_t*)iconDungeonSmall); break;
        case 12: fb.pushImage(x, y, 32, 32, (uint16_t*)iconTowerDefenseSmall); break;
        case 13: draw2048IconSmall(x, y); break;    // --- V3.3 --- prosedurel 2048 karolari
        case 14: drawRhythmIconSmall(x, y); break;  // --- V3.4 --- prosedurel ritim seritleri
        case 15: fb.pushImage(x, y, 32, 32, (uint16_t*)iconSettingsSmall); break;
        case 16: fb.pushImage(x, y, 32, 32, (uint16_t*)iconFlightSmall); break;
        case 17: drawToolsIconSmall(x, y); break;   // --- V3.1 --- prosedurel kronometre
        case 18: drawDrawIconSmall(x, y); break;    // --- V3.2 --- prosedurel piksel-kalp
    }
}


// Seçili oyunun merkez kartını çizer (48x48 ikon + çerçeve)
// Kart, ekranın tam ortasında konumlanır — 3 katmanlı çerçeve (sönük/parlak/iç)
// Parametre: sel — seçili oyun indeksi
// Return: yok
void drawCenterCard(int sel, int offsetY) {
    // Kart konumu — ekranin ortasinda (160px genislik icin)
    int cx = 46 + scrollX;  // Yatay kaydirma animasyonu dahil
    int cy = 16 + offsetY;  // Üst boşluk (status bar'ın altında)
    int cardW = 68;
    int cardH = 68;

    // Seçili oyunun tema rengi — border için
    uint16_t borderCol = games[sel].primaryColor;

    // Çok katmanlı çerçeve — derinlik etkisi
    // 1. Dış temizleme bandı (arka plan rengi) — eski kartı sil
    fb.fillRect(cx - 2, cy - 1, cardW + 4, cardH + 2, COL_BG_DARK);
    // 2. Sönük dış çerçeve (gölge)
    fb.drawRect(cx - 1, cy - 1, cardW + 2, cardH + 2, games[sel].dimColor);
    // 3. Parlak ana çerçeve (oyun rengi)
    fb.drawRect(cx, cy, cardW, cardH, borderCol);
    // 4. İç çerçeve (parlak, daha kalın border efekti)
    fb.drawRect(cx + 1, cy + 1, cardW - 2, cardH - 2, borderCol);
    // 5. İç dolgu — koyu arka plan (ikon buraya çizilir)
    fb.fillRect(cx + 2, cy + 2, cardW - 4, cardH - 4, RGB_FIX(12, 12, 20));
    // Büyük ikonu çiz — kartın içinde, 2px iç boşlukla
    drawBigIcon(sel, cx + 2, cy + 2);
}

// Sol veya sağ komşu kartı çizer (küçük ikon)
// Carousel'in yan taraflarındaki küçük kartlar — seçili oyunun komşuları
// Parametreler: sel (mevcut seçili), side (-1 = sol, +1 = sağ)
// Return: yok
// Not: Modular aritmetik ile carousel sarmalanır (son elemandan ilkine geçiş)
void drawSideCard(uint8_t row, uint8_t cursor, int side, int offsetY, bool extra = false) {
    uint8_t count = rows[row].count;
    int step = extra ? 2 : 1;
    uint8_t nextCursor = (cursor + side * step + count) % count;
    int index = rows[row].startIndex + nextCursor;

    int cardW = 34;
    int cardH = 34;
    int cx, cy;
    int baseX = (side == -1) ? 6 : 120;
    cx = baseX + scrollX;
    if (extra) cx += side * slideDistance;  // Ekstra kart bir slot ileride/geride
    cy = 32 + offsetY;

    // Komşu kartın tema rengi — sönük ton (seçili değil)
    uint16_t borderCol = games[index].dimColor;

    // Kart çizimi — temizlik + çerçeve + iç dolgu
    fb.fillRect(cx - 1, cy - 1, cardW + 2, cardH + 2, COL_BG_DARK);
    fb.drawRect(cx, cy, cardW, cardH, borderCol);
    fb.fillRect(cx + 1, cy + 1, cardW - 2, cardH - 2, RGB_FIX(8, 8, 16));
    // Küçük ikonu çiz — sönük renkle (komşu old. için vurgusuz)
    drawSmallIcon(index, cx + 1, cy + 1);
}

void drawLabelSingle(int sel, int offsetY, int xOffset) {
    const char* name = games[sel].label;
    int nameLen = strlen(name);
    int namePixelW = nameLen * 6;
    int nameX = (160 - namePixelW) / 2 + xOffset;
    if (nameX + namePixelW < 0 || nameX > 160) return;
    fb.setTextSize(1);
    // İsim oyunun tema rengiyle — kart border ile görsel bağ (Claude önerisi)
    fb.setTextColor(games[sel].primaryColor, COL_BG_DARK);
    fb.setCursor(nameX, 89 + offsetY);
    fb.print(name);
}

// Oyun adını çizer (merkez kartın altında)
// Sadece isim — alt çizgi kaldırıldı (PS5/Apple TV minimalizmi)
// Parametre: sel — seçili oyun indeksi
// Return: yok
void drawGameLabel(int sel, int offsetY) {
    fb.fillRect(0, 84 + offsetY, 160, 15, COL_BG_DARK);
    drawLabelSingle(sel, offsetY, 0);
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

// --- V3.0 RENDER PIPELINE ---
inline uint8_t getGlobalIndex(uint8_t row, uint8_t col) {
    return rows[row].startIndex + col;
}

void drawRowToSprite(uint8_t row, int offsetY) {
    offsetY += 11;
    int currentIdx = getGlobalIndex(row, rows[row].cursor);
    drawCenterCard(currentIdx, offsetY);
    drawSideCard(row, rows[row].cursor, -1, offsetY);
    drawSideCard(row, rows[row].cursor, 1, offsetY);
    if (slideAnimating && activeRow == row) {
        drawSideCard(row, rows[row].cursor, slideDirection, offsetY, true);
    }
    // Label: statik (kayma tearing yapıyor), snap'te yeni isim
    drawGameLabel(currentIdx, offsetY);
}

void renderFrameToSprite() {
    // 1. Arka planı tek ve pürüzsüz bir renkle doldur (Siyah barları kaldırmak için)
    fb.fillRect(0, 0, 160, 128, COL_BG_DARK);

    // 2. Y ekseninde sirayla her satiri ciz
    for (int r = 0; r < ROW_COUNT; r++) {
        int offsetY = (r * 128) - scrollY;
        if (offsetY <= -128 || offsetY >= 128) continue;
        drawRowToSprite(r, offsetY);
        yield(); // Her satir sonrasi FreeRTOS nefes alsin
    }

        // 3. Status Bar en ustte (Maskeleme yapar)
    drawStatusBar();



    // Control Hints kaldirildi (Menuyu dikeyde ortalamak icin yer acildi)

    // 5. Alt kose tamamen bos (PS5/Apple TV minimalizmi)
    // Kategori + pozisyon status bar'da, altta sadece kart + isim

    checkScreenshot(fb);
    fb.pushSprite(0, 0);
    yield(); // SPI DMA transferi sonrasi FreeRTOS'a nefes

    // FPS Hesaplama
    frameCount++;
    unsigned long now = millis();
    if (now - lastFpsTime >= 1000) {
        currentFps = frameCount;
        frameCount = 0;
        lastFpsTime = now;
        // Serial.print("FPS: ");
        // Serial.println(currentFps);
    }



}

void drawBootAnimation() {
    // Siyah başlangıç — ekranı temizle
    tft.fillScreen(COL_BLACK);

    // İki katmanlı lacivert arka plan (üst açık, alt koyu)
    tft.fillRect(0, 0, 160, 64, RGB_FIX(6, 6, 18));
    tft.fillRect(0, 64, 160, 64, RGB_FIX(3, 3, 10));

    // ─── "E-OS" logosu — karakter karakter belirir ───
    tft.setTextSize(2); // 2x font — büyük logo
    const char* logoText = "E-OS";
    int logoX = 45; // Tam ortaya hizalandı (160 - 70) / 2 = 45
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

    // ─── Versiyon etiketi (kaldirildi) ───
    delay(40); // Animasyon zamanlamasi bozulmasin diye bekleme eklendi

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
        int notes[] = {NOTE_C5, NOTE_D5, NOTE_E5, NOTE_G5};
        int durs[]  = {50, 50, 50, 40};
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
    osBuzzerOff(); // Buzzer kapat

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
    playSound(NOTE_E5, 50);
    delay(60);
    playSound(NOTE_G5, 40);
    delay(50);
    osBuzzerOff();

    // ─── Renk patlaması — karttan ekran boyutuna genişleme ───
    // Merkez kart koordinatları (drawCenterCard ile aynı)
    int cx = 54, cy = 18, cw = 52, ch = 56;

    // 1. adım: kartı biraz büyüt
    tft.fillRect(cx - 10, cy - 5, cw + 20, ch + 10, gameCol);
    yieldWait(40);
    // 2. adım: daha da büyüt
    tft.fillRect(cx - 30, cy - 15, cw + 60, ch + 30, gameCol);
    yieldWait(40);
    // 3. adım: tüm satırı kapla
    tft.fillRect(0, cy - 30, 160, ch + 60, gameCol);
    yieldWait(40);
    // 4. adım: tüm ekranı doldur
    tft.fillScreen(gameCol);
    yieldWait(60);

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
        tft.setCursor(32, 60); // Centered
        tft.print(">> FAST BOOT! <<");

        tft.setTextColor(COL_GRAY);
        tft.setCursor(29, 78); // (160 - (17 * 6)) / 2 = 29
        tft.print("Already loaded...");

        // Yeşil ilerleme çubuğu — hızlı dolar (4px adım, 5ms)
        tft.drawRect(25, 95, 110, 8, COL_SUCCESS);
        for (int p = 0; p < 106; p += 4) {
            tft.fillRect(27, 97, p, 4, COL_SUCCESS);
            yieldWait(5);
        }

        // Başarı sesi
        playSound(NOTE_E5, 50);
        yieldWait(200);
        osBuzzerOff();
    } else {
        // ─── NORMAL BAŞLATMA — flash yazma gerekecek ───
        tft.setTextSize(1);
        tft.setTextColor(COL_WHITE);
        tft.setCursor(29, 60);
        tft.print("Preparing game...");

        tft.setTextColor(COL_GRAY);
        tft.setCursor(47, 78);
        tft.print("Flashing...");

        // Accent ilerleme çubuğu — daha yavaş dolar (2px adım, 8ms)
        tft.drawRect(25, 95, 110, 8, COL_ACCENT_DIM);
        for (int p = 0; p < 106; p += 2) {
            tft.fillRect(27, 97, p, 4, COL_ACCENT);
            yieldWait(8);
        }
        yieldWait(200);
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
    if (!oledReady) return;

    // I2C bus'i saniyede 50 kere mesgul edip cihazi kasmamasi icin sadece 1 kere gonder.
    static bool powerSaved = false;
    if (!powerSaved) {
        oled.setPowerSave(1);
        powerSaved = true;
    }
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
bool saveSettings(int volume, bool fps, bool resetRecords) {
    Preferences prefs;
    if (!prefs.begin("os", false)) return false;
    bool ok = prefs.putBool("sound_en", volume > 0) == 1
           && prefs.putUChar("snd_vol", (uint8_t)volume) == 1
           && prefs.putBool("show_fps", fps) == 1;
    if (ok && resetRecords) {
        for (int i = 0; i < GAME_COUNT; i++) {
            const char* key = games[i].hsKey;
            if (key && key[0] && prefs.isKey(key) && !prefs.remove(key)) ok = false;
        }
        if (prefs.isKey("ach_dungeon") && !prefs.remove("ach_dungeon")) ok = false;
    }
    prefs.end();
    return ok;
}

void openSettingsMenu() {
    // Açan tuş basılıysa bırakılıp yeniden basılana kadar eylem üretmez.
    LauncherInput input;
    input.reset(millis());

    // Acilis sesi ve kisa gecikme
    playSound(NOTE_E5, 50);
    delay(60);

    // Lokal kopya — kullanıcı iptal ederse değişiklik kaybolur
    // Ses artik 3 kademe: 0=Kapali, 1=Kisik, 2=Yuksek (yazilimsal duty volume)
    int localVol = soundEnabled ? constrain((int)osSoundVolume, 1, 2) : 0;
    uint8_t prevVol = osSoundVolume;   // iptal edilirse canli onizleme geri alinir
    bool localFps = showFpsEnabled;
    bool localReset = false;           // Rekor sifirlama secimi
    bool settingsOpen = true;
    int settingsSel = 0; // 0 = Ses, 1 = FPS, 2 = Sifirla
    uint32_t lastSettingsMove = 0;
    const char *volNames[3] = { "OFF", "LOW", "HIGH" };

    // Ana popup döngüsü — settingsOpen false olana kadar dön
    while (settingsOpen) {
        // ─── Popup arka planı — 140x110 lacivert kutu ───
        // Arka planı temizle (status bar hariç) — arkadaki carousel'in kenardan sızmasını önler
        fb.fillRect(0, 15, 160, 113, COL_BG_DARK);
        fb.fillRect(10, 18, 140, 110, RGB_FIX(12, 12, 30));
        // Dış çerçeve — ayarlar tema rengi (gri)
        fb.drawRect(10, 18, 140, 110, games[SETTINGS_INDEX].primaryColor);
        // İç çerçeve — koyu gri vurgu
        fb.drawRect(11, 19, 138, 108, RGB_FIX(80, 80, 80));

        // Başlık — "AYARLAR" ortalanmış
        fb.setTextSize(1);
        fb.setTextColor(COL_WHITE);
        fb.setCursor(59, 24);
        fb.print("SETTINGS");
        // Başlık altı ayraç çizgisi
        fb.drawFastHLine(20, 34, 120, RGB_FIX(80, 80, 80));

        // ─── 1. Satır: SES ───
        if (settingsSel == 0) {
            fb.fillRect(18, 38, 124, 16, RGB_FIX(25, 25, 50));
            fb.drawRect(18, 38, 124, 16, COL_ACCENT);
        } else {
            fb.fillRect(18, 38, 124, 16, RGB_FIX(12, 12, 30)); // Arka plan
        }

        fb.setTextColor(COL_GRAY);
        fb.setCursor(24, 42);
        fb.print("Sound:");

        // OFF = kirmizi, LOW/HIGH = yesil; deger ortalanmis "< X >" kalibi
        fb.setTextColor(localVol == 0 ? COL_DANGER : COL_SUCCESS);
        fb.setCursor(76, 42);
        fb.print("< ");
        fb.print(volNames[localVol]);
        fb.print(" >");

        // ─── 2. Satır: FPS ───
        if (settingsSel == 1) {
            fb.fillRect(18, 56, 124, 16, RGB_FIX(25, 25, 50));
            fb.drawRect(18, 56, 124, 16, COL_ACCENT);
        } else {
            fb.fillRect(18, 56, 124, 16, RGB_FIX(12, 12, 30));
        }

        fb.setTextColor(COL_GRAY);
        fb.setCursor(24, 60);
        fb.print("FPS:");

        if (localFps) {
            fb.setTextColor(COL_SUCCESS);
            fb.setCursor(76, 60);
            fb.print("<  ON >");
        } else {
            fb.setTextColor(COL_DANGER);
            fb.setCursor(76, 60);
            fb.print("< OFF >");
        }

        // ─── 3. Satır: RECORDS ───
        if (settingsSel == 2) {
            fb.fillRect(18, 74, 124, 16, RGB_FIX(25, 25, 50));
            fb.drawRect(18, 74, 124, 16, COL_ACCENT);
        } else {
            fb.fillRect(18, 74, 124, 16, RGB_FIX(12, 12, 30));
        }

        fb.setTextColor(COL_GRAY);
        fb.setCursor(24, 78);
        fb.print("Records:");

        if (localReset) {
            fb.setTextColor(COL_SUCCESS);
            fb.setCursor(76, 78);
            fb.print("ON SAVE");
        } else if (settingsSel == 2) {
            fb.setTextColor(COL_DANGER);
            fb.setCursor(76, 78);
            fb.print("HOLD 'A'");
        } else {
            fb.setTextColor(COL_GRAY);
            fb.setCursor(76, 78);
            fb.print("< KEEP>");
        }

        // ─── Kontrol ipuçları ───
        fb.setTextColor(COL_GRAY);
        fb.setCursor(53, 94);
        fb.print("<> Change");

        fb.setCursor(22, 108);
        fb.print("A: Save");

        fb.setTextColor(COL_DARK_GRAY);
        fb.setCursor(84, 108);
        fb.print("B: Cancel");

        // ─── Ekrana Bas ───
    checkScreenshot(fb);
    fb.pushSprite(0, 0);
    yield(); // FreeRTOS nefes alsin, watchdog beslenir

        // ─── Kullanıcı girişi bekleme döngüsü ───
        bool inputReceived = false;
        while (!inputReceived) {
            uint32_t inputNow = millis();
            input.update(inputNow);
            // Cancel has priority, and discards a pending records reset.
            if (input.b.pressed) {
                osSetVolume(prevVol);
                osBuzzerOff();
                settingsOpen = false;
                break;
            }
            // Y ekseni ile menüde gezin
            int jy = analogRead(JOY_Y) - joyCenterY;
                if (abs(jy) > 500 && (millis() - lastSettingsMove > 180)) {
                if (jy > 0 && settingsSel < 2) settingsSel++;
                else if (jy < 0 && settingsSel > 0) settingsSel--;
                playSound(NOTE_F4, 25);
                inputReceived = true;
                lastSettingsMove = millis();
            }

            // X ekseni ile değeri değiştir

            int jx = analogRead(JOY_X) - joyCenterX;
                if (abs(jx) > 500 && (millis() - lastSettingsMove > 180)) {
                if (settingsSel == 0) {
                    // Ses seviyesi: sag(+)=arttir, sol(-)=azalt, 0..2 sinirli
                    if (jx > 0 && localVol < 2) localVol++;
                    else if (jx < 0 && localVol > 0) localVol--;
                    osSetVolume(localVol);          // canli onizleme icin uygula
                    osBuzzerPlay(NOTE_A4, 40);      // yeni seviyede test bip (0'da sessiz)
                } else if (settingsSel == 1) {
                    localFps = !localFps;
                    playSound(NOTE_A4, 25);
                }
                inputReceived = true;
                lastSettingsMove = millis();
            }

            // BTN_A ile kaydet ve çık
            if (input.a.pressed) {
                {
                    bool exitMenu = true;

                    // Records: Reset logic with interactive progress bar
                    if (settingsSel == 2 && !localReset) {
                        exitMenu = false;
                        uint32_t holdStart = millis();
                        bool cleared = false;
                        while (input.a.down()) {
                            input.update(millis());
                            if (input.b.pressed || !input.a.down()) break;
                            uint32_t elapsed = millis() - holdStart;
                            float progress = (float)elapsed / 2000.0f;
                            if (progress > 1.0f) progress = 1.0f;

                            // ─── İlerleme çubuğunu çiz ───
                            int fillWidth = (int)(progress * 58.0f);
                            fb.fillRect(75, 75, 62, 14, RGB_FIX(25, 25, 50));
                            if (fillWidth > 0) {
                                fb.fillRect(77, 77, fillWidth, 10, COL_DANGER);
                            }
                            fb.drawRect(76, 76, 60, 12, COL_DANGER);

                            fb.setTextColor(COL_WHITE);
                            fb.setCursor(80, 78);
                            fb.print("HOLD 'A'");

                            checkScreenshot(fb);
                            fb.pushSprite(0, 0);

                            if (elapsed >= 2000) { // 2 saniye doldu
                                // Stage deletion until A:Save; B:Cancel keeps records.
                                playSound(NOTE_C5, 100);
                                delay(100);
                                playSound(NOTE_E5, 150);

                                localReset = true;
                                cleared = true;

                                // Silme isteğini göster; ikinci A basışı kaydeder.
                                fb.fillRect(18, 74, 124, 16, RGB_FIX(25, 25, 50));
                                fb.drawRect(18, 74, 124, 16, COL_ACCENT);
                                fb.setTextColor(COL_GRAY);
                                fb.setCursor(24, 78);
                                fb.print("Records:");
                                fb.setTextColor(COL_SUCCESS);
                                fb.setCursor(76, 78);
                                fb.print("ON SAVE");
                                checkScreenshot(fb);
                                fb.pushSprite(0, 0);

                                break;
                            }
                            yieldWait(15);
                        }

                        if (input.b.pressed) {
                            osSetVolume(prevVol);
                            osBuzzerOff();
                            settingsOpen = false;
                        }
                        if (cleared) {
                            exitMenu = false;
                            inputReceived = true;
                            // Basılı A tekrar kaydetmesin; B her zaman çıkabilir.
                            input.reset(millis());
                        } else {
                            // Erken bıraktıysa ekranı normale döndür
                            inputReceived = true;
                        }
                    }

                    if (exitMenu) {
                        if (!saveSettings(localVol, localFps, localReset)) {
                            fb.fillRect(20, 94, 120, 10, COL_BG_DARK);
                            fb.setCursor(22, 94); fb.print("NVS SAVE ERROR!");
                            fb.pushSprite(0, 0);
                            yieldWait(1000);
                            input.reset(millis());
                            inputReceived = true;
                            continue;
                        }
                        soundEnabled = (localVol > 0);
                        osSetVolume(localVol);
                        showFpsEnabled = localFps;
                        playSound(NOTE_E5, 50);
                        delay(60);
                        playSound(NOTE_G5, 40);
                        delay(50);
                        osBuzzerOff();

                        input.reset(millis());

                        settingsOpen = false;
                        inputReceived = true;
                    }
                }
            }

            yield(); // Pacing — delay(30) yerine FreeRTOS nefesi
        }
    }

    // Popup kapandı — menüyü tamamen yeniden çiz
    renderFrameToSprite();
}

// ══════════════════════════════════════════════════════════════
// --- V3.1 EKLENTİSİ --- TOOLS MENÜSÜ (TFT POPUP — flash yok)
// ══════════════════════════════════════════════════════════════
//
// 3 sekmeli araç uygulaması: Kronometre / Geri Sayım / Metronom.
// SETTINGS popup'ından farkı: BLOK EDEN input bekleme YOK — her karede zaman
// güncellenir (kronometre sayarken/metronom çalarken donmaz). Butonlar kenar
// tespiti ve debounce ile okunur; açılışta basılı tuşlar devralınmaz.
//
// Kontroller:
//   C           : Sekme değiştir (SW → TIMER → METRO)
//   A           : Başlat/Duraklat (kronometre, geri sayım, metronom)
//   D           : Sıfırla (kronometre→0, geri sayım→ayarlı süre)
//   Joystick <> : Değer ayarla (geri sayım: ±30sn, metronom: ±5 BPM)
//   B           : TOOLS'tan çık → carousel
//
// Not: Durum fonksiyona yerel — TOOLS'a her girişte sıfırdan başlar (kalıcılık yok).
// 2x2 hizalı buton ipucu ızgarası — okuma sırası A(sol-üst) B(sağ-üst) C(sol-alt) D(sağ-alt).
// Sütunlar sabit x'te hizalı; null geçilen slot boş bırakılır.
static void osToolHints(const char* a, const char* b, const char* c, const char* d) {
    const int LX = 24, RX = 86, Y1 = 94, Y2 = 104;
    fb.setTextSize(1);
    fb.setTextColor(COL_GRAY);
    if (a) { fb.setCursor(LX, Y1); fb.print(a); }
    if (b) { fb.setCursor(RX, Y1); fb.print(b); }
    if (c) { fb.setCursor(LX, Y2); fb.print(c); }
    if (d) { fb.setCursor(RX, Y2); fb.print(d); }
}

void openToolsMenu() {
    // Açan A basışı araçta başlat/durdur işlemi üretmez.
    LauncherInput input;
    input.reset(millis());
    playSound(NOTE_E5, 50); delay(60); osBuzzerOff();

    enum { TOOL_SW = 0, TOOL_TIMER = 1, TOOL_METRO = 2 };
    int tool = TOOL_SW;
    const char* tabNames[3] = { "STOPWATCH", "TIMER", "METRONOME" };

    // Kronometre durumu
    bool     swRun   = false;
    uint32_t swAccum = 0;   // duraklatınca birikmiş ms
    uint32_t swStart = 0;   // son başlatma anı (millis)

    // Geri sayım durumu
    int      timerSetSec = 60;      // ayarlı süre (sn) — 30..3600
    bool     timerRun    = false;
    uint32_t timerEndMs  = 0;       // 0'a ulaşacağı an
    uint32_t timerRemPaused = 60000;// duraklatınca kalan ms
    bool     timerAlarm  = false;   // süre doldu, alarm çalıyor
    uint32_t lastAlarmBeep = 0;

    // Metronom durumu (sadece METRO sekmesindeyken tıklar)
    int      bpm       = 120;
    bool     metroRun  = false;
    uint32_t nextBeat  = 0;
    int      beatIdx   = 0;         // 0..3 (her 4'te vurgu)
    uint32_t beatFlash = 0;         // görsel nabız için

    // Buton kenar tespiti
    // Debounced buttons are polled independently of drawing.
    uint32_t lastJoy = 0;

    bool open = true;
    while (open) {
        uint32_t now = millis();
        input.update(now);
        if (input.b.pressed) { osBuzzerOff(); break; }

        // ─── Geri sayım: her karede kontrol (sekmeden bağımsız arka planda biter) ───
        if (timerRun) {
            if (deadlineReached(now, timerEndMs)) {
                timerRun = false;
                timerAlarm = true;
                tool = TOOL_TIMER;      // alarmı görmesi için sekmeye geç
                lastAlarmBeep = 0;      // hemen ilk bip
            }
        }
        // Alarm bip deseni (0.5sn'de bir çift bip) — A/D ile susturulana kadar
        if (timerAlarm && (now - lastAlarmBeep > 500)) {
            lastAlarmBeep = now;
            playSound(NOTE_G5, 120);
        }

        // ─── Metronom tık üretimi (sadece bu sekmedeyken) ───
        if (metroRun && tool == TOOL_METRO) {
            if (deadlineReached(now, nextBeat)) {
                bool accent = (beatIdx == 0);
                playSound(accent ? NOTE_G5 : NOTE_C5, 22);   // vurgu = tavan nota (784Hz), normal = C5
                beatFlash = now;
                beatIdx = (beatIdx + 1) % 4;
                uint32_t interval = 60000UL / (uint32_t)bpm;
                // Kaymayı önlemek için nextBeat'i sabit adımla ilerlet
                nextBeat += interval;
                if (deadlineReached(now, nextBeat)) nextBeat = now + interval; // gecikme birikirse resenkron
            }
        }

        // ══════════════════ ÇİZİM ══════════════════
        // Arka planı temizle (status bar hariç) — arkadaki carousel'in kenardan sızmasını önler
        fb.fillRect(0, 15, 160, 113, COL_BG_DARK);
        // Popup kutusu (SETTINGS ile aynı ölçü: 140x96 @ 10,20)
        fb.fillRect(10, 20, 140, 96, RGB_FIX(12, 20, 26));
        fb.drawRect(10, 20, 140, 96, games[TOOLS_INDEX].primaryColor);
        fb.drawRect(11, 21, 138, 94, RGB_FIX(0, 90, 80));

        // Başlık: "TOOLS" + aktif sekme adı
        fb.setTextSize(1);
        fb.setTextColor(COL_WHITE);
        fb.setCursor(16, 25); fb.print("TOOLS");
        fb.setTextColor(games[TOOLS_INDEX].primaryColor);
        { int w = strlen(tabNames[tool]) * 6; fb.setCursor(146 - w, 25); fb.print(tabNames[tool]); }
        fb.drawFastHLine(16, 35, 128, RGB_FIX(0, 90, 80));

        // Sekme göstergesi (3 nokta)
        for (int i = 0; i < 3; i++) {
            int dx = 66 + i * 10;
            if (i == tool) fb.fillCircle(dx, 41, 3, games[TOOLS_INDEX].primaryColor);
            else           fb.drawCircle(dx, 41, 3, RGB_FIX(0, 90, 80));
        }

        char buf[16];
        if (tool == TOOL_SW) {
            // ─── Kronometre — MM:SS.d (onda bir) ───
            uint32_t el = swRun ? (swAccum + (now - swStart)) : swAccum;
            uint32_t mm = (el / 60000) % 100;
            uint32_t ss = (el / 1000) % 60;
            uint32_t td = (el / 100) % 10;
            snprintf(buf, sizeof(buf), "%02u:%02u.%u", (unsigned)mm, (unsigned)ss, (unsigned)td);
            fb.setTextSize(2);
            fb.setTextColor(swRun ? COL_SUCCESS : COL_WHITE);
            { int w = strlen(buf) * 12; fb.setCursor((160 - w) / 2, 58); fb.print(buf); }
            osToolHints(swRun ? "A:Pause" : "A:Start", "B:Exit", "C:Tab", "D:Reset");
        } else if (tool == TOOL_TIMER) {
            // ─── Geri Sayım — MM:SS ───
            uint32_t rem = timerAlarm ? 0
                          : (timerRun ? (deadlineRemaining(now, timerEndMs))
                                      : timerRemPaused);
            uint32_t mm = (rem + 999) / 60000;              // yukarı yuvarla (görsel)
            uint32_t ss = ((rem + 999) / 1000) % 60;
            snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)mm, (unsigned)ss);
            fb.setTextSize(3);
            // Alarm çalıyorsa yanıp sönen kırmızı
            bool blink = timerAlarm && ((now / 300) % 2 == 0);
            fb.setTextColor(timerAlarm ? (blink ? COL_DANGER : COL_WHITE)
                                       : (timerRun ? COL_SUCCESS : COL_WHITE));
            { int w = strlen(buf) * 18; fb.setCursor((160 - w) / 2, 50); fb.print(buf); }
            // Ayar/durum satırı — ortalı (y=82): alarm mesajı veya "<> Set"
            fb.setTextSize(1);
            if (timerAlarm) {
                bool bl = (now / 300) % 2 == 0;
                fb.setTextColor(bl ? COL_DANGER : COL_WHITE);
                const char* m = "TIME'S UP!"; int w = strlen(m) * 6;
                fb.setCursor((160 - w) / 2, 82); fb.print(m);
            } else if (!timerRun) {
                fb.setTextColor(COL_GRAY);
                const char* m = "<> Set"; int w = strlen(m) * 6;
                fb.setCursor((160 - w) / 2, 82); fb.print(m);
            }
            osToolHints(timerAlarm ? "A:OK" : (timerRun ? "A:Pause" : "A:Start"),
                        "B:Exit", "C:Tab", "D:Reset");
        } else {
            // ─── Metronom — BPM ───
            snprintf(buf, sizeof(buf), "%d", bpm);
            // Timer ve Stopwatch gibi sadece büyük metni tam merkeze ortala
            int numW = strlen(buf) * 18;      // size3
            int gx   = (160 - numW) / 2;
            
            fb.setTextSize(3);
            fb.setTextColor(metroRun ? COL_SUCCESS : COL_WHITE);
            fb.setCursor(gx, 50); fb.print(buf);
            
            // Ayar/durum satırı — ortalı (y=82): Timer'daki "<> Set" gibi "<> BPM" yazısı
            fb.setTextSize(1);
            fb.setTextColor(COL_GRAY);
            const char* m = "<> BPM"; int w = strlen(m) * 6;
            fb.setCursor((160 - w) / 2, 82); fb.print(m);
            
            // Metronomda D eylemi: D:Reset eklendi
            osToolHints(metroRun ? "A:Stop" : "A:Start", "B:Exit", "C:Tab", "D:Reset");
        }

        checkScreenshot(fb);
        fb.pushSprite(0, 0);

        // ══════════════════ GİRİŞ ══════════════════
        // C — sekme değiştir (metronomu durdur, tık karışmasın)
        if (input.c.pressed) {
            tool = (tool + 1) % 3;
            metroRun = false; osBuzzerOff();
            playSound(NOTE_A4, 25);
        }

        // A — başlat/duraklat (araca göre)
        if (input.a.pressed && !input.c.pressed) {
            if (tool == TOOL_SW) {
                if (swRun) { swAccum += now - swStart; swRun = false; }
                else       { swStart = now; swRun = true; }
                playSound(NOTE_D5, 30);
            } else if (tool == TOOL_TIMER) {
                if (timerAlarm) {                       // alarmı sustur + sıfırla
                    timerAlarm = false; osBuzzerOff();
                    timerRemPaused = (uint32_t)timerSetSec * 1000;
                } else if (timerRun) {                  // duraklat
                    timerRemPaused = (deadlineRemaining(now, timerEndMs));
                    timerRun = false;
                } else {                                // başlat (kalan süreyle)
                    if (timerRemPaused == 0) timerRemPaused = (uint32_t)timerSetSec * 1000;
                    timerEndMs = now + timerRemPaused;
                    timerRun = true;
                }
                playSound(NOTE_D5, 30);
            } else { // METRO
                metroRun = !metroRun;
                if (metroRun) { beatIdx = 0; nextBeat = now; }  // hemen ilk vuruş
                else osBuzzerOff();
            }
        }

        // D — sıfırla
        if (input.d.pressed && !input.c.pressed && !input.a.pressed) {
            if (tool == TOOL_SW) {
                swRun = false; swAccum = 0; swStart = now;
                playSound(NOTE_F4, 25);
            } else if (tool == TOOL_TIMER) {
                timerRun = false; timerAlarm = false; osBuzzerOff();
                timerRemPaused = (uint32_t)timerSetSec * 1000;
                playSound(NOTE_F4, 25);
            } else if (tool == TOOL_METRO) {
                metroRun = false; osBuzzerOff();
                bpm = 120;
                playSound(NOTE_F4, 25);
            }
        }

        // Joystick <> — değer ayarla (debounce 160ms)
        int jx = analogRead(JOY_X) - joyCenterX;
        if (abs(jx) > 500 && (now - lastJoy > 160)) {
            if (tool == TOOL_TIMER && !timerRun && !timerAlarm) {
                if (jx > 0 && timerSetSec < 3600) timerSetSec += 30;
                else if (jx < 0 && timerSetSec > 30) timerSetSec -= 30;
                timerRemPaused = (uint32_t)timerSetSec * 1000;   // ayar kalan süreyi de günceller
                playSound(NOTE_A4, 20);
                lastJoy = now;
            } else if (tool == TOOL_METRO) {
                if (jx > 0 && bpm < 240) bpm += 5;
                else if (jx < 0 && bpm > 40) bpm -= 5;
                playSound(NOTE_A4, 20);
                lastJoy = now;
            }
        }

        yield();  // FreeRTOS nefesi + watchdog
    }

    renderFrameToSprite();
}

// ══════════════════════════════════════════════════════════════
// --- V3.2 EKLENTİSİ --- DRAW (PİKSEL ÇİZİM — TFT tuval + OLED HUD, SD'ye BMP)
// ══════════════════════════════════════════════════════════════
//
// TFT = saf tuval (üst palet şeridi + 40x28 hücre ızgara). OLED = kontrol/HUD
// (koordinat, renk adı, tuş açıklamaları, kayıt mesajı). Launcher-içi, flash yok.
//
// Kontroller:
//   Joystick     : imleç (hücre hücre)
//   A            : çiz (basılı tut + hareket = sürekli çizgi)
//   D            : sil (basılı tut + hareket = sürekli silme)
//   C            : renk döngüsü (8 renk)
//   Joystick tık : SD'ye kaydet (/draw_N.bmp, 160x112 24-bit)
//   B            : çık → carousel
//
// Kanvas `static` → oturum içinde korunur (çık/gir silmez); güç kesilince BMP'den geri gelir.

#define DRAW_PIX 4
#define DRAW_GW  40         // ızgara genişliği (40*4=160)
#define DRAW_GH  28         // ızgara yüksekliği (28*4=112)
#define DRAW_TOP 13         // tuval üst y (üstte 0..12 palet şeridi)

struct DrawColor { uint8_t r, g, b; const char* name; };
static const DrawColor DPAL[8] = {
    {255,255,255,"WHITE"}, {255,60,60,"RED"},   {255,150,20,"ORANGE"}, {255,225,40,"YELLOW"},
    {60,220,90,"GREEN"},   {40,200,255,"CYAN"}, {90,120,255,"BLUE"},   {235,90,235,"PINK"}
};
static const uint8_t DRAW_EMPTY = 0xFF;   // boş hücre = arka plan

// NOT: SD'ye BMP kaydı kaldırıldı — bu kartta TFT_eSPI ile SD ortak SPI veriyolu,
// aktif TFT'yle SD'ye yazma bus mutex'inde kilitleniyor (dev_tools.h de belgeliyor).
// Galeri görüntüsü diğer oyunlar gibi USB ekran görüntüsüyle alınır. Çizim oturum
// içinde `static grid`'te korunur; joystick-tık = tuvali temizle.

void openDrawApp() {
    LauncherInput input;
    input.reset(millis());
    playSound(NOTE_E5, 40); delay(50); osBuzzerOff();

    static uint8_t grid[DRAW_GH][DRAW_GW];
    static bool gridInit = false;
    if (!gridInit) {
        memset(grid, DRAW_EMPTY, sizeof(grid));
        gridInit = true;
    }

    int cx = DRAW_GW / 2, cy = DRAW_GH / 2;   // imleç
    int curColor = 4;                          // GREEN
    uint16_t canvasBg = RGB_FIX(8, 8, 18);

    // OLED'i uyandır (menüde uyku modundaydı)
    if (oledReady) oled.setPowerSave(0);

    // Held entry buttons are suppressed until released.
    uint32_t lastMove = 0, lastHud = 0;
    bool hudDirty = true;

    bool open = true;
    while (open) {
        uint32_t now = millis();
        input.update(now);
        if (input.b.pressed) { osBuzzerOff(); break; }

        // ─── Giriş: imleç hareketi (hücre hücre, tekrar hızı 55ms) ───
        int jx = analogRead(JOY_X) - joyCenterX;
        int jy = analogRead(JOY_Y) - joyCenterY;
        if (now - lastMove > 55) {
            bool moved = false;
            if (jx >  500 && cx < DRAW_GW-1) { cx++; moved = true; }
            else if (jx < -500 && cx > 0)    { cx--; moved = true; }
            if (jy >  500 && cy < DRAW_GH-1) { cy++; moved = true; }
            else if (jy < -500 && cy > 0)    { cy--; moved = true; }
            if (moved) { lastMove = now; hudDirty = true; }
        }

        // ─── Çiz / Sil (basılı tutunca sürekli) ───
        if (input.a.held()) grid[cy][cx] = (uint8_t)curColor;
        if (input.d.held()) grid[cy][cx] = DRAW_EMPTY;

        // ─── Renk döngüsü (C, kenar) ───
        if (input.c.pressed) { curColor = (curColor + 1) % 8; hudDirty = true; playSound(NOTE_A4, 20); }

        // ─── Temizle (joystick tık, kenar) — tüm tuvali sil ───
        if (input.joy.pressed) {
            memset(grid, DRAW_EMPTY, sizeof(grid));
            hudDirty = true;
            playSound(NOTE_F4, 30); delay(30); playSound(NOTE_C4, 40); osBuzzerOff();
        }

        // ══════════════ TFT ÇİZİM ══════════════
        // Üst palet şeridi (0..12)
        fb.fillRect(0, 0, 160, DRAW_TOP, RGB_FIX(22, 22, 34));
        for (int i = 0; i < 8; i++) {
            int sx = 4 + i * 9;
            fb.fillRect(sx, 2, 7, 8, RGB_FIX(DPAL[i].r, DPAL[i].g, DPAL[i].b));
            if (i == curColor) fb.drawRect(sx - 1, 1, 9, 10, COL_WHITE);
        }
        fb.drawFastHLine(0, DRAW_TOP - 1, 160, RGB_FIX(60, 60, 80));

        // Tuval: zemin + dolu hücreler
        fb.fillRect(0, DRAW_TOP, 160, DRAW_GH * DRAW_PIX, canvasBg);
        for (int gy = 0; gy < DRAW_GH; gy++)
            for (int gx = 0; gx < DRAW_GW; gx++) {
                uint8_t v = grid[gy][gx];
                if (v != DRAW_EMPTY)
                    fb.fillRect(gx * DRAW_PIX, DRAW_TOP + gy * DRAW_PIX, DRAW_PIX, DRAW_PIX,
                                RGB_FIX(DPAL[v].r, DPAL[v].g, DPAL[v].b));
            }

        // İmleç: yanıp sönen beyaz çerçeve
        if ((now / 350) % 2 == 0) {
            int px = cx * DRAW_PIX, py = DRAW_TOP + cy * DRAW_PIX;
            fb.drawRect(px, py, DRAW_PIX, DRAW_PIX, COL_WHITE);
        }

        checkScreenshot(fb);
        fb.pushSprite(0, 0);

        // ══════════════ OLED HUD ══════════════
        if (oledReady && hudDirty && (now - lastHud > 100)) {
            lastHud = now; hudDirty = false;
            oled.clearBuffer();
            oled.setFont(u8g2_font_6x12_tr);
            oled.drawStr(28, 11, "PIXEL DRAW");
            oled.drawHLine(0, 15, 128);
            oled.setFont(u8g2_font_5x7_tr);
            char b[26];
            snprintf(b, sizeof(b), "X:%2d  Y:%2d", cx, cy);   oled.drawStr(4, 26, b);
            snprintf(b, sizeof(b), "Color: %s", DPAL[curColor].name); oled.drawStr(4, 35, b);
            oled.drawStr(4, 46, "Joy:Move  A:Draw  D:Erase");
            oled.drawStr(4, 54, "C:Color  Click:Clear");
            oled.drawStr(4, 62, "B:Exit");
            oled.sendBuffer();
        }

        yield();
    }

    if (oledReady) oled.setPowerSave(1);   // OLED'i tekrar uykuya al (menü modu)
    renderFrameToSprite();
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
void calibrateLauncherJoystick() {
    uint32_t start = millis();
    int x = analogRead(JOY_X), y = analogRead(JOY_Y);
    bool warningShown = false;
    while (x < 1400 || x > 2600 || y < 1400 || y > 2600) {
        if (!warningShown) {
            fb.fillScreen(COL_BLACK); fb.setTextColor(COL_DANGER);
            fb.setTextSize(1); fb.setCursor(23, 60);
            fb.print("RELEASE JOYSTICK!"); fb.pushSprite(0, 0);
            warningShown = true;
        }
        if (millis() - start >= 5000) {
            joyCenterX = joyCenterY = 2048;
            return;
        }
        delay(50);
        x = analogRead(JOY_X); y = analogRead(JOY_Y);
    }
    fb.fillScreen(COL_BLACK); fb.setTextColor(COL_ACCENT);
    fb.setTextSize(1); fb.setCursor(38, 58);
    fb.print("Calibration..."); fb.pushSprite(0, 0);
    long sumX = 0, sumY = 0;
    for (int i = 0; i < 10; i++) {
        x = analogRead(JOY_X); y = analogRead(JOY_Y);
        if (x < 1400 || x > 2600 || y < 1400 || y > 2600) {
            joyCenterX = joyCenterY = 2048;
            return;
        }
        sumX += x; sumY += y; delay(2);
    }
    joyCenterX = sumX / 10; joyCenterY = sumY / 10;
}

void setup() {
    // ─── BÖLÜM 1: Seri port + I2C hattı ───
    Serial.begin(115200);       // Debug için seri port
    // Start screenshot buffers only after bootloader work and framebuffer allocation.
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
    osAudioInit();   // BUZZER'i LEDC'ye bagla (+ oto-durdurma timer + NVS volume)

    // SPI CS pinleri — yüksek = deaktif
    // Birden fazla SPI cihazı (TFT + SD) var, başlangıçta ikisi de pasif
    pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS, OUTPUT);  digitalWrite(SD_CS, HIGH);

    // ─── BÖLÜM 4: SPI hattı + SD kart başlatma ───
    // SPI hattını başlat
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1); delay(50);
    sdReady = SD.begin(SD_CS, SPI, 20000000); // 20 MHz (sweet spot - kararli ve hizli)

    // Launcher tools remain available without a card. Flash loading needs SD.
    {
        Preferences prefs;
        if (prefs.begin("os", true)) {
            soundEnabled = prefs.getBool("sound_en", true);
            showFpsEnabled = prefs.getBool("show_fps", false);
            prefs.end();
        }
        osSetVolume(soundEnabled ? constrain((int)osSoundVolume, 0, 2) : 0);
        soundEnabled = osSoundVolume > 0;
    }
    // ─── BÖLÜM 5: Bootloader modu — flash yazma ───
    // Bootloader modunda mıyız? (RTC magic kontrolü)
    if (enterBootloader && sdReady) {
        runEarlyBootloader(flashTarget);
        // Bu fonksiyon ya ESP.restart() ile çıkar ya da return ile normale döner
    }

    // ─── BÖLÜM 6: TFT başlatma + joystick switch pini ───
    tft.init(); tft.setRotation(1); tft.setSwapBytes(true); tft.fillScreen(TFT_BLACK);
    framebufferReady = fb.createSprite(160, 128) != nullptr;
    if (!framebufferReady) {
        tft.setTextColor(TFT_RED); tft.setTextSize(1);
        tft.setCursor(8, 55); tft.print("FRAMEBUFFER ERROR");
        return;
    }
    startFrameDumper();
    pinMode(JOY_SW, INPUT_PULLUP); // Joystick switch butonu (basılı 0)

    // Renk paletini ve oyun listesini başlat (TFT hazır olmalı!)
    // RGB_FIX() artık güvenle çağrılabilir
    initColorsAndGames();

    calibrateLauncherJoystick();

    // ═══ BÖLÜM 9: PREMİUM BOOT ANİMASYONU ═══
    drawBootAnimation();

    // ─── BÖLÜM 10: OLED başlat + ilk carousel çizimi ───
    // OLED başlatma
    oled.begin();
    oledReady = true; // Artık OLED çizim fonksiyonları güvenle çağrılabilir

    // İlk carousel çizimi — fullRedraw=true (frame dahil)
    renderFrameToSprite();
    menuInput.reset(millis());
    updateOLED(carouselSel); // OLED'i uyku moduna al (güç tasarrufu)
}

// ══════════════════════════════════════════════════════════════
//  LOOP — Carousel Navigasyonu ve Oyun Başlatma
// ══════════════════════════════════════════════════════════════

// loop — Arduino ana döngüsü; sürekli çalışır
// Akış (her iterasyonda):
//   1. Joystick Y oku → satır değiştirme (dikey animasyon)
//   2. Joystick X oku → yatay carousel kaydırma (cubic ease-out)
//   3. OLED güç tasarrufu
//   4. BTN_A: oyun başlat / SETTINGS-TOOLS-DRAW popup aç
//   5. Frame limiting (8ms = ~125FPS cap)
// Parametre: yok
// Return: yok (Arduino loop imzası)

void launchGame(uint8_t index) {
    const char* filename = games[index].filename;
    bool fastBoot = cachedGameMatches(filename);
    if (!fastBoot && !sdReady) {
        fb.fillScreen(COL_BLACK); fb.setTextColor(COL_DANGER);
        fb.setTextSize(1); fb.setCursor(8, 55); fb.print("NO SD CARD!");
        fb.pushSprite(0, 0); yieldWait(1200);
        return;
    }
    drawLaunchAnimation(index, fastBoot);
    if (fastBoot) {
        const esp_partition_t* part = gamePartition();
        if (part && esp_ota_set_boot_partition(part) == ESP_OK) {
            ESP.restart();
            return;
        }
        // Activation failed: reload from SD instead of blindly restarting.
        if (!clearGameCache() || !sdReady) return;
    }
    bootModeMagic = 0xDEADBEEF;
    strncpy(bootFilename, filename, sizeof(bootFilename) - 1);
    bootFilename[sizeof(bootFilename) - 1] = '\0';
    ESP.restart();
}

void loop() {
    if (!framebufferReady) { delay(50); return; }
    uint32_t current_ms = millis();
    static uint32_t lastMoveX = 0;
    static uint32_t lastMoveY = 0;
    static uint32_t lastFrameMs = 0;

    // Kare hızı sınırı (~125 FPS cap) — delay(8) yerine millis tabanlı
    if (current_ms - lastFrameMs < 8) { yield(); return; }
    lastFrameMs = current_ms;
    menuInput.update(current_ms);
    bool movingAtFrameStart = slideAnimating || rowAnimating;

    // Joystick oku
    int jx = analogRead(JOY_X) - joyCenterX;
    int jy = analogRead(JOY_Y) - joyCenterY;

    // --- Y Ekseni: Satir Degistirme (V3.0) ---
    bool yMoved = false;
    if (!slideAnimating && !rowAnimating && abs(jy) > 500 && (current_ms - lastMoveY > 200)) {
        if (jy > 0 && activeRow < ROW_COUNT - 1) {
            activeRow++;
            scrollYTarget = activeRow * 128;
            scrollYStart = scrollY;
            rowStartTime = current_ms;
            rowAnimating = true;
            playSound(NOTE_F4, 40);
            lastMoveY = current_ms;
            yMoved = true;
        } else if (jy < 0 && activeRow > 0) {
            activeRow--;
            scrollYTarget = activeRow * 128;
            scrollYStart = scrollY;
            rowStartTime = current_ms;
            rowAnimating = true;
            playSound(NOTE_F4, 40);
            lastMoveY = current_ms;
            yMoved = true;
        }
    }

    // --- X Ekseni: Yatay Kaydirma Animasyonu (PS5 Tarzi) ---
    if (!yMoved && !slideAnimating && !rowAnimating && abs(jx) > 500 && (current_ms - lastMoveX > 200)) {
        if (jx > 0) {
            slideDirection = 1;
            slideDistance = 57;   // Merkez-merkeze 57px (simetrik)
            scrollXTarget = -57;
        } else {
            slideDirection = -1;
            slideDistance = 57;   // Merkez-merkeze 57px (simetrik)
            scrollXTarget = 57;
        }
        slideAnimating = true;
        slideStartX = scrollX;
        slideStartTime = current_ms;
        lastMoveX = current_ms;
        playSound(NOTE_A4, 25);
    }

    // --- Yatay Animasyon Isleme (Zaman Bazli, Cubic Ease-Out) ---
    if (slideAnimating) {
        uint32_t elapsed = current_ms - slideStartTime;
        const uint32_t duration = 180; // 180ms sabit sure

        if (elapsed >= duration) {
            scrollX = scrollXTarget;
            int count = rows[activeRow].count;
            if (scrollXTarget < 0) {
                rows[activeRow].cursor = (rows[activeRow].cursor + 1) % count;
            } else {
                rows[activeRow].cursor = (rows[activeRow].cursor == 0) ? count - 1 : rows[activeRow].cursor - 1;
            }
            carouselSel = getGlobalIndex(activeRow, rows[activeRow].cursor);
            scrollX = 0;
            scrollXTarget = 0;
            slideAnimating = false;
        } else {
            // Cubic ease-out: t = 1 - (1 - x)^3
            int t = (elapsed * 1000) / duration; // 0..1000
            int oneMinus = 1000 - t;
            int eased = 1000 - ((oneMinus * oneMinus / 1000) * oneMinus / 1000);
            scrollX = slideStartX + (scrollXTarget - slideStartX) * eased / 1000;
        }
        renderFrameToSprite();
    }

    // --- Dikey Animasyon Isleme (Zaman Bazli, Cubic Ease-Out) ---
    if (rowAnimating) {
        uint32_t elapsed = current_ms - rowStartTime;
        const uint32_t duration = 300;  // 300ms sabit sure

        if (elapsed >= duration) {
            scrollY = scrollYTarget;
            rowAnimating = false;
        } else {
            // Cubic ease-out: 1 - (1-t)^3 (X ekseni ile ayni formul)
            int t = (elapsed * 1000) / duration;
            int oneMinus = 1000 - t;
            int eased = 1000 - ((oneMinus * oneMinus / 1000) * oneMinus / 1000);
            scrollY = scrollYStart + (scrollYTarget - scrollYStart) * eased / 1000;
        }

        renderFrameToSprite();

        if (!rowAnimating) {
            carouselSel = getGlobalIndex(activeRow, rows[activeRow].cursor);
        }
    }

    // --- OLED Guc Tasarrufu ---
    updateOLED(carouselSel);

    // Presses during movement are consumed; a new press selects the settled card.
    if ((movingAtFrameStart || slideAnimating || rowAnimating || yMoved) && menuInput.a.raw == LOW) {
        menuInput.a.reset(LOW, current_ms);
    }
    if (menuInput.a.pressed && !movingAtFrameStart && !slideAnimating && !rowAnimating && !yMoved) {
        uint8_t globalIdx = getGlobalIndex(activeRow, rows[activeRow].cursor);
        if (globalIdx == SETTINGS_INDEX) openSettingsMenu();
        else if (globalIdx == TOOLS_INDEX) openToolsMenu();
        else if (globalIdx == DRAW_INDEX) openDrawApp();
        else launchGame(globalIdx);
        menuInput.reset(millis());
        lastMoveX = lastMoveY = millis();
        renderFrameToSprite();
    }

    checkScreenshot(fb);
}
