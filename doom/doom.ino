// ============================================================
//  EMİR DOOM V7.8 — OS UPDATE
//  Özellikler: Saf FreeRTOS İzolasyonu, Çökmeyen Task Yapısı, OLED/TFT Ayrımı
// ============================================================
//  Bu proje, ESP32-S3 tabanlı bir el yapımı oyun konsolunda DOOM'un
//  raycasting tabanlı bir klonunu çalıştırır. Çift çekirdek (dual-core)
//  mimarisinden faydalanılarak oyun mantığı ve ekran çizimi ayrı task'lara
//  bölünmüştür. PSRAM üzerinde triple-buffering ile tear-free (yırtılmasız)
//  görüntü elde edilir.
// ============================================================

// ============================================================
//  PRAGMA GCC OPTIMIZE
//  O3: Maksimum optimizasyon (hız odaklı, kod boyutu artar).
//  unroll-loops: Döngüleri açarak döngü başyükünü (branch/counter) azaltır.
//  Raycasting ve texture mapping gibi dar geçitlerde ciddi FPS artışı sağlar.
//  IRAM_ATTR ile birlikte kullanıldığında özellikle TaskEngine'de etkili olur.
// ============================================================
#pragma GCC optimize ("O3")
#pragma GCC optimize ("unroll-loops")

// TFT_eSPI: TFT ekrana hızlı piksel/imagen basmak için kullanılan grafik kütüphanesi
#include <TFT_eSPI.h>
// SPI: SD kart ve TFT ortak SPI veriyolunu paylaşır (farklı CS pinleri ile)
#include <SPI.h>
// SD: BMP texture'ları ve TITLEPIC'i SD karttan okumak için
#include <SD.h>
// Wire: OLED (SH1106) I2C haberleşmesi için (TaskRadar kullanır)
#include <Wire.h>
// U8g2lib: OLED monochrome ekrana radar haritasını çizen kütüphane
#include <U8g2lib.h>
// math.h: sqrt, sin, atan2, fabs gibi raycasting ve fizik hesapları için
#include <math.h>
// FreeRTOS: Görev (task) oluşturma, çekirdeğe sabitleme ve zamanlama için
#include <freertos/FreeRTOS.h>
// semphr: Mutex (karşılıklı dışlama) — çift çekirdek veri çakışmasını önler
#include <freertos/semphr.h>
// Update.h / esp_ota_ops: OTA (Over-The-Air) bölümleri ve boot partition yönetimi
#include <Update.h>
#include <esp_ota_ops.h>
// Preferences: NVS (Non-Volatile Storage) üzerinden ses ayarı gibi kalıcı veri okuma
#include <Preferences.h>

// hardware_config.h: Donanım pin tanımları (TFT_CS, SD_CS, BTN_*, JOY_*, BUZZER, SPI_*)
// Projenin diğer .ino dosyalarıyla paylaşılan ortak pin haritası
#include "../hardware_config.h"
// dev_tools.h: Geliştirici araçları — takeScreenshotFB, checkScreenshotFB, setScreenshotMode, initDevTools
// Ekran görüntüsü alma ve debug yardımcıları buradan gelir
#include "../dev_tools.h"

// TFT_eSPI ekran nesnesi — ana renkli TFT (SPI üzerinden veri alır)
TFT_eSPI tft = TFT_eSPI();

// OLED nesnesi — SH1106 128x64 monochrome ekran, yazılım I2C (SDA=9, SCL=8)
// Radar/mini-harita buraya çizilir, TFT'den tamamen bağımsız çalışır
U8G2_SH1106_128X64_NONAME_F_SW_I2C oled(U8G2_R0, 9, 8, U8X8_PIN_NONE);

// ============================================================
//  returnToOS — Ana menüye (OS) dönmek için ESP32'yi yeniden başlatır.
//  Çünkü bu program standalone (tek başına) çalışır; OS menüsü ayrı bir
//  sketch'tedir. Bu yüzden restart ile OS'a dönüş simüle edilir.
//  Parametre: yok. Return: yok.
// ============================================================
void returnToOS() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(20, 60);
    tft.print("Ana Menuye Donuluyor...");
    delay(500);
    ESP.restart();
}

// ============================================================
//  EKRAN GEOMETRY'Sİ VE TRIPLE BUFFER (ÜÇLÜ TAMpon)
//  SW/SH: Oyun alanı çözünürlüğü (160x104). Alt 24 satır HUD'a ayrılır.
//  fb[3]: Üç adet framebuffer. PSRAM'de tutulur (her biri ~33KB).
//
//  Triple-buffering mantığı:
//   - fb_render : TaskEngine'in ŞU AN yazdığı buffer (Core 0)
//   - fb_ready  : Yazımı tamamlanmış, TaskDisplay'in beklediği buffer
//   - fb_display: TaskDisplay'in TFT'ye bastığı buffer (Core 1)
//  Her zaman 3 farklı buffer kullanıldığı için iki çekirdek asla aynı
//  belleğe erişmez -> tear-free ve kilitlenmesiz (lock-free-ish) çizim.
//  fb_swap_mutex: bu üç indeksin atomik takası için kullanılan mutex.
//  fb_ready=-1 başlangıçta "hazır frame yok" anlamına gelir.
// ============================================================
#define SW 160
#define SH 104
uint16_t* fb[3];
volatile int8_t fb_render = 0;    // Engine'in yazdığı buffer indeksi
volatile int8_t fb_ready = -1;    // Hazır (tamamlanmış) frame indeksi; -1 = yok
volatile int8_t fb_display = 1;   // Display'in bastığı buffer indeksi
SemaphoreHandle_t fb_swap_mutex;  // Buffer takasını atomik yapan mutex

// zBuffer: Her dikey sütun için duvara olan dik mesafe (perpendicular distance).
// Sprite çiziminde, sprite'ın duvarın arkasında kalıp kalmadığını belirler.
float zBuffer[SW];
// camXTable: Her piksel sütunu için kamera düzlemi koordinatı (-1..+1).
// setup()'ta önceden hesaplanır, raycasting döngüsünde tekrar tekrar hesaplanmaz.
float camXTable[SW];

// ============================================================
//  TEXTURE ATLAS SİSTEMİ
//  TEX_W/TEX_H: Her texture 64x64 piksel (klasik DOOM boyutu).
//  MAX_TEX: Maksimum 50 adet texture slot'u (duvarlar, düşmanlar, silahlar, eşyalar).
//  tex[]: PSRAM'de ayrılan texture buffer'ları. İndeks = texture ID.
//         Örn: tex[1]=duvar1, tex[22]=zombi duruş karesi, tex[14]=tabanca bekle.
//  sdReady: SD kartın başarıyla başlatıldığını belirten bayrak.
// ============================================================
#define TEX_W 64
#define TEX_H 64
#define MAX_TEX 50 
uint16_t *tex[MAX_TEX];
bool sdReady = false; 

// 0xDEADBEEF Bootloader mekanizması iptal (Standalone Game)

// Çift Çekirdek Güvenliği için Mutex Kilidi ve Task İşaretçileri
// gameMutex: TaskEngine oyun verisini güncellerken TaskRadar'ın aynı veriyi
// okumasını engeller. Radar, kopyalama öncesi mutex'i alır, çakışma olmaz.
SemaphoreHandle_t gameMutex;


// MW/MH: Harita 32x32 kare. Klasik raycasting grid boyutu.
#define MW 32
#define MH 32
// MAP: Aktif seviyenin haritası. 0=boş, 1-5=duvar, 6=kapı, 7=kilitli kapı, 8=çıkış, 31=gizli duvar
uint8_t MAP[MH][MW];

// ============================================================
//  RGB_FIX — Renk kanalı sıralamasını TFT'ye göre düzeltir.
//  TFT_eSPI'nin color565 fonksiyonu R,G,B sıralı bekler; ancak bu ekran
//  TFT_BGR_ORDER modunda çalıştığı için R ve B kanalları yer değiştirmiş
//  görünür. Bu fonksiyon, color565'e (b, g, r) sıralı geçirerek R/B swap
//  telafisini yapar. Böylece BMP'lerdeki renkler ekranda doğru görünür.
//  Parametreler: r,g,b (0-255 arası kanal değerleri).
//  Return: 16-bit RGB565 renk değeri.
// ============================================================
uint16_t RGB_FIX(uint8_t r, uint8_t g, uint8_t b) { return tft.color565(b, g, r); }

// HUD Cache Değişkenleri — Sadece değer değiştiğinde ekrana tekrar basmak için
// (örn: ammo 75'te sabitken her frame yeniden çizilmez). lastInfState=INF modu bayrağı
int lastAmmo = -1, lastHp = -1, lastArmor = -1, lastFps = -1;
bool lastInfState = false;

// --- V2.1: Ses Ayarı --- (NVS'ten okunur, kapatılabilir)
bool soundEnabled = true;

// ============================================================
//  playSound — Buzzer üzerinden basit ton üretir.
//  freq: frekans (Hz), dur: süre (ms). Ses kapalıysa hiçbir şey yapmaz.
//  Return: yok.
// ============================================================
void playSound(uint16_t freq, uint32_t dur) {
    if (soundEnabled) { tone(BUZZER, freq, dur); }
}

