#pragma once
// ============================================================
//  Config.h — Rhythm Beats sabitleri
//  Serit yerlesimi, zamanlama pencereleri, puan/saglik degerleri,
//  RGB565 renk paleti, Note struct'i ve state enum'u.
// ============================================================

#include <TFT_eSPI.h>
#include "../GameBase.h"

// ============ Ekran (Landscape) ============
constexpr int SCR_W = 160;
constexpr int SCR_H = 128;
constexpr int TARGET_FPS = 60;
constexpr int FRAME_MS   = 1000 / TARGET_FPS;

// ============ Yon sabitleri (SharedJoystick bunlari bekler) ============
constexpr int DIR_UP    = 0;
constexpr int DIR_RIGHT = 1;
constexpr int DIR_DOWN  = 2;
constexpr int DIR_LEFT  = 3;

// ============ Serit duzeni ============
// 160 px / 4 serit = 40 px. Soldan saga fiziksel buton sirasi:
// D(sol), A(ust), C(alt), B(sag) — elmas dizilim rahatligi icin.
constexpr int     LANE_COUNT              = 4;
constexpr int     LANE_W                  = 40;
constexpr int     LANE_CENTER[LANE_COUNT] = { 20, 60, 100, 140 };
constexpr uint8_t LANE_PINS[LANE_COUNT]   = { BTN_D, BTN_A, BTN_C, BTN_B };
constexpr char    LANE_LABELS[LANE_COUNT] = { 'D', 'A', 'C', 'B' };

// ============ Nota geometrisi ============
constexpr int   NOTE_W  = 14;      // yatay dikdortgen nota
constexpr int   NOTE_H  = 8;
constexpr int   HIT_Y   = 108;     // vurus cizgisi
constexpr float SPAWN_Y = -10.0f;  // notalar ekran ustunden dogar

// Alt bar (serit etiketleri + basili geri bildirim)
constexpr int BOTTOM_BAR_Y = 112;

// ============ Zamanlama pencereleri (hit line'a px mesafe) ============
constexpr float WIN_PERFECT_PX = 6.0f;
constexpr float WIN_GOOD_PX    = 14.0f;
constexpr float WIN_OK_PX      = 22.0f;

// ============ Puanlar ============
constexpr int SCORE_PERFECT = 100;   // x combo carpani
constexpr int SCORE_GOOD    = 50;    // x combo carpani
constexpr int SCORE_OK      = 20;    // carpansiz

// Combo carpani: <10 -> x1, <25 -> x2, <50 -> x3, >=50 -> x4
inline int comboMultiplier(int combo) {
    if (combo >= 50) return 4;
    if (combo >= 25) return 3;
    if (combo >= 10) return 2;
    return 1;
}

// ============ Saglik ============
constexpr int HEALTH_START   = 80;
constexpr int HEALTH_MAX     = 100;
constexpr int HP_PERFECT     = 3;
constexpr int HP_GOOD        = 1;
constexpr int HP_MISS        = -8;   // OK = +0
constexpr int HEALTH_BAR_X   = 0;    // sol kenar dikey bar
constexpr int HEALTH_BAR_W   = 2;
constexpr int HEALTH_BAR_TOP = 14;
constexpr int HEALTH_BAR_BOT = 106;

// ============ Sureler ============
constexpr unsigned long JUDGE_SHOW_MS     = 400;   // PERFECT/GOOD/OK/MISS yazisi
constexpr unsigned long FLASH_MS          = 80;    // PERFECT hit-line beyaz flash
constexpr unsigned long COUNTDOWN_STEP_MS = 800;   // 3-2-1-GO adim suresi
constexpr unsigned long SONG_END_PAD_MS   = 2000;  // son notadan sonra bekleme
constexpr unsigned long MELODY_MUTE_MS    = 400;   // miss sonrasi melodi susar
constexpr unsigned long RESULT_LOCK_MS    = 600;   // sonuc ekrani girdi kilidi
constexpr unsigned long STANDALONE_MSG_MS = 1200;

// ============ Renkler (RGB565, neon/retro-arcade) ============
constexpr uint16_t C565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t COL_BG        = C565(8, 8, 20);       // cok koyu lacivert
constexpr uint16_t COL_BG_HYPE   = C565(16, 8, 34);      // combo >= 20 canli fon
constexpr uint16_t COL_LANE_LINE = C565(30, 30, 60);     // serit ayirici cizgiler

// Nota renkleri — serit sirasina gore (D, A, C, B)
constexpr uint16_t COL_LANES[LANE_COUNT] = {
    C565(255,  60,  80),   // 0: kirmizi-pembe (D)
    C565( 60, 200, 255),   // 1: cyan-mavi (A)
    C565(100, 255,  80),   // 2: yesil (C)
    C565(255, 200,  40),   // 3: altin-sari (B)
};
// Nota kenar pariltisi (ayni tonlarin aciklari)
constexpr uint16_t COL_LANES_GLOW[LANE_COUNT] = {
    C565(255, 140, 155),
    C565(150, 225, 255),
    C565(175, 255, 160),
    C565(255, 225, 130),
};

constexpr uint16_t COL_HIT_LINE  = C565(255, 255, 255);  // beyaz vurus cizgisi
constexpr uint16_t COL_HIT_GLOW  = C565(80, 80, 120);    // soluk parlama

// Geri bildirim renkleri
constexpr uint16_t COL_PERFECT   = C565(255, 255, 80);
constexpr uint16_t COL_GOOD      = C565(80, 255, 80);
constexpr uint16_t COL_OK        = C565(180, 180, 180);
constexpr uint16_t COL_MISS      = C565(255, 40, 40);

// Saglik bari renkleri
constexpr uint16_t COL_HP_HIGH   = C565(60, 220, 60);    // > 60
constexpr uint16_t COL_HP_MID    = C565(230, 200, 40);   // 30-60
constexpr uint16_t COL_HP_LOW    = C565(230, 40, 40);    // < 30

constexpr uint16_t COL_GRAY_TXT  = C565(120, 120, 130);

// ============ Vurus dereceleri ============
enum HitGrade : uint8_t { GRADE_NONE = 255, GRADE_MISS = 0, GRADE_OK = 1, GRADE_GOOD = 2, GRADE_PERFECT = 3 };

// ============ Nota havuzu ============
// En yogun sarki: 188 ms araliklarla nota, ekranda ~2 sn'lik yol
// -> ayni anda en fazla ~12 nota. 24 guvenli tavan.
constexpr int MAX_NOTES = 24;

struct Note {
    uint8_t  lane;        // 0-3 serit
    float    y;           // ekran Y (her frame songTime'dan hesaplanir)
    uint32_t targetTime;  // ms — hit line'a ulasmasi gereken an
    bool     active;      // havuzda kullanimda mi
    bool     hit;         // vuruldu mu
};

// ============ State machine ============
enum GameState { ST_MENU, ST_READY, ST_PLAYING, ST_RESULTS, ST_GAMEOVER, ST_PAUSE };
