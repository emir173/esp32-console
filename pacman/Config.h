#pragma once
// ============================================================
//  Config.h — Pacman sabitleri, renk paleti, enum ve harita
//  Tum .h dosyalari ve .ino tarafindan include edilir.
// ============================================================

#include <TFT_eSPI.h>
#include <Wire.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"

// ============================================================
//  Ekran boyutlari ve grid
// ============================================================
constexpr int SCR_W       = 160;   // Ekran genisligi (piksel)
constexpr int SCR_H       = 128;   // Ekran yuksekligi (piksel)
constexpr int HUD_H       = 16;    // Ust HUD (skor/can) seridinin yuksekligi
constexpr int TILE        = 8;     // Bir kare (tile) boyutu — piksel
constexpr int HALF_TILE   = 4;     // Kare yari boyutu — hizalama icin
constexpr int COLS        = 20;    // Harita sutun sayisi
constexpr int ROWS        = 14;    // Harita satir sayisi
constexpr int MAP_Y       = HUD_H; // Haritanin baslangic Y'si (HUD altinda)

// ============================================================
//  Hucre tipleri
// ============================================================
constexpr uint8_t CELL_EMPTY = 0;
constexpr uint8_t CELL_WALL  = 1;
constexpr uint8_t CELL_DOT   = 2;
constexpr uint8_t CELL_POWER = 3;

// ============================================================
//  Renk paleti (RGB565)
// ============================================================
constexpr uint16_t COL_BG            = TFT_BLACK;
constexpr uint16_t COL_WALL          = 0x18F9;  // tft.color565(30, 30, 200)
constexpr uint16_t COL_WALL_INNER    = 0x39FF;  // tft.color565(60, 60, 250)
constexpr uint16_t COL_DOT           = 0xFD92;  // tft.color565(255, 180, 150)
constexpr uint16_t COL_POWER         = 0xFFEC;  // tft.color565(255, 255, 100)
constexpr uint16_t COL_PAC           = TFT_YELLOW;
constexpr uint16_t COL_HUD_BG        = 0x0845;  // tft.color565(10, 10, 40)
constexpr uint16_t COL_GHOST_RED     = TFT_RED;
constexpr uint16_t COL_GHOST_PINK    = 0xFD9F;  // tft.color565(255, 180, 255)
constexpr uint16_t COL_GHOST_CYAN    = 0x07FF;  // tft.color565(0, 255, 255)
constexpr uint16_t COL_SCARED_BLUE   = 0x319F;  // tft.color565(50, 50, 255)
constexpr uint16_t COL_EATEN_BLUE    = 0x665F;  // tft.color565(100, 200, 255)
constexpr uint16_t COL_TITLE_SHADOW  = 0x3180;  // tft.color565(50, 50, 0)
constexpr uint16_t COL_LIGHTGREY     = 0xBDF7;
constexpr uint16_t COL_GAMEOVER_BG   = 0x2020;  // tft.color565(33, 4, 4)
constexpr uint16_t COL_DARK_RED      = 0x8000;
constexpr uint16_t COL_PAUSE_BG      = 0x0841;  // tft.color565(10, 10, 15)

// ============================================================
//  Oynanis sabitleri
// ============================================================
constexpr int   TARGET_FPS     = 60;
constexpr float FRAME_SEC      = 1.0f / TARGET_FPS;
constexpr float DT_CAP         = 0.05f;
constexpr int   NUM_GHOSTS     = 3;
constexpr int   DEADZONE       = 400;
constexpr float SNAP_TOLERANCE = 1.5f;

// ============================================================
//  State sureleri (ms)
// ============================================================
constexpr uint32_t READY_DURATION    = 2000;  // "HAZIR!" suresi
constexpr uint32_t DYING_DURATION    = 1500;  // Olum animasyon suresi
constexpr uint32_t DYING_STEP        = 300;   // Olum animasyon adim araligi
constexpr uint32_t SCARED_DURATION   = 7000;  // Power pellet korkutma suresi
constexpr uint32_t SCARED_WARN       = 2000;  // Son 2 sn beyaz yanip sonme
constexpr uint32_t DEBOUNCE_DELAY    = 50;    // Buton debounce (ms)

