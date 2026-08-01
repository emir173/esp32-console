#pragma once

#include <TFT_eSPI.h>
#include "../GameBase.h"

// ============ Screen Dimensions (Landscape) ============
constexpr int SCR_W = 160;
constexpr int SCR_H = 128;

// ============ Direction Constants (SharedJoystick bunlari bekler) ============
constexpr int DIR_UP    = 0;
constexpr int DIR_RIGHT = 1;
constexpr int DIR_DOWN  = 2;
constexpr int DIR_LEFT  = 3;

// ============ Board Layout ============
// 4x4 tahta solda (110x110), sag serit skor paneli.
// Hucre piksel = BOARD_X + GAP + col*(CELL+GAP)
constexpr int BOARD_N  = 4;
constexpr int CELL     = 25;
constexpr int GAP      = 2;
constexpr int BOARD_X  = 4;
constexpr int BOARD_Y  = 9;
constexpr int BOARD_W  = 2 * GAP + BOARD_N * CELL + (BOARD_N - 1) * GAP;  // 110

// ============ Sag Panel ============
constexpr int PANEL_X  = 118;
constexpr int PANEL_W  = 40;   // 118..158
constexpr int MINI_TILE = 24;  // paneldeki "TOP" mini karo boyu

// ============ Timing ============
constexpr int TARGET_FPS = 60;
constexpr int FRAME_MS   = (1000 / TARGET_FPS);
constexpr unsigned long ANIM_MS       = 90;    // kaydirma animasyonu
constexpr unsigned long POP_MS        = 100;   // birlesme pop'u
constexpr unsigned long SPAWN_MS      = 110;   // yeni karo buyume animasyonu
constexpr unsigned long GAIN_POPUP_MS = 700;   // paneldeki "+N" gosterimi
constexpr unsigned long STANDALONE_MSG_MS = 1200;

// ============ Colors (RGB565) ============
constexpr uint16_t C565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t COL_BG          = C565(10, 10, 26);    // E-OS koyu lacivert
constexpr uint16_t COL_BOARD_FRAME = C565(84, 78, 112);
constexpr uint16_t COL_BOARD_BG    = C565(34, 32, 54);
constexpr uint16_t COL_CELL_EMPTY  = C565(24, 24, 42);
constexpr uint16_t COL_TXT_DARK    = C565(119, 110, 101); // acik karolarda koyu rakam
constexpr uint16_t COL_GRAY_TXT    = C565(120, 120, 130);
constexpr uint16_t COL_GOLD        = C565(237, 194, 46);  // 2048 karosu = tema rengi

// Karo renkleri — indeks = us (grid degeri), 1 -> "2", 11 -> "2048".
// 12+ (4096 ve otesi) mor tonlari.
constexpr uint16_t TILE_COLORS[17] = {
    COL_CELL_EMPTY,        //  0 (bos, kullanilmaz)
    C565(238, 228, 218),   //  1: 2
    C565(237, 224, 200),   //  2: 4
    C565(242, 177, 121),   //  3: 8
    C565(245, 149,  99),   //  4: 16
    C565(246, 124,  95),   //  5: 32
    C565(246,  94,  59),   //  6: 64
    C565(237, 207, 114),   //  7: 128
    C565(237, 204,  97),   //  8: 256
    C565(237, 200,  80),   //  9: 512
    C565(237, 197,  63),   // 10: 1024
    COL_GOLD,              // 11: 2048
    C565(160,  80, 220),   // 12: 4096
    C565(140,  60, 200),   // 13: 8192
    C565(120,  45, 180),   // 14: 16K
    C565(100,  35, 160),   // 15: 32K
    C565( 85,  25, 140),   // 16: 65K
};

// ============ Enums ============
enum GameState { ST_MENU, ST_PLAYING, ST_ANIM, ST_WIN, ST_GAMEOVER, ST_PAUSE };