// ============================================================
//  HARİTA DİZİLERİ (3 Seviye)
//  Her biri 32x32 kare. Değer anlamları:
//   0  = boş/zemin (yürünebilir)
//   1-5= duvar çeşitleri (texture 1-5'e karşılık gelir)
//   6  = normal kapı (BTN_B ile açılır, haritadan silinir)
//   7  = kilitli kapı (anahtar gerektirir, hasKey ile açılır)
//   8  = çıkış kapısı (sonraki seviyeye geçiş)
//   31 = gizli duvar (gizli bölüm açılınca 0'a dönüşür)
// ============================================================
// Harita Dizileri
const uint8_t LEVEL1[MH][MW] = {
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,0,0,0,6,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
{1,1,1,1,0,0,0,1,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1},
{1,0,0,1,1,1,1,1,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,7,1,1,0,1,1},
{1,0,0,0,0,1,1,1,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,0,1},
{1,0,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,1,1,1,1,6,1,1,6,1,1,1,1,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,6,0,0,0,0,6,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,1,1,1,1,1,6,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,6,0,1,1,0,7,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,1,1,1,1,6,1,1,6,1,1,1,1,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,1},
{1,0,0,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,1},
{1,0,0,1,1,1,0,0,0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0,0,0,1,1,1,0,8,1},
{1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// LEVEL2: "DEIMOS — The Shores of Hell" teması. Daha açık alanlar ve 2=tuğla duvar kullanımı.
const uint8_t LEVEL2[MH][MW] = {
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,6,6,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
{1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1},
{1,1,8,1,1,1,0,0,0,0,0,0,2,2,2,0,0,2,2,2,0,0,0,0,0,0,1,1,1,0,0,1},
{1,0,0,0,0,1,1,1,0,0,0,0,2,0,0,0,0,0,0,2,0,0,0,0,1,1,1,0,0,0,0,1},
{1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,2,2,2,2,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,2,2,2,2,0,0,0,0,1},
{1,0,0,0,2,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,2,0,0,0,0,1},
{1,0,0,0,2,0,0,0,0,0,0,0,0,0,1,1,6,0,0,0,0,0,0,0,0,0,2,0,0,0,0,1},
{1,0,0,0,2,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,2,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,2,0,0,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,2,0,0,0,0,1},
{1,0,0,0,2,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,2,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,0,2,2,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,1,7,1,0,0,0,0,2,2,2,2,0,0,1,1,1,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,2,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1},
{1,0,0,1,1,1,1,1,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,1,7,1,0,0,0,0,1},
{1,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,1},
{1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1},
{1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};


// LEVEL3: "CEHENNEM — Inferno" teması. Labirentvari koridorlar, 8=çıkış kapısı ortada.
const uint8_t LEVEL3[MH][MW] = {
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,1,1,1,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,0,0,1},
{1,0,0,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,0,0,1},
{1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1},
{1,0,0,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,1,0,0,1},
{1,0,0,1,0,0,1,0,0,0,0,0,1,1,1,0,0,1,1,1,0,0,0,0,0,1,0,0,1,0,0,1},
{1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,1},
{1,0,0,0,0,0,1,0,0,1,1,0,0,0,1,0,0,1,0,0,0,1,1,0,0,1,0,0,0,0,0,1},
{1,0,0,0,0,0,1,0,0,1,0,0,0,0,1,0,0,1,0,0,0,0,1,0,0,1,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,6,6,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,1,0,0,1,0,0,0,0,1,0,0,1,0,0,0,0,1,0,0,1,0,0,0,0,0,1},
{1,0,0,0,0,0,1,0,0,1,1,0,0,0,1,0,0,1,0,0,0,1,1,0,0,1,0,0,0,0,0,1},
{1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,1},
{1,0,0,1,0,0,1,0,0,0,0,0,1,1,1,0,0,1,1,1,0,0,0,0,0,1,0,0,1,0,0,1},
{1,0,0,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,1,0,0,1},
{1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1},
{1,0,0,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,0,0,1},
{1,0,0,1,1,1,1,1,0,0,1,1,1,1,1,8,8,1,1,1,1,1,0,0,1,1,1,1,1,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// ============================================================
//  AnimState — Düşman/sprite animasyon durum makinesi (state machine).
//  IDLE   : duruyor (oyuncuyu henüz fark etmedi)
//  WALK   : oyuncuya doğru yürüyor/kovaliyor (chase)
//  ATTACK : saldiriyor (isrik, ates veya yakin dovus)
//  HIT    : hasar aldi, kisa sure geri tepiyor
//  DYING  : olum animasyonu oynuyor (2 kare)
//  DEAD   : oldu, ceset yerde sabit kalir
// ============================================================
enum AnimState : uint8_t { ANIM_IDLE=0, ANIM_WALK=1, ANIM_ATTACK=2, ANIM_HIT=3, ANIM_DYING=4, ANIM_DEAD=5 };

// ============================================================
//  Sprite — Hem dusmanlari hem esyalari hem mermileri temsil eden genel yapi.
//  x,y     : harita uzerindeki kayan nokta konumu
//  type    : sprite turu (5=zombi, 14=baron, 17=pinky, 15=varil, 9=mermi kutusu,
//            10=can kutusu, 11=anahtar, 43=armor, 12=dusman mermisi, 13=sekme mermisi)
//  state   : -1=gizli (gizli duvar acilana dek), 0=olu/inaktif, >=1=aktif
//  dx,dy   : mermiler icin hareket yonu vektoru
//  hp      : dusmanin cani (esyalarda 0)
//  animState/animFrame/animTimer: animasyon durum makinesi ve kare zamanlamasi
//  lastFireTime: her dusmanin kendi mermi sayaci (spam'i onler)
// ============================================================
struct Sprite { 
    float x, y; 
    int type; 
    int state;   
    float dx, dy; 
    int hp;      
    uint8_t animState;
    uint8_t animFrame;
    uint32_t animTimer;
    uint32_t lastFireTime; // YENİ EKLENDİ: Her düşmanın kendi mermi sayacı
};

// Maksimum 140 sprite ayni anda bulunabilir (dusmanlar + esyalar + mermiler)
#define NUM_SPRITES 140
Sprite sprites[NUM_SPRITES];

// --- OYUNCU VE KAMERA DEĞİŞKENLERİ ---
// px,py : oyuncunun harita konumu (kayan nokta)
// dirX,dirY : oyuncunun baktığı yön vektörü (birim vektör)
// planeX,planeY : kamera düzlemi vektörü (FOV'u belirler, 0.66 = ~90 derece)
float px = 1.5, py = 1.5; 
float dirX = 1, dirY = 0;
float planeX = 0, planeY = 0.66;
// Joystick kalibrasyon merkezi (başlangıçta 2048 = 12-bit ADC orta değeri)
int joyCenterX = 2048, joyCenterY = 2048;
// TaskJoy tarafından sürekli güncellenen ham analog değerler (volatile: çift çekirdek)
volatile int joyRawX = 0, joyRawY = 0;

// --- ZAMANLAYICI ve İSTATİSTİK ---
// lastFrame: delta time (dt) hesabı için önceki frame zamanı
// fpsTimer/frameCount/fps: saniyede frame sayacı
uint32_t lastFrame = 0, fpsTimer = 0;
int fps = 0, frameCount = 0;
// Oyuncu istatistikleri: hp=can, ammo=mermi, armor=zırh
int hp = 100, ammo = 75, armor = 0; 
int currentLevel = 1; // Aktif seviye (1-3)
bool hasKey = false;  // Kilitli kapıları açmak için anahtar bayrağı
// Çeşitli cooldown/zamanlama sayaçları:
// fireT=son atış, lastDamageTime=son hasar (kırmızı ekran flash'ı), sonKullanma=son BTN_B kullanımı
// shieldSawTime=son kalkan-testere vuruşu, lastEnemyFire=son düşman atışı, shieldStartTime=kalkan açılış anı
uint32_t fireT = 0, lastDamageTime = 0, sonKullanma = 0;
uint32_t shieldSawTime = 0, lastEnemyFire = 0;
uint32_t shieldStartTime = 0;
bool lastShieldState = false;   // Kalkan (BTN_D) önceki durumu (edge detection için)
uint32_t meleeTimer = 0;        // Yakin dovus (B) animasyon zamanlayicisi

// --- MASTER OS DURUM MAKİNESİ ---
// Oyunun genel durumunu belirler. TaskEngine bu duruma göre farklı dallar çalıştırır.
enum GameState {
    STATE_BOOT,        // Açılış: TITLEPIC göster
    STATE_MASTER_MENU, // Ana OS menüsü (Doom, Snake, Bootloader)
    STATE_MENU,        // Doom iç menüsü (Yeni Oyun / Seviye Seç)
    STATE_PLAYING,     // Doom oynanıyor
    STATE_SNAKE,       // Yılan oyunu
    STATE_BOOTLOADER,  // SD'den .bin flash ekranı
    STATE_PAUSE        // Doom Duraklatıldı
};
volatile GameState gameState = STATE_BOOT;

// --- TITLE / MENÜ DEĞİŞKENLERİ ---
bool titleDrawn = false;
uint32_t bootStartTime = 0;
uint16_t *titlePicBuf = nullptr;  // TITLEPIC.bmp'nin PSRAM'deki tam kopyası (160x128)

int masterMenuSel = 0;    // 0=DOOM, 1=SNAKE, 2=BOOTLOADER
bool masterMenuDrawn = false;

// DOOM Menü değişkenleri
int menuSelection = 0;       // Ana menüde seçili öge (0=Yeni Oyun, 1=Seviye Seç)
int levelSelectIdx = 0;      // Seviye seçim ekranında seçili seviye (0-2)
bool inLevelSelect = false;  // Seviye seçim alt-menüsünde miyiz?
bool menuDrawn = false;      // Menü tamamen yeniden çizilsin mi? (önbellek bayrağı)

// SNAKE Oyunu değişkenleri
// SNAKE_W/H: yılan oyun alanı hücre sayısı. CS=5 piksel/hücre ile ekrana çizilir.
#define SNAKE_W 32
#define SNAKE_H 26  
struct SnakeCell { int8_t x, y; };
#define SNAKE_MAX 200          // Maksimum yılan uzunluğu
SnakeCell snakeBody[SNAKE_MAX];
int snakeLen = 3;
int8_t snakeDirX = 1, snakeDirY = 0;
int8_t snakeFoodX = 15, snakeFoodY = 13;
uint32_t snakeLastMove = 0;
int snakeScore = 0;
bool snakeDead = false;
bool snakeDrawn = false;

// weaponType: 0=tabanca (hızlı, tek hedef), 1=pompalı (yavaş, çoklu hedef, 3 mermi)
int weaponType = 0; 

// ============================================================
//  DÜŞMAN ANİMASYON KARE TABLOSU
//  Her satır [animState] -> {kare0, kare1} texture ID çifti.
//  animFrame & 1 ile 0/1 arasında değişerek yürüme vb. iki kare animasyon sağlar.
//  ZOMBIE(5)=z_, PINKY(17)=p_, BARON(14)=b_, VARIL(15)=v_ BMP'lerine karşılık gelir.
// ============================================================
const uint8_t ZOMBIE_FRAMES[][2] = {{22,22}, {22,23}, {24,24}, {22,22}, {25,26}, {26,26}};
const uint8_t PINKY_FRAMES[][2]  = {{27,27}, {28,29}, {30,44}, {27,27}, {32,33}, {33,33}};
const uint8_t BARON_FRAMES[][2]  = {{34,34}, {34,35}, {36,36}, {37,37}, {38,39}, {39,39}};
const uint8_t VARIL_FRAMES[][2]  = {{40,40}, {40,40}, {40,40}, {40,40}, {41,42}, {42,42}};

// ============================================================
//  getEnemyTexID — Sprite'ın tür ve animasyon durumuna göre texture ID döndürür.
//  spriteIndex: sprites[] dizisindeki indis.
//  Return: çizimde kullanılacak tex[] indeksi. Düşman değilse type直接 döner.
// ============================================================
int getEnemyTexID(int spriteIndex) {
    Sprite &s = sprites[spriteIndex];
    if(s.type == 5) return ZOMBIE_FRAMES[s.animState][s.animFrame & 1];
    if(s.type == 17) return PINKY_FRAMES[s.animState][s.animFrame & 1];
    if(s.type == 14) return BARON_FRAMES[s.animState][s.animFrame & 1];
    if(s.type == 15) return VARIL_FRAMES[s.animState][s.animFrame & 1];
    return s.type;
}

// ============================================================
//  initSprite — Belirtilen indeksteki sprite'ı başlatır (konum, tür, can atar).
//  i: sprites[] indis. x,y: harita konumu. type: sprite türü. state: aktiflik.
//  Düşman türlerine göre hp ayarlanır: zombi=30, pinky=60, baron=150, varil=10.
//  Return: yok.
// ============================================================
void initSprite(int i, float x, float y, int type, int state) {
    sprites[i].x = x; sprites[i].y = y; sprites[i].type = type; sprites[i].state = state;
    sprites[i].dx = 0; sprites[i].dy = 0;
    sprites[i].animState = ANIM_IDLE; sprites[i].animFrame = 0; sprites[i].animTimer = 0;
    sprites[i].lastFireTime = 0; // YENİ EKLENDİ
    
    if(type==5) sprites[i].hp = 30;         
    else if(type==17) sprites[i].hp = 60;   
    else if(type==14) sprites[i].hp = 150;  
    else if(type==15) sprites[i].hp = 10;   
    else sprites[i].hp = 0;
}

// ============================================================
//  drawStaticHUD — Oyun altındaki sabit HUD çerçevesini tek seferlik çizer.
//  160x24 piksellik gri bant; AMMO/HEALTH/ARMOR etiketleri ve ayraç çizgileri.
//  Dinamik değerler (sayılar) TaskDisplay'de yalnızca değişince güncellenir.
//  Return: yok.
// ============================================================
void drawStaticHUD() {
  int hy = SH; 
  tft.fillRect(0, hy, 160, 24, RGB_FIX(40, 40, 40));
  tft.drawFastHLine(0, hy, 160, RGB_FIX(100, 100, 100)); 
  tft.drawFastHLine(0, 127, 160, RGB_FIX(10, 10, 10));   
  tft.drawFastVLine(50, hy, 24, RGB_FIX(20, 20, 20));
  tft.drawFastVLine(51, hy, 24, RGB_FIX(100, 100, 100));
  tft.drawFastVLine(105, hy, 24, RGB_FIX(20, 20, 20));
  tft.drawFastVLine(106, hy, 24, RGB_FIX(100, 100, 100));
  tft.setTextSize(1);
  tft.setTextColor(RGB_FIX(200, 0, 0)); 
  tft.setCursor(12, hy + 4); tft.print("AMMO");
  tft.setCursor(62, hy + 4); tft.print("HEALTH");
  tft.setCursor(118, hy + 4); tft.print("ARMOR");
}

// ============================================================
//  loadLevel — Seçilen seviyeyi yükler: haritayı kopyalar, oyuncuyu ve
//  tüm sprite'ları (düşmanlar, eşyalar, variller) başlangıç konumlarına yerleştirir.
//  level: 1, 2 veya 3. Her seviyenin kendi haritası (LEVEL1/2/3) ve sprite listesi vardır.
//  HUD önbelleğini sıfırlar (yeni seviyede değerler yeniden çizilsin diye).
//  Return: yok.
// ============================================================
void loadLevel(int level) {
    // HUD'u zorla yenilemek için resetle
    lastAmmo = -1; lastHp = -1; lastArmor = -1; lastFps = -1; lastInfState = false;
    
    hasKey = false; 
    // Tüm sprite'ları temizle (state=0), sonra seviyeye göre yeniden doldur
    for(int i=0; i<NUM_SPRITES; i++) initSprite(i, 0, 0, 0, 0); 
    // Seviye haritasını aktif MAP dizisine kopyala
    for(int y=0; y<MH; y++) for(int x=0; x<MW; x++) { 
        if(level == 1) MAP[y][x] = LEVEL1[y][x];
        else if(level == 2) MAP[y][x] = LEVEL2[y][x];
        else if(level == 3) MAP[y][x] = LEVEL3[y][x];
    }
    // --- SEVİYE 1: PHOBOS ---
    // Oyuncu sol-üst köşeye yerleşir; sprite'lar harita boyunca dağıtılır.
    // type kodları: 5=zombi, 14=baron, 17=pinky, 15=varil, 9=mermi, 10=can,
    //               11=anahtar, 43=armor, 10=can
    if(level == 1) {
    px = 5.5; py = 1.5; dirX = 1; dirY = 0; planeX = 0; planeY = 0.66;

initSprite(0, 13.5, 1.5, 5, 1);
initSprite(1, 17.5, 1.5, 15, 1);
initSprite(2, 18.5, 1.5, 17, 1);
initSprite(3, 22.5, 1.5, 17, 1);
initSprite(4, 25.5, 1.5, 14, 1);
initSprite(5, 29.5, 1.5, 11, 1);
initSprite(6, 6.5, 2.5, 9, 1);
initSprite(7, 2.5, 3.5, 9, 1);
initSprite(8, 12.5, 3.5, 15, 1);
initSprite(9, 29.5, 3.5, 43, 1);
initSprite(10, 1.5, 5.5, 15, 1);
initSprite(11, 10.5, 5.5, 5, 1);
initSprite(12, 13.5, 5.5, 5, 1);
initSprite(13, 25.5, 5.5, 9, 1);
initSprite(14, 2.5, 6.5, 5, 1);
initSprite(15, 8.5, 6.5, 10, 1);
initSprite(16, 11.5, 6.5, 9, 1);
initSprite(17, 5.5, 8.5, 5, 1);
initSprite(18, 10.5, 8.5, 15, 1);
initSprite(19, 21.5, 8.5, 9, 1);
initSprite(20, 2.5, 9.5, 5, 1);
initSprite(21, 5.5, 9.5, 15, 1);
initSprite(22, 8.5, 9.5, 5, 1);
initSprite(23, 14.5, 9.5, 9, 1);
initSprite(24, 17.5, 9.5, 43, 1);
initSprite(25, 25.5, 9.5, 17, 1);
initSprite(26, 26.5, 9.5, 15, 1);
initSprite(27, 28.5, 9.5, 15, 1);
initSprite(28, 29.5, 9.5, 17, 1);
initSprite(29, 5.5, 10.5, 5, 1);
initSprite(30, 25.5, 10.5, 15, 1);
initSprite(31, 29.5, 10.5, 15, 1);
initSprite(32, 12.5, 11.5, 9, 1);
initSprite(33, 14.5, 11.5, 9, 1);
initSprite(34, 19.5, 11.5, 15, 1);
initSprite(35, 22.5, 11.5, 5, 1);
initSprite(36, 27.5, 11.5, 14, 1);
initSprite(37, 2.5, 12.5, 5, 1);
initSprite(38, 19.5, 12.5, 5, 1);
initSprite(39, 25.5, 12.5, 15, 1);
initSprite(40, 29.5, 12.5, 15, 1);
initSprite(41, 21.5, 13.5, 5, 1);
initSprite(42, 25.5, 13.5, 17, 1);
initSprite(43, 26.5, 13.5, 15, 1);
initSprite(44, 28.5, 13.5, 15, 1);
initSprite(45, 29.5, 13.5, 17, 1);
initSprite(46, 1.5, 14.5, 15, 1);
initSprite(47, 6.5, 14.5, 17, 1);
initSprite(48, 12.5, 14.5, 9, 1);
initSprite(49, 14.5, 14.5, 9, 1);
initSprite(50, 19.5, 14.5, 10, 1);
initSprite(51, 5.5, 16.5, 15, 1);
initSprite(52, 8.5, 16.5, 9, 1);
initSprite(53, 11.5, 16.5, 15, 1);
initSprite(54, 23.5, 16.5, 15, 1);
initSprite(55, 25.5, 16.5, 5, 1);
initSprite(56, 30.5, 16.5, 5, 1);
initSprite(57, 6.5, 18.5, 5, 1);
initSprite(58, 11.5, 18.5, 14, 1);
initSprite(59, 28.5, 18.5, 5, 1);
initSprite(60, 1.5, 19.5, 17, 1);
initSprite(61, 19.5, 20.5, 9, 1);
initSprite(62, 23.5, 20.5, 5, 1);
initSprite(63, 9.5, 21.5, 17, 1);
initSprite(64, 30.5, 21.5, 15, 1);
initSprite(65, 1.5, 22.5, 15, 1);
initSprite(66, 4.5, 22.5, 17, 1);
initSprite(67, 18.5, 22.5, 11, 1);
initSprite(68, 27.5, 22.5, 5, 1);
initSprite(69, 13.5, 23.5, 10, 1);
initSprite(70, 18.5, 23.5, 9, 1);
initSprite(71, 9.5, 24.5, 9, 1);
initSprite(72, 22.5, 24.5, 15, 1);
initSprite(73, 29.5, 24.5, 17, 1);
initSprite(74, 7.5, 25.5, 17, 1);
initSprite(75, 18.5, 25.5, 9, 1);
initSprite(76, 20.5, 25.5, 43, 1);
initSprite(77, 2.5, 26.5, 17, 1);
initSprite(78, 21.5, 26.5, 10, 1);
initSprite(79, 8.5, 27.5, 15, 1);
initSprite(80, 1.5, 28.5, 43, 1);
initSprite(81, 2.5, 28.5, 9, 1);
initSprite(82, 7.5, 28.5, 5, 1);
initSprite(83, 12.5, 28.5, 5, 1);
initSprite(84, 21.5, 28.5, 14, 1);
initSprite(85, 25.5, 28.5, 15, 1);
initSprite(86, 22.5, 29.5, 15, 1);
initSprite(87, 9.5, 30.5, 17, 1);
initSprite(88, 10.5, 30.5, 15, 1);
initSprite(89, 13.5, 30.5, 14, 1);
initSprite(90, 14.5, 30.5, 15, 1);
initSprite(91, 18.5, 30.5, 15, 1);
initSprite(92, 19.5, 30.5, 14, 1);
initSprite(93, 24.5, 30.5, 14, 1);

    } else if(level == 2) {
       // --- SEVİYE 2: DEIMOS ---
       px = 15.5; py = 1.5; dirX = 1; dirY = 0; planeX = 0; planeY = 0.66;

initSprite(0, 14.5, 2.5, 43, 1);
initSprite(1, 17.5, 2.5, 9, 1);
initSprite(2, 3.5, 4.5, 43, 1);
initSprite(3, 6.5, 4.5, 5, 1);
initSprite(4, 25.5, 4.5, 5, 1);
initSprite(5, 28.5, 4.5, 43, 1);
initSprite(6, 8.5, 5.5, 5, 1);
initSprite(7, 13.5, 5.5, 9, 1);
initSprite(8, 23.5, 5.5, 5, 1);
initSprite(9, 6.5, 6.5, 15, 1);
initSprite(10, 25.5, 6.5, 15, 1);
initSprite(11, 4.5, 7.5, 15, 1);
initSprite(12, 13.5, 7.5, 10, 1);
initSprite(13, 18.5, 7.5, 10, 1);
initSprite(14, 23.5, 7.5, 9, 1);
initSprite(15, 9.5, 8.5, 15, 1);
initSprite(16, 22.5, 8.5, 15, 1);
initSprite(17, 1.5, 9.5, 15, 1);
initSprite(18, 7.5, 9.5, 15, 1);
initSprite(19, 13.5, 9.5, 17, 1);
initSprite(20, 18.5, 9.5, 17, 1);
initSprite(21, 24.5, 9.5, 15, 1);
initSprite(22, 28.5, 9.5, 5, 1);
initSprite(23, 3.5, 10.5, 5, 1);
initSprite(24, 23.5, 10.5, 9, 1);
initSprite(25, 30.5, 10.5, 14, 1);
initSprite(26, 6.5, 11.5, 17, 1);
initSprite(27, 9.5, 11.5, 5, 1);
initSprite(28, 10.5, 11.5, 9, 1);
initSprite(29, 12.5, 11.5, 15, 1);
initSprite(30, 14.5, 11.5, 5, 1);
initSprite(31, 17.5, 11.5, 5, 1);
initSprite(32, 19.5, 11.5, 15, 1);
initSprite(33, 23.5, 11.5, 5, 1);
initSprite(34, 29.5, 11.5, 14, 1);
initSprite(35, 11.5, 12.5, 15, 1);
initSprite(36, 13.5, 12.5, 9, 1);
initSprite(37, 26.5, 12.5, 14, 1);
initSprite(38, 2.5, 13.5, 17, 1);
initSprite(39, 14.5, 13.5, 5, 1);
initSprite(40, 16.5, 13.5, 5, 1);
initSprite(41, 5.5, 14.5, 15, 1);
initSprite(42, 7.5, 14.5, 14, 1);
initSprite(43, 10.5, 14.5, 17, 1);
initSprite(44, 15.5, 14.5, 9, 1);
initSprite(45, 24.5, 14.5, 5, 1);
initSprite(46, 25.5, 14.5, 11, 1);
initSprite(47, 28.5, 14.5, 17, 1);
initSprite(48, 30.5, 14.5, 15, 1);
initSprite(49, 1.5, 15.5, 15, 1);
initSprite(50, 5.5, 15.5, 9, 1);
initSprite(51, 13.5, 15.5, 43, 1);
initSprite(52, 17.5, 15.5, 9, 1);
initSprite(53, 25.5, 15.5, 9, 1);
initSprite(54, 30.5, 15.5, 15, 1);
initSprite(55, 13.5, 16.5, 10, 1);
initSprite(56, 30.5, 16.5, 15, 1);
initSprite(57, 5.5, 17.5, 5, 1);
initSprite(58, 9.5, 17.5, 17, 1);
initSprite(59, 19.5, 17.5, 9, 1);
initSprite(60, 28.5, 17.5, 17, 1);
initSprite(61, 30.5, 17.5, 15, 1);
initSprite(62, 2.5, 18.5, 17, 1);
initSprite(63, 17.5, 18.5, 9, 1);
initSprite(64, 23.5, 18.5, 5, 1);
initSprite(65, 25.5, 18.5, 15, 1);
initSprite(66, 13.5, 19.5, 17, 1);
initSprite(67, 16.5, 19.5, 5, 1);
initSprite(68, 18.5, 19.5, 17, 1);
initSprite(69, 25.5, 19.5, 5, 1);
initSprite(70, 5.5, 21.5, 5, 1);
initSprite(71, 15.5, 21.5, 14, 1);
initSprite(72, 30.5, 21.5, 14, 1);
initSprite(73, 1.5, 22.5, 15, 1);
initSprite(74, 2.5, 22.5, 14, 1);
initSprite(75, 7.5, 22.5, 9, 1);
initSprite(76, 11.5, 22.5, 15, 1);
initSprite(77, 20.5, 22.5, 15, 1);
initSprite(78, 24.5, 22.5, 15, 1);
initSprite(79, 28.5, 22.5, 5, 1);
initSprite(80, 10.5, 23.5, 9, 1);
initSprite(81, 11.5, 23.5, 17, 1);
initSprite(82, 16.5, 23.5, 11, 1);
initSprite(83, 20.5, 23.5, 17, 1);
initSprite(84, 23.5, 24.5, 9, 1);
initSprite(85, 6.5, 25.5, 15, 1);
initSprite(86, 16.5, 25.5, 14, 1);
initSprite(87, 8.5, 26.5, 17, 1);
initSprite(88, 20.5, 26.5, 17, 1);
initSprite(89, 30.5, 26.5, 9, 1);
initSprite(90, 12.5, 27.5, 5, 1);
initSprite(91, 25.5, 27.5, 15, 1);

    } else if (level == 3) {
    // --- SEVİYE 3: CEHENNEM ---
    px = 15.5; py = 17.5; dirX = 1; dirY = 0; planeX = 0; planeY = 0.66;

initSprite(0, 1.5, 1.5, 10, 1);
initSprite(1, 4.5, 1.5, 5, 1);
initSprite(2, 8.5, 1.5, 14, 1);
initSprite(3, 11.5, 1.5, 5, 1);
initSprite(4, 13.5, 1.5, 14, 1);
initSprite(5, 18.5, 1.5, 14, 1);
initSprite(6, 20.5, 1.5, 5, 1);
initSprite(7, 23.5, 1.5, 14, 1);
initSprite(8, 27.5, 1.5, 5, 1);
initSprite(9, 30.5, 1.5, 10, 1);
initSprite(10, 1.5, 3.5, 5, 1);
initSprite(11, 8.5, 3.5, 15, 1);
initSprite(12, 9.5, 3.5, 9, 1);
initSprite(13, 22.5, 3.5, 9, 1);
initSprite(14, 23.5, 3.5, 15, 1);
initSprite(15, 30.5, 3.5, 5, 1);
initSprite(16, 4.5, 4.5, 43, 1);
initSprite(17, 9.5, 4.5, 17, 1);
initSprite(18, 22.5, 4.5, 17, 1);
initSprite(19, 23.5, 4.5, 5, 1);
initSprite(20, 27.5, 4.5, 43, 1);
initSprite(21, 1.5, 5.5, 14, 1);
initSprite(22, 7.5, 5.5, 5, 1);
initSprite(23, 15.5, 5.5, 15, 1);
initSprite(24, 16.5, 5.5, 15, 1);
initSprite(25, 30.5, 5.5, 14, 1);
initSprite(26, 15.5, 6.5, 5, 1);
initSprite(27, 16.5, 6.5, 5, 1);
initSprite(28, 7.5, 7.5, 5, 1);
initSprite(29, 8.5, 7.5, 9, 1);
initSprite(30, 9.5, 7.5, 15, 1);
initSprite(31, 11.5, 7.5, 10, 1);
initSprite(32, 20.5, 7.5, 10, 1);
initSprite(33, 22.5, 7.5, 15, 1);
initSprite(34, 23.5, 7.5, 9, 1);
initSprite(35, 24.5, 7.5, 5, 1);
initSprite(36, 1.5, 8.5, 14, 1);
initSprite(37, 2.5, 8.5, 9, 1);
initSprite(38, 3.5, 8.5, 15, 1);
initSprite(39, 11.5, 8.5, 17, 1);
initSprite(40, 13.5, 8.5, 15, 1);
initSprite(41, 18.5, 8.5, 15, 1);
initSprite(42, 20.5, 8.5, 17, 1);
initSprite(43, 28.5, 8.5, 15, 1);
initSprite(44, 29.5, 8.5, 9, 1);
initSprite(45, 30.5, 8.5, 14, 1);
initSprite(46, 10.5, 10.5, 5, 1);
initSprite(47, 21.5, 10.5, 5, 1);
initSprite(48, 1.5, 11.5, 5, 1);
initSprite(49, 6.5, 11.5, 15, 1);
initSprite(50, 10.5, 11.5, 9, 1);
initSprite(51, 21.5, 11.5, 9, 1);
initSprite(52, 25.5, 11.5, 15, 1);
initSprite(53, 30.5, 11.5, 5, 1);
initSprite(54, 6.5, 12.5, 9, 1);
initSprite(55, 25.5, 12.5, 9, 1);
initSprite(56, 14.5, 13.5, 9, 1);
initSprite(57, 17.5, 13.5, 9, 1);
initSprite(58, 8.5, 14.5, 5, 1);
initSprite(59, 13.5, 14.5, 9, 1);
initSprite(60, 18.5, 14.5, 9, 1);
initSprite(61, 24.5, 14.5, 5, 1);
initSprite(62, 26.5, 14.5, 15, 1);
initSprite(63, 2.5, 15.5, 5, 1);
initSprite(64, 9.5, 15.5, 15, 1);
initSprite(65, 4.5, 16.5, 15, 1);
initSprite(66, 29.5, 16.5, 5, 1);
initSprite(67, 6.5, 17.5, 5, 1);
initSprite(68, 13.5, 17.5, 9, 1);
initSprite(69, 18.5, 17.5, 9, 1);
initSprite(70, 24.5, 17.5, 5, 1);
initSprite(71, 27.5, 17.5, 15, 1);
initSprite(72, 14.5, 18.5, 9, 1);
initSprite(73, 17.5, 18.5, 9, 1);
initSprite(74, 6.5, 19.5, 9, 1);
initSprite(75, 25.5, 19.5, 9, 1);
initSprite(76, 1.5, 20.5, 5, 1);
initSprite(77, 6.5, 20.5, 15, 1);
initSprite(78, 10.5, 20.5, 9, 1);
initSprite(79, 21.5, 20.5, 9, 1);
initSprite(80, 25.5, 20.5, 15, 1);
initSprite(81, 30.5, 20.5, 5, 1);
initSprite(82, 10.5, 21.5, 5, 1);
initSprite(83, 21.5, 21.5, 5, 1);
initSprite(84, 13.5, 22.5, 15, 1);
initSprite(85, 18.5, 22.5, 15, 1);
initSprite(86, 1.5, 23.5, 14, 1);
initSprite(87, 2.5, 23.5, 9, 1);
initSprite(88, 3.5, 23.5, 15, 1);
initSprite(89, 11.5, 23.5, 17, 1);
initSprite(90, 20.5, 23.5, 17, 1);
initSprite(91, 28.5, 23.5, 15, 1);
initSprite(92, 29.5, 23.5, 9, 1);
initSprite(93, 30.5, 23.5, 14, 1);
initSprite(94, 7.5, 24.5, 5, 1);
initSprite(95, 8.5, 24.5, 9, 1);
initSprite(96, 11.5, 24.5, 10, 1);
initSprite(97, 20.5, 24.5, 10, 1);
initSprite(98, 23.5, 24.5, 9, 1);
initSprite(99, 24.5, 24.5, 5, 1);
initSprite(100, 10.5, 25.5, 15, 1);
initSprite(101, 15.5, 25.5, 5, 1);
initSprite(102, 16.5, 25.5, 5, 1);
initSprite(103, 21.5, 25.5, 15, 1);
initSprite(104, 1.5, 26.5, 14, 1);
initSprite(105, 14.5, 26.5, 15, 1);
initSprite(106, 17.5, 26.5, 15, 1);
initSprite(107, 30.5, 26.5, 14, 1);
initSprite(108, 4.5, 27.5, 43, 1);
initSprite(109, 8.5, 27.5, 5, 1);
initSprite(110, 9.5, 27.5, 17, 1);
initSprite(111, 22.5, 27.5, 17, 1);
initSprite(112, 23.5, 27.5, 5, 1);
initSprite(113, 27.5, 27.5, 43, 1);
initSprite(114, 1.5, 28.5, 5, 1);
initSprite(115, 8.5, 28.5, 15, 1);
initSprite(116, 9.5, 28.5, 9, 1);
initSprite(117, 22.5, 28.5, 9, 1);
initSprite(118, 23.5, 28.5, 15, 1);
initSprite(119, 30.5, 28.5, 5, 1);
initSprite(120, 1.5, 30.5, 10, 1);
initSprite(121, 4.5, 30.5, 5, 1);
initSprite(122, 8.5, 30.5, 14, 1);
initSprite(123, 11.5, 30.5, 5, 1);
initSprite(124, 13.5, 30.5, 14, 1);
initSprite(125, 18.5, 30.5, 14, 1);
initSprite(126, 20.5, 30.5, 5, 1);
initSprite(127, 23.5, 30.5, 14, 1);
initSprite(128, 27.5, 30.5, 5, 1);
initSprite(129, 30.5, 30.5, 10, 1);

}
}

// ============================================================
//  makeTex — Prosedürel (SD kart gerektirmeyen) texture üretir.
//  id=8 : yeşil düz yüzey (zemin bitki örtüsü)
//  id=9,10,11: siyah (BMP ile doldurulacak boş slot, çöp veriyi engeller)
//  id=12: düşman ateş topu (turuncu+ kırmızı halka, dairesel gradient)
//  id=13: sekme (parry) mermisi (camgöbeği mavi top)
//  Return: yok. tex[id][] içerisini yazar.
// ============================================================
void makeTex(int id) {
  // YENİ EKLENDİ: Eğer bu id 8,12,13 değilse varsayılan olarak siyah/boş yap (Çöp veriyi engelle)
  if(id == 9 || id == 10 || id == 11) {
      for(int i=0; i<TEX_W*TEX_H; i++) tex[id][i] = 0x0000;
      return; 
  }

  for(int y=0;y<TEX_H;y++) {
    for(int x=0;x<TEX_W;x++) {
      if(id==8) { tex[8][y*TEX_W+x] = RGB_FIX(0, 150, 0); } 
      else if(id==12) { 
        float dx = x - 32; float dy = y - 32; float r = sqrt(dx*dx + dy*dy);
        if(r < 18) tex[12][y*TEX_W+x] = RGB_FIX(255, 255 - (r*12), 0); 
        else if(r < 26) tex[12][y*TEX_W+x] = RGB_FIX(255, 0, 0);
        else tex[12][y*TEX_W+x] = 0x0000;                               
      }
      else if(id==13) { 
        float dx = x - 32; float dy = y - 32; float r = sqrt(dx*dx + dy*dy);
        if(r < 18) tex[13][y*TEX_W+x] = RGB_FIX(0, 255 - (r*12), 255); 
        else if(r < 26) tex[13][y*TEX_W+x] = RGB_FIX(0, 0, 255);
        else tex[13][y*TEX_W+x] = 0x0000;                               
      }
    }
  }
}

// LittleFS kopyalama fonksiyonu TAMAMEN KALDIRILDI! Artık direkt SD'den PSRAM'a okuyacağız.

// ============================================================
//  loadBMP — SD karttaki bir 24-bit BMP dosyasını tex[id] buffer'ına yükler.
//  filename: SD kök dizinindeki BMP yolu (örn "/duvar1.bmp").
//  id: tex[] slot indeksi. fileBuf: dosyayı tek seferde okumak için PSRAM tamponu.
//  BMP 64x64, 24bpp olmalıdır. bottom-up/top-down satır sırası otomatik algılanır.
//  Siyah pikseller (0,0,0) çok koyu griye çevrilir (siyah = saydam rengi bozar).
//  Mor (r>200, g<55, b>200) pikseller saydam (0x0000) yapılır — chroma-key.
//  Return: başarı=true, hata=false.
// ============================================================
bool loadBMP(const char* filename, int id, uint8_t* fileBuf) {
  if (!tex[id]) return false;
  
  // ÖNEMLİ: Artık LittleFS değil, doğrudan SD karttan okuyoruz!
  File f = SD.open(filename, FILE_READ);
  if (!f) return false;
  
  // Tüm dosyayı TEK SEFERDE PSRAM buffer'ına oku (Sesten hızlı yükleme için)
  size_t fSize = f.size();
  if(fSize > 80000) { f.close(); return false; }
  f.read(fileBuf, fSize);
  f.close();
  
  uint8_t* hdr = fileBuf;
  if (hdr[0]!='B' || hdr[1]!='M') { return false; }
  uint32_t dataOfs = (uint32_t)hdr[10] | ((uint32_t)hdr[11]<<8) | ((uint32_t)hdr[12]<<16)| ((uint32_t)hdr[13]<<24);
  int32_t w = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19]<<8) | ((uint32_t)hdr[20]<<16) | ((uint32_t)hdr[21]<<24));
  int32_t h = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23]<<8) | ((uint32_t)hdr[24]<<16) | ((uint32_t)hdr[25]<<24));
  uint16_t bpp = (uint16_t)(hdr[28] | (hdr[29]<<8));
  bool bottomUp = (h > 0); int32_t absH = bottomUp ? h : -h;
  if (w != TEX_W || absH != TEX_H || bpp != 24) { return false; }
  
  uint32_t rowSz = ((w * 3 + 3) & ~3); 
  for (int row = 0; row < TEX_H; row++) {
    int tgtY = bottomUp ? (TEX_H - 1 - row) : row;
    uint8_t* rowBuf = fileBuf + dataOfs + (row * rowSz);
    for (int x = 0; x < TEX_W; x++) {
      uint8_t b = rowBuf[x*3], g = rowBuf[x*3+1], r = rowBuf[x*3+2];
      uint16_t c = RGB_FIX(r, g, b);
      if (r > 200 && g < 55 && b > 200) c = 0x0000;
      else if (r == 0 && g == 0 && b == 0) c = RGB_FIX(8, 8, 8);
      tex[id][tgtY * TEX_W + x] = c;
    }
  }
  return true;
}

