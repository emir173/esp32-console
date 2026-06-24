// ============================================================
//  E-OS — PLATFORMER
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanli cift tamponlama (Flicker-Free)
//
//  Kontroller:
//    JOY_X  -> Sag/Sol hareket
//    BTN_A  -> Ziplama
//    BTN_B  -> OS Menu'ye don
//    Buzzer -> Ses efektleri
// ============================================================
// ============ Kutuphaneler ============
#include <TFT_eSPI.h>      // TFT LCD grafik kutuphanesi (ST7735 surucusu)
#include <SPI.h>           // SPI haberlesme (TFT icin kullanilir)
#include <Wire.h>          // I2C haberlesme (OLED icin)
#include <U8g2lib.h>       // OLED (SH1106) grafik kutuphanesi
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);  // OLED nesnesi (bu oyunda kullanilmiyor, hazir)
#include <Preferences.h>   // NVS (non-volatile storage) — high score kaydi icin
#include "../hardware_config.h"  // Donanim pin tanimlari (buton, joystick, buzzer)
#include "../dev_tools.h"        // Gelistirme araclari (screenshot, debug)
#include "../GameBase.h"         // OS ortak API (ses, ekran, vs.)

// ============ Ekran ============
#define SCR_W    160           // TFT ekran genisligi (piksel) — landscape mod
#define SCR_H    128           // TFT ekran yuksekligi (piksel)
#define HUD_H    10            // HUD (ust bilgi cubugu) yuksekligi
#define GAME_Y   HUD_H   // Oyun alani Y baslangici (HUD'in altindan baslar)

// ============ Tile Sistemi ============
#define TILE     8             // Bir tile'in piksel boyutu (8x8)
#define MAP_W    40            // Harita genisligi (tile sayisi) — 40*8=320 piksel
#define MAP_H    16            // Harita yuksekligi (tile sayisi) — 16*8=128 piksel

// Tile turleri — haritadaki her hucrenin anlamı
#define T_AIR    0              // Bosluk (hava) — gecilebilir
#define T_GROUND 1              // Cim zemin — katı, uzerine basilir
#define T_BRICK  2              // Tugla — katı, uzerine basilir
#define T_SPIKE  3              // Diken — dokunulunca olum
#define T_COIN   4              // Altin — toplanir, +10 puan
#define T_FLAG   5              // Bayrak — seviye bitimi
#define T_ENEMY  6   // Spawn noktasi (yukleme sirasinda dusman olusturulur)

// ============ Fizik (delta-time, 60fps-ready) ============ // FPS60: 30fps frame-based -> delta-time
// NOT: Tum hiz degerleri "orijinal frame" birimindedir. gameDT ile carpilinca
// 30fps veya 60fps'te ayni fizik sonucunu verir. DT_SCALE=30 => saniyedeki orijinal frame sayisi.
#define GRAVITY     0.55f     // FPS60: pixel/orijinal-frame² (DT_SCALE ile carpilinca saniyede 16.5px/s² eder)
#define JUMP_VEL   -5.6f      // FPS60: pixel/orijinal-frame (negatif = yukari)
#define MOVE_SPD    1.8f      // FPS60: pixel/orijinal-frame (yatay hareket hizi)
#define MAX_FALL    5.5f      // FPS60: pixel/orijinal-frame (terminal hiz — dusus suratni sinirlar)
#define JUMP_BUFFER 8         // FPS60: orijinal-frame karşılığı (8/30 = 0.267sn) — erken basışa izin
#define COYOTE_TIME 7         // FPS60: orijinal-frame karşılığı (7/30 = 0.233sn) — yerde iken havada zıplama

// FPS60: Sabit frame lock kaldırıldı, delta-time kullanılıyor
#define DT_SCALE    30.0f     // FPS60: Orijinal 30fps'ye göre zaman skalası (1.0 = 1 orijinal frame)
float gameDT = 0.0f;          // FPS60: global delta-time (orijinal-frame biriminde) — her frame yeniden hesaplanir

// ============ Oyuncu ============
#define PW 6   // Oyuncu genisligi (piksel)
#define PH 7   // Oyuncu yuksekligi (piksel) — TILE'dan biraz buyuk (2 tile temas)

// ============ Dusman ============
#define MAX_ENEMIES 8         // Ayni anda en fazla 8 dusman (RAM siniri)

// ============ Renkler (RGB565) ============
// RGB565 formatina donusturma makrosu: 5 bit R, 6 bit G, 5 bit B
#define RGB(r,g,b) ((uint16_t)(((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))

#define COL_SKY        RGB(25, 20, 65)    // Gokyuzu arka plan (koyu lacivert)
#define COL_GRASS_TOP  RGB(70, 200, 90)   // Cim ust yuzeyi (acik yesil)
#define COL_GRASS      RGB(45, 140, 55)   // Cim govdesi (koyu yesil)
#define COL_BRICK_A    RGB(185, 65, 50)   // Tugla ana renk (kirmizi-kahve)
#define COL_BRICK_B    RGB(145, 45, 35)   // Tugla golge/cizgi (koyu kahve)
#define COL_SPIKE      RGB(200, 200, 220) // Diken govdesi (acik gri)
#define COL_SPIKE_TIP  RGB(255, 60, 60)   // Diken ucu (kirmizi — tehlike vurgusu)
#define COL_COIN_A     RGB(255, 220, 40)  // Altin ana renk (sari)
#define COL_COIN_B     RGB(255, 255, 180) // Altin parlama (acik sari)
#define COL_FLAG_POLE  RGB(180, 180, 180) // Bayrak diregi (gri)
#define COL_FLAG_RED   RGB(255, 50, 50)   // Bayrak kumasi (kirmizi)
#define COL_PLR_A      RGB(80, 200, 255)  // Oyuncu govde (acik mavi)
#define COL_PLR_B      RGB(40, 130, 200)  // Oyuncu cerceve (koyu mavi)
#define COL_ENEMY_A    RGB(220, 60, 60)   // Dusman govde (kirmizi)
#define COL_ENEMY_B    RGB(170, 40, 40)   // Dusman cerceve (koyu kirmizi)
#define COL_WHITE      0xFFFF             // Tam beyaz (RGB565)
#define COL_BLACK      0x0000             // Tam siyah (RGB565)
#define COL_HUD_BG     RGB(10, 8, 20)     // HUD arka plan (cok koyu mavi)
#define COL_HUD_TXT    RGB(200, 200, 200) // HUD yazi rengi (acik gri)
#define COL_HUD_COIN   RGB(255, 220, 40)  // HUD altin ikonu (sari)

// ============ Nesneler ============
TFT_eSPI tft = TFT_eSPI();            // Ana TFT ekrani (donanima dogrudan cizim icin)
TFT_eSprite canvas = TFT_eSprite(&tft);  // Off-screen cift tamponlama sprite'i (flicker-free cizim)

