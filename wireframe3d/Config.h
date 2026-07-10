#pragma once
// ============================================================
//  Config.h — Wireframe3D sabitleri, renk paleti, enum'lar
//  Tum sihirli sayilar burada constexpr olarak tanimlanir.
// ============================================================

#include <TFT_eSPI.h>
#include <math.h>

// ============ Ekran Boyutlari (Landscape 160x128) ============
constexpr int SCR_W     = 160;
constexpr int SCR_H     = 128;
constexpr int HUD_H     = 12;

// ============ RGB565 Renk Donusturme ============
constexpr inline uint16_t RGB(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3);
}

// ============ Renk Paleti (RGB565 BGR sirali — RGB() ile hesaplanir) ============
constexpr uint16_t COL_BG         = RGB(2, 2, 8);          // koyu uzay mavisi
constexpr uint16_t COL_STAR       = RGB(100, 100, 140);    // soluk mavi-beyaz
constexpr uint16_t COL_WIRE_CYAN  = RGB(0, 255, 255);      // cyan
constexpr uint16_t COL_WIRE_GREEN = RGB(0, 255, 80);       // yesil
constexpr uint16_t COL_WIRE_RED   = RGB(255, 60, 60);      // kirmizi
constexpr uint16_t COL_WIRE_GOLD  = RGB(255, 200, 40);     // altin
constexpr uint16_t COL_WIRE_PINK  = RGB(255, 80, 200);     // pembe
constexpr uint16_t COL_BULLET     = RGB(255, 255, 100);    // sari
constexpr uint16_t COL_HIT        = RGB(255, 100, 100);    // acik kirmizi
constexpr uint16_t COL_CROSS      = RGB(0, 255, 0);        // yesil nisan
constexpr uint16_t COL_HUD_BG     = RGB(0, 0, 20);         // koyu lacivert
constexpr uint16_t COL_HUD_TXT    = RGB(180, 180, 200);    // gri
constexpr uint16_t COL_BOOM       = RGB(255, 180, 40);     // turuncu-sari
constexpr uint16_t COL_WHITE      = RGB(255, 255, 255);    // beyaz (HUD metin)

// ============ Kare Hizi ============
constexpr int TARGET_FPS = 60;
constexpr int FRAME_MS   = (1000 / TARGET_FPS);

// ============ Nesne Havuz Sinirlari (sabit dizi boyutlari) ============
constexpr int MAX_STARS      = 30;
constexpr int MAX_OBJECTS    = 8;
constexpr int MAX_BULLETS    = 6;
constexpr int MAX_EXPLOSIONS = 6;
constexpr int MAX_DUST       = 20;

// ============ 3D Projeksiyon Sabitleri ============
constexpr float FOCAL     = 80.0f;
constexpr float NEAR_CLIP = 1.0f;

// ============ Oyun Denge Sabitleri ============
constexpr float SPAWN_DIST_MIN   = 30.0f;
constexpr float SPAWN_DIST_RANGE = 20.0f;
constexpr float OBJ_BASE_SPEED   = 3.75f;
constexpr float BULLET_SPEED     = 37.5f;
constexpr float BULLET_LIFETIME  = 1.6f;
constexpr float COLLIDE_DIST     = 3.0f;
constexpr float DESPAWN_DIST     = 60.0f;
constexpr float SHOOT_CD         = 0.4f;
constexpr float SPAWN_BASE_INT   = 1.6f;
constexpr float SPAWN_WAVE_DEC   = 0.12f;
constexpr float SPAWN_MIN_INT    = 0.6f;
constexpr float HIT_FLASH_DUR    = 0.24f;
constexpr float BOOM_DUR         = 0.6f;
constexpr float DUST_SPEED       = 7.5f;
constexpr float DUST_Z_RESET     = 40.0f;
constexpr float CAM_YAW_SPEED    = 1.5f;
constexpr float CAM_PITCH_SPEED  = 1.0f;
constexpr float CAM_PITCH_MAX    = 0.5f;
constexpr int   INITIAL_HP       = 5;
constexpr float INITIAL_SPAWN_T  = 2.4f;

// ============ Mesh Tanimlari — Kup (8 kose, 12 kenar) ============
constexpr int CUBE_VERTS = 8;
constexpr int CUBE_EDGES = 12;
const float cubeVerts[CUBE_VERTS][3] = {
    {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
    {-1,-1,1},  {1,-1,1},  {1,1,1},  {-1,1,1}
};
constexpr uint8_t cubeEdges[CUBE_EDGES][2] = {
    {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
};

// ============ Piramit (5 kose, 8 kenar) ============
constexpr int PYR_VERTS = 5;
constexpr int PYR_EDGES = 8;
const float pyrVerts[PYR_VERTS][3] = {
    {0,-1.5f,0}, {-1,0.5f,-1}, {1,0.5f,-1}, {1,0.5f,1}, {-1,0.5f,1}
};
constexpr uint8_t pyrEdges[PYR_EDGES][2] = {
    {0,1},{0,2},{0,3},{0,4}, {1,2},{2,3},{3,4},{4,1}
};

// ============ Elmas (6 kose, 12 kenar) ============
constexpr int DIAM_VERTS = 6;
constexpr int DIAM_EDGES = 12;
constexpr float diamVerts[DIAM_VERTS][3] = {
    {0,-1.5f,0}, {0,1.5f,0}, {-1,0,-1}, {1,0,-1}, {1,0,1}, {-1,0,1}
};
constexpr uint8_t diamEdges[DIAM_EDGES][2] = {
    {0,2},{0,3},{0,4},{0,5}, {1,2},{1,3},{1,4},{1,5}, {2,3},{3,4},{4,5},{5,2}
};

// ============ Enum'lar ============
enum GameState { ST_TITLE, ST_PLAY, ST_GAMEOVER, ST_PAUSE };
enum ObjType   { OBJ_CUBE, OBJ_PYRAMID, OBJ_DIAMOND };
