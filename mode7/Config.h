#pragma once
// ============================================================
//  E-OS MODE7 RACING — Config.h
//  Tum sabitler, renk paleti, struct'lar ve statik bellek dizileri.
//  DINAMIK BELLEK (malloc) KULLANILMAZ — tum diziler constexpr boyutlu.
// ============================================================

#include <Wire.h>

// ============ Ekran Boyutlari (Landscape 160x128) ============
constexpr int SW      = 160;
constexpr int SH      = 128;
constexpr int ROAD_H  = 106;
constexpr int HUD_H   = SH - ROAD_H;
constexpr int HORIZON = 38;

// ============ RGB565 Renk Makrosu (constexpr fonksiyon) ============
constexpr uint16_t RGB(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3));
}

// ============ Renk Paleti ============
constexpr uint16_t COL_SKY_TOP   = RGB(100, 180, 255);
constexpr uint16_t COL_SKY_BOT   = RGB(60, 130, 220);
constexpr uint16_t COL_ROAD_A    = RGB(90, 90, 100);
constexpr uint16_t COL_ROAD_B    = RGB(80, 80, 90);
constexpr uint16_t COL_GRASS_A   = RGB(40, 160, 50);
constexpr uint16_t COL_GRASS_B   = RGB(35, 140, 42);
constexpr uint16_t COL_LINE      = RGB(255, 255, 255);
constexpr uint16_t COL_EDGE_A    = RGB(255, 60, 60);
constexpr uint16_t COL_EDGE_B    = RGB(255, 255, 255);
constexpr uint16_t COL_CAR_BODY  = RGB(255, 40, 40);
constexpr uint16_t COL_CAR_WIND  = RGB(40, 80, 160);
constexpr uint16_t COL_HUD_BG    = RGB(20, 20, 30);
constexpr uint16_t COL_HUD_TXT   = RGB(200, 200, 200);
constexpr uint16_t COL_HUD_BRIGHT= 0xBDF7;   // Açık gri (panel metin)
constexpr uint16_t COL_PANEL     = 0x2104;   // Koyu lacivert panel dolgu
constexpr uint16_t COL_RED_DARK  = 0x8000;   // Koyu kirmizi panel çerçeve
constexpr uint16_t COL_DARKEN_MASK = 0x7BEF; // Renk karartma bitmask'i (derinlik/spoiler)

// ============ Yapilar (Struct) ============
struct Vec2        { float x, y; };
struct Racer       { float x, y; float angle; float speed; };
struct Checkpoint  { float x, y; float radius; };

// ============ Oyun Durumlari (State Machine Enum) ============
enum GameState { ST_TITLE, ST_COUNTDOWN, ST_RACING, ST_FINISH, ST_PAUSE };

// ============ Pist Sabitleri ============
constexpr int MAP_W          = 200;
constexpr int MAP_H          = 200;
constexpr int ROAD_W         = 11;
constexpr int NUM_POSTS      = 36;
constexpr int NUM_CHECKPOINTS = 8;
constexpr int NUM_AI         = 2;

// ============ Yaris Sabitleri ============
constexpr int TOTAL_LAPS     = 3;

// ============ Fizik Sabitleri (saniye bazli, delta-time ile carpilir) ============
constexpr float ACCEL       = 16.0f;
constexpr float BRAKE_FORCE = 32.0f;
constexpr float DRAG        = 4.0f;
constexpr float STEER_RATE  = 1.2f;
constexpr float MAX_SPEED   = 30.0f;
constexpr float GRASS_SLOW  = 12.0f;

// ============ Motor Sesi Frekanslari (surekli ton, NOTE_* kapsamina girmez) ============
constexpr int MOTOR_GAS_FREQ   = 205;
constexpr int MOTOR_IDLE_FREQ  = 165;

// ============ AI Rubber-Band Hizlari (saniye bazli) ============
constexpr float AI_NORMAL_SPEED = 30.0f;
constexpr float AI_FAST_SPEED   = 36.0f;
constexpr float AI_SLOW_SPEED   = 24.0f;

// ============ Pist Viraj Dalgalanmasi (inline fonksiyon — #define yerine) ============
inline float TRK_WOB(float rad) {
    return 1.0f + 0.15f * sinf(3.0f * rad);
}

// ============ Statik Bellek Dizileri (extern — mode7.ino'da tanimlanir) ============
// MALLOC YASAK! Harita, direkler ve checkpoint'ler sabit boyutlu statik dizilerdir.
extern uint8_t    trackMap[MAP_W * MAP_H];
extern Vec2       posts[NUM_POSTS];
extern Checkpoint checkpoints[NUM_CHECKPOINTS];