// ============ Ses ============
bool soundEnabled = true;             // Ses acik/kapali bayragi (NVS'ten yuklenir)
// playSound — GameBase.h osPlaySound wrapper (eski API uyumu)
// Frekans (Hz) ve sure (ms) vererek buzzerdan ses cikarir.
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// returnToOS — Oyundan cikip E-OS ana menusune doner.
// Ekranı temizler, mesaj gosterir ve ESP'yi yeniden baslatir (OS boot eder).
void returnToOS() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(20, 60);
    tft.print("Ana Menuye Donuluyor...");
    delay(500);
    ESP.restart();
}

// ============ Oyun Durumu ============
// Durum makinesi — oyunun hangi ekraninda oldugunu belirler
enum State {
    ST_TITLE,        // Baslik/menu ekrani
    ST_PLAY,         // Aktif oyun (fizik/cizim calisir)
    ST_DEAD,         // Oyuncu oldu, kisa bekleme + respawn
    ST_LEVELCLEAR,   // Seviye tamamlandi, sonraki seviyeye gecis
    ST_GAMEOVER,     // Tum canlar bitti
    ST_WIN,          // Tum seviyeler bitirildi — kazandi
    ST_PAUSE         // Duraklatildi (JOY_SW ile toggle)
};
State state = ST_TITLE;  // Baslangicta baslik ekraninda

// ============ Oyuncu Struct ============
// Oyuncunun tum durum degiskenleri (pozisyon, hiz, can, vs.)
struct Player {
    float x, y;          // Piksel pozisyonu (sol ust kose)
    float vx, vy;        // Hiz vektoru (yatay/dikey, piksel/orijinal-frame)
    bool grounded;       // Yere degiyor mu? (ziplama izni icin)
    bool facingRight;    // Yon (goz cizimi icin)
    int lives;           // Kalan can sayisi (3 ile baslar)
    int coins;           // Toplanan altin sayisi
    int score;           // Toplam puan
    float invincTimer;  // FPS60: frame sayacı -> orijinal-frame birimi (60.0 = 2sn) — olumden sonra koruma
    float jumpBuf;      // FPS60: frame sayacı -> orijinal-frame birimi — ziplama tusuna erken basma suresi
    float coyoteT;      // FPS60: frame sayacı -> orijinal-frame birimi — kenardan sonraki ziplama izni
};
Player plr;

// ============ Dusman Struct ============
// Patrol yapan (iki nokta arasi gidip gelen) basit dusman
struct Enemy {
    float x, y;          // Piksel pozisyonu
    float vx;            // Yatay hiz (patrol yonu)
    float boundL, boundR; // Patrol sinirlari (sol/sag x koordinati)
    bool active;         // Hayatta mi? (stomp ile false olur)
};
Enemy enemies[MAX_ENEMIES];  // Dusman havuzu (MAX_ENEMIES adet)
int numEnemies = 0;          // Aktif dusman sayisi (seviyeye gore degisir)

// ============ Harita ============
uint8_t mapData[MAP_H][MAP_W];  // RAM'deki calisma kopyasi (tile'lar degistigince guncellenir)
int curLevel = 0;               // Su anki seviye indeksi (0-based)
#define NUM_LEVELS 3            // Toplam seviye sayisi

// ============ Kamera ============
int camX = 0;  // Kamera sol x koordinati (harita scroll icin) — oyuncuyu takip eder

// ============ Joystick ============
int joyCenterX, joyCenterY;  // Joystick sifir/kalibrasyon degerleri (baslangicta olculur)

// ============ Zamanlama ============
uint32_t lastFrameMs = 0;    // Bir onceki frame zaman damgasi (delta-time icin)
uint32_t animTick = 0;       // Animasyon sayaci (altin/bayrak dalgalanma, her frame artar)
uint32_t stateTimer = 0;     // Durum gecis zaman damgasi (olum/bekleme suresi icin)
uint32_t fpsFrameCount = 0;  // FPS sayaci (saniyede kac frame)
uint32_t fpsStartTime = 0;   // FPS olcum dongusu baslangici
int currentFPS = 0;          // Hesaplanan anlik FPS (HUD'da gosterilir)

// ============ Skor ============
int highScore = 0;  // En yuksek skor (NVS'ten yuklenir, kalici)

// ============================================================
//  SEVIYE VERILERI (PROGMEM)
//  0=hava, 1=cim, 2=tugla, 3=diken, 4=altin, 5=bayrak, 6=dusman
//  PROGMEM: Veriler flash'ta tutulur (RAM tasarrufu), pgm_read ile okunur.
//  Her satir MAP_W=40 tile'dan olusur, toplam MAP_H=16 satir.
// ============================================================
// ---- LEVEL 1: TUTORIAL (diken/dusman yok, merdiven ogretici) ----
// Merdiven kurali: her basamak 2 tile yukari, 1 tile yatay bosluk
// Zemin → R13(c19-22) → R11(c23-26) → R9(c27-30) → Bayrak R8(c30)
const uint8_t LVL1[MAP_H * MAP_W] PROGMEM = {
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 4
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 5
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 6
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0, // 7  Bayrak col30
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0, // 8  Coin rehber
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0, // 9  Basamak3 c27-31
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 10 Coin rehber
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0, // 11 Basamak2 c23-26
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 12 Coin rehber
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 13 Basamak1 c19-22
    0,0,0,4,0,4,0,4,0,0,0,4,0,4,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 14 Zemindeki coinler
    1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 15 Zemin (bosluk c8-9)
};

// ---- LEVEL 2: ORTA (dikenleri atla, 1 dusman, merdiven) ----
// Zemin → R13(c17-20) → R11(c21-24) → R9(c25-28) → R7(c29-32) → Bayrak R6(c31)
const uint8_t LVL2[MAP_H * MAP_W] PROGMEM = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 4
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 5
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,0, // 6  Bayrak c31
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0, // 7  Basamak4 c29-32
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0, // 8  Coin rehber
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0, // 9  Basamak3 c25-28
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 10 Coin + dusman
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 11 Basamak2 c21-24
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 12 Coin rehber
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 13 Basamak1 c17-20
    0,0,0,4,0,4,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 14 Zemindeki coinler
    1,1,1,1,1,1,1,1,3,3,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 15 Zemin (diken c8-9, bosluk c10-11)
};

// ---- LEVEL 3: ZOR (dikenler, 2 dusman, 2-tile bosluklu merdiven) ----
// Zemin → R13(c16-19) → R11(c21-24) → R9(c26-29) → R7(c31-34) → Bayrak R6(c33)
const uint8_t LVL3[MAP_H * MAP_W] PROGMEM = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 4
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 5
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0, // 6  Bayrak c33
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0, // 7  Basamak4 c31-34
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0, // 8  Coin rehber
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0, // 9  Basamak3 c26-29
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 10 Coin c22 + Dusman c24
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 11 Basamak2 c21-24
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 12 Coin rehber
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 13 Basamak1 c16-19
    0,0,0,4,0,4,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 14 Coinler
    1,1,1,1,1,1,3,3,0,0,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 15 Diken c6-7, bosluk c8-9, bosluk c19-21
};

// Seviye tablosu — PROGMEM'deki seviye dizilerine isaretci array'i
const uint8_t* const LEVELS[NUM_LEVELS] PROGMEM = { LVL1, LVL2, LVL3 };