// ============================================================
//  explodeBarrel — Bir varili patlatır ve zincirleme etkiyi uygular.
//  barrelIdx: patlatılan varilin sprite indis. now: güncel zaman (ms).
//  Mantık:
//   - Varil DYING durumuna alınır, patlama sesi çalınır.
//   - 2.5 birim yarıçap içindeki diğer variller REKÜRSİF patlar (zincirleme).
//   - Aynı yarıçaptaki düşmanlara 50 hasar verir (ölürse DYING, yoksa HIT).
//   - Oyuncu kalkansızsa ve yarıçap içindeyse mesafeye göre azalan hasar alır;
//     zırh varsa hasarın yarısı zırhtan emilir.
//  Return: yok.
// ============================================================
void explodeBarrel(int barrelIdx, uint32_t now) {
    if(sprites[barrelIdx].animState == ANIM_DYING || sprites[barrelIdx].animState == ANIM_DEAD) return;
    float bx = sprites[barrelIdx].x, by = sprites[barrelIdx].y;
    sprites[barrelIdx].animState = ANIM_DYING;
    sprites[barrelIdx].animFrame = 0;
    sprites[barrelIdx].animTimer = now;
    playSound(80, 200); 

    for(int i = 0; i < NUM_SPRITES; i++) {
        if(i == barrelIdx || sprites[i].state <= 0 || sprites[i].animState == ANIM_DEAD) continue;
        float dx = sprites[i].x - bx, dy = sprites[i].y - by;
        float distSq = dx*dx + dy*dy;
        if(distSq < 6.25f) { 
            if(sprites[i].type == 15) { explodeBarrel(i, now); } // ZİNCİRLEME PATLAMA
            else if(sprites[i].type == 5 || sprites[i].type == 14 || sprites[i].type == 17) {
                sprites[i].hp -= 50;
                if(sprites[i].hp <= 0) sprites[i].animState = ANIM_DYING;
                else sprites[i].animState = ANIM_HIT;
                sprites[i].animFrame = 0; sprites[i].animTimer = now;
            }
        }
    }
    
    float pdx = px - bx, pdy = py - by, pdistSq = pdx*pdx + pdy*pdy;
    if(pdistSq < 6.25f && !lastShieldState) {
        float pdist = sqrt(pdistSq);
        int dmg = (int)(40 * (1.0 - pdist/2.5));
        if(armor > 0) {
            int absorb = min(armor, (dmg + 1) / 2); 
            armor -= absorb;
            hp -= (dmg - absorb);
        } else {
            hp -= dmg;
        }
        lastDamageTime = now;
    }
}

