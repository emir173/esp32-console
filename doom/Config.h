#pragma once
// ============================================================
//  Config.h — E-OS DOOM: Proje geneli sabitler, enum, struct,
//  harita dizileri ve dış bildirimler
// ============================================================

#include <TFT_eSPI.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <U8g2lib.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <Preferences.h>

#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"

// ════════════════════════════════════════════════════════════
//  EKRAN GEOMETRİSİ
// ════════════════════════════════════════════════════════════
constexpr int SCR_W     = 160;
constexpr int SCR_H     = 128;
constexpr int SW        = 160;
constexpr int SH        = 104;

// ════════════════════════════════════════════════════════════
//  DOKU / TEXTURE SİSTEMİ
// ════════════════════════════════════════════════════════════
constexpr int TEX_W     = 64;
constexpr int TEX_H     = 64;
constexpr int MAX_TEX   = 56;

// ════════════════════════════════════════════════════════════
//  HARİTA
// ════════════════════════════════════════════════════════════
constexpr int MW        = 32;
constexpr int MH        = 32;

// ════════════════════════════════════════════════════════════
//  SPRITE / DÜŞMAN LİMİTLERİ
// ════════════════════════════════════════════════════════════
constexpr int NUM_SPRITES   = 140;
constexpr int MAX_PROJECTILES = NUM_SPRITES;

// ════════════════════════════════════════════════════════════
//  SNAKE MİNİ OYUN
// ════════════════════════════════════════════════════════════
constexpr int SNAKE_W       = 32;
constexpr int SNAKE_H       = 26;
constexpr int SNAKE_MAX     = 200;

// ════════════════════════════════════════════════════════════
//  ZAMANLAMA
// ════════════════════════════════════════════════════════════
constexpr int FRAME_MS      = 16;

// ════════════════════════════════════════════════════════════
//  MESAFE SİSİ & ZEMİN GRADYANI & DENGE
// ════════════════════════════════════════════════════════════
constexpr float FOG_START   = 3.0f;  // yumuşak sis: bu mesafeye kadar tam parlak
constexpr float FOG_END     = 11.0f; // bu mesafede taban karanlığa iner (32 ara seviye)
constexpr float FOG_SLOPE   = 32.0f / (FOG_END - FOG_START);
constexpr int   FOG_MIN     = 3;     // taban parlaklık (x/32) — tam siyah değil, doku seçilir
constexpr float FOG_SPRITE  = 7.0f;  // sprite: tek kademe, ekran küçük olduğu için
                                     // düşman hiç tam karanlığa gömülmez (oynanabilirlik)
constexpr int TEX_FLOOR = 45;        // zemin flat texture slotu (/doom/zemin.bmp)
constexpr int TEX_CEIL  = 46;        // tavan flat texture slotu (/doom/tavan.bmp)
constexpr int TEX_EXIT_ON = 49;      // çıkış switch'inin BASILI hali (/doom/cikis2.bmp)
                                     // normal hali tex[8] (/doom/cikis.bmp, yoksa yeşil fallback)
constexpr int TEX_ELEV  = 55;        // asansör (blok 9) kapı dokusu (/doom/asansor.bmp,
                                     // yoksa normal kapı dokusunun kopyası)

// ════════════════════════════════════════════════════════════
//  KAPI ANİMASYONU — Doom tarzı yukarı kalkan kapı.
//  Kapı hücresi animasyon boyunca MAP'te kalır (geçiş kapalı,
//  radar değişmez); t 0→1 yükselir, t=1'de hücre 0'lanır.
// ════════════════════════════════════════════════════════════
constexpr int   DOOR_ANIM_MAX   = 6;
constexpr float DOOR_OPEN_SPEED = 1.4f;   // t/saniye (~0.7s tam açılma)

struct DoorAnim { int8_t x, y; float t; };
constexpr int AMMO_MAX      = 200;   // cephane üst sınırı
constexpr float ALERT_DIST  = 9.0f;  // düşman fark-etme (görüş) yarıçapı — silah menzillerinden
                                     // (tabanca 8.0) geniş: düşman, vurulmadan ÖNCE uyanıp ses verir