// ============================================================
//  YARDIMCI FONKSIYONLAR
// ============================================================
// getTile — (col,row) koordinatindaki tile turunu dondurur.
// Harita sinirlari disinda T_GROUND (duvar) doner — oyuncu disari kacamaz.
uint8_t getTile(int col, int row) {
    if (col < 0 || col >= MAP_W || row < 0 || row >= MAP_H) return T_GROUND; // Sinir = duvar
    return mapData[row][col];
}

// isSolid — Verilen tile katı mi? (uzerine basilir, carpisma olur)
// Sadece T_GROUND ve T_BRICK katidir; diken/altin/bayrak gecilebilir.
bool isSolid(int col, int row) {
    uint8_t t = getTile(col, row);
    return (t == T_GROUND || t == T_BRICK);
}

// loadLevel — Verilen seviyeyi PROGMEM'den RAM'e (mapData) kopyalar.
// T_ENEMY tile'larini dusman objesine cevirir, oyuncuyu baslangica koyar.
void loadLevel(int lvl) {
    const uint8_t* src = (const uint8_t*)pgm_read_ptr(&LEVELS[lvl]);
    numEnemies = 0;

    for (int r = 0; r < MAP_H; r++) {
        for (int c = 0; c < MAP_W; c++) {
            uint8_t t = pgm_read_byte(&src[r * MAP_W + c]);
            if (t == T_ENEMY) {
                // Dusman spawn noktasi — T_ENEMY tile'i dusman objesine donusur
                if (numEnemies < MAX_ENEMIES) {
                    Enemy& e = enemies[numEnemies++];
                    e.x = c * TILE;            // Dusman x pozisyonu (piksel)
                    e.y = r * TILE;            // Dusman y pozisyonu (piksel)
                    e.vx = 0.5f;               // Baslangic patrol hizi
                    e.boundL = (c - 3) * TILE; // Sol patrol siniri (3 tile sol)
                    e.boundR = (c + 3) * TILE; // Sag patrol siniri (3 tile sag)
                    e.active = true;
                }
                mapData[r][c] = T_AIR;  // Spawn noktasi bosluk olur
            } else {
                mapData[r][c] = t;
            }
        }
    }

    // Oyuncu baslangic pozisyonu (sol alt kose)
    plr.x = 2 * TILE;        // 2. tile x'te
    plr.y = 14 * TILE;       // 14. tile y'de (zemin ustu)
    plr.vx = 0; plr.vy = 0;
    plr.grounded = false;
    plr.facingRight = true;
    plr.invincTimer = 60.0f;   // FPS60: float (orijinal-frame birimi, 60.0 = 2sn) — baslangicta koruma
    plr.jumpBuf = 0.0f;        // FPS60: float
    plr.coyoteT = 0.0f;        // FPS60: float
    camX = 0;                  // Kamera baslangicta solunda
}

// resetGame — Oyunu tamamen sifirlar (yeni oyun baslatir).
// Can, altin, skor sifirlanir, seviye 0 yuklenir.
void resetGame() {
    plr.lives = 3;       // 3 can ile basla
    plr.coins = 0;
    plr.score = 0;
    curLevel = 0;
    loadLevel(0);
}

// ============================================================
//  FİZİK VE CARPISMA
//  updatePhysics — Oyuncunun hareketini uygular ve tile ile carpisma cozer.
//  Oncelik yatay, sonra dikey (ayri ayri — tunneling onlenir).
//  Delta-time (gameDT) ile carpildigi icin fps-bagimsizdir.
// ============================================================
void updatePhysics() {
    // --- Yatay Hareket --- (once x, sonra carpisma coz)
    plr.x += plr.vx * gameDT;  // FPS60: dt ile carp

    // Sol carpisma — oyuncu sola giderken sol kenar tile'ini kontrol et
    if (plr.vx < 0) {
        int col = (int)plr.x / TILE;
        int rowT = (int)plr.y / TILE;                       // Ust satir
        int rowB = (int)(plr.y + PH - 1) / TILE;            // Alt satir
        if (isSolid(col, rowT) || isSolid(col, rowB)) {
            plr.x = (col + 1) * TILE;  // Tile sag kenarina yasla
            plr.vx = 0;
        }
    }
    // Sag carpisma — oyuncu saga giderken sag kenar tile'ini kontrol et
    if (plr.vx > 0) {
        int col = (int)(plr.x + PW - 1) / TILE;
        int rowT = (int)plr.y / TILE;
        int rowB = (int)(plr.y + PH - 1) / TILE;
        if (isSolid(col, rowT) || isSolid(col, rowB)) {
            plr.x = col * TILE - PW;  // Tile sol kenarina yasla (oyuncu genisligi kadar)
            plr.vx = 0;
        }
    }

    // --- Dikey Hareket --- (yer cekimi + dusus + zemin/tavan carpisma)
    plr.vy += GRAVITY * gameDT;  // FPS60: dt ile carp — yer cekimi ivmesi
    if (plr.vy > MAX_FALL) plr.vy = MAX_FALL;  // Terminal hiz siniri
    plr.y += plr.vy * gameDT;    // FPS60: dt ile carp

    plr.grounded = false;  // Her frame sifirla, asagida tekrar set edilir

    // Asagi carpisma (yere inis) — ayak altindaki tile'i prob et (jitter fix)
    if (plr.vy >= 0) {
        int colL = (int)plr.x / TILE;                       // Sol ayak
        int colR = (int)(plr.x + PW - 1) / TILE;            // Sag ayak
        int rowBelow = (int)(plr.y + PH) / TILE;   // ayagin tam altindaki satir
        if (isSolid(colL, rowBelow) || isSolid(colR, rowBelow)) {
            plr.y = rowBelow * TILE - PH;          // ayak zemin sinirina otursun
            plr.vy = 0;
            plr.grounded = true;
            plr.coyoteT = (float)COYOTE_TIME;      // FPS60: float cast — yere degince coyote sifirlanir
        }
    }
    // Yukari carpisma (tavana vurma) — kafa ustundeki tile'i kontrol et
    if (plr.vy < 0) {
        int rowT = (int)plr.y / TILE;
        int colL = (int)plr.x / TILE;
        int colR = (int)(plr.x + PW - 1) / TILE;
        if (isSolid(colL, rowT) || isSolid(colR, rowT)) {
            plr.y = (rowT + 1) * TILE;  // Tile alt kenarina yasla
            plr.vy = 0;                 // Yukari hiz sifirlanir (ziplama kesilir)
        }
    }

    // Harita sinirlari — oyuncu harita disina cikmasin
    if (plr.x < 0) { plr.x = 0; plr.vx = 0; }
    if (plr.x > MAP_W * TILE - PW) { plr.x = MAP_W * TILE - PW; plr.vx = 0; }

    // Dusme kontrolu (haritadan cikti) — bosluga dusunce olum
    // +4 piksel tolerans (tam sinirda false trigger onlenir)
    if (plr.y > MAP_H * TILE + 4) {
        playerDie();
    }

    // Coyote timer — FPS60: dt ile azalt
    // Havada iken coyoteT sayar; 0 olunca ziplama izni kalkar
    if (!plr.grounded && plr.coyoteT > 0.0f) plr.coyoteT -= gameDT;
}

