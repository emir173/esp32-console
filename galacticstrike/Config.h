#pragma once
// ============================================================
//  Config.h — Galactic Strike oyun sabitleri ve tip tanimlari
//  Tum moduller bu dosyayi include eder.
//  Include zinciri: Config.h → Sinif dosyalari → Renderer.h → .ino
// ============================================================

#include <TFT_eSPI.h>
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include "../hardware_config.h"
#include "../GameBase.h"
#include "../dev_tools.h"
#include <Preferences.h>

// ============ Ekran Boyutlari ============
constexpr int SCR_W     = 160;
constexpr int SCR_H     = 128;
constexpr int HUD_H     = 12;
constexpr int FRAME_MS  = 16;

// ============ RGB565 Renk Donusum Fonksiyonu ============
constexpr uint16_t RGB(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

// ============ Renk Paleti ============
// Arkaplan ve yildiz renkleri
constexpr uint16_t COL_BG         = RGB(5, 5, 15);
constexpr uint16_t COL_STAR_DIM   = RGB(60, 60, 80);
constexpr uint16_t COL_STAR_BRI   = RGB(180, 180, 220);
constexpr uint16_t COL_STAR_GOLD  = RGB(255, 220, 100);

// Oyuncu gemisi renkleri
constexpr uint16_t COL_SHIP_A     = RGB(80, 200, 255);
constexpr uint16_t COL_SHIP_B     = RGB(40, 130, 200);
constexpr uint16_t COL_SHIP_ENG   = RGB(255, 150, 40);

// Mermi renkleri
constexpr uint16_t COL_BULLET_P   = RGB(255, 255, 100);
constexpr uint16_t COL_BULLET_E   = RGB(255, 80, 80);

// Dusman tip renkleri
constexpr uint16_t COL_EN_BASIC   = RGB(220, 60, 60);
constexpr uint16_t COL_EN_FAST    = RGB(255, 180, 40);
constexpr uint16_t COL_EN_TANK    = RGB(100, 200, 100);
constexpr uint16_t COL_EN_BOSS    = RGB(200, 60, 200);

// Patlama renkleri
constexpr uint16_t COL_BOOM_A     = RGB(255, 200, 60);
constexpr uint16_t COL_BOOM_B     = RGB(255, 100, 40);
constexpr uint16_t COL_BOOM_C     = RGB(255, 60, 20);

// Power-up renkleri
constexpr uint16_t COL_PWR_TRIPLE = RGB(255, 255, 100);
constexpr uint16_t COL_PWR_SHIELD = RGB(80, 200, 255);
constexpr uint16_t COL_PWR_LIFE   = RGB(255, 80, 80);

// HUD renkleri
constexpr uint16_t COL_HUD_BG     = RGB(0, 0, 30);
constexpr uint16_t COL_HUD_TXT    = RGB(180, 180, 200);
constexpr uint16_t COL_WHITE      = 0xFFFF;   // Beyaz
constexpr uint16_t COL_BLACK      = 0x0000;   // Siyah
constexpr uint16_t COL_HUD_BRIGHT = 0xBDF7;   // Açık gri metin
constexpr uint16_t COL_PANEL      = 0x2104;   // Koyu lacivert panel dolgu
constexpr uint16_t COL_RED_DARK   = 0x8000;   // Koyu kirmizi çerçeve

// ============ Oyun Sabitleri ============
constexpr int MAX_P_BULLETS   = 16;
constexpr int MAX_E_BULLETS   = 24;
constexpr int MAX_ENEMIES     = 12;
constexpr int MAX_STARS       = 40;
constexpr int MAX_EXPLOSIONS  = 8;
constexpr int MAX_POWERUPS    = 3;

constexpr int SHIP_W          = 9;
constexpr int SHIP_H          = 10;
constexpr float SHOOT_COOLDOWN = 0.267f;

// ============ Durum Sabitleri ============
enum State { ST_TITLE, ST_PLAY, ST_GAMEOVER, ST_PAUSE };

// ============ Dusman Tipleri ============
enum EnemyType { EN_BASIC, EN_FAST, EN_TANK, EN_BOSS };

// ============ Power-Up Turleri ============
enum PwrType { PWR_TRIPLE, PWR_SHIELD, PWR_LIFE };

// ============ Veri Yapilari ============
struct Star { float x, y, speed; uint8_t layer; };
struct Bullet { float x, y, vx, vy; bool active; };

struct Explosion { float x, y; float elapsed; bool active; };

struct Enemy {
    float x, y;
    float vx, vy;
    int hp, maxHp;
    float shootTimer;
    float shootInterval;
    EnemyType type;
    bool active;
};

struct PowerUp { float x, y; PwrType type; bool active; };

struct Ship {
    float x, y;
    int hp, maxHp;
    float shootCD;
    float tripleTimer;
    float shieldTimer;
    float invincTimer;
    int score;
};

// ============ Harici (extern) Nesne Bildirimleri ============
extern TFT_eSPI tft;
extern TFT_eSprite canvas;
extern State state;

extern Star stars[MAX_STARS];
extern Bullet pBullets[MAX_P_BULLETS];
extern Bullet eBullets[MAX_E_BULLETS];
extern Enemy enemies[MAX_ENEMIES];
extern Explosion explosions[MAX_EXPLOSIONS];
extern PowerUp powerUps[MAX_POWERUPS];
extern Ship ship;

extern int curWave;
extern float waveSpawnTimer;
extern int waveEnemiesSpawned;
extern int waveEnemiesPerWave;
extern bool waveActive;

extern int joyCenterX, joyCenterY;
extern uint32_t lastFrameMs;
extern float animTime;
extern int animFrame;
extern uint32_t stateTimer;
extern int highScore;

extern uint32_t fpsFrameCount;
extern uint32_t fpsStartTime;
extern int currentFPS;

extern bool soundEnabled;
extern bool showFps;

// ============ Wrapper Fonksiyon Bildirimleri ============
void playSound(uint16_t freq, uint32_t dur);
void returnToOS();