// ============================================================
//  updateAnimations — Tüm sprite'ların animasyon durum makinesini ilerletir.
//  now: güncel zaman (ms). Her sprite için elapsed süresine göre kare değiştirir.
//  WALK: iki kare arasında belirli aralıklarla geçiş (zombi=200ms, pinky=150ms, baron=300ms).
//  ATTACK: 300ms sonra WALK'a döner; pinky 150ms'de ikinci kareye geçer.
//  HIT: 150ms sonra hp<=0 ise DYING, değilse WALK'a döner.
//  DYING: 250ms'de 2 kare oynar, sonra DEAD'e geçer. Varil 150ms (hızlı patlama).
//  Return: yok.
// ============================================================
void updateAnimations(uint32_t now) {
    const uint16_t WALK_MS[] = {200, 150, 300}; 
    const uint16_t DYING_MS = 250;
    
    for(int i=0; i<NUM_SPRITES; i++) {
        Sprite &s = sprites[i];
        if(s.state <= 0 || s.animState == ANIM_DEAD) continue;

        uint32_t elapsed = now - s.animTimer;
        if(s.type == 5 || s.type == 17 || s.type == 14) {
            int tIdx = (s.type==5)?0 : (s.type==17)?1 : 2;
            if(s.animState == ANIM_WALK) {
                if(elapsed > WALK_MS[tIdx]) { s.animFrame = 1 - s.animFrame; s.animTimer = now; }
            } else if(s.animState == ANIM_ATTACK) {
                if(elapsed > 300) { s.animState = ANIM_WALK; s.animFrame = 0; s.animTimer = now; }
                else if (s.type == 17 && elapsed > 150 && s.animFrame == 0) { s.animFrame = 1; } 
            } else if(s.animState == ANIM_HIT) {
                if(elapsed > 150) {
                    if(s.hp <= 0) { s.animState = ANIM_DYING; s.animFrame = 0; s.animTimer = now; }
                    else { s.animState = ANIM_WALK; s.animFrame = 0; s.animTimer = now; }
                }
            } else if(s.animState == ANIM_DYING) {
                if(s.animFrame == 0 && elapsed > DYING_MS) {
                    s.animFrame = 1; s.animTimer = now; 
                } else if(s.animFrame == 1 && elapsed > DYING_MS) {
                    s.animState = ANIM_DEAD;
                }
            }
        } else if (s.type == 15) { 
            if(s.animState == ANIM_DYING) {
                if(s.animFrame == 0 && elapsed > 150) {
                    s.animFrame = 1; s.animTimer = now;
                } else if(s.animFrame == 1 && elapsed > 150) {
                    // BUG FIX: Hayalet Varil Engellendi (Atomik Geçiş)
                    s.animState = ANIM_DEAD;
                    s.state = 0; 
                }
            }
        }
    }
}


// ==========================================
// ÇEKİRDEK 0: YARDIMCI SİSTEMLER (I2C ve RADAR)
// ==========================================
// ============================================================
//  TaskRadar — OLED ekrana mini radar/harita çizen FreeRTOS task (Core 1, düşük öncelik).
//  Sadece STATE_PLAYING iken çalışır; diğer durumlarda uyuyarak CPU tasarrufu yapar.
//  gameMutex ile oyuncu/harita/sprite verisinin güvenli kopyasını alır (çift çekirdek
//  çakışmasını önler). I2C 400kHz'e sabitlenir ve power-save her döngüde kapatılır
//  (OLED'in kendiliğinden uykuya dalması engellenir).
//  Render: duvarlar=dolu kare, kapılar=boş kare, düşmanlar=kare, eşyalar=piksel,
//  oyuncu=disk + yön çizgisi. 80ms'de bir yenilenir (~12fps radar).
// ============================================================
void TaskRadar(void * pvParameters) {
  oled.begin();
  
  // 1. KORUMA: I2C hattını yüksek hıza sabitleyerek parazitleri eziyoruz
  Wire.setClock(400000); 

  for(;;) {
    // Sadece oyundayken radar çiz, diğer menülerde (OS, Boot, Snake) CPU tasarrufu
    if(gameState != STATE_PLAYING) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
    // 2. KORUMA: Parazit OLED'i uyku moduna soksada, onu her saniye zorla uyandırıyoruz!
    oled.setPowerSave(0); 

    oled.clearBuffer();
    
    // GÜVENLİK DÜZELTMESİ: Kapsam Hatası Önleme
    int bs=2, ox=32;
    
    // Oyun verisinin anlık güvenli kopyasını al (Çift Çekirdek Çakışma Koruması)
    xSemaphoreTake(gameMutex, portMAX_DELAY);
    float r_px = px, r_py = py, r_dirX = dirX, r_dirY = dirY;
    uint8_t r_map[MH][MW];
    memcpy(r_map, MAP, sizeof(MAP));
    Sprite r_sprites[NUM_SPRITES];
    memcpy(r_sprites, sprites, sizeof(sprites));
    xSemaphoreGive(gameMutex);

    for(int y=0;y<MH;y++) {
      for(int x=0;x<MW;x++) {
        if(r_map[y][x] > 0 && r_map[y][x] <= 5) oled.drawBox(ox+x*bs,y*bs,bs-1,bs-1); 
        else if(r_map[y][x] >= 6 && r_map[y][x] <= 8) oled.drawFrame(ox+x*bs,y*bs,bs-1,bs-1); 
        else if(r_map[y][x] == 31) oled.drawFrame(ox+x*bs,y*bs,bs-1,bs-1); 
      }
    }
    for(int i=0; i<NUM_SPRITES; i++) {
       if(r_sprites[i].state >= 1 && r_sprites[i].animState != ANIM_DEAD) {
          if(r_sprites[i].type == 5 || r_sprites[i].type == 14 || r_sprites[i].type == 17) oled.drawBox(ox + (int)r_sprites[i].x*bs, (int)r_sprites[i].y*bs, 2, 2);
          else oled.drawPixel(ox + (int)r_sprites[i].x*bs + 1, (int)r_sprites[i].y*bs + 1); 
       }
    }
    int rx=ox+r_px*bs, ry=r_py*bs;
    oled.drawDisc(rx,ry,1); oled.drawLine(rx, ry, rx + r_dirX * 3, ry + r_dirY * 3); oled.sendBuffer();
    vTaskDelay(pdMS_TO_TICKS(80)); // 80ms I2C dinlenme
  }
}

// ============================================================
//  loadTitlePic — SD'den /TITLEPIC.bmp'i okuyup titlePicBuf'a (160x128) yükler.
//  fileBuf: dosyayı tek seferde okumak için PSRAM tamponu.
//  BMP 160x128, 24bpp olmalı. Renkler RGB_FIX ile düzeltilerek yazılır.
//  Menüde arka plan olarak kullanılır.
//  Return: başarı=true, hata=false.
// ============================================================
bool loadTitlePic(uint8_t* fileBuf) {
    if (!titlePicBuf) return false;
    
    const int TW = 160, TH = 128;
    // ÖNEMLİ: Artık LittleFS değil, doğrudan SD karttan okuyoruz!
    File f = SD.open("/TITLEPIC.bmp", FILE_READ);
    if (!f) return false;
    
    size_t fSize = f.size();
    if(fSize > 80000) { f.close(); return false; }
    f.read(fileBuf, fSize);
    f.close();
    
    uint8_t* hdr = fileBuf;
    if (hdr[0] != 'B' || hdr[1] != 'M') { return false; }
    
    uint32_t dataOfs = hdr[10] | (hdr[11]<<8) | (hdr[12]<<16) | (hdr[13]<<24);
    int32_t w = hdr[18] | (hdr[19]<<8) | (hdr[20]<<16) | (hdr[21]<<24);
    int32_t h = hdr[22] | (hdr[23]<<8) | (hdr[24]<<16) | (hdr[25]<<24);
    uint16_t bpp = hdr[28] | (hdr[29]<<8);
    
    bool bottomUp = (h > 0);
    int32_t absH = bottomUp ? h : -h;
    
    if (w != TW || absH != TH || bpp != 24) { return false; }
    
    uint32_t rowSz = ((w * 3 + 3) & ~3);
    
    for (int row = 0; row < TH; row++) {
        int tgtY = bottomUp ? (TH - 1 - row) : row;
        uint8_t* rowBuf = fileBuf + dataOfs + (row * rowSz);
        
        for (int x = 0; x < TW; x++) {
            uint8_t b = rowBuf[x*3], g = rowBuf[x*3+1], r = rowBuf[x*3+2];
            titlePicBuf[tgtY * TW + x] = RGB_FIX(r, g, b);
        }
    }
    
    return true;
}

// drawIconDoom — Ana menüde DOOM ikonu (artı işareti + daire). sel=seçili mi.
void drawIconDoom(int x, int y, bool sel) {
    uint16_t c = sel ? RGB_FIX(255, 80, 0) : RGB_FIX(80, 20, 0);
    tft.drawCircle(x+12, y+12, 8, c);
    tft.drawFastHLine(x+2, y+12, 8, c);
    tft.drawFastHLine(x+15, y+12, 8, c);
    tft.drawFastVLine(x+12, y+2, 8, c);
    tft.drawFastVLine(x+12, y+15, 8, c);
    tft.drawPixel(x+12, y+12, c);
}

// drawIconSnake — Ana menüde SNAKE ikonu (yılan başı/gövde/kuyruk + göz).
void drawIconSnake(int x, int y, bool sel) {
    uint16_t c = sel ? RGB_FIX(0, 255, 80) : RGB_FIX(0, 80, 20);
    tft.fillRect(x+4, y+14, 16, 6, c); // Kuyruk
    tft.fillRect(x+14, y+6, 6, 14, c); // Govde
    tft.fillRect(x+6, y+6, 10, 6, c);  // Bas
    tft.drawPixel(x+8, y+8, TFT_BLACK); // Goz
}

// drawIconSystem — Ana menüde sistem/SD kart ikonu (kart + çentik).
void drawIconSystem(int x, int y, bool sel) {
    uint16_t c = sel ? RGB_FIX(80, 180, 255) : RGB_FIX(20, 60, 100);
    tft.drawRect(x+5, y+4, 14, 16, c);
    tft.drawLine(x+5, y+8, x+9, y+4, c); // Kesik SD Kart kosesi
    tft.fillRect(x+7, y+6, 2, 4, c);
    tft.fillRect(x+10, y+6, 2, 4, c);
    tft.fillRect(x+13, y+6, 2, 4, c);
}

// ============================================================
//  drawMasterMenu — Ana OS menüsünü (DOOM/SNAKE) TFT'ye çizer.
//  fullRedraw: tam ekran yeniden çiz (ilk çizim); false ise sadece kartları güncelle.
//  Her öge: ikon + başlık + alt başlık, seçili öge vurgu renkli kenarlıkla.
//  Return: yok.
// ============================================================
void drawMasterMenu(bool fullRedraw) {
    if (fullRedraw) {
        tft.fillScreen(TFT_BLACK);
        tft.fillRect(0, 0, 160, 22, RGB_FIX(15, 15, 30));
        tft.setTextColor(RGB_FIX(255, 255, 255));
        tft.setTextSize(1);
        tft.setCursor(60, 7);
        tft.print("E-OS");
        tft.drawFastHLine(0, 22, 160, RGB_FIX(80, 180, 255));
        masterMenuDrawn = true;
    }
    
    struct { const char* label; const char* sub; uint16_t color; } items[] = {
        { "DOOM", "FPS Klasigi", RGB_FIX(255, 80, 0) },
        { "SNAKE", "Retro Yilan", RGB_FIX(0, 255, 80) }
    };
    
    int yBase = 32; // Biraz daha ortalayalim
    for (int i = 0; i < 2; i++) {
        bool sel = (i == masterMenuSel);
        int cy = yBase + i * 32;
        
        // Kart arka plani ve kenarligi
        uint16_t bg = sel ? RGB_FIX(30, 30, 30) : TFT_BLACK;
        uint16_t border = sel ? items[i].color : RGB_FIX(40, 40, 40);
        
        tft.fillRect(4, cy, 152, 28, bg);
        tft.drawRect(4, cy, 152, 28, border);
        tft.drawRect(6, cy+2, 24, 24, border); // Ikon kutusu
        
        // Ikonlari ciz
        if (i == 0) drawIconDoom(6, cy+2, sel);
        else if (i == 1) drawIconSnake(6, cy+2, sel);
        else if (i == 2) drawIconSystem(6, cy+2, sel);
        
        // Yazilar
        tft.setTextSize(1);
        tft.setTextColor(sel ? items[i].color : RGB_FIX(120, 120, 120), bg);
        tft.setCursor(38, cy + 5);
        tft.print(items[i].label);
        
        tft.setTextColor(sel ? RGB_FIX(200, 200, 200) : RGB_FIX(80, 80, 80), bg);
        tft.setCursor(38, cy + 16);
        tft.print(items[i].sub);
    }
}

// ============================================================
//  snakeReset — Yılan oyununu başlangıç durumuna getirir.
//  Uzunluk=3, yön=sağa, skor=0, yem rastgele konumlanır. Return: yok.
// ============================================================
void snakeReset() {
    snakeLen = 3;
    snakeDirX = 1; snakeDirY = 0;
    snakeScore = 0; snakeDead = false; snakeDrawn = false;
    for (int i = 0; i < snakeLen; i++) {
        snakeBody[i].x = 5 - i;
        snakeBody[i].y = 5;
    }
    snakeFoodX = (millis() % (SNAKE_W - 2)) + 1;
    snakeFoodY = (millis() % (SNAKE_H - 2)) + 1;
}

// ============================================================
//  snakeDraw — Yılan oyununu TFT'ye çizer. CS=5 piksel/hücre.
//  İlk çizimde ekran temizlenir (snakeDrawn bayrağı ile). Yem turuncu,
//  gövde koyu yeşilden açık yeşile gradyan, baş en açık. Skor altta.
//  Ölünce GAME OVER ekranı + tekrar/çıkış ipuçları. Return: yok.
// ============================================================
void snakeDraw() {
    const int CS = 5; 
    const int OX = 0, OY = 0; 
    
    if (!snakeDrawn) {
        tft.fillScreen(RGB_FIX(10, 10, 10));
        tft.drawFastHLine(0, SNAKE_H * CS, 160, RGB_FIX(0, 150, 0));
        snakeDrawn = true;
    }
    
    tft.setTextSize(1);
    tft.setTextColor(RGB_FIX(0, 200, 0), RGB_FIX(10, 10, 10));
    tft.setCursor(4, SNAKE_H * CS + 4);
    tft.printf("SCORE: %04d", snakeScore);
    
    tft.fillRect(OX + snakeFoodX * CS, OY + snakeFoodY * CS, CS-1, CS-1, RGB_FIX(255, 60, 0));
    
    for (int i = 1; i < snakeLen; i++) {
        uint8_t g = 100 + (i * 100 / snakeLen);
        tft.fillRect(OX + snakeBody[i].x * CS, OY + snakeBody[i].y * CS, CS-1, CS-1, RGB_FIX(0, g, 0));
    }
    tft.fillRect(OX + snakeBody[0].x * CS, OY + snakeBody[0].y * CS, CS-1, CS-1, RGB_FIX(0, 255, 80));
    
    if (snakeDead) {
        tft.setTextSize(2);
        tft.setTextColor(RGB_FIX(255, 0, 0));
        tft.setCursor(20, 55);
        tft.print("GAME OVER");
        tft.setTextSize(1);
        tft.setTextColor(RGB_FIX(180,180,180));
        tft.setCursor(25, 80);
        tft.print("BTN_A: Tekrar");
        tft.setCursor(25, 92);
        tft.print("BTN_B: Cikis");
    }
}