// ============================================================
//  ALTIN + BAYRAK + DİKEN KONTROLU
//  checkTilePickups — Oyuncunun kapladigi tum tile'lari tarar.
//  Altin -> topla, Bayrak -> seviye bitir, Diken -> olum.
// ============================================================
void checkTilePickups() {
    // Oyuncunun doldugu tile'lari kontrol et (sol-ust ve sag-alt kose araligi)
    int c1 = (int)plr.x / TILE;
    int c2 = (int)(plr.x + PW - 1) / TILE;
    int r1 = (int)plr.y / TILE;
    int r2 = (int)(plr.y + PH - 1) / TILE;

    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            uint8_t t = getTile(c, r);
            if (t == T_COIN) {
                mapData[r][c] = T_AIR;  // Altini haritadan sil
                plr.coins++;
                plr.score += 10;        // Altin = 10 puan
                playSound(1200, 40);    // Altin sesi (yuksek frekans)
            }
            else if (t == T_FLAG) {
                // Seviye tamamlandi! — bayrak seviye bitimi tetikler
                state = ST_LEVELCLEAR;
                stateTimer = millis();
                plr.score += 100;       // Bayrak = 100 puan
                playSound(523, 100);    // Zafer sesi (Do nota)
            }
            else if (t == T_SPIKE && plr.invincTimer <= 0.0f) { // FPS60: float karsilastirma
                playerDie();            // Dikene dokununca olum (invinc degilse)
            }
        }
    }
}

// ============================================================
//  DUSMAN GUNCELLEME
//  updateEnemies — Dusmanlari patrol yaptirir, oyuncu carpisma cozer.
//  Stomp (ustten bas) -> dusman olur; yandan temas -> oyuncu olur.
// ============================================================
void updateEnemies() {
    for (int i = 0; i < numEnemies; i++) {
        if (!enemies[i].active) continue;  // Olu dusman atla
        Enemy& e = enemies[i];

        // Patrol hareketi — FPS60: dt ile carp
        // boundL/boundR arasinda gidip gelir, sinira gelince yon degistir
        e.x += e.vx * gameDT;
        if (e.x <= e.boundL || e.x >= e.boundR) e.vx = -e.vx;

        // Oyuncu ile carpisma — invincibility varsa atla
        if (plr.invincTimer > 0.0f) continue; // FPS60: float karsilastirma

        // AABB carpisma testi (dusman 7x7 piksel kabul edilir)
        bool overlapX = plr.x + PW > e.x && plr.x < e.x + 7;
        bool overlapY = plr.y + PH > e.y && plr.y < e.y + 7;

        if (overlapX && overlapY) {
            // Oyuncu dusmanin ustunden mi geliyor? (yukari dusus + kafa ustunde)
            if (plr.vy > 0 && plr.y + PH < e.y + 5) {
                // Stomp! Dusman oldu — ziplama ile uste basildi
                e.active = false;
                plr.vy = JUMP_VEL * 0.6f; // Ziplama (tam ziplamadan biraz kisa)
                plr.score += 50;          // Stomp = 50 puan
                playSound(400, 60);       // Stomp sesi
            } else {
                // Oyuncu hasar aldi — yandan/alttan temas
                playerDie();
            }
        }
    }
}

// ============================================================
//  OYUNCU OLUMU
//  playerDie — Can azaltir, olum sesi, state gecisi.
//  Son can bitince ST_GAMEOVER, degilse ST_DEAD (respawn).
//  Yeni high score varsa NVS'e kaydeder.
// ============================================================
void playerDie() {
    if (plr.invincTimer > 0.0f) return; // FPS60: float karsilastirma — invincible ise olmez
    plr.lives--;
    playSound(200, 300);  // Olum sesi (alcak frekans, uzun)

    if (plr.lives <= 0) {
        // Son can bitti — oyun bitti
        state = ST_GAMEOVER;
        stateTimer = millis();
        // High score guncelle + NVS'e kaydet (kalici)
        if (plr.score > highScore) {
            highScore = plr.score;
            Preferences prefs;
            prefs.begin("platf", false);   // "platf" namespace, yazma modu
            prefs.putInt("hi", highScore);
            prefs.end();
        }
    } else {
        // Hali var — kisa bekleme sonrasi respawn
        state = ST_DEAD;
        stateTimer = millis();
    }
}

// ============================================================
//  CİZİM — TILE'LAR
//  drawTiles — Kamera (camX) ile gorunen tile'lari canvas'a cizer.
//  Sadece ekranda gorunen kolonlari cizer (performans optimizasyonu).
//  Her tile turu (cim/tugla/diken/altin/bayrak) ayri stil ile cizilir.
// ============================================================
void drawTiles() {
    int startCol = camX / TILE;                              // Gorunen ilk kolon
    int endCol = min(startCol + SCR_W / TILE + 2, MAP_W);    // Gorunen son kolon (+2 tolerans)

    for (int r = 0; r < MAP_H; r++) {
        int sy = r * TILE;  // Tile'in ekran y koordinati
        if (sy + TILE < 0 || sy >= SCR_H) continue;  // Ekranda degilse atla

        for (int c = startCol; c < endCol; c++) {
            int sx = c * TILE - camX;  // Tile'in ekran x koordinati (kamera offset)
            if (sx + TILE < 0 || sx >= SCR_W) continue;  // Ekranda degilse atla

            uint8_t t = mapData[r][c];
            if (t == T_AIR) continue;  // Bosluk cizilmez

            if (t == T_GROUND) {
                // Cim zemin — koyu govde + acik ust yuzey
                canvas.fillRect(sx, sy, TILE, TILE, COL_GRASS);
                canvas.drawFastHLine(sx, sy, TILE, COL_GRASS_TOP);
                canvas.drawFastHLine(sx, sy + 1, TILE, COL_GRASS_TOP);
                // Kucuk cim detaylari — her 3 tile'de bir parlama pikseli
                if ((c + r) % 3 == 0) canvas.drawPixel(sx + 3, sy, RGB(90, 230, 110));
            }
            else if (t == T_BRICK) {
                // Tugla — ana dolgu + cizgi deseni
                canvas.fillRect(sx, sy, TILE, TILE, COL_BRICK_A);
                // Tugla deseni — yatay+dusey cizgiler ile derzer izlenimi
                canvas.drawFastHLine(sx, sy + 3, TILE, COL_BRICK_B);
                canvas.drawFastVLine(sx + 3, sy, 3, COL_BRICK_B);
                canvas.drawFastVLine(sx + 6, sy + 4, 4, COL_BRICK_B);
            }
            else if (t == T_SPIKE) {
                // Ucgen diken — satir satira genisleyen cizgiler ile ucgen
                for (int dy = 0; dy < TILE; dy++) {
                    int hw = dy * TILE / (2 * TILE);  // Yari genislik (yukseklik arttikca genisler)
                    int cx = sx + TILE / 2;            // Merkez x
                    canvas.drawFastHLine(cx - hw, sy + dy, hw * 2 + 1, COL_SPIKE);
                }
                // Kirmizi ucu — tehlike vurgusu
                canvas.drawPixel(sx + TILE/2, sy, COL_SPIKE_TIP);
                canvas.drawPixel(sx + TILE/2, sy + 1, COL_SPIKE_TIP);
            }
            else if (t == T_COIN) {
                // Altin — yanip sonen (sparkle) animasyonlu daire
                bool sparkle = (animTick / 12) % 2;  // FPS60: 6->12 (60fps'de ayni animasyon hizi)
                uint16_t cc = sparkle ? COL_COIN_B : COL_COIN_A;
                canvas.fillCircle(sx + 3, sy + 3, 2, cc);  // 2 piksel yaricap daire
                canvas.drawPixel(sx + 2, sy + 2, COL_COIN_B);  // Parlama noktasi
            }
            else if (t == T_FLAG) {
                // Bayrak diregi — gri dusey cizgi
                canvas.drawFastVLine(sx + 1, sy, TILE, COL_FLAG_POLE);
                // Bayrak kumaşı (dalgalanan) — animTick ile yukari/asagi oynar
                int wave = (animTick / 8) % 2;  // FPS60: 4->8 (60fps'de ayni animasyon hizi)
                canvas.fillRect(sx + 2, sy + wave, 5, 4, COL_FLAG_RED);
            }
        }
    }
}

