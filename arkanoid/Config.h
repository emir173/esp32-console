#pragma once
// ============================================================
//  E-OS ARKANOID — Config.h
//  Tum sihirli sayilar, RGB565 renk paleti ve enum'lar
// ============================================================
#include <TFT_eSPI.h>

// ============ Ekran Boyutlari (Landscape) ============
constexpr int SCR_W = 160;
constexpr int SCR_H = 128;

// ============ Tugla Izgarasi ============
constexpr int BRICK_COLS  = 8;
constexpr int BRICK_ROWS  = 6;
constexpr int BRICK_W     = 16;
constexpr int BRICK_H     = 7;
constexpr int BRICK_GAP_X = 2;
constexpr int BRICK_GAP_Y = 2;
// Izgara X orta hizali: (160 - (8*16 + 7*2)) / 2 = 9
constexpr int BRICK_START_X = 9;
constexpr int BRICK_START_Y = 14;

// ============ Cubuk (Paddle) ============
constexpr int PADDLE_W = 28;
constexpr int PADDLE_H = 4;
constexpr int PADDLE_Y = SCR_H - 10;  // 118

// ============ Top (Ball) ============
constexpr int BALL_R          = 3;
constexpr float BASE_BALL_SPD = 75.0f;   // piksel/sn
constexpr float MAX_BALL_SPD  = 130.0f;  // piksel/sn
constexpr int TRAIL_LEN       = 6;

// ============ HUD ============
constexpr int HUD_H = 10;

// ============ Oyun Ayarlari ============
constexpr int   DEADZONE      = 300;
constexpr int   START_LIVES   = 3;
constexpr int   MAX_PARTICLES = 40;
constexpr int   TARGET_FPS    = 60;
constexpr int   FRAME_MS      = (1000 / TARGET_FPS);
constexpr float MAX_DT        = 0.05f;   // Lag spike korumasi

// ============ Oyun Durumlari ============
enum GameState { MENU, PLAYING, BALL_LOST, LEVEL_CLEAR, GAMEOVER, PAUSE };

// ============ Renk Paleti — Modern Premium Arcade (RGB565) ============
constexpr uint16_t COL_BG          = 0x0004;   // Cok koyu lacivert
constexpr uint16_t COL_PADDLE      = 0xCE79;   // Gumus cubuk
constexpr uint16_t COL_PADDLE_HL   = 0xFFFF;   // Beyaz vurgu
constexpr uint16_t COL_PADDLE_DK   = 0x7BEF;   // Koyu gumus golge
constexpr uint16_t COL_BALL        = 0xFFFF;   // Beyaz top
constexpr uint16_t COL_BALL_GLOW   = 0xBDF7;   // Top pariltisi
constexpr uint16_t COL_WALL        = 0x2945;   // Duvar cerceve
constexpr uint16_t COL_HUD_LINE    = 0x2104;   // HUD ayirici
constexpr uint16_t COL_HUD_TEXT    = 0xBDF7;   // HUD metin
constexpr uint16_t COL_TITLE_SHADOW = 0x0010;  // Baslik golgesi
constexpr uint16_t COL_RED_DARK    = 0x8000;   // Koyu kirmizi (panel ic cerceve)

// Tugla renkleri (satir bazli, ustten alta — 6 satir)
constexpr uint16_t BRICK_COLORS[BRICK_ROWS] = {
    0xF800,   // Row 0: Kirmizi  —  60 puan (en zor ulasilan)
    0xFC00,   // Row 1: Turuncu  —  50 puan
    0xFFE0,   // Row 2: Sari     —  40 puan
    0x07E0,   // Row 3: Yesil    —  30 puan
    0x07FF,   // Row 4: Cyan     —  20 puan
    0xF81F,   // Row 5: Magenta  —  10 puan (en kolay)
};

// Tugla vurgu renkleri (3D efekti: ust+sol kenar)
constexpr uint16_t BRICK_HL[BRICK_ROWS] = {
    0xFC48,   // Acik kirmizi
    0xFE20,   // Acik turuncu
    0xFFF8,   // Acik sari
    0x47F0,   // Acik yesil
    0x5FFF,   // Acik cyan
    0xFC7F,   // Acik magenta
};

// Satir bazli puanlar (ust satirlar daha degerli)
constexpr int BRICK_POINTS[BRICK_ROWS] = {60, 50, 40, 30, 20, 10};

// Tugla kirilma ses frekanslari (ust satir parlak, alt satir tok)
constexpr uint16_t BRICK_HIT_FREQ[BRICK_ROWS] = {
    NOTE_C5,   // Row 0: 523 Hz (ust — en parlak)
    NOTE_A4,   // Row 1: 440 Hz
    NOTE_G4,   // Row 2: 392 Hz
    NOTE_E4,   // Row 3: 330 Hz
    NOTE_C4,   // Row 4: 262 Hz
    NOTE_G3,   // Row 5: 196 Hz (alt — en tok)
};
