#pragma once

#include <TFT_eSPI.h>

// ============ Screen Dimensions (Landscape) ============
constexpr int SCR_W = 160;
constexpr int SCR_H = 128;

// ============ Grid Constants ============
constexpr int GRID       = 8;
constexpr int COLS       = 20;
constexpr int ROWS       = 14;
constexpr int OFFSET_Y   = 15;
constexpr int HUD_H      = 14;

// ============ Snake Constants ============
constexpr int MAX_SNAKE  = 200;
constexpr int INIT_LEN   = 5;
constexpr int BASE_SPEED = 150;
constexpr int MIN_SPEED  = 55;
constexpr int SPEED_DEC  = 2;

// ============ Direction Constants ============
constexpr int DIR_UP    = 0;
constexpr int DIR_RIGHT = 1;
constexpr int DIR_DOWN  = 2;
constexpr int DIR_LEFT  = 3;

// ============ Particle Constants ============
constexpr int MAX_PARTICLES = 40;

// ============ Juice Constants ============
constexpr float SHAKE_FOOD       = 1.0f;
constexpr float SHAKE_GOLD       = 3.0f;
constexpr float SHAKE_POISON     = 2.0f;
constexpr float SHAKE_DEATH      = 8.0f;
constexpr unsigned long HITSTOP_GOLD_MS  = 60;
constexpr unsigned long HITSTOP_DEATH_MS = 120;
constexpr unsigned long SQUASH_MS        = 80;
constexpr float SQUASH_AMP        = 4.0f;

// ============ Combo Constants ============
constexpr unsigned long COMBO_WINDOW_MS        = 1800;  // <1.8s aralik = combo
constexpr unsigned long COMBO_GOLD_EXTEND_MS   = 1500;  // gold +1.5s uzma
constexpr int COMBO_TRAIL_LOW          = 4;
constexpr int COMBO_TRAIL_HIGH         = 8;
constexpr int COMBO_TRAIL_HIGH_THRESH  = 4;     // combo >=4 -> trail 8

inline int comboMultiplier(int cc) {
    if (cc >= 7) return 5;
    if (cc >= 4) return 3;
    if (cc >= 2) return 2;
    return 1;
}

// ============ Power-Up Constants ============
constexpr unsigned long GHOST_DURATION_MS  = 1000;   // 1s ghost
constexpr unsigned long GHOST_RECHARGE_MS  = 8000;   // 8s sarj
constexpr unsigned long MAGNET_DURATION_MS = 3000;   // 3s magnet
constexpr unsigned long MAGNET_RECHARGE_MS = 10000;  // 10s sarj
constexpr int POWERUP_SPAWN_CHANCE = 5;              // 1/20 ihtimal

// Power-Up renkleri (RGB565)
constexpr uint16_t COL_GHOST       = 0x3DFF;  // acik mavi
constexpr uint16_t COL_GHOST_DIM   = 0x01EF;
constexpr uint16_t COL_GHOST_HL    = 0x7FFF;
constexpr uint16_t COL_MAGNET      = 0xF81F;  // magenta/mor
constexpr uint16_t COL_MAGNET_DIM  = 0xB81F;
constexpr uint16_t COL_MAGNET_HL   = 0xFFFF;

// ============ Arena Shrink Constants ============
constexpr unsigned long ARENA_SHRINK_START_MS    = 30000;  // 30s sonra daralma baslar
constexpr unsigned long ARENA_SHRINK_INTERVAL_MS = 10000;   // her 10s'de 1 hücre içeri
constexpr int ARENA_SHRINK_MAX     = 3;     // max inset (hücre sayisi)
constexpr int ARENA_EXPAND_ON_EAT  = 1;     // yem yiyince 1 hücre disari genisler
constexpr uint16_t COL_ARENA_DEAD  = 0x3000; // koyu kirmizi dead-zone (RGB565)

// ============ Death Cinematic Constants (Hamle 6B) ============
constexpr unsigned long DEATH_CINEMATIC_MS        = 1500;  // hedef toplam sinematik süresi
constexpr unsigned long DEATH_SEGMENT_INTERVAL_MS = 40;    // varsayilan segment araligi
constexpr int DEATH_SEGMENT_PARTICLES            = 4;     // segment başina partikül
constexpr unsigned long DEATH_FLASH_MS            = 80;    // beyaz flash süresi

// ============ General Constants ============
constexpr int TARGET_FPS    = 60;
constexpr int FRAME_MS      = (1000 / TARGET_FPS);

// ============ Poison Timer (ms) ============
constexpr unsigned long POISON_LIFETIME  = 8000;
constexpr unsigned long POISON_BLINK_MS  = 6000;

// ============ Colors (RGB565) ============