// ============================================================
//  snakeUpdate — Yılan oyununun bir adımını hesaplar (hareket, çarpışma, yeme).
//  now: güncel zaman. Hız uzunluk arttıkça artar (min 80ms).
//  Duvara/kendine çarpınca ölür. Yem yiyince uzunluk +1, skor +10, yem taşınır.
//  Return: yok.
// ============================================================
void snakeUpdate(uint32_t now) {
    if (snakeDead) return;
    
    uint32_t speed = max((uint32_t)80, (uint32_t)(200 - snakeLen * 2));
    if (now - snakeLastMove < speed) return;
    snakeLastMove = now;
    
    const int CS = 5;
    tft.fillRect(snakeBody[snakeLen-1].x * CS, snakeBody[snakeLen-1].y * CS, CS-1, CS-1, RGB_FIX(10,10,10));
    
    for (int i = snakeLen - 1; i > 0; i--) snakeBody[i] = snakeBody[i-1];
    
    snakeBody[0].x += snakeDirX;
    snakeBody[0].y += snakeDirY;
    
    if (snakeBody[0].x < 0 || snakeBody[0].x >= SNAKE_W ||
        snakeBody[0].y < 0 || snakeBody[0].y >= SNAKE_H) {
        snakeDead = true; playSound(100, 500); return;
    }
    
    for (int i = 1; i < snakeLen; i++) {
        if (snakeBody[0].x == snakeBody[i].x && snakeBody[0].y == snakeBody[i].y) {
            snakeDead = true; playSound(100, 500); return;
        }
    }
    
    if (snakeBody[0].x == snakeFoodX && snakeBody[0].y == snakeFoodY) {
        if (snakeLen < SNAKE_MAX) snakeLen++;
        snakeScore += 10;
        playSound(1200, 50);
        do {
            snakeFoodX = (millis() % (SNAKE_W - 2)) + 1;
            snakeFoodY = (millis() % (SNAKE_H - 2)) + 1;
        } while ([&](){
            for(int i=0;i<snakeLen;i++) 
                if(snakeBody[i].x==snakeFoodX && snakeBody[i].y==snakeFoodY) return true;
            return false;
        }());
        tft.fillRect(snakeFoodX * CS, snakeFoodY * CS, CS-1, CS-1, RGB_FIX(255, 60, 0));
    }
}

// Standalone Game - Bootloader iptal edildi

