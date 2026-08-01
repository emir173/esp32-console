#pragma once
// ============================================================
//  towerdefense/Map.h — Çok Bölümlü Harita ve Yol (Path) Sistemi
//
//  Sorumluluk: Seçili bölümün haritasının çalışma zamanı kopyası,
//  tile sorguları (tileAt, canBuildTower), waypoint listesinin
//  tile-tile genişletilmiş yol dizisine dönüştürülmesi ve
//  spawn/kale koordinatlarının (uçan düşman rotası için) tutulması.
//  (Dungeon/Map.h grid sorgu pattern'i referans alınmıştır.)
// ============================================================

#include "Config.h"

// ------------------------------------------------------------
//  Çalışma zamanı harita (kule yerleştirme TILE_TOWER yazar)
// ------------------------------------------------------------
uint8_t tiles[MAP_H][MAP_W];

// ------------------------------------------------------------
//  Genişletilmiş yol: waypoint'ler arası her tile tek adım
// ------------------------------------------------------------
struct PathTile { int8_t x, y; };

PathTile pathTiles[MAX_PATH];
int pathLen = 0;

// Spawn ve kale tile koordinatları (bölüme göre değişir)
int spawnTileX = 0, spawnTileY = 0;
int baseTileX  = 0, baseTileY  = 0;
float flyDistPx = 1.0f;   // Spawn→kale kuş uçuşu mesafe (uçan düşman ilerleme metriği)

// ------------------------------------------------------------
//  Sorgular
// ------------------------------------------------------------
inline bool inBounds(int x, int y) {
    return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H;
}

// Sınır dışı için TILE_ROCK döner (güvenli varsayılan: inşa edilemez)
inline uint8_t tileAt(int x, int y) {
    if (!inBounds(x, y)) return TILE_ROCK;
    return tiles[y][x];
}

// Kule sadece boş çimene yerleştirilebilir
inline bool canBuildTower(int x, int y) {
    return tileAt(x, y) == TILE_GRASS;
}

// ------------------------------------------------------------
//  Waypoint listesini tile-tile yola genişlet.
//  Waypoint'ler eksen hizalı olduğundan her segment tek yönde yürür.
// ------------------------------------------------------------
inline void expandPath(int level) {
    int li = level - 1;
    int wpCount = PATH_WP_COUNTS[li];

    pathLen = 0;
    pathTiles[pathLen++] = { PATH_WPS[li][0][0], PATH_WPS[li][0][1] };
    for (int w = 1; w < wpCount; w++) {
        int cx = PATH_WPS[li][w - 1][0], cy = PATH_WPS[li][w - 1][1];
        int tx = PATH_WPS[li][w][0],     ty = PATH_WPS[li][w][1];
        int sx = (tx > cx) ? 1 : (tx < cx) ? -1 : 0;
        int sy = (ty > cy) ? 1 : (ty < cy) ? -1 : 0;
        while ((cx != tx || cy != ty) && pathLen < MAX_PATH) {
            cx += sx;
            cy += sy;
            pathTiles[pathLen++] = { (int8_t)cx, (int8_t)cy };
        }
    }

    spawnTileX = pathTiles[0].x;
    spawnTileY = pathTiles[0].y;
    baseTileX  = pathTiles[pathLen - 1].x;
    baseTileY  = pathTiles[pathLen - 1].y;

    // Uçan düşmanlar için kuş uçuşu toplam mesafe (piksel)
    float fdx = (baseTileX - spawnTileX) * (float)TILE_PX;
    float fdy = (baseTileY - spawnTileY) * (float)TILE_PX;
    flyDistPx = sqrtf(fdx * fdx + fdy * fdy);
    if (flyDistPx < 1.0f) flyDistPx = 1.0f;
}

// ------------------------------------------------------------
//  Bölüm haritasını yükle: sabit veriyi kopyala + yolu genişlet
//  (Bölüm geçişi ve restart'ta kule tile'ları da temizlenmiş olur)
// ------------------------------------------------------------
inline void initMap(int level) {
    if (level < 1) level = 1;
    if (level > LEVEL_COUNT) level = LEVEL_COUNT;
    const uint8_t (*src)[MAP_W] = LEVEL_MAPS[level - 1];

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            tiles[y][x] = src[y][x];

    expandPath(level);
}