// --- Sabit renkler (tema degismez): arkaplan / arena / UI ---
constexpr uint16_t COL_BG_A      = 0x0000;
constexpr uint16_t COL_BG_B      = 0x10A2; // Biraz daha açık koyu gri (TFT ekranda görünebilmesi için parlaklaştırıldı)
constexpr uint16_t COL_HUD_LINE  = 0x2104;
constexpr uint16_t COL_HUD_TEXT  = 0xBDF7;
constexpr uint16_t COL_BORDER    = 0x2945;
constexpr uint16_t COL_RED_DARK  = 0x8000;
constexpr uint16_t COL_ORANGE_DARK = 0x8200; // Koyu turuncu (ölüm dimCol)
constexpr uint16_t COL_YELLOW_DARK = 0x8400; // Koyu sari (ölüm dimCol)

// ============ Theme System (Hamle 5A) ============
// Yilan + yiyecek renkleri her 100 puanda degisir (5 tema dondurur).
// COL_* makrolari aktif temayi (g_theme[]) okur — mevcut draw
// fonksiyonlari kendi COL_* kullanimlariyla OTOMATIK temaya gecer,
// ayri refactor gerekmez. Arkaplan/arena/UI renkleri (COL_BG_*,
// COL_BORDER, COL_ARENA_DEAD, COL_HUD_*, COL_RED_DARK, COL_GHOST*,
// COL_MAGNET*) sabit kalir — tema bunlari degistirmez.
//
// 5x16 LUT PSRAM'de (g_themeLUT) tutulur; applyTheme() aktif temayi
// g_theme[]'e (internal RAM) kopyalar. Draw her frame hizli internal
// RAM'den okur; PSRAM'a sadece tema degisince (nadir) dokunulur.
enum ThemeColor {
    THEME_HEAD = 0, THEME_HEAD_DK, THEME_BODY_A, THEME_BODY_B, THEME_BODY_BRD,
    THEME_FOOD, THEME_FOOD_DIM, THEME_FOOD_HL, THEME_FOOD_STEM, THEME_FOOD_LEAF,
    THEME_GOLD, THEME_GOLD_DIM, THEME_GOLD_HL,
    THEME_POISON, THEME_POISON_DIM, THEME_POISON_HL,
    THEME_COLOR_COUNT
};

constexpr int THEME_COUNT = 5;
constexpr const char* THEME_NAMES[THEME_COUNT] = {"NEON","SUNSET","MATRIX","INFERNO","ARCTIC"};

// 5 tema x 16 renk (RGB565). Index 0 = Neon (orijinal) degerler.
constexpr uint16_t THEME_DATA[THEME_COUNT][THEME_COLOR_COUNT] = {
    // Neon      (yeşil yilan, kirmizi yem, sari altin, mor zehir)
    {0x07C0,0x0580,0x04A0,0x0560,0x0320, 0xF800,0xC000,0xFBE0,0x04A0,0x07E0, 0xFFE0,0xCC00,0xFFFF, 0x780F,0x4808,0xB81F},
    // Sunset    (turuncu yilan, magenta yem, sari altin, camgibi zehir)
    {0xFD20,0xF980,0xF800,0xFB00,0xB800, 0xF81F,0xB81F,0xFFFF,0x8200,0x07E0, 0xFFE0,0xCC00,0xFFFF, 0x07FF,0x047F,0xBFFF},
    // Matrix    (parlak yeşil yilan, magenta yem, beyaz altin, kirmizi zehir)
    {0x07E0,0x05E0,0x04E0,0x06C0,0x03E0, 0xF81F,0xB81F,0xFFFF,0x04E0,0x07E0, 0xFFFF,0x8410,0xFFFF, 0xF800,0xC000,0xFBE0},
    // Inferno   (kirmizi yilan, camgibi yem, sari altin, mavi zehir)
    {0xF800,0xB800,0xB800,0xD800,0x7800, 0x07FF,0x047F,0xFFFF,0x8200,0x07E0, 0xFFE0,0xCC00,0xFFFF, 0x001F,0x0010,0x7FFF},
    // Arctic    (buz camgibi yilan, turuncu yem, sari altin, magenta zehir)
    {0x07FF,0x057F,0x047F,0x067F,0x033F, 0xFD20,0xFA80,0xFFFF,0x8200,0x07E0, 0xFFE0,0xCC00,0xFFFF, 0xF81F,0xB81F,0xFFFF}
};

// Aktif tema — internal RAM (draw fonksiyonlari buradan okur)
extern uint16_t  g_theme[THEME_COLOR_COUNT];
// PSRAM LUT (5x16) — applyTheme buradan kopyalar; NULL ise THEME_DATA'ya fallback
extern uint16_t* g_themeLUT;

inline void applyTheme(int idx) {
    if (idx < 0 || idx >= THEME_COUNT) idx = 0;
    const uint16_t* src = g_themeLUT ? (g_themeLUT + (size_t)idx * THEME_COLOR_COUNT)
                                     : &THEME_DATA[idx][0];
    for (int i = 0; i < THEME_COLOR_COUNT; i++) g_theme[i] = src[i];
}

