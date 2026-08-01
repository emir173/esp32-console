#pragma once
// ============================================================
//  E-OS SPACE INVADERS — Config.h
//  Tum sabitler, enum'lar, renkler ve ortak veri yapilari
// ============================================================
#include <TFT_eSPI.h>

// ============ Ekran Boyutlari (Landscape) ============
constexpr int SCR_W = 160;
constexpr int SCR_H = 128;

// ============ Uzayli Izgarasi ============
constexpr int ALIEN_COLS    = 8;
constexpr int ALIEN_ROWS    = 4;
constexpr int ALIEN_W       = 9;
constexpr int ALIEN_H       = 7;
constexpr int ALIEN_GAP_X   = 3;
constexpr int ALIEN_GAP_Y   = 4;
constexpr int ALIEN_START_Y = 14;
constexpr int ALIEN_DROP    = 5;

// ============ Oyuncu ============
constexpr int PLAYER_W = 14;
constexpr int PLAYER_H = 10;
constexpr int PLAYER_Y = SCR_H - 14;

// ============ Mermiler ============
constexpr int   MAX_PBULLETS = 3;
constexpr int   MAX_EBULLETS = 5;
constexpr float PBULLET_SPD  = 130.0f;
constexpr float EBULLET_SPD  = 65.0f;

// ============ Kalkanlar (Bunker) ============
constexpr int BUNKER_COUNT    = 3;
constexpr int BUNKER_BLOCK_W  = 3;
constexpr int BUNKER_BLOCK_H  = 3;
constexpr int BUNKER_COLS     = 6;
constexpr int BUNKER_ROWS     = 4;
constexpr int BUNKER_Y        = 90;
constexpr int BUNKER_X[3]     = {14, 70, 126};

// ============ Parallax Yildiz Tarlasi ============
constexpr int STAR_FAR_COUNT  = 16;
constexpr int STAR_MID_COUNT  = 10;
constexpr int STAR_NEAR_COUNT = 6;
constexpr float STAR_FAR_SPD  = 18.0f;
constexpr float STAR_MID_SPD  = 40.0f;
constexpr float STAR_NEAR_SPD = 72.0f;

// ============ Parcacik Patlama Fizigi ============
constexpr int MAX_PARTICLES = 50;

// ============ Oyun Ayarlari ============
constexpr int   DEADZONE       = 300;
constexpr int   HUD_H          = 10;
constexpr int   START_LIVES    = 3;
constexpr int   INVINCIBLE_MS  = 1500;
constexpr int   FIRE_COOLDOWN  = 250;
constexpr int   TARGET_FPS     = 60;
constexpr int   FRAME_MS       = (1000 / TARGET_FPS);
constexpr float SHAKE_DECAY    = 0.85f;
constexpr int   FLASH_DURATION = 1;

// ============ Joystick Advanced Processing ============
constexpr int   JOY_DEADZONE_RADIUS    = 500;
constexpr float EMA_ALPHA_MIN          = 0.10f;
constexpr float EMA_ALPHA_MAX          = 1.00f;
constexpr int   FLICK_THRESHOLD        = 350;
constexpr float CONE_HALF_ANGLE        = 45.0f;
constexpr float CONE_DEAD_DEG          = 5.0f;
constexpr unsigned long DIR_COOLDOWN_MS      = 60;
constexpr unsigned long AUTO_CAL_IDLE_MS     = 5000;
constexpr unsigned long AUTO_CAL_INTERVAL_MS = 500;
constexpr int   AUTO_CAL_MAX_DRIFT     = 200;

// ============ Yon Sabitleri (Joystick.h ile uyumlu) ============
constexpr int DIR_UP    = 0;
constexpr int DIR_RIGHT = 1;
constexpr int DIR_DOWN  = 2;
constexpr int DIR_LEFT  = 3;

// ============ Oyun Durumlari ============
enum GameState { MENU, PLAYING, WAVE_CLEAR, GAMEOVER, PAUSE };

// ============ Renk Paleti (RGB565 — Modern Premium Arcade) ============
constexpr uint16_t COL_SPACE        = 0x0000;
constexpr uint16_t COL_SPACE_DEEP   = 0x0821;
constexpr uint16_t COL_PLAYER       = 0x07C0;
constexpr uint16_t COL_PLAYER_HL    = 0x07F0;
constexpr uint16_t COL_PLAYER_CABIN = 0xB7F0;
constexpr uint16_t COL_PBULLET      = 0xFFE0;
constexpr uint16_t COL_PBULLET_HL   = 0xFFFF;
constexpr uint16_t COL_EBULLET      = 0xF800;
constexpr uint16_t COL_EBULLET_CORE = 0xFC00;
constexpr uint16_t COL_BUNKER       = 0x07E0;
constexpr uint16_t COL_BUNKER_DMG   = 0x6B40;
constexpr uint16_t COL_BUNKER_HL    = 0x07F0;
constexpr uint16_t COL_HUD_LINE     = 0x2104;
constexpr uint16_t COL_HUD_TEXT     = 0xBDF7;
constexpr uint16_t COL_TITLE_SHADOW = 0x0010;
constexpr uint16_t COL_RED_DARK     = 0x8000;
constexpr uint16_t COL_FLASH_RED    = 0xF800;
constexpr uint16_t COL_PANEL_BG     = 0x0821;

// Yildiz renkleri (parallax katman)
constexpr uint16_t COL_STAR_FAR   = 0x3186;
constexpr uint16_t COL_STAR_MID   = 0x7BCF;
constexpr uint16_t COL_STAR_NEAR  = 0xFFFF;
constexpr uint16_t COL_WHITE      = 0xFFFF;   // Beyaz (mermi/parçacık pikseli)

// Uzayli renkleri (satir bazli)
constexpr uint16_t COL_ALIEN_ROW[4] = {
    0x07FF,  0xF81F,  0xFFE0,  0xF800
};

// Uzayli puanlari (satir bazli)
constexpr int ALIEN_POINTS[4] = {40, 30, 20, 10};

// ============ Ortak Veri Yapilari ============
struct Bullet {
    float x, y;
    uint16_t color;
    bool active;
};