// ==========================================
// ÇEKİRDEK 1: ANA OYUN MOTORU VE GRAFİKLER (SPI)
// ==========================================
// ============================================================
//  TaskEngine — Oyun mantığını hesaplayan FreeRTOS task (Core 0, IRAM_ATTR).
//  IRAM_ATTR: fonksiyonu hızlı iç SRAM'e yerleştirir (flash'tan çalışmaz) -> performans.
//  Sorumluluklar:
//   - STATE_MENU: Doom iç menüsü (Yeni Oyun / Seviye Seç) çizimi ve navigasyon
//   - STATE_PAUSE: duraklatma ekranı
//   - STATE_PLAYING: ana oyun döngüsü:
//       * Delta time (dt) hesabı
//       * Joystick ile kamera dönüşü + WASD/ileri-geri hareket
//       * Butonlar: A=ateş, B=kullan/kapı/yakın dövüş, C=silah değiştir, D=kalkan, JOY_SW=pause
//       * Düşman AI (chase/patrol/ateş), mermi fiziği, varil patlaması
//       * Raycasting render -> framebuffer'a yazar (duvarlar + sprite'lar + silah + HUD flash)
//       * Triple-buffer takası (fb_render -> fb_ready)
//  Bu task bir frame'i tamamladıktan sonra 1ms bekler (vTaskDelay) diğer task'lara yer açar.
//  Parametre: pvParameters (kullanılmıyor). Return: yok (sonsuz döngü).
// ============================================================
void IRAM_ATTR TaskEngine(void * pvParameters) {
  bool joySw_prev = true;
  for(;;) {
      uint32_t current_ms = millis();

      // OS Logic silindi (Standalone DOOM)


      // ==========================================
      // DOOM İÇ MENÜSÜ (STATE_MENU) — Joystick ile gez, A=seç, B=OS'a dön
      // ==========================================
      if(gameState == STATE_MENU) {

          // ── ORTAK DEĞİŞKENLER ─────────────────────────────
          int  jy_menu  = joyRawY - joyCenterY;
          static uint32_t lastMenuMove = 0;
          uint32_t mnow = millis();

          // ── ANA MENÜ ──────────────────────────────────────
          if(!inLevelSelect) {

              // Joystick navigasyon
              if(abs(jy_menu) > 500 && (mnow - lastMenuMove > 250)) {
                  menuSelection = (jy_menu < 0) ? 0 : 1;
                  lastMenuMove  = mnow;
                  playSound(800, 30);
              }

              // ── TAM YENİDEN ÇİZİM (sadece menuDrawn=false iken) ──
              if(!menuDrawn) {
                  // 1) TITLEPIC tüm ekrana bas
                  if(titlePicBuf) tft.pushImage(0, 0, 160, 128, titlePicBuf);
                  else { tft.fillScreen(TFT_BLACK); }

                  // 2) Alt yarı karartma gradientı (24 satır, opacity artan)
                  for(int y = 0; y < 28; y++) {
                      if(y > 10) {
                          uint8_t a8 = min((uint32_t)255, (uint32_t)(y - 10) * 20);
                          uint16_t darkRow = tft.color565(
                              (uint8_t)(10 * (255 - a8) / 255),
                              0,
                              0);
                          tft.drawFastHLine(0, 100 + y, 160, darkRow);
                      }
                  }
                  tft.fillRect(0, 110, 160, 18, tft.color565(0, 0, 0)); // saf siyah alt şerit

                  // 3) Ayırıcı kırmızı çizgi
                  tft.drawFastHLine(0,  100, 160, RGB_FIX(180, 0,  0));
                  tft.drawFastHLine(0,  101, 160, RGB_FIX(80,  0,  0));

                  menuDrawn = true;
              }

              // ── SEÇİM SATIRLARI (her frame güncellenir, sadece menü bandı) ──
              // Seçenekleri içeren bant: y=102..127
              // Siyah arka planı tekrar çizmeden sadece yazıları güncelle:
              tft.fillRect(0, 102, 160, 26, tft.color565(0, 0, 0)); // band temizle

              // Öge 0 — YENİ OYUN
              {
                  bool sel = (menuSelection == 0);
                  tft.setTextSize(1);
                  tft.setCursor(47, 105);
                  if(sel) {
                      // Ok imleci — kırmızımsı turuncu, titrek efekt
                      uint8_t pulse = (uint8_t)(128 + 127 * sin(mnow * 0.008f));
                      tft.setTextColor(tft.color565(255, pulse/2, 0));
                      tft.print("> "); 
                      tft.setTextColor(RGB_FIX(255, 200, 0));
                      tft.print("YENI OYUN");
                  } else {
                      tft.setTextColor(tft.color565(120, 30, 10));
                      tft.print("  YENI OYUN");
                  }
              }

              // Öge 1 — SEVİYE SEÇ
              {
                  bool sel = (menuSelection == 1);
                  tft.setTextSize(1);
                  tft.setCursor(44, 118);
                  if(sel) {
                      uint8_t pulse = (uint8_t)(128 + 127 * sin(mnow * 0.008f));
                      tft.setTextColor(tft.color565(255, pulse/2, 0));
                      tft.print("> ");
                      tft.setTextColor(RGB_FIX(255, 200, 0));
                      tft.print("SEVIYE SEC");
                  } else {
                      tft.setTextColor(tft.color565(120, 30, 10));
                      tft.print("  SEVIYE SEC");
                  }
              }

              // ── BTN_A SEÇİM ──────────────────────────────
              if(!digitalRead(BTN_A)) {
                  playSound(1000, 80);
                  delay(200);
                  if(menuSelection == 0) {
                      currentLevel = 1;
                      hp = 100; ammo = 75; armor = 0; hasKey = false; weaponType = 0;
                      loadLevel(1);
                      tft.fillScreen(TFT_BLACK);
                      drawStaticHUD();
                      lastFrame = millis(); fpsTimer = millis();
                      gameState   = STATE_PLAYING;
                      menuDrawn   = false;
                  } else {
                      inLevelSelect  = true;
                      levelSelectIdx = 0;
                      menuDrawn      = false; // seviye ekranını sıfırdan çiz
                  }
              }
              
              // ── BTN_B ÇIKIŞ (OS MENÜSÜNE DÖN) ────────────────────
              if(!digitalRead(BTN_B)) {
                  playSound(400, 50);
                  delay(200);
                  returnToOS();
              }

          // ── SEVİYE SEÇİM ALT MENÜSÜ ──────────────────────
          } else {

              // Joystick navigasyon
              if(abs(jy_menu) > 500 && (mnow - lastMenuMove > 250)) {
                  if(jy_menu < 0) { levelSelectIdx--; if(levelSelectIdx < 0) levelSelectIdx = 2; }
                  else             { levelSelectIdx++; if(levelSelectIdx > 2) levelSelectIdx = 0; }
                  lastMenuMove = mnow;
                  playSound(800, 30);
              }

              // ── TAM YENİDEN ÇİZİM ──
              if(!menuDrawn) {
                  // TITLEPIC karartılmış
                  if(titlePicBuf) {
                      uint16_t* darkBuf = (uint16_t*)heap_caps_malloc(160 * 128 * 2, MALLOC_CAP_SPIRAM);
                      if (darkBuf) {
                          for(int py = 0; py < 128; py++) {
                              for(int px2 = 0; px2 < 160; px2++) {
                                  uint16_t c = titlePicBuf[py * 160 + px2];
                                  uint8_t r = ((c >> 11) & 0x1F) * 35 / 100;
                                  uint8_t g = ((c >> 5)  & 0x3F) * 35 / 100;
                                  uint8_t b = ( c        & 0x1F) * 35 / 100;
                                  darkBuf[py * 160 + px2] = (r << 11) | (g << 5) | b;
                              }
                          }
                          tft.pushImage(0, 0, 160, 128, darkBuf);
                          free(darkBuf);
                      } else {
                          tft.fillScreen(tft.color565(8, 0, 0));
                      }
                  } else {
                      tft.fillScreen(tft.color565(8, 0, 0));
                  }

                  // Merkezi panel kaldirildi

                  // Başlık (EPIZOT SEC Kaldırıldı)

                  menuDrawn = true;
              }

              // Seviye satırları
              const struct { const char* ep; const char* name; const char* sub; } levels[3] = {
                  { "EP.1", "PHOBOS",   "KNEE-DEEP IN THE DEAD" },
                  { "EP.2", "DEIMOS",   "THE SHORES OF HELL"    },
                  { "EP.3", "CEHENNEM", "INFERNO"               },
              };

              for(int i = 0; i < 3; i++) {
                  int yRow = 48 + i * 17;
                  bool sel = (i == levelSelectIdx);

                  // Seçili satır vurgulama bandı
                  if(sel) {
                      tft.fillRect(14, yRow - 1, 132, 15, tft.color565(30, 5, 0));
                      tft.drawFastHLine(14, yRow - 1,  132, RGB_FIX(120, 30, 0));
                      tft.drawFastHLine(14, yRow + 13, 132, RGB_FIX(80, 15, 0));
                  } else {
                      tft.fillRect(14, yRow - 1, 132, 15, tft.color565(5, 0, 0));
                  }

                  // İmleç
                  tft.setTextSize(1);
                  tft.setCursor(17, yRow + 2);
                  if(sel) {
                      uint8_t blink = ((mnow / 300) % 2) ? 255 : 180;
                      tft.setTextColor(tft.color565(blink, blink/3, 0));
                      tft.print("> ");
                  } else {
                      tft.setTextColor(tft.color565(40, 8, 0));
                      tft.print("  ");
                  }

                  // Bölüm numarası
                  if(sel) tft.setTextColor(RGB_FIX(255, 80, 0));
                  else    tft.setTextColor(tft.color565(50, 10, 0));
                  tft.print(levels[i].ep);
                  tft.print(" ");

                  // Bölüm adı
                  if(sel) tft.setTextColor(RGB_FIX(255, 210, 0));
                  else    tft.setTextColor(tft.color565(80, 20, 0));
                  tft.setTextSize(1);
                  tft.print(levels[i].name);
              }

              // Alt ipucu kaldirildi
              // ── BTN_A / BTN_B ──────────────────────────────
              if(!digitalRead(BTN_A)) {
                  playSound(1000, 80);
                  delay(200);
                  currentLevel = levelSelectIdx + 1;
                  hp = 100; ammo = 75; armor = 0; hasKey = false; weaponType = 0;
                  loadLevel(currentLevel);
                  tft.fillScreen(TFT_BLACK);
                  drawStaticHUD();
                  lastFrame = millis(); fpsTimer = millis();
                  gameState     = STATE_PLAYING;
                  menuDrawn     = false;
                  inLevelSelect = false;
              }
              if(!digitalRead(BTN_B)) {
                  playSound(400, 50);
                  delay(200);
                  inLevelSelect = false;
                  menuDrawn     = false; // TITLEPIC'i taze bas
              }
          }

          vTaskDelay(pdMS_TO_TICKS(30));
          continue;
      }
      
      // ==========================================
      // DOOM PAUSE MENÜSÜ (STATE_PAUSE) — A=devam, B=menüye dön
      // ==========================================
      if(gameState == STATE_PAUSE) {
          if(!menuDrawn) {
              vTaskDelay(pdMS_TO_TICKS(100)); // TaskDisplay'in durduğundan emin ol
              
              // Framebuffer yerine doğrudan TFT'ye çiziyoruz (Premium Doom Temalı)
              tft.fillRoundRect(25, 30, 110, 65, 5, RGB_FIX(30, 0, 0));
              tft.drawRoundRect(25, 30, 110, 65, 5, RGB_FIX(255, 50, 50));
              tft.drawRoundRect(26, 31, 108, 63, 4, RGB_FIX(150, 0, 0));
              
              tft.setTextSize(2);
              tft.setTextColor(RGB_FIX(255, 50, 50));
              const char* pTitle = "PAUSE";
              tft.setCursor((SW - strlen(pTitle)*12)/2, 38);
              tft.print(pTitle);

              tft.setTextSize(1);
              tft.setTextColor(TFT_WHITE);
              const char* pTxtA = "[A] Devam Et";
              tft.setCursor((SW - strlen(pTxtA)*6)/2, 62);
              tft.print(pTxtA);
              
              tft.setTextColor(RGB_FIX(180, 180, 180));
              const char* pTxtB = "[B] Menu";
              tft.setCursor((SW - strlen(pTxtB)*6)/2, 76);
              tft.print(pTxtB);
              
              menuDrawn = true;
          }
          
          if(!digitalRead(BTN_A)) {
              playSound(800, 50);
              delay(200);
              gameState = STATE_PLAYING;
              lastFrame = millis(); // Delta time zıplamasını engelle
          }
          if(!digitalRead(BTN_B)) {
              playSound(400, 50);
              delay(200);
              gameState = STATE_MENU;
              menuDrawn = false;
          }
          vTaskDelay(pdMS_TO_TICKS(50));
          continue;
      }

      // ==========================================
      // OYUN DURUMU (STATE_PLAYING) — Ana oyun döngüsü
      // ==========================================
      // Delta time: frame'ler arası süre. Maksimum 0.1sn ile sınırlandırılır
      // (pause'dan dönüşte dev zıplamayı önler). Hareketler dt'ye bağlıdır.
      uint32_t now = millis();
      float dt = (now - lastFrame) / 1000.0f; lastFrame = now; if(dt > 0.1) dt = 0.1;

      // Oyun Mantığı Güncellemeleri için Kilidi Al (Radar Çakışma Koruması)
      xSemaphoreTake(gameMutex, portMAX_DELAY);
      
      bool joySw_curr = digitalRead(JOY_SW);
      if(joySw_prev && !joySw_curr) {
          // Oyunu duraklat
          gameState = STATE_PAUSE;
          menuDrawn = false;
          joySw_prev = joySw_curr;
          xSemaphoreGive(gameMutex);
          playSound(600, 50); // Pause tuşuna basıldığını belirten geri bildirim sesi
          continue;
      }
      joySw_prev = joySw_curr;
      
      updateAnimations(now);
      
      if(hp <= 0) {
        xSemaphoreGive(gameMutex);
        
        // Cift Cekirdek SPI Cakisikligini Onle (BMP bozulmalarini engeller)
        // TaskDisplay'i durdurmak icin gameState'i gecici olarak degistiriyoruz
        gameState = STATE_BOOT; 
        vTaskDelay(pdMS_TO_TICKS(100)); // TaskDisplay'in frame cizimini bitirmesi icin bekle
        
        tft.fillScreen(TFT_BLACK); tft.setTextColor(RGB_FIX(255,0,0)); tft.setTextSize(2);
        tft.setCursor(20, SH/2 - 10); tft.print("GAME OVER"); playSound(100, 1000);
        
        // Eger oyuncu ates ederken olduyse hemen gecmesin
        vTaskDelay(pdMS_TO_TICKS(1500));
        while(!digitalRead(BTN_A)) { vTaskDelay(pdMS_TO_TICKS(50)); }
        
        // Oyuncu BTN_A'ya basinca menuye donsun
        while(1) {
            if(!digitalRead(BTN_A)) {
                delay(300);
                hp = 100; ammo = 75; armor = 0; hasKey = false; weaponType = 0;
                gameState = STATE_MENU;
                menuDrawn = false;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        continue; 
      }
      
      // --- KALKAN / PARRY SİSTEMİ (BTN_D) ---
      // Kalkan basılı tutulunca hasar emilir (kontaktan sekme). Kalkan açıldıktan
      // ilk 300ms = "parry" penceresi: mermiler düşmana geri sektirilir (type 13).
      bool currentShieldState = !digitalRead(BTN_D);
      if(currentShieldState && !lastShieldState && (now - shieldStartTime > 500)) shieldStartTime = now;
      lastShieldState = currentShieldState;
      bool isParrying = currentShieldState && (now - shieldStartTime < 300);

      // --- DÜŞÜK CAN KALP ATIŞI SESİ --- (hp<=25 iken her 250ms'de ritmik bip)
      static int lastBeat = 0;
      if (hp > 0 && hp <= 25) {
          int currentBeat = (now % 1000) / 250;
          if (currentBeat == 0 && lastBeat != 0) { playSound(120, 80); lastBeat = 0; }
          else if (currentBeat == 1 && lastBeat != 1) { playSound(90, 100); lastBeat = 1; }
          else if (currentBeat == 2 && lastBeat != 2) { lastBeat = 2; }
          else if (currentBeat == 3 && lastBeat != 3) { lastBeat = 3; }
      }

      // --- JOYSTICK / WASD KONTROL MANTIĞI ---
      // jx = X ekseni (yatay) = kamera dönüşü. jy = Y ekseni (dikey) = ileri/geri.
      // Deadzone 300: küçük titremeleri ve kendiliğinden kaymayı engeller.
      // Dönüş: dirX/dirY ve planeX/planeY vektörleri bir rotasyon matrisi ile güncellenir.
      // Hareket: kalkanlıyken yavaş (1.5x), normalde hızlı (3.0x). Çarpışma kontrolü
      // için margin (0.2) eklenir — duvara yapışmayı önler, sadece boş hücreye girilir.
      int jx=joyRawX-joyCenterX, jy=joyRawY-joyCenterY;
      // Joystick deadzone artırıldı (Titremeyi ve kendi kendine yürümeyi kesmek için)
      if(abs(jx)<300) jx=0; if(abs(jy)<300) jy=0;
      if(jx) {
        float rs=(jx/2000.0f)*2.5f*dt, s=rs, c=1.0f-rs*rs*0.5f, odx=dirX;
        dirX=dirX*c-dirY*s;
        dirY=odx*s+dirY*c;
        float opx=planeX; planeX=planeX*c-planeY*s; planeY=opx*s+planeY*c;
      }
      
      if(jy) {
        float mv = (-jy/2000.0f) * (currentShieldState ? 1.5f : 3.0f) * dt;
        float moveX = dirX * mv;
        float moveY = dirY * mv;
        float marginX = (moveX > 0) ? 0.2f : -0.2f;
        float marginY = (moveY > 0) ? 0.2f : -0.2f;
        
        if(MAP[(int)py][(int)(px + moveX + marginX)] == 0) px += moveX;
        if(MAP[(int)(py + moveY + marginY)][(int)px] == 0) py += moveY;
      }

      // --- BTN_B: KULLAN / KAPI AÇ / YAKIN DÖVÜŞ ---
      // 300ms cooldown ile. Ön tarafta (dirX/dirY doğrultusunda) 2 birim mesafe
      // içindeki ilk nesneyle etkileşir:
      //   6=kapı aç (sil), 7=kilitli kapı (anahtar gerek), 8=çıkış (sonraki seviye),
      //   4=gizli duvar (5'e dönüp gizli sprite'ları ve 31'lik duvarları açar).
      // Ön tarafta düşman varsa ve kalkan kapalıysa: yakın dövüş (25 hasar).
      if (!digitalRead(BTN_B) && now - sonKullanma > 300) {
        bool kapiAcildi = false;
        for(float d = 0.1; d <= 2.0; d += 0.2) {
            int tx = (int)(px + dirX * d), ty = (int)(py + dirY * d);
            if(tx >= 0 && tx < MW && ty >= 0 && ty < MH) {
                int block = MAP[ty][tx];
                if(block >= 6) { 
                    if(block == 6) { MAP[ty][tx] = 0; playSound(800, 100); }
                    else if(block == 7 && hasKey) { MAP[ty][tx] = 0; hasKey = false; playSound(1200, 150); }
                    else if(block == 8) { 
                        currentLevel++;
                        if(currentLevel > 3) currentLevel=1; 
                        loadLevel(currentLevel); 
                        drawStaticHUD(); 
                        kapiAcildi = true;
                        break; 
                    }
                    kapiAcildi = true;
                    break;
                } else if (block == 4) { 
                    MAP[ty][tx] = 5; 
                    for(int y=0; y<MH; y++) {
                        for(int x=0; x<MW; x++) {
                            if(MAP[y][x] == 31) MAP[y][x] = 0; 
                        }
                    }
                    for(int i = 0; i < NUM_SPRITES; i++) { if(sprites[i].state == -1) sprites[i].state = 1; }
                    playSound(100, 500); 
                    kapiAcildi = true;
                    break;
                } else if (block > 0 && block < 6) { break; } 
            }
        }

        if(!kapiAcildi && !currentShieldState) {
            bool dusmanVar = false;
            int hedefID = -1;
            
            for(int i=0; i<NUM_SPRITES; i++) {
                if(sprites[i].state >= 1 && (sprites[i].type == 5 || sprites[i].type == 14 || sprites[i].type == 17) && sprites[i].animState != ANIM_DEAD) {
                    float dx = sprites[i].x - px, dy = sprites[i].y - py, dist = sqrt(dx*dx + dy*dy);
                    if (dist < 0.001f) continue;
                    float ang = atan2(dy,dx)-atan2(dirY,dirX);
                    while(ang > PI) ang -= 2*PI; while(ang < -PI) ang += 2*PI;
                    bool hasLOS = true;
                    for(float s = 0.2; s < dist; s += 0.2) {
                        int cx = (int)(px + (dx/dist)*s), cy = (int)(py + (dy/dist)*s);
                        if(cx < 0 || cx >= MW || cy < 0 || cy >= MH || MAP[cy][cx] > 0) { hasLOS = false; break; }
                    }
                    if(hasLOS && abs(ang) < 0.5 && dist < 1.8) { 
                        dusmanVar = true; hedefID = i; break; 
                    }
                }
            }
            
            if(dusmanVar) {
                meleeTimer = now; 
                playSound(350, 50); 
                sprites[hedefID].hp -= 25; 
                if(sprites[hedefID].hp <= 0) { 
                    sprites[hedefID].animState = ANIM_DYING; 
                    hp += 10; if(hp>100) hp=100; 
                    playSound(2000, 50); 
                } else { 
                    sprites[hedefID].animState = ANIM_HIT; 
                    playSound(300, 40); 
                }
                sprites[hedefID].animFrame = 0; sprites[hedefID].animTimer = now;
            }
        }
        sonKullanma = now;
      }

      // --- DÜŞMAN AI & MERMI FIZIGI (sprite dongusu) ---
      // Her sprite icin oyuncuya mesafe + görüş hattı (LOS) hesaplanir.
      // LOS: duvarlar arkasindaki dusmani gormeyi engeller (ray march ile).
      // Dusman tipleri:
      //   5=zombi: 0.5x hiz, uzaktan ates edebilir (fireball=type 12)
      //   14=baron: 0.3x hiz, uzaktan ates, 30 hasar, 150 hp
      //   17=pinky: 1.2x hiz + zigzag, ATES ETMEZ (sadece isirir), 25 hasar
      //   12=dusman mermisi: oyuncuya dogru 4.0x, carpinca 15 hasar (kalkanla sekme)
      //   13=sekme mermisi: dusmana geri donmus, 40 hasar verir
      //   9/10/11/43: esya (yakin gelince toplanir)
      // Kalkan + ileri (jy<0) + yakin = testere saldirisi (40 hasar).
      // Parry penceresinde mermi geri seker (type 12->13, hiz 1.5x).
      for(int i=0; i<NUM_SPRITES; i++) {
        if(sprites[i].state >= 1) {
            float dx = px - sprites[i].x, dy = py - sprites[i].y, dist = sqrt(dx*dx + dy*dy);
            if (dist < 0.001f) continue;
            
            bool hasLOS = true;
            for(float s = 0.2; s < dist; s += 0.2) {
                int cx = (int)(sprites[i].x + (dx/dist) * s), cy = (int)(sprites[i].y + (dy/dist) * s);
                if(cx >= 0 && cx < MW && cy >= 0 && cy < MH && MAP[cy][cx] > 0) { hasLOS = false; break; }
            }
            if(sprites[i].type == 5 || sprites[i].type == 14 || sprites[i].type == 17) { 
                if(sprites[i].animState == ANIM_DEAD || sprites[i].animState == ANIM_DYING) continue;
                
                if(dist < 6.0 && dist > 0.6) { 
                    if(sprites[i].animState == ANIM_IDLE && hasLOS) { sprites[i].animState = ANIM_WALK; sprites[i].animTimer = now; }
                    if(sprites[i].type != 17 && hasLOS && dist > 2.5 && random(0, 100) < 2 && now - sprites[i].lastFireTime > 3000) {
                       sprites[i].animState = ANIM_ATTACK; sprites[i].animFrame = 0; sprites[i].animTimer = now;
                       for(int j=0; j<NUM_SPRITES; j++) if(sprites[j].state == 0) { 
                          initSprite(j, sprites[i].x, sprites[i].y, 12, 1);
                          sprites[j].dx = (dx/dist); 
                          sprites[j].dy = (dy/dist);
                          sprites[i].lastFireTime = now; 
                          playSound(500, 80); break;
                        }
                }
                    
                    float spd; float moveX, moveY;
                    if(sprites[i].type == 17) {
                        spd = 1.2f * dt; float zigzag = sin(now / 250.0) * 0.3; 
                        moveX = (dx/dist) * spd + (-dy/dist) * zigzag * spd;
                        moveY = (dy/dist) * spd + (dx/dist) * zigzag * spd;
                    } else {
                        spd = (sprites[i].type == 14 ? 0.3f : 0.5f) * dt;
                        moveX = (dx/dist) * spd; moveY = (dy/dist) * spd;
                    }
                    int nx = (int)(sprites[i].x + moveX), ny = (int)(sprites[i].y);
                    if(nx >= 0 && nx < MW && ny >= 0 && ny < MH && MAP[ny][nx] == 0) sprites[i].x += moveX; 
                    nx = (int)(sprites[i].x); ny = (int)(sprites[i].y + moveY);
                    if(nx >= 0 && nx < MW && ny >= 0 && ny < MH && MAP[ny][nx] == 0) sprites[i].y += moveY;
                } 
                
                if(hasLOS && currentShieldState && (jy < 0) && dist <= 0.9 && now - shieldSawTime > 400) {
                    sprites[i].hp -= 40;
                    if(sprites[i].hp <= 0) { sprites[i].animState = ANIM_DYING; playSound(100, 80); } 
                    else { sprites[i].animState = ANIM_HIT; playSound(150, 120); }
                    sprites[i].animFrame = 0; sprites[i].animTimer = now; shieldSawTime = now;
                } 
                else if(hasLOS && dist <= 0.6 && now - lastDamageTime > 1000) { 
                    sprites[i].animState = ANIM_ATTACK; sprites[i].animFrame = 0; sprites[i].animTimer = now;
                    if(isParrying) { playSound(4000, 50); sprites[i].x -= dx*1.5; sprites[i].y -= dy*1.5; lastDamageTime = now-200; }
                    else if(currentShieldState) { playSound(3000, 50); sprites[i].x -= dx*0.5; lastDamageTime = now-600; }
                    else { 
                        int dmg = (sprites[i].type == 14) ? 30 : ((sprites[i].type == 17) ? 25 : 20);
                        if(armor > 0) { int absorb = min(armor, (dmg + 1) / 2); armor -= absorb; hp -= (dmg - absorb); }
                        else { hp -= dmg; }
                        lastDamageTime = now; playSound(150, 300); 
                    }
                }
            } else if (sprites[i].type == 12) { 
                float projSpeed = 4.0f * dt;
                sprites[i].x += sprites[i].dx * projSpeed; 
                sprites[i].y += sprites[i].dy * projSpeed;
                float f_dx = px - sprites[i].x, f_dy = py - sprites[i].y, f_dist = sqrt(f_dx*f_dx + f_dy*f_dy);
                int bx = (int)sprites[i].x, by = (int)sprites[i].y;
                if(bx < 0 || bx >= MW || by < 0 || by >= MH || MAP[by][bx] > 0) sprites[i].state = 0;
                else if(f_dist < 0.5) {
                    if(isParrying) { sprites[i].dx *= -1.5; sprites[i].dy *= -1.5; sprites[i].type = 13; playSound(4000, 50); }
                    else if(currentShieldState) { sprites[i].state = 0; playSound(3000, 50); }
                    else { 
                        sprites[i].state = 0; 
                        int dmg = 15;
                        if(armor > 0) { int absorb = min(armor, (dmg + 1) / 2); armor -= absorb; hp -= (dmg - absorb); }
                        else { hp -= dmg; }
                        lastDamageTime = now; playSound(150, 300); 
                    }
                }
            } else if (sprites[i].type == 13) { 
                float projSpeed = 4.0f * dt;
                sprites[i].x += sprites[i].dx * projSpeed; 
                sprites[i].y += sprites[i].dy * projSpeed;
                int bx = (int)sprites[i].x, by = (int)sprites[i].y;
                if(bx < 0 || bx >= MW || by < 0 || by >= MH || MAP[by][bx] > 0) sprites[i].state = 0; 
                else for(int j=0; j<NUM_SPRITES; j++) if(sprites[j].state >= 1 && (sprites[j].type == 5 || sprites[j].type == 14 || sprites[j].type == 17) && sprites[j].animState != ANIM_DEAD) {
                    float diffX = sprites[j].x - sprites[i].x; float diffY = sprites[j].y - sprites[i].y;
                    if((diffX*diffX + diffY*diffY) < 0.25f) { 
                        sprites[j].hp -= 40;
                        if(sprites[j].hp <= 0) sprites[j].animState = ANIM_DYING; else sprites[j].animState = ANIM_HIT;
                        sprites[j].animFrame = 0; sprites[j].animTimer = now; sprites[i].state = 0; playSound(1200, 100); break;
                    }
                }
            } else if(dist < 0.8) { 
                if(sprites[i].type == 9 || sprites[i].type == 10 || sprites[i].type == 11 || sprites[i].type == 43) {
                    if(sprites[i].type == 9) ammo += 15;
                    else if(sprites[i].type == 10) { hp += 25; if(hp>100) hp=100; } 
                    else if(sprites[i].type == 11) hasKey = true;
                    else if(sprites[i].type == 43) { armor += 50; if(armor>100) armor=100; } 
                    sprites[i].state = 0; playSound(1500, 100);
                }
            }
        }
      }

      // --- SİLAH DEĞİŞTİRME (BTN_C) --- Edge detection ile 0<->1 arasında geçiş
      static bool btnC_prev = true;
      bool btnC_curr = digitalRead(BTN_C);
      if (btnC_prev && !btnC_curr) {
          weaponType = (weaponType + 1) % 2; 
          if (weaponType == 1) { playSound(400, 50); } 
          else { playSound(800, 50); } 
      }
      btnC_prev = btnC_curr;

      // --- ATEŞ MEKANİĞİ (BTN_A) ---
      // Kalkan açıkken ates edilemez. Yakin dovus (B) animasyonu sirasinda bekle.
      // weaponType 0 = TABANCA: 250ms cooldown, 1 mermi, 20 hasar, tek hedef, menzil 8.0
      //   Açı ±0.2 rad, LOS gerekli. Varil vurunca patlar.
      // weaponType 1 = POMPALI: 600ms cooldown, 3 mermi, 45 hasar, max 3 hedef, menzil 4.5
      //   Açı ±0.5 rad (daha geniş saçma). Düşman ölünce zırh kazandırır.
      bool btnA_curr = digitalRead(BTN_A);
      if(!btnA_curr && !currentShieldState && (now - meleeTimer >= 300)) { 
          if (weaponType == 0 && ammo > 0 && now - fireT > 250) { 
              ammo--; playSound(300, 100); fireT = now; 
              for(int i=0; i<NUM_SPRITES; i++) {
                  if(sprites[i].state >= 1 && (sprites[i].type == 5 || sprites[i].type == 14 || sprites[i].type == 15 || sprites[i].type == 17) && sprites[i].animState != ANIM_DEAD) { 
                      float dx = sprites[i].x-px, dy = sprites[i].y-py, dist = sqrt(dx*dx+dy*dy);
                      if (dist < 0.001f) continue;
                      float ang = atan2(dy,dx)-atan2(dirY,dirX);
                      while(ang > PI) ang -= 2*PI; while(ang < -PI) ang += 2*PI;
                      bool hasLOS = true;
                      for(float s = 0.2; s < dist; s += 0.2) {
                        int cx=(int)(px+(dx/dist)*s), cy=(int)(py+(dy/dist)*s);
                        if(cx<0||cx>=MW||cy<0||cy>=MH) { hasLOS=false; break; }
                        if(MAP[cy][cx]>0) { hasLOS=false; break; }
                      }
                      if(hasLOS && abs(ang) < 0.2 && dist < 8.0) { 
                          if(sprites[i].type == 15) { explodeBarrel(i, now); }
                          else { 
                              sprites[i].hp -= 20;
                              if(sprites[i].hp <= 0) { sprites[i].animState = ANIM_DYING; armor += (sprites[i].type == 14 ? 30 : 15); if(armor > 100) armor = 100; playSound(800, 100); }
                              else { sprites[i].animState = ANIM_HIT; playSound(500, 50); }
                              sprites[i].animFrame = 0; sprites[i].animTimer = now;
                          }
                          break;
                      }
                  }
              }
          }
          else if (weaponType == 1 && ammo > 0 && now - fireT > 600) { 
              int cost = ammo >= 3 ? 3 : ammo; ammo -= cost;
              playSound(100, 250); fireT = now; 
              int hits = 0;
              for(int i=0; i<NUM_SPRITES; i++) {
                  if(sprites[i].state >= 1 && (sprites[i].type == 5 || sprites[i].type == 14 || sprites[i].type == 15 || sprites[i].type == 17) && sprites[i].animState != ANIM_DEAD) { 
                      float dx = sprites[i].x-px, dy = sprites[i].y-py, dist = sqrt(dx*dx+dy*dy);
                      if (dist < 0.001f) continue;
                      float ang = atan2(dy,dx)-atan2(dirY,dirX);
                      while(ang > PI) ang -= 2*PI; while(ang < -PI) ang += 2*PI;
                      bool hasLOS = true;
                      for(float s = 0.2; s < dist; s += 0.2) {
                        int cx=(int)(px+(dx/dist)*s), cy=(int)(py+(dy/dist)*s);
                        if(cx<0||cx>=MW||cy<0||cy>=MH) { hasLOS=false; break; }
                        if(MAP[cy][cx]>0) { hasLOS=false; break; }
                      }
                      if(hasLOS && abs(ang) < 0.5 && dist < 4.5) { 
                          if(sprites[i].type == 15) { explodeBarrel(i, now); }
                          else { 
                              sprites[i].hp -= 45;
                              if(sprites[i].hp <= 0) { sprites[i].animState = ANIM_DYING; armor += (sprites[i].type == 14 ? 30 : 15); if(armor > 100) armor = 100; playSound(800, 100); }
                              else { sprites[i].animState = ANIM_HIT; playSound(500, 50); }
                              sprites[i].animFrame = 0; sprites[i].animTimer = now;
                          }
                          hits++; if(hits >= 3) break;
                      }
                  }
              }
          }
      }

      // Oyun Mantığı Bitti, Kilidi Bırak (Radar artık okuyabilir)
      xSemaphoreGive(gameMutex);

      // ==========================================
      // RENDER DÖNGÜLERİ (Güvenli Çizim Alanı)
      // Bu noktadan sonra gameMutex gerekmez; sadece fb_render buffer'ına yazılır.
      // ==========================================
      // activeFB: bu frame için yazılacak framebuffer (triple-buffer'dan fb_render)
      uint16_t* activeFB = fb[fb_render];
      // LCG (lineer congruential) rastgele sayı — hasar alınca ekran sarsıntısı (pitch) için
      static uint32_t lcg = 12345;
      lcg = lcg * 1103515245 + 12345;
      // pitch: hasar alınmışsa son 200ms'de dikey sarsıntı (-6..+6 piksel). Kalkanlıyken yok.
      int pitch = (now - lastDamageTime < 200 && !currentShieldState) ? (int)(lcg % 13) - 6 : 0;
      
      // ============================================================
      //  RAYCASTING — DDA (Digital Differential Analyzer) ALGORİTMASI
      //  Her piksel sütunu (x) için bir ışın (ray) gönderilir:
      //   1. camX: sütunun kamera düzlemindeki yatay konumu (-1..+1)
      //   2. rayX,rayY: ışın yönü = dirX + planeX*camX (ve Y karşılığı)
      //   3. DDA: ışın grid hücrelerinden geçerek ilk dolu hücreyi bulur.
      //      ddx/ddy = bir hücre geçmek için gereken ışın uzunluğu (X/Y ekseninde).
      //      sdx/sdy = bir sonraki grid çizgisine olan kümülatif mesafe.
      //      side: çarpan yüzeyin yönü (0=X duvarı, 1=Y duvarı -> daha koyu gölge).
      //   4. perp: duvara dik mesafe (balık gözü bozulmasını önler).
      //   5. lh: duvar yüksekliği (ekran yüksekliği / mesafe) -> uzak duvar küçük.
      //   6. ds/de: duvarın ekran üst/alt sınırları (pitch ile sarsıntı eklenir).
      //   7. Texture mapping: duvar yüzeyindeki isabet noktası (wx) -> texel x (tx);
      //      dikey olarak tp adımıyla ty hesaplanır, tex[]'den renk okunur.
      //      Y duvarları (side==1) yarı parlaklıkla gölgelenir (derinlik hissi).
      //   Tavan RGB(30,30,60), zemin RGB(60,60,60). Boş ışın (harita dışı) = koyu mavi.
      //  zBuffer[x]: sprite çiziminde derinlik testi için saklanır.
      // ============================================================
      for(int x=0;x<SW;x++) {
        float camX=camXTable[x], rayX=dirX+planeX*camX, rayY=dirY+planeY*camX;
        int mx=(int)px, my=(int)py;
        float ddx=fabs(1/rayX), ddy=fabs(1/rayY), sdx, sdy; int sx, sy, hit=0, side, ht=1;
        if(rayX<0){sx=-1; sdx=(px-mx)*ddx;} else{sx=1; sdx=(mx+1-px)*ddx;}
        if(rayY<0){sy=-1; sdy=(py-my)*ddy;} else{sy=1; sdy=(my+1-py)*ddy;}
        while(!hit) { 
            if(sdx<sdy){sdx+=ddx; mx+=sx; side=0;} else{sdy+=ddy; my+=sy; side=1;} 
            if(mx < 0 || mx >= MW || my < 0 || my >= MH) break; 
            if(MAP[my][mx]>0){hit=1; ht=MAP[my][mx];} 
        }
        // FIX: Eğer hiçbir şeye çarpmadan harita dışına çıktıysa, karanlık çiz ve geç!
        if(!hit) {
            zBuffer[x] = 999.0f;
            for(int y=0;y<SH;y++) activeFB[y*SW+x]=RGB_FIX(30,30,60);
            continue;
        }
        float perp = (side==0)?(sdx-ddx):(sdy-ddy); zBuffer[x] = perp;
        int lh=(int)(SH/perp), ds=max(0, -lh/2+SH/2+pitch), de=min(SH-1, lh/2+SH/2+pitch);
        float wx = (side==0)?py+perp*rayY:px+perp*rayX; wx-=floor(wx); int tx=(int)(wx*TEX_W);
        float st=1.0*TEX_H/lh, tp=(ds-SH/2-pitch+lh/2)*st;
        for(int y=0;y<ds;y++) activeFB[y*SW+x]=RGB_FIX(30,30,60);
        for(int y=ds;y<de;y++) { int ty=((int)tp)&63; tp+=st;
        uint16_t c=tex[ht][ty*TEX_W+tx]; if(side==1) c=((c>>1)&0x7BEF); activeFB[y*SW+x]=c; }
        for(int y=de;y<SH;y++) activeFB[y*SW+x]=RGB_FIX(60,60,60);
      }

      // ============================================================
      //  SPRITE RENDERING (düşmanlar, eşyalar, mermiler, silah)
      //  1. Sıralama: aktif sprite'lar oyuncuya olan mesafeye göre UZAKTAN YAKINA
      //     insertion sort ile dizilir (yakın olanlar sonra çizilir, üstte kalır).
      //  2. Kamera dönüşümü: sprite'ın ekran koordinatına (tx, ty) çevrilmesi.
      //     ty = derinlik (önünde mi?), ty>0 ise kamera arkasında değil = çizilebilir.
      //  3. Z-test: her sütunda ty < zBuffer[x] ise sprite duvardan daha yakın = çiz.
      //     Chroma-key: renk 0x0000 (siyah) = saydam, atlanır.
      //  4. HIT efekti: hasar almış düşman pikselleri parlaklaştırılır (r+12, g+24, b+12).
      // ============================================================
      int order[NUM_SPRITES]; float dists[NUM_SPRITES];
      int aliveCount = 0;
      for(int i=0; i<NUM_SPRITES; i++) {
          if(sprites[i].state<=0 && sprites[i].type!=15) continue;
          if(sprites[i].type==15 && sprites[i].state<=0 && sprites[i].animState==ANIM_DEAD) continue;
          float dx_s = px-sprites[i].x; float dy_s = py-sprites[i].y;
          dists[aliveCount] = dx_s*dx_s + dy_s*dy_s;
          order[aliveCount] = i;
          // Insertion sort (far to near)
          int j = aliveCount;
          while(j > 0 && dists[order[j-1]] < dists[order[j]]) {
              int t = order[j]; order[j] = order[j-1]; order[j-1] = t;
              j--;
          }
          aliveCount++;
      }
      for(int i=0; i<aliveCount; i++) {
        int idx=order[i]; if(sprites[idx].state<=0 && sprites[idx].type!=15) continue; 
        if(sprites[idx].type==15 && sprites[idx].state<=0 && sprites[idx].animState == ANIM_DEAD) continue;
        float sx=sprites[idx].x-px, sy=sprites[idx].y-py, det=1.0/(planeX*dirY-dirX*planeY), tx=det*(dirY*sx-dirX*sy), ty=det*(-planeY*sx+planeX*sy);
        if(ty>0) {
          int ssx=int((SW/2)*(1+tx/ty)), sh=abs(int(SH/ty)), dyS=max(0,-sh/2+SH/2+pitch), dyE=min(SH-1,sh/2+SH/2+pitch), dw=abs(int(SH/ty)), dxS=max(0,-dw/2+ssx), dxE=min(SW-1,dw/2+ssx);
          
          int texID = getEnemyTexID(idx);
          if (!tex[texID]) continue; 
          
          for(int b=dxS; b<dxE; b++) if(ty<zBuffer[b]) {
            int tX=int(256*(b-(-dw/2+ssx))*TEX_W/dw)/256;
            if(tX < 0) tX = 0; else if(tX >= TEX_W) tX = TEX_W - 1; 
            
            for(int y=dyS; y<dyE; y++) { 
                int d=y*256-SH*128-pitch*256+sh*128, tY=((d*TEX_H)/sh)/256;
                if(tY < 0) tY = 0; else if(tY >= TEX_H) tY = TEX_H - 1;
                
                uint16_t col=tex[texID][tY*TEX_W+tX]; 
                if(col!=0x0000) {
                    if(sprites[idx].animState == ANIM_HIT) { 
                        uint8_t r = ((col >> 11) & 0x1F); uint8_t g = ((col >> 5) & 0x3F); uint8_t b_col = (col & 0x1F);
                        r = min(31, r + 12); g = min(63, g + 24); b_col = min(31, b_col + 12);
                        col = (r << 11) | (g << 5) | b_col;
                    }
                    activeFB[y*SW+b]=col;
                }
            }
          }
        }
      }

      // ============================================================
      //  SİLAH/GÖRÜNÜM RENDERING (ekran alt-ortasında 64x64)
      //  gW/gH=64, gX=ortalanmış, gY=alt+aşağı (pitch ile sarsıntı takip eder).
      //  Hareket halinde (jx||jy) silah yukarı-aşağı sallanır (sin dalgası).
      //  weaponType 0 = silah 15 piksel daha aşağıda (farklı konum).
      //  wTex seçimi (duruma göre texture ID):
      //   - meleeTimer<300: y_vur (21) = yakın dövüş animasyonu
      //   - kalkan açık + sekme anı: k_sektir (20); k_vur<200ms (19); else k_dur (18)
      //   - tabanca: atış sonrası 100ms = t_ates (15), else t_bekle (14)
      //   - pompalı: çok kademeli geri tepme (17->47->48->47->16)
      //  Chroma-key: siyah pikseller atlanır (silah arka planı saydam).
      // ============================================================
      int gW=64, gH=64, gX=(SW-gW)/2, gY=SH-gH+5+pitch;
      if(jx||jy) gY+=sin(now/150.0)*4; 
      if (weaponType == 0) gY += 15; 

      int wTex = 14; 
      if (now - meleeTimer < 300) { wTex = 21; } 
      else if (currentShieldState) { 
          if (now - shieldSawTime < 200) wTex = 19;
          else if (isParrying) wTex = 20; // k_sektir (sekme anı)
          else wTex = 18; // k_dur
      } 
      else {
          if (weaponType == 0) {
              uint32_t elapsed = now - fireT;
              if (elapsed < 100) { wTex = 15; gY += 10; } 
              else { wTex = 14; }
          }
          else if (weaponType == 1) {
              uint32_t elapsed = now - fireT;
              if      (elapsed < 80)  { wTex = 17; gY += 8; } 
              else if (elapsed < 200) wTex = 47;   
              else if (elapsed < 400) wTex = 48;   
              else if (elapsed < 520) wTex = 47;   
              else                    wTex = 16;   
          }
      }
      
      for(int sy=0; sy<gH; sy++) {
          int drawY = gY + sy;
          if (drawY < 0 || drawY >= SH) continue; 
          
          for(int sx=0; sx<gW; sx++) { 
              int drawX = gX + sx;
              if (drawX < 0 || drawX >= SW) continue;
              
              int c=tex[wTex][sy*TEX_W+sx]; 
              if(c!=0x0000) { activeFB[drawY*SW + drawX] = c; }
          }
      }
      
      // --- HASAR / DÜŞÜK CAN EKRAN EFEKTİ ---
      // Hasar alınmışsa (son 200ms) VEYA hp<=25 iken (her 1sn'de 150ms) ekran
      // kenarlarına kırmızı vinyet (dış=koyu, iç=açık) çizilir — tehlike uyarısı.
      bool lowHpFlash = (hp > 0 && hp <= 25) && ((now % 1000) < 150);
      if((now-lastDamageTime<200 && !currentShieldState) || lowHpFlash) {
        uint16_t redOut = RGB_FIX(255, 0, 0); uint16_t redIn = RGB_FIX(150, 0, 0);
        for(int y=0; y<SH; y++) { activeFB[y*SW]=activeFB[y*SW+SW-1]=redOut; activeFB[y*SW+1]=activeFB[y*SW+SW-2]=redIn; }
        for(int x=0; x<SW; x++) { activeFB[x]=activeFB[(SH-1)*SW+x]=redOut; activeFB[SW+x]=activeFB[(SH-2)*SW+x]=redIn; }
      }

      // ============================================================
      //  TRIPLE-BUFFER TAKASI (frame tamamlanınca)
      //  fb_swap_mutex altında atomik olarak:
      //   - fb_render (yazılan) -> fb_ready (hazır) yapılır.
      //   - Eski fb_ready varsa, onu yeni fb_render yaparız (geri dönüşümlü kullanım).
      //   - Yoksa, fb_display olmayan üçüncü buffer'ı fb_render seçeriz.
      //  Böylece Engine hep boş bir buffer bulur, Display ise en son hazırlananı basar.
      //  5ms timeout:极端 durumlarda kilitlenmeyi önler.
      // ============================================================
      if (xSemaphoreTake(fb_swap_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          int8_t old_ready = fb_ready;
          fb_ready = fb_render;
          if (old_ready >= 0) {
              fb_render = old_ready;
          } else {
              fb_render = 3 - fb_render - fb_display;
              if (fb_render == fb_display) fb_render = (fb_display + 1) % 3;
          }
          xSemaphoreGive(fb_swap_mutex);
      }
      vTaskDelay(pdMS_TO_TICKS(1));
            
  }
}


// ============================================================
//  TaskDisplay — Hazır framebuffer'ı TFT'ye basan FreeRTOS task (Core 1).
//  IRAM_ATTR: hızlı iç SRAM'de çalışır. STATE_PLAYING dışında uyur (100ms).
//  Mantık:
//   1. fb_ready >= 0 ise (yeni frame var) mutex altında fb_display=fb_ready, fb_ready=-1.
//   2. SCREENSHOT: C+D buton kombinasyonu (rising edge) -> takeScreenshotFB çağrısı.
//      (checkScreenshotFB/takeScreenshotFB çağrılarına dokunma — dev_tools.h'den gelir.)
//   3. tft.pushImage ile framebuffer TFT'ye aktarılır.
//   4. Dinamik HUD güncellemesi: yalnızca değer değişince çizilir (cache sistemi):
//      - AMMO (INF modu = kalkan/yakın dövüş sırasında sınırsız gösterimi)
//      - HEALTH (hp<=25 ise kırmızı, else beyaz)
//      - ARMOR, anahtar ikonu, FPS (her 1 sn'de).
//  1ms vTaskDelay ile diğer task'lara yer verilir.
//  Parametre: pvParameters (kullanılmıyor). Return: yok (sonsuz döngü).
// ============================================================
void IRAM_ATTR TaskDisplay(void * pvParameters) {
    for(;;) {
        if (gameState != STATE_PLAYING) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (fb_ready >= 0) {
            int8_t toDisplay = -1;
            if (xSemaphoreTake(fb_swap_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
                if (fb_ready >= 0) {
                    fb_display = fb_ready;
                    fb_ready = -1;
                    toDisplay = fb_display;
                }
                xSemaphoreGive(fb_swap_mutex);
            }
            if (toDisplay >= 0) {
                // Screenshot: C+D kombinasyonu (Menüde B ile çakışmayı önlemek için)
                {
                    static bool prevCD = false;
                    bool cd = (!digitalRead(BTN_C) && !digitalRead(BTN_D));
                    if (cd && !prevCD) {
                        takeScreenshotFB(fb[toDisplay], SW, SH);
                    }
                    prevCD = cd;
                }
                tft.pushImage(0, 0, SW, SH, fb[toDisplay]);
                
                uint32_t now = millis();
                int hy = SH;
                bool currentInfState = (lastShieldState || (now - meleeTimer < 300));
                if (currentInfState != lastInfState || ammo != lastAmmo) {
                    tft.setTextColor(RGB_FIX(255,160,0), RGB_FIX(40,40,40));
                    if (currentInfState) {
                        tft.setCursor(15, hy+14); tft.print("INF");
                    } else {
                        tft.setCursor(15, hy+14); tft.printf("%03d", ammo);
                    }
                    lastInfState = currentInfState; lastAmmo = ammo;
                }
                
                if (hp != lastHp) {
                    tft.setTextColor(hp>25?0xFFFF:0xF800, RGB_FIX(40,40,40));
                    tft.setCursor(68, hy+14); tft.printf("%03d%%", hp);
                    lastHp = hp;
                }
                
                if (armor != lastArmor) {
                    tft.setTextColor(0xFFFF, RGB_FIX(40,40,40));
                    tft.setCursor(120, hy+14); tft.printf("%03d%%", armor);
                    lastArmor = armor;
                }
                
                if (hasKey) tft.fillRect(145, hy+13, 8, 8, RGB_FIX(255, 255, 0));
                else tft.fillRect(145, hy+13, 8, 8, RGB_FIX(40, 40, 40));
                
                frameCount++;
                if (now - fpsTimer >= 1000) {
                    fps = frameCount; frameCount = 0; fpsTimer = now;
                    if (fps != lastFps) {
                        tft.setTextColor(RGB_FIX(100, 100, 100), RGB_FIX(40, 40, 40));
                        tft.setCursor(142, hy+3); tft.printf("%03d", fps);
                        lastFps = fps;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Allow other tasks
    }
}

// ============================================================
//  TaskJoy — Joystick analog değerlerini arka planda okuyan FreeRTOS task (Core 1).
//  TaskEngine'in analogRead ile bloke olmasını önlemek için ayrı task'ta 10ms'de
//  bir X/Y eksenini örnekler ve joyRawX/joyRawY'ye (volatile) yazar.
//  Kalibrasyon merkezi setup()'ta belirlenir; bu task sadece ham değer verir.
//  Parametre: pvParameters (kullanılmıyor). Return: yok (sonsuz döngü).
// ============================================================
void TaskJoy(void * pvParameters) {
    for(;;) {
        joyRawX = analogRead(JOY_X);
        joyRawY = analogRead(JOY_Y);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ============================================================
//  setup — ESP32-S3 başlangıç konfigürasyonu. Sırasıyla:
//   1. Buzzer susturma + Seri port
//   2. SPI çakışmasını önleme (CS pinlerini HIGH)
//   3. SPI veriyolu başlatma
//   4. SD kart başlatma (40MHz) + dev_tools
//   5. OTA boot partition güvenliği
//   6. Triple-buffer PSRAM tahsisi
//   7. Buton pinleri
//   8. PSRAM: TITLEPIC + texture buffer'ları
//   9. Texture'ların SD'den PSRAM'a yüklenmesi (makeTex + loadBMP)
//  10. SD'yi kapat + TFT başlat (SPI hattını serbest bırak)
//  11. Joystick kalibrasyonu
//  12. NVS'ten ses ayarı
//  13. FreeRTOS task'larını çekirdeklere sabitleme (dual-core)
//  ESP32-S3: çift çekirdek (Core 0=motor, Core 1=ekran/radar/joystick), PSRAM
//  desteği ile büyük texture/framebuffer'lar internal RAM'i şişirmeden tutulur.
// ============================================================
void setup() {
  // Buzzer Boot Beep Engelleme
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  Serial.begin(115200); 

  // 1. SPI ÇAKIŞMASINI ÖNLEYEN SUSTURUCU EMİRLER
  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH); 
  pinMode(SD_CS, OUTPUT);  digitalWrite(SD_CS, HIGH);  

  // 2. SPI BAŞLAT (MISO=42)
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1); delay(50);

  // 3. TFT'Yİ BEKLET! ÖNCE SADECE SD KARTI BAŞLAT!
  // Maksimum sınırı zorluyoruz: 40 MHz! (Flashlama ve Yükleme hızını uçuracak)
   sdReady = SD.begin(SD_CS, SPI, 40000000);
   initDevTools(tft);

  // GÜVENLİK: Elektrik kesilirse her zaman OS'tan başla
  const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
  if (os_part) { esp_ota_set_boot_partition(os_part); }
  
  // ============================================================
  //  TRIPLE BUFFERING INIT — 3 framebuffer PSRAM'den ayrılır.
  //  heap_caps_malloc + MALLOC_CAP_SPIRAM: veriyi ESP32-S3'ün external PSRAM'ına koyar.
  //  Her buffer = 160*104*2 = 33KB. Internal RAM yetmezdi; PSRAM sayesinde mümkün.
  //  PSRAM yoksa (fall-back) malloc ile internal RAM denenir ama genelde çöker.
  // ============================================================
  for (int i = 0; i < 3; i++) {
      fb[i] = (uint16_t*)heap_caps_malloc(SW * SH * 2, MALLOC_CAP_SPIRAM);
      if (!fb[i]) {
          Serial.println("KRITİK HATA: FB PSRAM'a tahsis edilemedi!");
          fb[i] = (uint16_t*)malloc(SW * SH * 2);
      }
  }
  fb_swap_mutex = xSemaphoreCreateMutex();

  // Mutex Kilidini Başlat
  gameMutex = xSemaphoreCreateMutex();
  
  // Butonları başlat
  pinMode(BTN_A, INPUT_PULLUP); pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_C, INPUT_PULLUP); pinMode(BTN_D, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  // ============================================================
  //  PSRAM ALOKASYONLARI — Texture atlas ve TITLEPIC buffer'ları
  //  MALLOC_CAP_SPIRAM: hepsi external PSRAM'de (internal RAM'i serbest bırakır).
  //  Başarısız olursa internal RAM denenir; o da olmazsa kritik hata ekranı + dur.
  // ============================================================

  // TITLEPIC buffer'ı PSRAM'dan ayır (160x128x2 = 40KB)
  titlePicBuf = (uint16_t*)heap_caps_malloc(160 * 128 * 2, MALLOC_CAP_SPIRAM);
  if (!titlePicBuf) {
      // PSRAM yoksa internal RAM'den dene
      titlePicBuf = (uint16_t*)malloc(160 * 128 * 2);
  }

  // MAX_TEX adet texture slot'u ayır (her biri 64x64x2 = 8KB). 0. indeks kullanılmaz.
  for (int i = 1; i < MAX_TEX; i++) {
    tex[i] = (uint16_t*)heap_caps_malloc(TEX_W * TEX_H * 2, MALLOC_CAP_SPIRAM);
    if (!tex[i]) tex[i] = (uint16_t*)malloc(TEX_W * TEX_H * 2);
    
    // YENİ EKLENEN GÜVENLİK KONTROLÜ
    if (!tex[i]) {
        Serial.println("KRİTİK HATA: Bellek yetersiz!");
        tft.fillScreen(TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(10, 50);
        tft.print("BELLEK HATASI! PSRAM YOK");
        while(1) { vTaskDelay(pdMS_TO_TICKS(100)); } // Motoru güvenlice durdur
    }
  }
  // Prosedürel texture'ları üret (SD gerektirmeyen: yeşil zemin, ateş topu, sekme mermisi)
  for (int i = 8; i <= 13; i++) makeTex(i); 
  
  // ============================================================
  //  TEXTURE'LARIN SD'DEN PSRAM'A YÜKLENMESİ
  //  SD kart varsa tüm BMP'ler tek tek tex[] slotlarına yüklenir.
  //  fileBuf (80KB PSRAM): bir dosyayı tek seferde okuyup hızlı işleyen tampon.
  //  Yükleme sırasında OLED'de "Grafikler Yukleniyor..." mesajı gösterilir.
  //  Texture grubu:
  //   1-3=duvarlar, 4-5=anahtar düğme (off/on), 6=kapı, 7=kilitli, 31=gizli duvar
  //   9=mermi, 10=can, 11=anahtar, 43=armor
  //   14-15=tabanca, 16-17,47-48=pompalı, 18-20=kalkan, 21=yakın dövüş
  //   22-26=zombi, 27-30,32-33,44=pinky, 34-39=baron, 40-42=varil
  //  İş bitince fileBuf serbest bırakılır ve SD kapatılır (SPI hattı TFT'ye devredilir).
  // ============================================================
  if (sdReady) {
      oled.begin();
      oled.clearBuffer();
      oled.setFont(u8g2_font_6x10_tr);
      oled.drawStr(0, 20, "Grafikler Yukleniyor...");
      oled.drawStr(0, 40, "Lutfen bekleyin (3 sn)");
      oled.sendBuffer();
      
      // SD'den okuma yaparken devasa hız için geçici buffer al!
      uint8_t* fileBuf = (uint8_t*)heap_caps_malloc(80000, MALLOC_CAP_SPIRAM);
      if(!fileBuf) fileBuf = (uint8_t*)malloc(80000); // PSRAM yoksa internal
      
      // Texture'ları doğrudan sesten hızlı SD'den PSRAM'a çekiyoruz!
      if(fileBuf) {
          loadBMP("/duvar1.bmp", 1, fileBuf);  loadBMP("/duvar2.bmp", 2, fileBuf);  loadBMP("/duvar3.bmp", 3, fileBuf);
          loadBMP("/s_off.bmp", 4, fileBuf);   loadBMP("/s_on.bmp", 5, fileBuf);    
          loadBMP("/kapi.bmp", 6, fileBuf);    loadBMP("/kilitli.bmp", 7, fileBuf);
          loadBMP("/kilitli.bmp", 31, fileBuf); 
          
          loadBMP("/mermi.bmp", 9, fileBuf);
          loadBMP("/can.bmp", 10, fileBuf);
          loadBMP("/anahtar.bmp", 11, fileBuf);
          loadBMP("/armor.bmp", 43, fileBuf); 
          
          loadBMP("/t_bekle.bmp", 14, fileBuf); loadBMP("/t_ates.bmp", 15, fileBuf);
          
          loadBMP("/p_bekle.bmp", 16, fileBuf); loadBMP("/p_ates.bmp", 17, fileBuf);
          loadBMP("/p_cek1.bmp", 47, fileBuf);  loadBMP("/p_cek2.bmp", 48, fileBuf);
          
          loadBMP("/k_dur.bmp", 18, fileBuf);   loadBMP("/k_vur.bmp", 19, fileBuf);
          loadBMP("/k_sektir.bmp", 20, fileBuf);
          loadBMP("/y_vur.bmp", 21, fileBuf);
          
          loadBMP("/z_dur.bmp", 22, fileBuf);   loadBMP("/z_yuru.bmp", 23, fileBuf);
          loadBMP("/z_ates.bmp", 24, fileBuf);  loadBMP("/z_dus.bmp", 25, fileBuf);
          loadBMP("/z_ceset.bmp", 26, fileBuf);
          
          loadBMP("/p_dur.bmp", 27, fileBuf);   loadBMP("/p_yuru1.bmp", 28, fileBuf);
          loadBMP("/p_yuru2.bmp", 29, fileBuf); loadBMP("/p_isir.bmp", 30, fileBuf);
          loadBMP("/p_isir2.bmp", 44, fileBuf); loadBMP("/p_dus.bmp", 32, fileBuf);
          loadBMP("/p_ceset.bmp", 33, fileBuf);
          
          loadBMP("/b_dur.bmp", 34, fileBuf);   loadBMP("/b_yuru.bmp", 35, fileBuf);
          loadBMP("/b_ates.bmp", 36, fileBuf);  loadBMP("/b_irkil.bmp", 37, fileBuf);
          loadBMP("/b_dus.bmp", 38, fileBuf);   loadBMP("/b_ceset.bmp", 39, fileBuf);
          
          loadBMP("/v_dur.bmp", 40, fileBuf);   loadBMP("/v_patla.bmp", 41, fileBuf);
          loadBMP("/v_duman.bmp", 42, fileBuf);
          
          loadTitlePic(fileBuf); // TITLEPIC'i belleğe yükle
          free(fileBuf); // Buffer'ı sil, işimiz bitti!
      }
      
      SD.end(); // OKUMA İŞİ TAMAMEN BİTTİ, SPI HATTINI TERK EDİYOR
  } else {
      // SD Kart bulunamazsa OLED'den uyarı ver
      oled.begin();
      oled.clearBuffer();
      oled.setFont(u8g2_font_6x10_tr);
      oled.drawStr(0, 30, "SD KART BULUNAMADI!");
      oled.sendBuffer();
      delay(2000);
  }
  
  // === 4. TFT BAŞLATMA ===
  // SD kapatıldı, SPI hattı artık TFT'ye ayrıldı. SD_CS HIGH + SPI.end + TFT init.
  // setRotation(1)=yatay. setSwapBytes(true)=16-bit pushImage için byte sıralama.
  // camXTable: her sütunun kamera düzlemi koordinatı önceden hesaplanır (raycasting'de hız).
  // setScreenshotMode(SCR_BGR_NOSWAP): screenshot'ta byte-swap kapalı (BGR düzeni).
  // === 4. ARTIK SD KART OLMADIĞINA GÖRE TFT'Yİ GÜVENLE BAŞLATABİLİRİZ ===
  digitalWrite(SD_CS, HIGH); SPI.end(); delay(50); 
  tft.init();
  tft.setRotation(1);
  tft.setSwapBytes(true);
  for(int i=0;i<SW;i++) camXTable[i]=2.0f*i/SW-1.0f;
  setScreenshotMode(SCR_BGR_NOSWAP);
  tft.fillScreen(TFT_BLACK);
  
  // Butonlar ve buzzer zaten yukarıda başlatıldı, sadece joystick kaldı
  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12);
  tft.setTextColor(RGB_FIX(255,255,0)); tft.setCursor(20, 60); tft.print("KALIBRASYON...");
  long sumX = 0, sumY = 0;
  for (int i = 0; i < 10; i++) { sumX += analogRead(JOY_X); sumY += analogRead(JOY_Y); delay(2); }
  joyCenterX = sumX / 10; joyCenterY = sumY / 10;
  
  // Doğrudan DOOM Menüsünden Başla
  gameState = STATE_MENU;
  menuDrawn = false;
  titleDrawn = false;
  inLevelSelect = false;
  
  // --- V2.1: NVS'ten ses ayarını oku ---
  { Preferences prefs; prefs.begin("os", true); soundEnabled = prefs.getBool("sound_en", true); prefs.end(); }
  
  // --- FREERTOS ÇİFT ÇEKİRDEK ATAMALARI (ESP32-S3 dual-core) ---
  // Core 0: Oyun Motoru + Render (yüksek stack=30KB, öncelik 2) — ağır hesaplama
  // Core 1: Display (TFT basım), Radar (OLED, düşük öncelik 0), Joy (analog okuma)
  // Bu dağılım SPI çakışmasını önler: Engine buffer'a yazar, Display TFT'ye basar.
  
  // Core 0: Oyun Motoru ve Render
  xTaskCreatePinnedToCore(TaskEngine, "TaskEngine", 30000, NULL, 2, NULL, 0);
  
  // Core 1: TFT Ekran Basimi (Display)
  xTaskCreatePinnedToCore(TaskDisplay, "TaskDisplay", 10000, NULL, 2, NULL, 1);
  
  // Core 1: Radar (Dusuk Oncelik)
  xTaskCreatePinnedToCore(TaskRadar, "TaskRadar", 20000, NULL, 0, NULL, 1);
  
  // Core 1: Joystick polling (async analogRead)
  xTaskCreatePinnedToCore(TaskJoy, "TaskJoy", 2048, NULL, 1, NULL, 1);
  
  lastFrame = millis(); fpsTimer = millis();
}

// ============================================================
//  loop — FreeRTOS kullanıldığı için Arduino loop()'a ihtiyaç yok.
//  setup() sonunda vTaskDelete(NULL) ile bu task kendini siler; tüm iş
//  FreeRTOS task'larında (TaskEngine/Display/Radar/Joy) yürür.
// ============================================================
void loop() {
  // FreeRTOS kullanırken loop()'a ihtiyaç kalmaz, görevi tamamen siliyoruz
  vTaskDelete(NULL); 
}