// ============================================================
//  Hiz sabitleri (piksel/saniye — temel)
// ============================================================
constexpr float PAC_BASE_SPEED    = 40.0f;
constexpr float PAC_SPEED_PER_LV  = 1.5f;
constexpr float GHOST_BASE_SPEED  = 22.0f;
constexpr float GHOST_SPEED_PER_LV= 1.0f;
constexpr float SCARED_SPEED_MUL  = 0.6f;  // Scared mod hiz carpani
constexpr float EATEN_SPEED_MUL   = 2.0f;  // Eaten mod hiz carpani

// ============================================================
//  Joystick ve ADC
// ============================================================
constexpr int ADC_CENTER = 2048;    // 12-bit ADC orta degeri

// ============================================================
//  Hayalet tipi ve modlari
// ============================================================
constexpr int GHOST_CHASE  = 0;
constexpr int GHOST_SCARED = 1;
constexpr int GHOST_EATEN  = 2;

constexpr int GHOST_TYPE_BLINKY = 0;
constexpr int GHOST_TYPE_PINKY  = 1;
constexpr int GHOST_TYPE_INKY   = 2;

// ============================================================
//  Oyun durum makinesi
// ============================================================
enum GameState { TITLE, READY, PLAYING, DYING, GAMEOVER, WIN, PAUSE };

// ============================================================
//  Harita sablonu (salt okunur baslangic)
// ============================================================
const uint8_t MAP_TEMPLATE[ROWS][COLS] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,2,2,2,2,2,2,2,2,1,1,2,2,2,2,2,2,2,2,1},
  {1,3,1,1,2,1,1,1,2,1,1,2,1,1,1,2,1,1,3,1},
  {1,2,1,1,2,1,1,1,2,1,1,2,1,1,1,2,1,1,2,1},
  {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
  {1,2,1,1,2,1,2,1,1,1,1,1,1,2,1,2,1,1,2,1},
  {1,2,2,2,2,1,2,2,2,1,1,2,2,2,1,2,2,2,2,1},
  {1,1,1,1,2,1,1,0,0,0,0,0,0,1,1,2,1,1,1,1},
  {0,0,0,1,2,1,0,0,1,0,0,1,0,0,1,2,1,0,0,0},
  {1,1,1,1,2,1,0,0,1,0,0,1,0,0,1,2,1,1,1,1},
  {0,0,0,0,2,0,0,0,1,1,1,1,0,0,0,2,0,0,0,0},
  {1,1,1,1,2,1,1,0,0,0,0,0,0,1,1,2,1,1,1,1},
  {1,2,2,2,2,2,2,2,2,1,1,2,2,2,2,2,2,2,2,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// ============================================================
//  Harici degiskenler (.ino'da tanimli)
// ============================================================
extern TFT_eSPI tft;
extern TFT_eSprite canvas;

extern bool soundEnabled;
extern bool showFps;
extern int score;
extern int highScore;
extern int lives;
extern int dotsLeft;
extern int level;
extern int joyCenterX;
extern int joyCenterY;
extern uint32_t soundEndTime;
extern uint32_t stateTimer;
extern GameState state;
extern uint8_t gameMap[ROWS][COLS];

// ============================================================
//  isWall — Verilen kare duvar mi?
//  c: sutun, r: satir. Yatay sinir disi tunnel (false),
//  dikey sinir disi duvar (true) kabul edilir.
// ============================================================
inline bool isWall(int c, int r) {
    if (c < 0 || c >= COLS) return false;
    if (r < 0 || r >= ROWS) return true;
    return gameMap[r][c] == CELL_WALL;
}