// ============================================================
//  CİZİM — OYUNCU
//  drawPlayer — Oyuncu karakterini canvas'a cizer.
//  Yanip sonme (invincibility) efekti, yone gore goz konumu.
// ============================================================
void drawPlayer() {
    int sx = (int)plr.x - camX;  // Kamera offset ile ekran koordinati
    int sy = (int)plr.y;

    // Yanip sonme (invincibility) — FPS60: float -> int cast
    // invincTimer / 3 ile 3-frame blok, %2 ile yan/son — yarisi gizlenir
    if (plr.invincTimer > 0.0f && ((int)plr.invincTimer / 3) % 2) return;

    // Govde — dolu kare + cerceve
    canvas.fillRect(sx, sy, PW, PH, COL_PLR_A);
    canvas.drawRect(sx, sy, PW, PH, COL_PLR_B);

    // Goz — yone gore sol veya sagda
    int eyeX = plr.facingRight ? sx + 3 : sx + 1;
    canvas.fillRect(eyeX, sy + 2, 2, 2, COL_WHITE);
    canvas.drawPixel(eyeX + (plr.facingRight ? 1 : 0), sy + 3, COL_BLACK);  // Bebeğin icine siyah nokta
}

// ============================================================
//  CİZİM — DUSMANLAR
//  drawEnemies — Aktif dusmanlari cizer (ekrandan cikani atlar).
// ============================================================
void drawEnemies() {
    for (int i = 0; i < numEnemies; i++) {
        if (!enemies[i].active) continue;
        Enemy& e = enemies[i];
        int sx = (int)e.x - camX;  // Kamera offset
        int sy = (int)e.y;
        if (sx < -8 || sx > SCR_W + 8) continue;  // Ekranda degilse atla (culling)

        // Govde — kirmizi dolu kare + cerceve
        canvas.fillRect(sx, sy, 7, 7, COL_ENEMY_A);
        canvas.drawRect(sx, sy, 7, 7, COL_ENEMY_B);
        // Gozler — iki beyaz kare + siyah bebeğin
        canvas.fillRect(sx + 1, sy + 2, 2, 2, COL_WHITE);
        canvas.fillRect(sx + 4, sy + 2, 2, 2, COL_WHITE);
        canvas.drawPixel(sx + 2, sy + 3, COL_BLACK);
        canvas.drawPixel(sx + 5, sy + 3, COL_BLACK);
    }
}

// ============================================================
//  CİZİM — HUD
//  drawHUD — Ust bilgi cubugu: skor, altin, FPS, can.
//  HUD_H piksel yuksekliginde, ekranin tepesinde.
// ============================================================
void drawHUD() {
    canvas.fillRect(0, 0, SCR_W, HUD_H, COL_HUD_BG);  // HUD arka plani
    canvas.setTextSize(1);  // 1x font (6x8 piksel)

    // Skor — sol ustte
    canvas.setTextColor(COL_HUD_TXT);
    canvas.setCursor(2, 1);
    canvas.printf("SKR:%d", plr.score);

    // Altin — skor yaninda (sari ikon + sayi)
    canvas.setTextColor(COL_HUD_COIN);
    canvas.setCursor(55, 1);
    canvas.printf("x%d", plr.coins);
    canvas.fillCircle(50, 4, 2, COL_COIN_A);  // Altin ikonu

    // FPS — ortada (kirmizi)
    canvas.setTextColor(RGB(255, 50, 50));
    char fpsStr[16];
    snprintf(fpsStr, sizeof(fpsStr), "FPS:%d", currentFPS);
    canvas.setCursor(80, 1);
    canvas.print(fpsStr);

    // Can (Kirmizi Kareler - Artik MADCTL duzeltmesiyle dogru renkte)
    // Her can icin bir kirmizi kare, saga dogru sirali
    for (int i = 0; i < plr.lives; i++) {
        canvas.fillRect(125 + i * 10, 2, 6, 6, RGB(255, 60, 60));  // 10 piksel aralik
    }

    // HUD alt cizgisi — oyun alanindan ayiran ince cizgi
    canvas.drawFastHLine(0, HUD_H - 1, SCR_W, RGB(40, 40, 60));
}

