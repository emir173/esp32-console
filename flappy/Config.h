#pragma once

#include <TFT_eSPI.h>

// ============ Screen Dimensions (Landscape) ============
constexpr int SCR_W = 160;
constexpr int SCR_H = 128;

// ============ Bird Constants ============
constexpr int   BIRD_X      = 25;
constexpr int   BIRD_R      = 5;
constexpr float GRAVITY     = 360.0f;
constexpr float JUMP_VEL    = -90.0f;
constexpr float BIRD_MAX_ANGLE = 90.0f;
constexpr float BIRD_MIN_ANGLE = -30.0f;
constexpr float BIRD_ANGLE_SPEED = 360.0f;

// ============ Pipe Constants ============
constexpr int   PIPE_W      = 16;
constexpr int   PIPE_GAP    = 36;
constexpr float BASE_SPEED  = 45.0f;
constexpr float MAX_SPEED   = 96.0f;
constexpr int   NUM_PIPES   = 4;
constexpr int   PIPE_DIST   = 56;

// ============ Ground Constants ============
constexpr int GROUND_H      = 12;
constexpr int GROUND_Y      = (SCR_H - GROUND_H);

// ============ Effects ============
constexpr int SHAKE_DURATION_MS  = 100;
constexpr int SHAKE_INTENSITY    = 4;
constexpr int FLASH_FRAMES       = 3;
constexpr float SCORE_POPUP_DUR = 0.45f;

// ============ Timing ============
constexpr int TARGET_FPS = 60;

// ============ Colors (RGB565) ============
constexpr uint16_t COL_SKY           = 0x65BF;  // (100, 180, 255)
constexpr uint16_t COL_BIRD          = 0xFEE0;  // (255, 220, 0)
constexpr uint16_t COL_WING          = 0xFD20;  // (255, 165, 0)
constexpr uint16_t COL_BEAK          = 0xFB20;  // Red-orange
constexpr uint16_t COL_PIPE          = 0x2444;  // (34, 139, 34)
constexpr uint16_t COL_PIPE_LIP      = 0x3666;  // (50, 205, 50)
constexpr uint16_t COL_PIPE_BORDER   = 0x18A2;  // Dark green border
constexpr uint16_t COL_PIPE_HIGHLIGHT= 0x4EE9;  // Bright green highlight
constexpr uint16_t COL_GROUND        = 0x6204;  // (101, 67, 33)
constexpr uint16_t COL_GRASS         = 0x07E0;
constexpr uint16_t COL_PANEL         = 0x2104;
constexpr uint16_t COL_HUD_TEXT      = 0xBDF7;
constexpr uint16_t COL_FLAPPY_SHADOW = 0x4208;
constexpr uint16_t COL_RED_DARK      = 0x8000;

// ============ Game States ============
enum GameState { MENU, PLAYING, DYING, GAMEOVER, PAUSE };

// ============ Ground Scroll Pattern Period (LCM(3, 7) = 21) ============
constexpr int GROUND_PATTERN_PERIOD = 21;