// Tema-li renkler: makro -> g_theme[...] (runtime). constexpr
// kullanimi (switch/case, array-size, template) YOK — makro guvenli.
#define COL_HEAD       (g_theme[THEME_HEAD])
#define COL_HEAD_DK    (g_theme[THEME_HEAD_DK])
#define COL_BODY_A     (g_theme[THEME_BODY_A])
#define COL_BODY_B     (g_theme[THEME_BODY_B])
#define COL_BODY_BRD   (g_theme[THEME_BODY_BRD])
#define COL_FOOD       (g_theme[THEME_FOOD])
#define COL_FOOD_DIM   (g_theme[THEME_FOOD_DIM])
#define COL_FOOD_HL    (g_theme[THEME_FOOD_HL])
#define COL_FOOD_STEM  (g_theme[THEME_FOOD_STEM])
#define COL_FOOD_LEAF  (g_theme[THEME_FOOD_LEAF])
#define COL_GOLD       (g_theme[THEME_GOLD])
#define COL_GOLD_DIM   (g_theme[THEME_GOLD_DIM])
#define COL_GOLD_HL    (g_theme[THEME_GOLD_HL])
#define COL_POISON     (g_theme[THEME_POISON])
#define COL_POISON_DIM (g_theme[THEME_POISON_DIM])
#define COL_POISON_HL  (g_theme[THEME_POISON_HL])

// ============ Menu Text Positions ============
constexpr int MENU_TITLE_X     = 50;
constexpr int MENU_TITLE_Y     = 8;
constexpr int MENU_TITLE_SH_X  = 51;
constexpr int MENU_TITLE_SH_Y  = 9;
constexpr int MENU_DEMO_Y      = 48;
constexpr int MENU_DEMO_HEAD_X = 100;
constexpr int MENU_APPLE_X     = 118;
constexpr int MENU_BTN_A_X     = 10;
constexpr int MENU_BTN_A_Y     = 95;
constexpr int MENU_BTN_B_X     = 85;
constexpr int MENU_BTN_B_Y     = 95;
constexpr int MENU_JOY_X       = 10;
constexpr int MENU_JOY_Y       = 110;
constexpr int MENU_RECORD_X    = 87;
constexpr int MENU_RECORD_Y    = 110;

// ============ Game Over Panel Positions ============
constexpr int GO_PANEL_X      = 15;
constexpr int GO_PANEL_Y      = 6;
constexpr int GO_PANEL_W      = 130;
constexpr int GO_PANEL_H      = 120;
constexpr int GO_PANEL_R      = 5;
constexpr int GO_TITLE_X      = 22;
constexpr int GO_TITLE_Y      = 12;
constexpr int GO_LABEL_X      = 30;
constexpr int GO_VALUE_X      = 75;
constexpr int GO_SCORE_Y      = 40;
constexpr int GO_FOOD_Y       = 60;
constexpr int GO_RECORD_Y     = 80;
constexpr int GO_NEWREC_Y     = 94;
constexpr int GO_RESTART_Y    = 104;
constexpr int GO_MENU_Y       = 114;

// ============ Pause Panel Positions ============
constexpr int PAUSE_PANEL_X   = 30;
constexpr int PAUSE_PANEL_Y   = 36;
constexpr int PAUSE_PANEL_W   = 100;
constexpr int PAUSE_PANEL_H   = 56;
constexpr int PAUSE_TITLE_X   = 50;
constexpr int PAUSE_TITLE_Y   = 42;
constexpr int PAUSE_RESUME_X  = 44;
constexpr int PAUSE_RESUME_Y  = 64;
constexpr int PAUSE_MENU_X    = 47;
constexpr int PAUSE_MENU_Y    = 78;

// ============ Enums ============
enum GameState { MENU, PLAYING, GAMEOVER, PAUSE };
enum FoodType  { FOOD_NORMAL, FOOD_GOLD, FOOD_POISON, FOOD_GHOST, FOOD_MAGNET };

// ============ Joystick Advanced Processing ============
// 1. Radial deadzone — Pythagorean magnitude (circular, not square)
constexpr int JOY_DEADZONE_RADIUS = 500;

// 2. Dynamic EMA alpha — adaptive smoothing
constexpr float EMA_ALPHA_MIN     = 0.10f;  // stable: heavy jitter suppression
constexpr float EMA_ALPHA_MAX     = 1.00f;  // flick:  instant response (zero lag)
constexpr int   FLICK_THRESHOLD   = 350;    // raw-EMA delta above this = full flick

// 3. Octagonal gating — atan2() 4 equal angular cones
constexpr float CONE_HALF_ANGLE   = 45.0f;  // each cone = 90 degrees (half = 45)
constexpr float CONE_DEAD_DEG     = 5.0f;   // angular hysteresis at cone boundaries
constexpr unsigned long DIR_COOLDOWN_MS = 60;  // min time between direction changes

// 4. Auto-center drift calibration — silent background recalibration
constexpr unsigned long AUTO_CAL_IDLE_MS    = 5000;  // idle time before auto-cal starts
constexpr unsigned long AUTO_CAL_INTERVAL_MS = 500;  // drift step interval
constexpr int AUTO_CAL_MAX_DRIFT  = 200;    // max center drift from original (safety)