// ============================================================
//  BASLIK EKRANI
//  drawTitle — Acilis/menu ekrani: baslik, demo karakter, zemin, menuler.
//  [A] basilarak oyun baslar, [B] ile OS menu'ye donulur.
// ============================================================
void drawTitle() {
    canvas.fillSprite(COL_SKY);  // Gokyuzu arka plan

    // Baslik Ortalanmis — once golge (koyu mavi), sonra ana (acik mavi)
    canvas.setTextSize(2);  // 2x font (12x16 piksel)
    const char* title = "PLATFORM";
    int tw = strlen(title) * 12;  // Metin genisligi (her karakter 12 piksel)
    canvas.setTextColor(RGB(20, 60, 120));  // Golge rengi
    canvas.setCursor((SCR_W - tw)/2 + 1, 21);  // Golge 1 piksel saga/asagi kayik
    canvas.print(title);

    canvas.setTextColor(COL_PLR_A);  // Ana baslik rengi
    canvas.setCursor((SCR_W - tw)/2, 20);
    canvas.print(title);

    // Demo karakter (Ortalanmis) — 2x boyutunda gosterim
    int demoX = (SCR_W - (PW * 2)) / 2;
    int demoY = 55;
    canvas.fillRect(demoX, demoY, PW * 2, PH * 2, COL_PLR_A);
    canvas.drawRect(demoX, demoY, PW * 2, PH * 2, COL_PLR_B);
    canvas.fillRect(demoX + 7, demoY + 3, 3, 3, COL_WHITE);  // Goz
    canvas.drawPixel(demoX + 9, demoY + 5, COL_BLACK);       // Bebeğin

    // Zemin dekorasyon — alt sira cim tile'lari
    for (int x = 0; x < SCR_W; x += TILE) {
        canvas.fillRect(x, 78, TILE, TILE, COL_GRASS);
        canvas.drawFastHLine(x, 78, TILE, COL_GRASS_TOP);
    }

    // Alt Menuler — [A] basla (sol), [B] OS menu (sag)
    canvas.setTextSize(1);
    const char* txtA = "[A] Basla";
    canvas.setTextColor(COL_WHITE);
    canvas.setCursor(10, 95);
    canvas.print(txtA);

    const char* txtB = "[B] OS Menu";
    canvas.setTextColor(0xBDF7);  // Acik mavi (RGB565)
    canvas.setCursor(SCR_W - (strlen(txtB)*6) - 10, 95);
    canvas.print(txtB);

    // Rekor — varsa ortada goster
    if (highScore > 0) {
        canvas.setTextColor(COL_HUD_COIN);
        char recStr[32];
        snprintf(recStr, sizeof(recStr), "Rekor: %d", highScore);
        canvas.setCursor((SCR_W - strlen(recStr)*6)/2, 110);
        canvas.print(recStr);
    }

    checkScreenshot(canvas);  // Gelistirme: ekran goruntusu yakalama (varsa)
    canvas.pushSprite(0, 0);  // Off-screen canvas'i TFT'ye transfer et (flicker-free)
}

// ============================================================
//  GAME OVER EKRANI
//  drawGameOver — Oyun bitti ekrani: cerceve, skor, altin, rekor, menuler.
// ============================================================
void drawGameOver() {
    canvas.fillSprite(COL_BLACK);  // Siyah arka plan

    // Genisletilmis Premium Kırmızı Çerçeve — dolgu + 2 cizgi katmani
    canvas.fillRoundRect(8, 10, 144, 108, 5, 0x2104);  // Koyu dolgu (RGB565)
    canvas.drawRoundRect(8, 10, 144, 108, 5, RGB(255, 50, 50));  // Dis kirmizi cerceve
    canvas.drawRoundRect(9, 11, 142, 106, 4, 0x8000);  // Ic koyu kirmizi cerceve

    // Baslik — kirmizi "OYUN BITTI"
    canvas.setTextSize(2);
    canvas.setTextColor(RGB(255, 50, 50));
    const char* title = "OYUN BITTI";
    int tw = strlen(title) * 12;
    canvas.setCursor((SCR_W - tw)/2, 18);
    canvas.print(title);

    // Skor — etiket (kucuk) + deger (buyuk)
    canvas.setTextSize(1);
    canvas.setTextColor(COL_WHITE);
    canvas.setCursor(25, 46);
    canvas.print("Skor: ");
    canvas.setTextSize(2);
    canvas.setTextColor(COL_HUD_TXT);
    canvas.setCursor(70, 40);
    canvas.printf("%d", plr.score);

    // Altin
    canvas.setTextSize(1);
    canvas.setTextColor(COL_WHITE);
    canvas.setCursor(25, 66);
    canvas.print("Altin:");
    canvas.setTextSize(2);
    canvas.setTextColor(COL_HUD_COIN);
    canvas.setCursor(70, 60);
    canvas.printf("%d", plr.coins);

    // Rekor — yesil
    canvas.setTextSize(1);
    canvas.setTextColor(COL_WHITE);
    canvas.setCursor(25, 86);
    canvas.print("Rekor:");
    canvas.setTextSize(2);
    canvas.setTextColor(RGB(100, 255, 100));
    canvas.setCursor(70, 80);
    canvas.printf("%d", highScore);

    // Alt Menu — [A] tekrar, [B] OS menu
    canvas.setTextSize(1);
    canvas.setTextColor(COL_WHITE);
    canvas.setCursor(15, 104);
    canvas.print("[A] Tekrar");
    canvas.setTextColor(0xBDF7);
    canvas.setCursor(80, 104);
    canvas.print("[B] OS Menu");

    checkScreenshot(canvas);  // Gelistirme: ekran goruntusu yakalama
    canvas.pushSprite(0, 0);  // Canvas'i TFT'ye yansit
}

// ============================================================
//  KAZANMA EKRANI
//  drawWin — Tum seviyeler bitirildi, yesil tema ile zafer ekrani.
// ============================================================
void drawWin() {
    canvas.fillSprite(COL_BLACK);

    // Genisletilmis Premium Yesil Çerçeve — dolgu + 2 cizgi katmani
    canvas.fillRoundRect(8, 10, 144, 108, 5, RGB(0, 30, 0));  // Koyu yesil dolgu
    canvas.drawRoundRect(8, 10, 144, 108, 5, RGB(80, 255, 80));  // Dis yesil cerceve
    canvas.drawRoundRect(9, 11, 142, 106, 4, RGB(0, 100, 0));  // Ic yesil cerceve

    // Baslik — yesil "KAZANDIN!"
    canvas.setTextSize(2);
    canvas.setTextColor(RGB(80, 255, 80));
    const char* title = "KAZANDIN!";
    int tw = strlen(title) * 12;
    canvas.setCursor((SCR_W - tw)/2, 18);
    canvas.print(title);

    // Skor
    canvas.setTextSize(1);
    canvas.setTextColor(COL_WHITE);
    canvas.setCursor(25, 46);
    canvas.print("Skor: ");
    canvas.setTextSize(2);
    canvas.setTextColor(COL_HUD_TXT);
    canvas.setCursor(70, 40);
    canvas.printf("%d", plr.score);

    // Altin
    canvas.setTextSize(1);
    canvas.setTextColor(COL_WHITE);
    canvas.setCursor(25, 66);
    canvas.print("Altin:");
    canvas.setTextSize(2);
    canvas.setTextColor(COL_HUD_COIN);
    canvas.setCursor(70, 60);
    canvas.printf("%d", plr.coins);

    // Rekor — altin/sari
    canvas.setTextSize(1);
    canvas.setTextColor(COL_WHITE);
    canvas.setCursor(25, 86);
    canvas.print("Rekor:");
    canvas.setTextSize(2);
    canvas.setTextColor(RGB(255, 220, 0));
    canvas.setCursor(70, 80);
    canvas.printf("%d", highScore);

    // Alt Menu — [A] tekrar, [B] OS menu
    canvas.setTextSize(1);
    canvas.setTextColor(COL_WHITE);
    canvas.setCursor(15, 104);
    canvas.print("[A] Tekrar");
    canvas.setTextColor(0xBDF7);
    canvas.setCursor(80, 104);
    canvas.print("[B] OS Menu");

    checkScreenshot(canvas);  // Gelistirme: ekran goruntusu yakalama
    canvas.pushSprite(0, 0);  // Canvas'i TFT'ye yansit
}