// ════════════════════════════════════════════════════════════
//  RENK PALETİ — BGR565 (TFT BGR order ile uyumlu)
//  COL_RGB(r,g,b) = tft.color565(b,g,r) karşılığı constexpr
// ════════════════════════════════════════════════════════════
constexpr uint16_t COL_RGB(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(b & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (r >> 3);
}
#define RGB_FIX(r,g,b) COL_RGB(r,g,b)

// --- Sık kullanılan oyun renkleri ---
constexpr uint16_t COL_SKY         = COL_RGB(30, 30, 60);
constexpr uint16_t COL_FLOOR       = COL_RGB(60, 60, 60);
constexpr uint16_t COL_RED         = COL_RGB(255, 0, 0);
constexpr uint16_t COL_DARKRED     = COL_RGB(150, 0, 0);
constexpr uint16_t COL_HUDBG       = COL_RGB(40, 40, 40);
constexpr uint16_t COL_HUDLINE     = COL_RGB(100, 100, 100);
constexpr uint16_t COL_HUDDARK     = COL_RGB(10, 10, 10);
constexpr uint16_t COL_HUDVDARK    = COL_RGB(20, 20, 20);
constexpr uint16_t COL_AMMO_LABEL  = COL_RGB(200, 0, 0);
constexpr uint16_t COL_AMMO_VAL    = COL_RGB(255, 160, 0);
constexpr uint16_t COL_ARMOR_VAL   = COL_RGB(255, 255, 255);
constexpr uint16_t COL_FPS_VAL     = COL_RGB(100, 100, 100);
constexpr uint16_t COL_GREEN_ITEM  = COL_RGB(0, 150, 0);
constexpr uint16_t COL_FIREBALL    = COL_RGB(255, 255, 0);
constexpr uint16_t COL_SNAKE_BG    = COL_RGB(10, 10, 10);
constexpr uint16_t COL_SNAKE_HLINE = COL_RGB(0, 150, 0);
constexpr uint16_t COL_SNAKE_FOOD  = COL_RGB(255, 60, 0);
constexpr uint16_t COL_SNAKE_HEAD  = COL_RGB(0, 255, 80);
constexpr uint16_t COL_MENU_TITLE  = COL_RGB(255, 80, 0);
constexpr uint16_t COL_MENU_SNAKE  = COL_RGB(0, 255, 80);
constexpr uint16_t COL_MENU_SYS    = COL_RGB(80, 180, 255);
constexpr uint16_t COL_GAMEOVER    = COL_RGB(255, 0, 0);
constexpr uint16_t COL_DARKGRAY    = COL_RGB(40, 40, 40);
constexpr uint16_t COL_NEARBLACK   = COL_RGB(8, 8, 8);

// --- RGB565 sabitleri (TFT_eSPI metin/cizim icin — BGR framebuffer disinda) ---
constexpr uint16_t COL_WHITE       = 0xFFFF;   // Beyaz (TFT metin)
constexpr uint16_t COL_HP_LOW      = 0xF800;   // Kirmizi (dusuk HP metin, RGB565)
constexpr uint16_t COL_DARKEN_MASK = 0x7BEF;   // Renk karartma bitmask'i (duvar karanligi)
constexpr uint16_t COL_CHROMA      = 0x0000;   // Chroma-key (seffaflik anahtari)

// ════════════════════════════════════════════════════════════
//  ANİMASYON DURUM MAKİNESİ
// ════════════════════════════════════════════════════════════
enum AnimState : uint8_t {
    ANIM_IDLE   = 0,
    ANIM_WALK   = 1,
    ANIM_ATTACK = 2,
    ANIM_HIT    = 3,
    ANIM_DYING  = 4,
    ANIM_DEAD   = 5
};

// ════════════════════════════════════════════════════════════
//  SPRITE TÜRLERİ
//  Değerler texture ID / harita editörü çıktısıyla hizalı —
//  loadLevel'daki sayısal listeler bu değerleri kullanır, değiştirme!
// ════════════════════════════════════════════════════════════
enum SpriteType : uint8_t {
    ST_ZOMBIE    = 5,    // hp 30, ateş eder
    ST_AMMO      = 9,    // +15 mermi
    ST_HEALTH    = 10,   // +25 can
    ST_KEY       = 11,   // kilitli kapı anahtarı
    ST_FIREBALL  = 12,   // düşman mermisi (ışık kaynağı, sise girmez)
    ST_PARRYBALL = 13,   // parry ile sekmiş mermi
    ST_BARON     = 14,   // hp 150
    ST_BARREL    = 15,   // hp 10, patlayan varil
    ST_PINKY     = 17,   // hp 60, zigzag, ateş etmez
    ST_ARMOR     = 43,   // +50 zırh
    ST_LAMP      = 50,   // dekor: zemin lambası (tex 50, pasif)
    ST_PILLAR    = 51,   // dekor: tekno sütun (tex 51, pasif)
    ST_CORPSE    = 52,   // dekor: ceset (tex 33 = p_ceset yeniden kullanılır)
    ST_CBRA      = 53,   // dekor: şamdan (tex 53, pasif)
    ST_SKULLS    = 54    // dekor: kafatası yığını (tex 54, pasif)
};

// ════════════════════════════════════════════════════════════
//  OYUN DURUM MAKİNESİ
// ════════════════════════════════════════════════════════════
enum GameState {
    STATE_BOOT,
    STATE_MASTER_MENU,
    STATE_MENU,
    STATE_PLAYING,
    STATE_SNAKE,
    STATE_BOOTLOADER,
    STATE_PAUSE
};

// ════════════════════════════════════════════════════════════
//  VERİ YAPILARI
// ════════════════════════════════════════════════════════════
struct Sprite {
    float x, y;
    int type;
    int state;
    float dx, dy;
    int hp;
    uint8_t animState;
    uint8_t animFrame;
    uint32_t animTimer;
    uint32_t lastFireTime;
};

struct SnakeCell { int8_t x, y; };

// ════════════════════════════════════════════════════════════
//  DÜŞMAN ANİMASYON KARE TABLOLARI
//  Her satır [animState] → {kare0, kare1} texture ID çifti.
// ════════════════════════════════════════════════════════════
const uint8_t ZOMBIE_FRAMES[][2] = {{22,22}, {22,23}, {24,24}, {22,22}, {25,26}, {26,26}};
const uint8_t PINKY_FRAMES[][2]  = {{27,27}, {28,29}, {30,44}, {27,27}, {32,33}, {33,33}};
const uint8_t BARON_FRAMES[][2]  = {{34,34}, {34,35}, {36,36}, {37,37}, {38,39}, {39,39}};
const uint8_t VARIL_FRAMES[][2]  = {{40,40}, {40,40}, {40,40}, {40,40}, {41,42}, {42,42}};

// ════════════════════════════════════════════════════════════
//  HARİTA DİZİLERİ — 3 Seviye, her biri 32×32
//  0=boş  1-5=duvar  6=kapı  7=kilitli  8=çıkış  9=asansör  31=gizli
//  Asansörler çift çalışır: 9'a B ile basınca haritadaki diğer 9'un
//  önüne ışınlanır (kabin sekansı TaskEngine'de).
// ════════════════════════════════════════════════════════════
const uint8_t LEVEL1[MH][MW] = {
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3},
{1,1,1,1,1,1,1,1,1,1,1,1,1,3,3,3,9,3,3,3,3,3,3,3,3,3,3,3,3,8,3,3},
{1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3},
{1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3},
{1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,3},
{1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3},
{1,1,1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3},
{1,1,1,1,1,1,1,1,1,1,1,1,1,3,3,3,3,3,3,3,3,6,3,3,3,3,3,3,3,3,3,3},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,0,2,2,2,2,2,2,2,1,1,1},
{1,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,1,1,1},
{1,2,0,0,0,2,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,1},
{1,2,0,0,0,2,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,31,0,0,1},
{1,2,0,0,0,2,0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,31,0,0,1},
{1,2,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,1},
{1,2,0,0,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,1},
{1,2,0,0,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,1,1,1},
{1,2,2,2,2,6,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,1,1,1},
{1,1,1,1,1,0,1,1,1,1,1,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,1,1,1},
{1,1,1,1,1,0,1,1,1,1,1,1,1,2,2,2,2,2,6,2,2,2,2,2,2,4,2,2,2,1,1,1},
{1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
{1,9,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

const uint8_t LEVEL2[MH][MW] = {
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,3,3,3,3,8,3,3,3,3,3,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,3,3,3,3,7,3,3,3,3,3,1,1,2,2,2,2,2,2,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,2,0,0,0,0,2,1,1,1},
{1,2,2,2,2,2,2,2,2,1,1,1,1,1,1,0,1,1,1,1,1,1,1,2,0,0,0,0,2,1,1,1},
{1,2,0,0,0,0,0,0,2,1,1,1,1,1,1,6,1,1,1,1,1,2,2,2,2,31,31,2,2,2,2,1},
{1,2,0,0,0,0,0,0,2,1,1,1,0,0,0,0,0,0,0,0,1,2,2,0,0,0,0,0,0,0,9,1},
{1,9,0,0,2,2,0,0,2,1,1,1,0,0,0,0,0,0,0,0,1,2,2,0,0,0,0,0,0,0,2,1},
{1,2,0,0,0,0,0,0,2,1,1,1,0,0,0,0,0,0,0,0,1,2,2,0,0,0,0,0,0,0,2,1},
{1,2,0,0,0,0,0,0,0,0,0,6,0,0,0,2,2,0,0,0,1,2,2,0,0,0,0,0,0,0,2,1},
{1,2,0,0,0,0,0,0,2,1,1,1,0,0,0,2,2,0,0,0,6,0,0,0,0,0,0,0,0,0,4,1},
{1,2,0,0,2,2,0,0,2,1,1,1,0,0,0,0,0,0,0,0,1,2,2,0,0,0,0,0,0,0,2,1},
{1,2,0,0,0,0,0,0,2,1,1,1,0,0,0,0,0,0,0,0,1,2,2,0,0,0,0,0,0,0,2,1},
{1,2,0,0,0,0,0,0,2,1,1,1,0,0,0,0,0,0,0,0,1,2,2,0,0,0,0,0,0,0,2,1},
{1,2,2,2,6,2,2,2,2,1,1,1,1,1,1,1,6,1,1,1,1,2,2,2,2,2,2,2,2,2,2,1},
{1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

const uint8_t LEVEL3[MH][MW] = {
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,3,3,3,3,3,3,8,3,3,3,3,3,3,3,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,2,2,2,2,1},
{1,1,1,1,1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,2,0,0,0,1},
{1,1,1,1,1,3,3,3,3,3,3,3,3,3,3,7,3,3,3,3,3,3,3,4,3,3,3,2,0,0,0,1},
{1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,0,0,0,1},
{1,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,31,31,2,1},
{1,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,1},
{1,2,0,0,0,2,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,3,0,0,0,0,1},
{1,2,0,0,0,6,0,0,0,0,0,3,0,0,0,0,0,0,0,0,3,0,0,0,0,0,3,0,0,0,0,1},
{1,2,0,0,0,2,0,0,0,0,0,6,0,0,0,0,0,0,0,0,3,0,0,0,0,0,3,0,0,0,0,9},
{1,2,0,0,0,2,0,0,0,0,0,3,0,0,0,0,0,0,0,0,6,0,0,0,0,0,3,0,0,0,0,1},
{1,2,0,0,0,2,0,0,0,0,0,3,0,0,0,0,0,0,0,0,3,0,0,0,0,0,6,0,0,0,0,1},
{1,2,0,0,0,2,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,3,0,0,0,0,1},
{1,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,1},
{1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,2,2,2,2,1},
{1,1,1,1,1,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,1,1,1,1,1},
{1,1,1,1,1,3,3,3,3,3,3,3,3,3,3,6,6,3,3,3,3,3,3,3,3,3,3,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,9,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// ════════════════════════════════════════════════════════════
//  EXTERN GLOBAL DEĞİŞKEN BİLDİRİMLERİ
//  (Tanımlar doom.ino içindedir)
// ════════════════════════════════════════════════════════════
extern TFT_eSPI tft;
extern U8G2_SH1106_128X64_NONAME_F_SW_I2C oled;

extern uint16_t* fb[3];
extern volatile int8_t fb_render;
extern volatile int8_t fb_ready;
extern volatile int8_t fb_display;
extern SemaphoreHandle_t fb_swap_mutex;

extern float zBuffer[SW];
extern float camXTable[SW];

extern uint16_t *tex[MAX_TEX];
extern bool sdReady;

extern SemaphoreHandle_t gameMutex;

extern uint8_t MAP[MH][MW];

extern Sprite sprites[NUM_SPRITES];

extern float px, py, dirX, dirY, planeX, planeY;
extern int joyCenterX, joyCenterY;
extern volatile int joyRawX, joyRawY;

extern uint32_t lastFrame, fpsTimer;
extern int fps, frameCount;
extern int hp, ammo, armor;
extern int currentLevel;
extern bool hasKey;

extern uint32_t fireT, lastDamageTime, sonKullanma;
extern uint32_t shieldSawTime, lastEnemyFire;
extern uint32_t shieldStartTime;
extern bool lastShieldState;
extern uint32_t meleeTimer;

extern int lastAmmo, lastHp, lastArmor, lastFps;
extern bool lastInfState;

extern bool soundEnabled;

extern volatile GameState gameState;

extern bool titleDrawn;
extern uint32_t bootStartTime;
extern uint16_t *titlePicBuf;

extern int masterMenuSel;
extern bool masterMenuDrawn;

extern int menuSelection;
extern int levelSelectIdx;
extern bool inLevelSelect;
extern bool menuDrawn;

extern SnakeCell snakeBody[SNAKE_MAX];
extern int snakeLen;
extern int8_t snakeDirX, snakeDirY;
extern int8_t snakeFoodX, snakeFoodY;
extern uint32_t snakeLastMove;
extern int snakeScore;
extern bool snakeDead;
extern bool snakeDrawn;

extern int weaponType;

// --- Kapı animasyonu & çıkış switch'i ---
extern DoorAnim doorAnims[DOOR_ANIM_MAX];
extern int doorAnimCount;
extern bool exitPressed;             // çıkış switch'ine basıldı (tex 8 → 49 çizilir)
extern bool elevatorPending;         // asansöre basıldı; TaskEngine kabin sekansını çalıştırır
extern int8_t elevSrcX, elevSrcY;    // basılan asansörün hücresi (hedef = diğer 9)

// --- Seviye istatistikleri (orijinal Doom tarzı intermission; skor yok) ---
extern volatile bool levelDone;      // çıkış kapısı kullanıldı, intermission bekliyor
extern int levelItemTotal;           // seviyedeki toplam eşya (loadLevel'da sayılır)
extern uint32_t levelPlayMs;         // seviyede geçen oyun süresi (pause hariç)
extern int totalKills, totalKillTotal;   // victory ekranı için birikimli
extern uint32_t totalTimeMs;

// ════════════════════════════════════════════════════════════
//  ORTAK YARDIMCILAR
// ════════════════════════════════════════════════════════════
inline bool isMonsterType(int t) { return t == ST_ZOMBIE || t == ST_BARON || t == ST_PINKY; }
inline bool isItemType(int t)    { return t == ST_AMMO || t == ST_HEALTH || t == ST_KEY || t == ST_ARMOR; }
inline bool isDecorType(int t)   { return t == ST_LAMP || t == ST_PILLAR || t == ST_CORPSE ||
                                          t == ST_CBRA || t == ST_SKULLS; }

//  doorAnimT — hücre animasyonlu bir kapıysa açılma oranını (0..1),
//  değilse -1 döndürür. renderWalls ve handleUseAction kullanır.
inline float doorAnimT(int x, int y) {
    for (int i = 0; i < doorAnimCount; i++)
        if (doorAnims[i].x == x && doorAnims[i].y == y) return doorAnims[i].t;
    return -1.0f;
}

//  checkLOS — (x0,y0)'dan hedefe görüş hattı. dx,dy = hedef-kaynak
//  farkı, dist = uzunluk. 0.2 birim adımlarla harita örneklenir;
//  duvara (veya harita dışına) rastlarsa görüş yok.
inline bool checkLOS(float x0, float y0, float dx, float dy, float dist) {
    float stepX = dx / dist, stepY = dy / dist;
    for (float s = 0.2f; s < dist; s += 0.2f) {
        int cx = (int)(x0 + stepX * s);
        int cy = (int)(y0 + stepY * s);
        if (cx < 0 || cx >= MW || cy < 0 || cy >= MH || MAP[cy][cx] > 0) return false;
    }
    return true;
}

// ════════════════════════════════════════════════════════════
//  ORTAK FONKSİYON BİLDİRİMLERİ
// ════════════════════════════════════════════════════════════
void playSound(uint16_t freq, uint32_t dur);
void returnToOS();

void startDoorAnim(int x, int y);

int  getEnemyTexID(int spriteIndex);
void initSprite(int i, float x, float y, int type, int state);
void makeTex(int id);
void makeFlatTexes();
bool loadBMP(const char* filename, int id, uint8_t* fileBuf);
void explodeBarrel(int barrelIdx, uint32_t now);
void updateAnimations(uint32_t now);
void loadLevel(int level);

int countLevelMonsters();
int countLevelKills();
int countLevelItemsLeft();

void drawStaticHUD();
bool loadTitlePic(uint8_t* fileBuf);

void drawMasterMenu(bool fullRedraw);
void drawIconDoom(int x, int y, bool sel);
void drawIconSnake(int x, int y, bool sel);

void snakeReset();
void snakeDraw();
void snakeUpdate(uint32_t now);

// Renderer.h fonksiyonları
void renderWalls(uint16_t* activeFB, int pitch);
void renderSprites(uint16_t* activeFB, int pitch);
void renderWeapon(uint16_t* activeFB, int pitch, uint32_t now);
void renderDamageEffect(uint16_t* activeFB, uint32_t now);

// Player.h fonksiyonları
void updateShieldState(uint32_t now);
void updateMovement(float dt);
void handleUseAction(uint32_t now, float dt);
void handleWeaponSwitch(uint32_t now);
void handleShooting(uint32_t now);

// Enemies.h fonksiyonları
void updateAllEnemies(float dt, uint32_t now, bool isParrying);