// ============================================================
//  SETUP
//  setup — Donanim baslatma, pin ayari, NVS yukleme, TFT/sprite hazirligi.
//  Arduino boot sirasinda bir kez calisir.
// ============================================================
void setup() {
    // Buzzer susturma — baslangicta sessiz
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);

    // OLED kapatma (hizli baslatma) — bu oyunda OLED kullanilmiyor
    Wire.begin(8, 9);  // I2C pinleri (SDA=8, SCL=9)
    Wire.beginTransmission(0x3C);  // OLED I2C adresi
    Wire.write(0x00);   // Komut modu
    Wire.write(0xAE);   // OLED display OFF komutu
    Wire.endTransmission();

    // OTA guvenlik — sonraki OS boot icin update partition'ı sec
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    // Pin ayarlari — butonlar pull-up (basili = LOW)
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP);
    pinMode(BTN_D, INPUT_PULLUP);
    pinMode(JOY_SW, INPUT_PULLUP);  // Joystick basma (click)

    // Ses ayari NVS'ten — "os" namespace, sadece okuma
    { Preferences prefs; prefs.begin("os", true); soundEnabled = prefs.getBool("sound_en", true); prefs.end(); }

    // High score NVS'ten — "platf" namespace, sadece okuma
    { Preferences prefs; prefs.begin("platf", true); highScore = prefs.getInt("hi", 0); prefs.end(); }

    // SPI — TFT icin donanim SPI pinleri
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);  // -1 = CS pin TFT_eSPI tarafindan yonetilir

    initDevTools(tft);  // Gelistirme araclari baslatma (screenshot, debug)

    // TFT — ST7735 baslatma + landscape (yatay) mod
    tft.init();
    tft.setRotation(1);  // 1 = landscape (160x128)

    // Renk duzeltmesi (MADCTL - RGB modu aktif eder, kirmizi/mavi tersligini onler)
    // 0x36 = MADCTL komutu, 0xA0 = RGB modu + yon bitleri
    tft.startWrite();
    tft.writecommand(0x36);
    tft.writedata(0xA0);
    tft.endWrite();

    setScreenshotMode(SCR_RGB_SWAP);  // Screenshot icin RGB swap modu (TFT renk sirasi)
    tft.fillScreen(TFT_BLACK);  // Ekran temizle

    // Sprite tamponu — off-screen canvas (cift tamponlama, flicker-free)
    canvas.setColorDepth(16);  // 16-bit RGB565
    canvas.createSprite(SCR_W, SCR_H);  // 160x128 sprite olustur

    // Joystick kalibrasyon — acilista sifir pozisyonu olc
    joyCenterX = analogRead(JOY_X);
    joyCenterY = analogRead(JOY_Y);

    // Rastgele tohum — analog gurultu + micros karisimi (deterministik olmayan)
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    state = ST_TITLE;       // Baslangicta baslik ekraninda
    lastFrameMs = millis(); // Delta-time baslangici
    fpsStartTime = millis(); // FPS olcum dongusu baslangici
}

// ============================================================
//  LOOP — Delta-Time (60fps-ready, frame lock KALDIRILDI)
//  loop — Ana oyun dongusu. Her cagride bir frame isler.
//  Delta-time ile fps-bagimsiz fizik; durum makinesi ile ekran yonetimi.
// ============================================================
void loop() {
    uint32_t now = millis();  // Su anki zaman (ms)

    // FPS60: Delta-Time hesaplama (orijinal 30fps'ye gore skalali)
    // gameDT=1.0 = 1 orijinal frame (33ms). 60fps'te gameDT≈0.5.
    float rawDT = (now - lastFrameMs) / 1000.0f;  // Saniye cinsinden gecen sure
    gameDT = rawDT * DT_SCALE;                     // Orijinal-frame birimine cevir
    if (gameDT > 1.5f) gameDT = 1.5f;  // FPS60: Lag spike korumasi (50ms orijinal esdeger) — buyuk adimi onle
    lastFrameMs = now;
    animTick++;  // Animasyon sayaci (altin/bayrak dalgalanma)

    // --- BTN_B: OS'a don --- (sadece menu/gameover/win ekranlarinda)
    if (!digitalRead(BTN_B) && (state == ST_TITLE || state == ST_GAMEOVER || state == ST_WIN)) {
        returnToOS();
    }

    // --- BTN_A: kenar tetikleme (basildi anini yakala) ---
    bool btnA = !digitalRead(BTN_A);  // Pull-up: basili = LOW = true
    static bool prevA = false;
    bool btnA_pressed = (btnA && !prevA);  // Yukselan kenar — sadece basildiği ilk frame
    prevA = btnA;

    // ---- FPS Hesaplama — saniyede bir guncelle ----
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {  // 1 saniye gecti
        currentFPS = fpsFrameCount;    // O saniyedeki frame sayisi = FPS
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // ---- JOY_SW: Pause toggle — joystick basma ile duraklat ----
    static bool prevJoySw = true;  // Pull-up varsayilan = HIGH
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {  // Yukselden dusen kenar (basildi)
        if (state == ST_PLAY) {
            state = ST_PAUSE;
            playSound(400, 50);   // Pause sesi
            noTone(BUZZER);        // Calsan ses varsa durdur
        }
    }
    prevJoySw = currJoySw;

    // --- Joystick --- yatay eksen oku, deadzone uygula
    float jx = (analogRead(JOY_X) - joyCenterX) / 2048.0f;  // -1..+1 normalize
    if (fabsf(jx) < 0.15f) jx = 0;  // Deadzone — kucuk sapmalar yok say (15%)

    // === DURUM MAKİNESİ === — her state icin ayri isleme
    switch (state) {

    case ST_TITLE:
        // Baslik ekrani — ciz + [A] ile oyun baslat
        drawTitle();
        if (btnA_pressed) {
            resetGame();        // Yeni oyun (can/skor sifir, seviye 0)
            state = ST_PLAY;
            playSound(660, 80); // Baslangic sesi (Mi nota)
        }
        break;

    case ST_PLAY:
        // --- Giris --- — joystick ile yatay hareket + surunme
        // Yatay hareket (jx deadzone zaten uygulandi)
        if (jx < 0) { plr.vx = -MOVE_SPD; plr.facingRight = false; }
        else if (jx > 0) { plr.vx = MOVE_SPD; plr.facingRight = true; }
        // FPS60: Surunme (friction) dt-duyarli hale getirildi.
        // vx *= pow(0.6, dt) yerine lineer yaklasim: vx *= 1.0 - 0.4*gameDT
        // 30fps'te (gameDT=1): vx *= 0.6; 60fps'te (gameDT=0.5): vx *= 0.8 (2 frame'de 0.64≈0.6)
        else { plr.vx *= (1.0f - 0.4f * gameDT); if (fabsf(plr.vx) < 0.3f) plr.vx = 0; }  // 0.3 = durma esigi

        // Ziplama buffer — FPS60: dt ile azalt/arttir
        // Tusa erken basilsa bile bir sure hatirla (grounded olunca ziplar)
        if (btnA_pressed) plr.jumpBuf = (float)JUMP_BUFFER;
        if (plr.jumpBuf > 0.0f) plr.jumpBuf -= gameDT;

        // Ziplama (coyote time + jump buffer) — FPS60: float karsilastirma
        // Yerde VEYA coyote suresi icinde + buffer dolu -> zipla
        if (plr.jumpBuf > 0.0f && (plr.grounded || plr.coyoteT > 0.0f)) {
            plr.vy = JUMP_VEL;
            plr.grounded = false;
            plr.coyoteT = 0.0f;   // Coyote tuketildi
            plr.jumpBuf = 0.0f;   // Buffer tuketildi
            playSound(800, 25);   // Ziplama sesi
        }

        // Variable jump: A birakilinca yukselisi kis
        // Tusu erken birak -> kisa ziplama; basili tut -> tam ziplama
        if (!btnA && plr.vy < -2.0f) plr.vy = -2.0f;  // -2.0 = minimum ziplama hizi

        // Invincibility timer — FPS60: dt ile azalt (olum sonrasi koruma sayaci)
        if (plr.invincTimer > 0.0f) plr.invincTimer -= gameDT;

        // --- Fizik --- — hareket + carpisma + etkilesim
        updatePhysics();       // Hareket ve tile carpisma
        checkTilePickups();    // Altin/bayrak/diken
        updateEnemies();       // Dusman patrol + carpisma

        // --- Kamera --- — oyuncuyu ekran ortasinda tut, sinirlari uygula
        camX = (int)plr.x - SCR_W / 2 + PW / 2;  // Oyuncu merkezli kamera
        if (camX < 0) camX = 0;                  // Sol sinir (harita basi)
        if (camX > MAP_W * TILE - SCR_W) camX = MAP_W * TILE - SCR_W;  // Sag sinir (harita sonu)

        // --- Cizim --- — katman sirasi: sky -> tiles -> enemies -> player -> HUD
        canvas.fillSprite(COL_SKY);  // Arka plan
        drawTiles();
        drawEnemies();
        drawPlayer();
        drawHUD();
        checkScreenshot(canvas);  // Gelistirme: ekran goruntusu yakalama (aktif oyun)
        canvas.pushSprite(0, 0);  // Canvas'i TFT'ye transfer et
        break;

    case ST_DEAD:
        // Kisa bekleme, sonra respawn — 1000ms sonra ayni seviyeyi yeniden yukle
        if (now - stateTimer > 1000) {
            loadLevel(curLevel);  // Ayni seviyeyi baslangictan yukle
            state = ST_PLAY;
        }
        // Olum ekrani — harita + HUD + kalan can goster
        canvas.fillSprite(COL_SKY);
        drawTiles();
        drawHUD();
        canvas.setTextSize(1);
        canvas.setTextColor(RGB(255, 80, 80));
        canvas.setCursor(50, 55);
        canvas.printf("CAN: %d", plr.lives);  // Kalan can sayisi
        checkScreenshot(canvas);  // Gelistirme: ekran goruntusu yakalama (olum ekranı)
        canvas.pushSprite(0, 0);
        break;

    case ST_LEVELCLEAR:
        // Seviye tamamlandi — 2000ms goster, sonra sonraki seviyeye gec
        if (now - stateTimer > 2000) {
            curLevel++;
            if (curLevel >= NUM_LEVELS) {
                // Tum seviyeler bitti — KAZANMA
                state = ST_WIN;
                stateTimer = now;
                // High score guncelle + kaydet
                if (plr.score > highScore) {
                    highScore = plr.score;
                    Preferences prefs;
                    prefs.begin("platf", false);
                    prefs.putInt("hi", highScore);
                    prefs.end();
                }
            } else {
                // Sonraki seviyeyi yukle
                loadLevel(curLevel);
                state = ST_PLAY;
            }
        }
        // Seviye tamamlandi ekrani — harita + HUD + kutlama kutusu
        canvas.fillSprite(COL_SKY);
        drawTiles();
        drawHUD();
        
        // Premium Tamamlandi Kutusu — mavi dolgu + cerceve
        canvas.fillRoundRect(20, 35, 120, 50, 5, RGB(0, 0, 50));
        canvas.drawRoundRect(20, 35, 120, 50, 5, RGB(100, 150, 255));
        
        canvas.setTextSize(1);
        canvas.setTextColor(RGB(150, 255, 150));  // Yesil baslik
        char lvlStr[32];
        snprintf(lvlStr, sizeof(lvlStr), "SEVIYE %d TAMAM!", curLevel + 1);  // 1-based gosterim
        canvas.setCursor((SCR_W - strlen(lvlStr)*6)/2, 45);  // Ortalanmis
        canvas.print(lvlStr);
        
        // Anlik skor gosterimi
        canvas.setTextColor(COL_WHITE);
        canvas.setCursor(40, 65);
        canvas.print("Skor: ");
        canvas.setTextColor(COL_HUD_COIN);
        canvas.setCursor(75, 65);
        canvas.printf("%d", plr.score);
        
        checkScreenshot(canvas);  // Gelistirme: ekran goruntusu yakalama (seviye tamamlandi)
        canvas.pushSprite(0, 0);
        break;

    case ST_GAMEOVER:
        // Oyun bitti ekrani — [A] ile basliga don
        drawGameOver();
        if (btnA_pressed) {
            state = ST_TITLE;
            playSound(440, 60);  // La nota
        }
        break;

    case ST_WIN:
        // Kazanma ekrani — [A] ile basliga don
        drawWin();
        if (btnA_pressed) {
            state = ST_TITLE;
            playSound(440, 60);
        }
        break;

    case ST_PAUSE:
        // Premium PAUSE Menüsü — yesil tema, ortalanmis kutu
        canvas.fillRoundRect(25, 35, 110, 60, 5, RGB(0, 30, 0));  // Koyu yesil dolgu
        canvas.drawRoundRect(25, 35, 110, 60, 5, RGB(0, 255, 0)); // Yesil cerceve
        
        canvas.setTextSize(2);  // Buyuk "PAUSE" basligi
        canvas.setTextColor(RGB(255, 255, 0));  // Sari
        canvas.setCursor(50, 42); 
        canvas.print("PAUSE");
        
        // Menu secenekleri
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_WHITE);
        const char* pTxtA = "[A] Devam Et";
        canvas.setCursor((SCR_W - strlen(pTxtA)*6) / 2, 65);  // Ortalanmis
        canvas.print(pTxtA);
        
        canvas.setTextColor(0xBDF7);  // Acik mavi
        const char* pTxtB = "[B] OS Menu";
        canvas.setCursor((SCR_W - strlen(pTxtB)*6) / 2, 78);
        canvas.print(pTxtB);
        
        checkScreenshot(canvas);  // Gelistirme: ekran goruntusu yakalama (pause menusu)
        canvas.pushSprite(0, 0);
        
        // [A] ile devam — pause'dan cik
        if (btnA_pressed) {
            playSound(800, 50);  // Devam sesi
            delay(100);  // FPS60: 200ms -> 100ms (buton debounce icin max 100ms) — cift tetiklenmeyi onle
            state = ST_PLAY;
            lastFrameMs = millis(); // FPS60: Delta-time reset (pause sonrasi spike onlenir)
        }
        break;
    }
}
