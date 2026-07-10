#pragma once
// ============================================================
//  towerdefense/Config.h — E-OS TOWER DEFENSE Yapılandırma Merkezi
//
//  Sorumluluk: Tüm constexpr sabitler, RGB565 renk paleti,
//  enum'lar, 3 bölümün sabit harita verileri ve yol waypoint'leri,
//  kule/düşman istatistik tabloları ve ortak ileri bildirimler.
//  Diğer TÜM başlık dosyaları bunu include eder.
//  Sihirli sayı YASAK — her sabit burada isimlendirilmiştir.
// ============================================================

#include <Arduino.h>
#include <TFT_eSPI.h>

// ------------------------------------------------------------
//  EKRAN VE GRID
// ------------------------------------------------------------
constexpr int SCR_W   = 160;                             // Ekran genişliği (piksel)
constexpr int SCR_H   = 128;                             // Ekran yüksekliği (piksel)
constexpr int HUD_H   = 14;                              // Üst HUD şeridi yüksekliği
constexpr int TILE_PX = 8;                               // Bir hücrenin piksel boyutu
constexpr int MAP_W   = 20;                              // Harita genişliği (tile)
constexpr int MAP_H   = 14;                              // Harita yüksekliği (tile)

// ------------------------------------------------------------
//  ZAMAN / FPS
// ------------------------------------------------------------
constexpr int   TARGET_FPS = 60;                         // Hedef kare hızı
constexpr float FRAME_SEC  = 1.0f / TARGET_FPS;          // Bir karenin ideal süresi (sn)
constexpr uint32_t FRAME_MS = 1000 / TARGET_FPS;         // Kare hızı sınırı (ms)
constexpr float DT_CAP     = 0.05f;                      // Lag spike koruması: dt üst sınırı (sn)
constexpr uint32_t FPS_WINDOW_MS = 1000;                 // FPS sayacı ölçüm penceresi

// ------------------------------------------------------------
//  TILE TÜRLERİ
// ------------------------------------------------------------
constexpr uint8_t TILE_GRASS = 0;                        // Kule yerleştirilebilir zemin
constexpr uint8_t TILE_PATH  = 1;                        // Düşman yolu (kule yok)
constexpr uint8_t TILE_TOWER = 2;                        // Kule var (çalışma anında atanır)
constexpr uint8_t TILE_BASE  = 3;                        // Kale (bitiş noktası)
constexpr uint8_t TILE_SPAWN = 4;                        // Düşman başlangıç noktası
constexpr uint8_t TILE_ROCK  = 5;                        // Dekoratif kaya (kule yok)

// ------------------------------------------------------------
//  BİYOMLAR (3 tema × 3 bölüm = 9 bölüm)
//  Biyom = (level-1)/3: 0 ORMAN (b1-3), 1 DONMUŞ (b4-6), 2 CEHENNEM (b7-9)
// ------------------------------------------------------------
enum Biome : uint8_t { BIOME_FOREST, BIOME_FROST, BIOME_HELL, BIOME_COUNT };
constexpr int LEVELS_PER_BIOME = 3;                      // Her biyomda bölüm sayısı

// ------------------------------------------------------------
//  BÖLÜMLER (9 el yapımı harita)
//  Legenda: 0=çimen  1=yol  3=kale  4=spawn  5=kaya
// ------------------------------------------------------------
constexpr int LEVEL_COUNT = 9;                           // Toplam bölüm sayısı (3 biyom × 3)
constexpr int MAX_WP      = 8;                           // Bölüm başına maks waypoint
constexpr int MAX_PATH    = 90;                          // Genişletilmiş yol tile havuzu

// Level (1..9) → biyom indeksi (0..2)
inline int biomeOf(int level) {
    int b = (level - 1) / LEVELS_PER_BIOME;
    if (b < 0) b = 0;
    if (b >= BIOME_COUNT) b = BIOME_COUNT - 1;
    return b;
}

// BÖLÜM 1 — "S Yolu": sol orta giriş, sağ orta kale
// Yol: (0,7)→(5,7)→(5,3)→(12,3)→(12,10)→(17,10)→(17,7)→(19,7)
constexpr uint8_t MAP_L1[MAP_H][MAP_W] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=0
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=1
    {0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0},  // y=2
    {0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0},  // y=3
    {0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,5,0,0,0,0},  // y=4
    {0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0},  // y=5
    {0,0,0,0,0,1,0,0,0,5,0,0,1,0,0,0,0,0,0,0},  // y=6
    {4,1,1,1,1,1,0,0,0,0,0,0,1,0,0,0,0,1,1,3},  // y=7
    {0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0},  // y=8
    {0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0},  // y=9
    {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0},  // y=10
    {0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=11
    {0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0},  // y=12
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}   // y=13
};

// BÖLÜM 2 — "Yılan": üstten giriş, yatay şeritler halinde iner
// Yol: (0,2)→(16,2)→(16,6)→(3,6)→(3,10)→(19,10)
constexpr uint8_t MAP_L2[MAP_H][MAP_W] = {
    {0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0},  // y=0
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=1
    {4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},  // y=2
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0},  // y=3
    {0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,1,0,0,5},  // y=4
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0},  // y=5
    {0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},  // y=6
    {0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=7
    {0,0,0,1,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0},  // y=8
    {0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=9
    {0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3},  // y=10
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=11
    {0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,5,0,0,0,0},  // y=12
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}   // y=13
};

// BÖLÜM 3 — "Girdap": alttan giriş, içe kıvrılan spiral, kale merkezde
// Yol: (0,12)→(17,12)→(17,1)→(2,1)→(2,9)→(13,9)→(13,4)→(6,4)
constexpr uint8_t MAP_L3[MAP_H][MAP_W] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=0
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},  // y=1
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,5,0,1,0,0},  // y=2
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},  // y=3
    {0,0,1,0,0,0,3,1,1,1,1,1,1,1,0,0,0,1,0,0},  // y=4
    {0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},  // y=5
    {0,0,1,0,0,0,0,0,0,0,5,0,0,1,0,0,0,1,0,0},  // y=6
    {0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},  // y=7
    {0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},  // y=8
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0,0},  // y=9
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},  // y=10
    {0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},  // y=11
    {4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},  // y=12
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}   // y=13
};

// ---- DONMUŞ DİYAR (biyom 1) ----
// BÖLÜM 4 — "Buz Koridoru": sol üstten yatay zikzak, sağ altta kale
constexpr uint8_t MAP_L4[MAP_H][MAP_W] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0},  // y=0
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=1
    {4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},  // y=2
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0},  // y=3
    {0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,1,0},  // y=4
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0},  // y=5
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0},  // y=6
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},  // y=7
    {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=8
    {0,1,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0},  // y=9
    {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=10
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3},  // y=11
    {0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0},  // y=12
    {0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}   // y=13
};

// BÖLÜM 5 — "Çift Sarkaç": üstten dikey serpantin, sağ ortada kale
constexpr uint8_t MAP_L5[MAP_H][MAP_W] = {
    {0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=0
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=1
    {0,0,1,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0},  // y=2
    {0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0},  // y=3
    {0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,3,0,0},  // y=4
    {0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0},  // y=5
    {0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0},  // y=6
    {0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,5,0,1,0,0},  // y=7
    {0,0,1,0,5,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0},  // y=8
    {0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0},  // y=9
    {0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0},  // y=10
    {0,0,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,0,0},  // y=11
    {0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0},  // y=12
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}   // y=13
};

// BÖLÜM 6 — "Buz Girdabı": içe kıvrılan spiral, kale merkezde
constexpr uint8_t MAP_L6[MAP_H][MAP_W] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=0
    {4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},  // y=1
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0},  // y=2
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0},  // y=3
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0},  // y=4
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0},  // y=5
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,5,0,1,0},  // y=6
    {0,0,1,0,0,5,0,0,0,0,0,0,0,0,1,0,0,0,1,0},  // y=7
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0},  // y=8
    {0,0,1,0,0,0,0,3,1,1,1,1,1,1,1,0,0,0,1,0},  // y=9
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0},  // y=10
    {0,0,1,0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,1,0},  // y=11
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},  // y=12
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}   // y=13
};

// ---- CEHENNEM (biyom 2) ----
// BÖLÜM 7 — "Lav Nehri": sol ortadan geniş S, sağ altta kale
constexpr uint8_t MAP_L7[MAP_H][MAP_W] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=0
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=1
    {0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0},  // y=2
    {0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0},  // y=3
    {0,0,0,0,0,0,1,0,0,5,0,0,0,1,0,0,0,0,0,0},  // y=4
    {0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0},  // y=5
    {4,1,1,1,1,1,1,0,0,0,0,0,0,1,0,0,5,0,0,0},  // y=6
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0},  // y=7
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0},  // y=8
    {0,0,0,5,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0},  // y=9
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0},  // y=10
    {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,3},  // y=11
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=12
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}   // y=13
};

// BÖLÜM 8 — "Alev Labirenti": sıkışık dört dönüş, sol altta kale
constexpr uint8_t MAP_L8[MAP_H][MAP_W] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=0
    {4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},  // y=1
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0},  // y=2
    {0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,1,0,0,0},  // y=3
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0},  // y=4
    {0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},  // y=5
    {0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0},  // y=6
    {0,0,0,1,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0},  // y=7
    {0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=8
    {0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},  // y=9
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0},  // y=10
    {0,0,0,0,0,0,0,0,0,5,0,0,0,0,0,0,1,0,0,0},  // y=11
    {0,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},  // y=12
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}   // y=13
};

// BÖLÜM 9 — "Cehennem Kapısı": uzun spiral, kale merkezde
constexpr uint8_t MAP_L9[MAP_H][MAP_W] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // y=0
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},  // y=1
    {0,0,1,0,0,0,5,0,0,0,0,0,0,0,0,0,0,1,0,0},  // y=2
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,5,0},  // y=3
    {0,0,1,0,0,0,0,0,0,3,1,1,1,1,1,1,0,1,0,0},  // y=4
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0},  // y=5
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0},  // y=6
    {4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},  // y=7
    {0,0,1,0,0,0,0,0,0,0,0,5,0,0,0,1,0,0,0,0},  // y=8
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0},  // y=9
    {0,0,1,0,0,5,0,0,0,0,0,0,0,0,0,1,0,0,0,0},  // y=10
    {0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0},  // y=11
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0},  // y=12
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}   // y=13
};

// Bölüm indeksi (0..8) → harita verisi (initMap kopyalar)
constexpr const uint8_t (*LEVEL_MAPS[LEVEL_COUNT])[MAP_W] = {
    MAP_L1, MAP_L2, MAP_L3, MAP_L4, MAP_L5, MAP_L6, MAP_L7, MAP_L8, MAP_L9
};

// Bölüm başına waypoint listeleri (tile koordinatı) ve sayıları.
// Kısa bölümlerde son waypoint tekrar edilerek MAX_WP'ye tamamlanır.
constexpr int8_t PATH_WPS[LEVEL_COUNT][MAX_WP][2] = {
    { {0,7}, {5,7}, {5,3}, {12,3}, {12,10}, {17,10}, {17,7}, {19,7} },      // L1
    { {0,2}, {16,2}, {16,6}, {3,6}, {3,10}, {19,10}, {19,10}, {19,10} },    // L2
    { {0,12}, {17,12}, {17,1}, {2,1}, {2,9}, {13,9}, {13,4}, {6,4} },       // L3
    { {0,2}, {18,2}, {18,7}, {1,7}, {1,11}, {19,11}, {19,11}, {19,11} },    // L4
    { {2,0}, {2,11}, {7,11}, {7,2}, {12,2}, {12,11}, {17,11}, {17,4} },     // L5
    { {0,1}, {18,1}, {18,12}, {2,12}, {2,4}, {14,4}, {14,9}, {7,9} },       // L6
    { {0,6}, {6,6}, {6,2}, {13,2}, {13,11}, {19,11}, {19,11}, {19,11} },    // L7
    { {0,1}, {16,1}, {16,5}, {3,5}, {3,9}, {16,9}, {16,12}, {1,12} },       // L8
    { {0,7}, {17,7}, {17,1}, {2,1}, {2,12}, {15,12}, {15,4}, {9,4} }        // L9
};
constexpr int PATH_WP_COUNTS[LEVEL_COUNT] = { 8, 6, 8, 6, 8, 8, 6, 8, 8 };

// Biyom adları (menü / bölüm seçimi / geçiş ekranları)
constexpr const char *BIOME_NAME[BIOME_COUNT] = { "FOREST", "FROZEN", "INFERNO" };

// ------------------------------------------------------------
//  GİRDİ
// ------------------------------------------------------------
constexpr int      JOY_DEADZONE   = 500;                 // Joystick ölü bölge (ADC birimi)
constexpr uint32_t MOVE_REPEAT_MS = 140;                 // İmleç sürekli tutuşta tekrar aralığı
constexpr uint32_t MENU_REPEAT_MS = 180;                 // Menü imleç tekrar aralığı

// Yönler (imleç hareketi için)
constexpr int DIR_UP    = 0;
constexpr int DIR_RIGHT = 1;
constexpr int DIR_DOWN  = 2;
constexpr int DIR_LEFT  = 3;
constexpr int8_t DIR_DX[4] = { 0, 1, 0, -1 };            // Yön → x delta
constexpr int8_t DIR_DY[4] = { -1, 0, 1, 0 };            // Yön → y delta

// ------------------------------------------------------------
//  KULELER (5 tür)
// ------------------------------------------------------------
enum TowerType : uint8_t {
    TOWER_ARROW, TOWER_CANNON, TOWER_FROST, TOWER_LASER, TOWER_POISON,
    TOWER_TYPE_COUNT
};

constexpr int MAX_TOWERS = 16;                           // Kule havuzu boyutu

// Tür bazlı temel istatistikler (indeks = TowerType)
//                                              OKCU  TOP  BUZ  LAZER ZEHIR
constexpr int      TOWER_COST[TOWER_TYPE_COUNT]   = { 15,  40,  25,  60,  35  };
constexpr int      TOWER_RANGE[TOWER_TYPE_COUNT]  = { 3,   2,   2,   4,   2   };
constexpr int      TOWER_DMG[TOWER_TYPE_COUNT]    = { 2,   6,   1,   2,   1   };
constexpr uint16_t TOWER_INT_MS[TOWER_TYPE_COUNT] = { 250, 900, 500, 160, 800 };

constexpr int TOWER_MAX_LEVEL   = 3;                     // Maks kule seviyesi
constexpr int TOWER_MAX_RANGE   = 5;                     // Yükseltmeyle ulaşılabilir maks menzil
constexpr int SPLASH_RADIUS_PX  = TILE_PX;               // Cannon alan hasarı yarıçapı (1 tile)
constexpr uint32_t FROST_SLOW_MS = 1500;                 // Frost yavaşlatma süresi
constexpr float    FROST_SLOW_FACTOR = 0.5f;             // Yavaşlatma hız çarpanı (%50)
constexpr uint32_t POISON_DUR_MS   = 2500;               // Zehir etki süresi
constexpr uint32_t POISON_TICK_MS  = 500;                // Zehir hasar aralığı
constexpr int      POISON_TICK_DMG = 1;                  // Zehir tik hasarı

// ------------------------------------------------------------
//  YILDIRIM YETENEĞİ (dalga sırasında BTN_D)
// ------------------------------------------------------------
constexpr int   LIGHTNING_COST = 40;                     // Para maliyeti
// Hasar dalga+biyomla ölçeklenir (düşman HP eğrisiyle aynı): sabit hasar ileri
// bölümlerde etkisiz kalıyordu (v4.1). dmg = (BASE + (dalga-1)×PER_WAVE) × BIOME_HP_MULT.
constexpr int   LIGHTNING_DMG_BASE     = 8;              // Taban hasar (dalga 1, orman)
constexpr int   LIGHTNING_DMG_PER_WAVE = 3;              // Dalga başına ek (fodder HP artışıyla aynı → temel düşmanı hep temizler)
constexpr float LIGHTNING_CD_S = 15.0f;                  // Bekleme süresi (sn)

// ------------------------------------------------------------
//  DÜŞMANLAR (5 tür)
// ------------------------------------------------------------
enum EnemyType : uint8_t {
    ENEMY_RUNNER, ENEMY_SOLDIER, ENEMY_TANK, ENEMY_FLYER, ENEMY_BOSS,
    ENEMY_ARMORED, ENEMY_HEALER, ENEMY_SWARM,
    ENEMY_TYPE_COUNT
};

constexpr int MAX_ENEMIES = 28;                          // Düşman havuzu (swarm kalabalığı için +4)

// Tür bazlı temel istatistikler (indeks = EnemyType)
//                                              RUNNER SOLDIER TANK  FLYER BOSS ARMORED HEALER SWARM
constexpr int   ENEMY_HP[ENEMY_TYPE_COUNT]     = { 6,    14,   35,   10,   60,  20,    22,    3    };
constexpr float ENEMY_SPEED[ENEMY_TYPE_COUNT]  = { 1.2f, 0.8f, 0.4f, 1.0f, 0.3f,0.5f,  0.6f,  1.7f };
constexpr int   ENEMY_REWARD[ENEMY_TYPE_COUNT] = { 4,    7,    14,   6,    40,  12,    15,    2    };
constexpr int   ENEMY_BASE_DMG[ENEMY_TYPE_COUNT] = { 1,  1,    3,    1,    5,   2,     1,     1    };
// Sabit hasar azaltma (her vuruştan düşülür; düşük hasarlı OKÇU spamını kırar)
constexpr int   ENEMY_ARMOR[ENEMY_TYPE_COUNT]  = { 0,    0,    0,    0,    0,   2,     0,     0    };

constexpr int   ENEMY_HP_PER_WAVE  = 3;                  // Dalga başına HP artışı (v4.0: 4→3, biyom çarpanına yaslandı)
constexpr int   BOSS_HP_PER_WAVE   = 7;                  // Boss için dalga başına HP artışı
constexpr float SPEED_SCALE_PER_WAVE = 0.02f;            // Dalga başına hız artışı (+%2)
constexpr float SPEED_SCALE_MAX      = 1.6f;             // Hız çarpanı üst sınırı

// Şifacı (HEALER): periyodik olarak menzil içi CANLI düşmanları iyileştirir
constexpr float    HEAL_RANGE_TILES = 2.5f;              // İyileştirme yarıçapı (tile)
constexpr uint32_t HEAL_INTERVAL_MS = 1200;             // İyileştirme periyodu
constexpr int      HEAL_AMOUNT      = 4;                 // Her tikte iyileşme miktarı

// ------------------------------------------------------------
//  BİYOM OYNANIŞ MODİFİYELERİ (indeks = biome 0..2)
// ------------------------------------------------------------
// HP çarpanı: biyom sınırında duvar (orman taban, donmuş etli, cehennem kalın)
constexpr float BIOME_HP_MULT[BIOME_COUNT]    = { 1.0f, 1.6f, 2.5f };
// Taban hız çarpanı: donmuş hafif yavaş, cehennem hızlı
constexpr float BIOME_SPEED_MULT[BIOME_COUNT] = { 1.0f, 0.95f, 1.2f };
// Frost gücü: uygulanan yavaşlatmanın oranı. Donmuşta düşük → buz kulesi zayıf.
constexpr float BIOME_FROST_POWER[BIOME_COUNT]= { 1.0f, 0.35f, 1.0f };
// Zehir gücü: zehir süresinin oranı. Cehennemde düşük → ateşli düşman zehre dirençli.
constexpr float BIOME_POISON_POWER[BIOME_COUNT]={ 1.0f, 1.0f, 0.4f };
// Kaleye ekstra hasar: cehennem düşmanları daha yıkıcı
constexpr int   BIOME_BASE_DMG_ADD[BIOME_COUNT] = { 0, 0, 1 };

// ------------------------------------------------------------
//  DALGA SİSTEMİ (9 bölüm x 5 dalga = 45)
// ------------------------------------------------------------
constexpr int WAVES_PER_LEVEL   = 5;                     // Bölüm başına dalga
constexpr int TOTAL_WAVES       = LEVEL_COUNT * WAVES_PER_LEVEL;   // 45
constexpr int MAX_WAVE_GROUPS   = 8;                     // Dalga başına spawn grubu sayısı
constexpr int WAVE_BASE_COUNT   = 4;                     // Dalga düşman sayısı tabanı
constexpr int BOSS_EVERY_WAVE   = WAVES_PER_LEVEL;       // Her bölümün son dalgası boss finali
constexpr uint16_t SPAWN_GAP_BASE_MS = 800;              // Dalga 1 spawn aralığı
constexpr uint16_t SPAWN_GAP_STEP_MS = 20;               // Dalga başına aralık kısalması
constexpr uint16_t SPAWN_GAP_MIN_MS  = 340;              // Aralık alt sınırı (v4.0: 280→340, sprite yığılma azalt)
constexpr uint16_t SPAWN_GAP_SOLDIER_ADD = 200;          // Soldier grubunda ek aralık
constexpr uint16_t SPAWN_GAP_TANK_ADD    = 400;          // Tank grubunda ek aralık
constexpr uint16_t SPAWN_GAP_FLYER_SUB   = 150;          // Uçan grubunda aralık kısalması
constexpr uint16_t SPAWN_GAP_SWARM_MS    = 160;          // Sürü böceği: sık spawn (kasıtlı kalabalık)
constexpr uint16_t SPAWN_GAP_BOSS_MS     = 1200;         // Boss spawn aralığı

constexpr float FIRST_WAVE_PREP_S = 3.0f;                // İlk dalga öncesi geri sayım (sn)
constexpr float WAVE_PREP_S       = 8.0f;                // Dalgalar arası hazırlık (sn)
constexpr int   EARLY_BONUS_PER_S = 5;                   // Erken başlatma bonusu (v4.0: 10→5, para seli kıs)
constexpr int   WAVE_BONUS_BASE   = 4;                   // Dalga sonu bonus tabanı (v4.0: 8→4)
constexpr int   WAVE_BONUS_PER_N  = 1;                   // Dalga numarası çarpanı
constexpr int   LEVEL_BONUS       = 25;                  // Bölüm geçiş bonusu (v4.0: 50→25)
constexpr int   RUSH_BONUS        = 20;                  // Dalga sırasında D ile sonraki dalgayı erken çağırma ödülü (v4.1)

// ------------------------------------------------------------
//  EKONOMİ VE KALE
// ------------------------------------------------------------
constexpr int START_MONEY = 45;                          // Başlangıç parası (v4.0: 60→45)
constexpr int START_MONEY_PER_LEVEL = 60;                // Ortadan başlarken bölüm başına ek başlangıç parası (v4.1: 30→60, mid-start çok zordu)
constexpr int BASE_HP_MAX = 20;                          // Kale canı
constexpr int SELL_REFUND_PCT = 60;                      // Kule satışında geri ödeme yüzdesi

// ------------------------------------------------------------
//  EFEKTLER (Dungeon/Combat.h pattern'leri)
// ------------------------------------------------------------
constexpr int   MAX_PARTICLES    = 30;                   // Parçacık havuzu boyutu
constexpr float PARTICLE_GRAVITY = 35.0f;                // Parçacık yerçekimi (px/sn²)
constexpr float PART_SPEED_MIN   = 20.0f;                // Parçacık min hız (px/sn)
constexpr float PART_SPEED_MAX   = 70.0f;                // Parçacık max hız (px/sn)
constexpr float PART_LIFE_MIN    = 0.30f;                // Parçacık min ömür (sn) — v3.3 daha kısa
constexpr float PART_LIFE_MAX    = 0.55f;                // Parçacık max ömür (sn) — hızlı temizlenir
constexpr float PART_BIG_LIFE    = 0.30f;                // Bu ömrün üstünde 2x2 çizilir

constexpr int   MAX_POPUPS   = 8;                        // Hasar yazısı havuzu
constexpr float POPUP_LIFE_S = 0.30f;                    // Hasar yazısı ömrü
constexpr float POPUP_RISE   = 20.0f;                    // Hasar yazısı yükselme hızı (px/sn)

constexpr int   MAX_RAYS   = 16;                         // Atış çizgisi havuzu (Arrow/Lazer/Yıldırım)
constexpr float RAY_LIFE_S = 0.05f;                      // Çizgi ömrü (~2-3 kare)

constexpr int   MAX_SHOTS   = 12;                        // Mermi havuzu (Cannon/Frost/Zehir)
constexpr float SHOT_DUR_S  = 0.15f;                     // Mermi uçuş süresi (sn)

constexpr int   MAX_RINGS   = 8;                         // Genişleyen halka havuzu
constexpr float RING_LIFE_S = 0.35f;                     // Halka ömrü (sn)
constexpr int   RING_R_SPLASH = SPLASH_RADIUS_PX;        // Patlama halka yarıçapı
constexpr int   RING_R_BUILD  = 10;                      // İnşa/yükseltme halka yarıçapı
constexpr int   RING_R_BOLT   = 7;                       // Yıldırım halka yarıçapı

constexpr float SHAKE_CANNON    = 1.0f;                  // Cannon atışı sarsıntısı
constexpr float SHAKE_BASE_HIT  = 2.5f;                  // Kale hasarı sarsıntısı
constexpr float SHAKE_GAMEOVER  = 4.0f;                  // Kale yıkımı sarsıntısı
constexpr float SHAKE_LIGHTNING = 2.0f;                  // Yıldırım sarsıntısı
constexpr float SHAKE_DECAY  = 0.85f;                    // Üstel sönümlenme çarpanı
constexpr float SHAKE_MIN    = 0.2f;                     // Bu değerin altında sarsıntı biter

// Parçacık adetleri
constexpr int PART_N_HIT     = 2;                        // Vuruş parçacığı (v3.3: gürültü azalt)
constexpr int PART_N_KILL    = 5;                        // Düşman ölüm patlaması
constexpr int PART_N_SPLASH  = 6;                        // Cannon patlaması
constexpr int PART_N_POISON  = 3;                        // Zehir bulutu
constexpr int PART_N_BUILD   = 8;                        // Kule yerleştirme
constexpr int PART_N_UPGRADE = 8;                        // Kule yükseltme
constexpr int PART_N_BASEHIT = 5;                        // Kale hasarı

// ------------------------------------------------------------
//  SÜRELER (durum makinesi)
// ------------------------------------------------------------
constexpr uint32_t GAMEOVER_GUARD_MS = 600;              // GameOver girdi koruması
constexpr uint32_t WAVE_CLEAR_MS     = 2000;             // Dalga temiz ekranı süresi
constexpr uint32_t LEVEL_CLEAR_MS    = 2500;             // Bölüm geçiş ekranı süresi
constexpr uint32_t STANDALONE_MSG_MS = 800;              // "OS yok" bilgi ekranı süresi

// ------------------------------------------------------------
//  SES SÜRELERİ (ms) — E-OS Ses Paleti v2.0 kurallarına uygun
// ------------------------------------------------------------
constexpr uint32_t SND_START_MS    = 50;                 // Menü başlat (659 Hz)
constexpr uint32_t SND_RESTART_MS  = 50;                 // Restart (587 Hz)
constexpr uint32_t SND_RESUME_MS   = 40;                 // Pause devam (587 Hz)
constexpr uint32_t SND_PAUSE_MS    = 50;                 // Pause açma (392 Hz)
constexpr uint32_t SND_BUILD_MS    = 30;                 // Kule yerleştir (523 Hz)
constexpr uint32_t SND_UPGRADE_MS  = 30;                 // Kule yükselt (659 Hz)
constexpr uint32_t SND_KILL_MS     = 40;                 // Düşman öldürme (220 Hz)
constexpr uint32_t SND_BASEHIT_MS  = 80;                 // Kale hasarı (196 Hz)
constexpr uint32_t SND_WAVE_MS     = 40;                 // Dalga başlangıcı (392 Hz)
constexpr uint32_t SND_ERROR_MS    = 40;                 // Yanlış işlem (349 Hz)
constexpr uint32_t SND_NAV_MS      = 25;                 // Menü gezinme (349 Hz)
constexpr uint32_t SND_SHOT_MS     = 20;                 // Kule atışı (440 Hz, çok kısa)
constexpr uint32_t SND_LASER_MS    = 15;                 // Lazer atışı (440 Hz, en kısa)
constexpr uint32_t SND_POISON_MS   = 25;                 // Zehir atışı (330 Hz)
constexpr uint32_t SND_BOLT_MS     = 100;                // Yıldırım (247 Hz, ağır)
constexpr uint32_t SND_DIE1_MS     = 120;                // Kale yıkım 1. nota (165 Hz)
constexpr uint32_t SND_DIE_GAP_MS  = 130;                // Yıkım notaları arası bekleme
constexpr uint32_t SND_DIE2_MS     = 100;                // Kale yıkım 2. nota (196 Hz)
constexpr uint32_t FANFARE_NOTE_MS = 50;                 // Zafer fanfar nota süresi
constexpr uint32_t FANFARE_TOP_MS  = 40;                 // Fanfar tavan notası (784 Hz)
constexpr uint32_t FANFARE_GAP_MS  = 60;                 // Fanfar notaları arası

// ------------------------------------------------------------
//  RENK PALETİ (RGB565)
// ------------------------------------------------------------
// Zemin — yumuşak orman paleti (göz yormayan, dama kontrastı düşük)
constexpr uint16_t COL_GRASS      = 0x22A4;              // Koyu orman yeşili
constexpr uint16_t COL_GRASS_ALT  = 0x2B05;              // Bir ton açığı (dama deseni)
constexpr uint16_t COL_GRASS_DEC  = 0x19E3;              // Ot kümesi (koyu detay)
constexpr uint16_t COL_GRASS_LT   = 0x3D06;              // Ot ucu vurgusu (hafif açık)
constexpr uint16_t COL_FLOWER     = 0xCE59;              // Çiçek (kırık krem, soluk)
constexpr uint16_t COL_PATH       = 0x8B48;              // Toprak yol
constexpr uint16_t COL_PATH_ALT   = 0x7AC7;              // Koyu toprak (dama)
constexpr uint16_t COL_PATH_EDGE  = 0x51C4;              // Yol kenarı (koyu kahve şerit)
constexpr uint16_t COL_PATH_DOT   = 0x9BEA;              // Çakıl taşı (açık benek)
constexpr uint16_t COL_ROCK       = 0x632C;              // Gri kaya
constexpr uint16_t COL_ROCK_HL    = 0x8C71;              // Kaya ışık vurgusu
constexpr uint16_t COL_ROCK_SH    = 0x39E7;              // Kaya gölgesi
constexpr uint16_t COL_BASE       = 0xF800;              // Kale vurgu/bayrak kırmızısı

// Kale ve spawn mağarası
constexpr uint16_t COL_CASTLE     = 0x8410;              // Taş duvar
constexpr uint16_t COL_CASTLE_DK  = 0x4208;              // Koyu taş (gölge)
constexpr uint16_t COL_CASTLE_LT  = 0xC618;              // Açık taş (vurgu)
constexpr uint16_t COL_CAVE       = 0x2945;              // Mağara kayası
constexpr uint16_t COL_CAVE_IN    = 0x0841;              // Mağara içi (zifiri)

// Kuleler
constexpr uint16_t COL_TOWER_ARROW  = 0x07FF;            // Cyan
constexpr uint16_t COL_TOWER_CANNON = 0xF800;            // Kırmızı
constexpr uint16_t COL_TOWER_FROST  = 0x001F;            // Mavi
constexpr uint16_t COL_TOWER_LASER  = 0xFD20;            // Turuncu
constexpr uint16_t COL_TOWER_POISON = 0x7FE7;            // Asit yeşili
constexpr uint16_t COL_TOWER_BASE   = 0x4A49;            // Kule kaide (gri)
constexpr uint16_t COL_TOWER_RANGE  = 0x8410;            // Menzil halkası (soluk)

// Düşmanlar
constexpr uint16_t COL_ENEMY_RUNNER  = 0xFFE0;           // Sarı
constexpr uint16_t COL_ENEMY_SOLDIER = 0x91B9;           // Gerçek mor/menekşe (v3.3: eski 0xF81F magenta idi, paletle kavga ediyordu)
constexpr uint16_t COL_ENEMY_TANK    = 0x03E0;           // Koyu yeşil
constexpr uint16_t COL_ENEMY_FLYER   = 0xAEFF;           // Açık gök mavisi
constexpr uint16_t COL_ENEMY_BOSS    = 0xD8A7;           // Kızıl (orman boss)
constexpr uint16_t COL_ENEMY_BOSS_FR = 0x5C9F;           // Buz mavisi (donmuş boss)
constexpr uint16_t COL_ENEMY_BOSS_HL = 0xFB00;           // Akkor turuncu (cehennem boss)
constexpr uint16_t COL_ENEMY_ARMORED = 0x9CD3;           // Çelik gri-mavi (zırhlı)
constexpr uint16_t COL_ENEMY_HEALER  = 0xFFDF;           // Yumuşak beyaz (şifacı, kızıl haç)
constexpr uint16_t COL_ENEMY_SWARM   = 0xFB60;           // Kehribar (sürü böceği)

// ------------------------------------------------------------
//  BİYOM ZEMİN PALETİ (indeks = biome 0..2) — orman / donmuş / cehennem
//  Kule/HUD renkleri değişmez; yalnız zemin/yol/kaya tonlanır.
// ------------------------------------------------------------
constexpr uint16_t BIO_GRASS[BIOME_COUNT]     = { COL_GRASS,     0xBDFF, 0x39C7 };
constexpr uint16_t BIO_GRASS_ALT[BIOME_COUNT] = { COL_GRASS_ALT, 0xAD5F, 0x4208 };
constexpr uint16_t BIO_GRASS_DEC[BIOME_COUNT] = { COL_GRASS_DEC, 0x6BBF, 0x6180 };
constexpr uint16_t BIO_GRASS_LT[BIOME_COUNT]  = { COL_GRASS_LT,  0xEF7F, 0x630C };
constexpr uint16_t BIO_FLOWER[BIOME_COUNT]    = { COL_FLOWER,    0xFFFF, 0xFB00 };
constexpr uint16_t BIO_PATH[BIOME_COUNT]      = { COL_PATH,      0x9E9F, 0x9A00 };
constexpr uint16_t BIO_PATH_ALT[BIOME_COUNT]  = { COL_PATH_ALT,  0x8DFF, 0x6000 };
constexpr uint16_t BIO_PATH_EDGE[BIOME_COUNT] = { COL_PATH_EDGE, 0x52BF, 0x4800 };
constexpr uint16_t BIO_PATH_DOT[BIOME_COUNT]  = { COL_PATH_DOT,  0xFFFF, 0xFD20 };
constexpr uint16_t BIO_ROCK[BIOME_COUNT]      = { COL_ROCK,      0x9E7F, 0x2124 };
constexpr uint16_t BIO_ROCK_HL[BIOME_COUNT]   = { COL_ROCK_HL,   0xF7FF, 0x630C };
constexpr uint16_t BIO_ROCK_SH[BIOME_COUNT]   = { COL_ROCK_SH,   0x52BF, 0x1082 };

// HUD
constexpr uint16_t COL_HUD_BG     = 0x0000;              // Siyah
constexpr uint16_t COL_HUD_LINE   = 0x2104;              // Ayırıcı
constexpr uint16_t COL_HUD_TEXT   = 0xBDF7;              // Açık gri
constexpr uint16_t COL_HP_FULL    = 0x07E0;              // Yeşil
constexpr uint16_t COL_HP_MID     = 0xFFE0;              // Sarı
constexpr uint16_t COL_HP_LOW     = 0xF800;              // Kırmızı
constexpr uint16_t COL_MONEY      = 0xFFE0;              // Sarı (para)
constexpr uint16_t COL_WAVE       = 0x07FF;              // Cyan (dalga)

// İmleç + menü
constexpr uint16_t COL_CURSOR     = 0xFFFF;              // Beyaz (yanıp söner)
constexpr uint16_t COL_CURSOR_BAD = 0xF800;              // Kırmızı (yerleştirilemez)
constexpr uint16_t COL_PANEL_BG   = 0x1082;              // Panel arka planı
constexpr uint16_t COL_PANEL_BRD  = 0x07FF;              // Panel çerçevesi
constexpr uint16_t COL_SEL_BOX    = 0x18E3;              // Menü seçim kutusu

// Sprite kontur + gölge (büyütülmüş sprite'lar için)
constexpr uint16_t COL_OUTLINE   = 0x0000;               // Sprite dış konturu (siyah — zeminde belirginlik)
constexpr uint16_t COL_SHADOW    = 0x18E3;               // Zemin gölgesi (koyu, hafif saydam hissi)

// Efekt renkleri
constexpr uint16_t COL_BG        = 0x0000;               // Genel arka plan (siyah)
constexpr uint16_t COL_RAY       = 0xFFFF;               // Arrow atış çizgisi (beyaz)
constexpr uint16_t COL_SHOT_CAN  = 0xFC00;               // Cannon mermisi (turuncu)
constexpr uint16_t COL_SHOT_FRO  = 0x867F;               // Frost mermisi (açık mavi)
constexpr uint16_t COL_SLOW_MARK = 0x867F;               // Yavaşlatılmış düşman işareti
constexpr uint16_t COL_LIGHTNING = 0xFFE6;               // Yıldırım (sarı-beyaz)
constexpr uint16_t COL_EMPTY_TXT = 0x630C;               // Soluk gri metin

// HP barı eşikleri
constexpr int HP_MID_PCT = 50;                           // Bu yüzdenin altında sarı
constexpr int HP_LOW_PCT = 25;                           // Bu yüzdenin altında kırmızı

// ------------------------------------------------------------
//  HUD YERLEŞİMİ
// ------------------------------------------------------------
constexpr int HUD_TEXT_Y   = 3;                          // HUD metin satırı Y
constexpr int HUD_HEART_X  = 2;                          // Kalp ikonu X
constexpr int HUD_BAR_X    = 12;                         // Kale can barı X
constexpr int HUD_BAR_Y    = 4;                          // Kale can barı Y
constexpr int HUD_BAR_W    = 40;                         // Kale can barı genişliği
constexpr int HUD_BAR_H    = 6;                          // Kale can barı yüksekliği
constexpr int HUD_MONEY_X  = 58;                         // Para göstergesi X
constexpr int HUD_WAVE_X   = 96;                         // Dalga sayacı X ("B2 13/30")
constexpr int HUD_BOLT_X   = 150;                        // Yıldırım hazır ikonu X
constexpr int HUD_FPS_X    = 140;                        // FPS göstergesi X

// Kule seçim paneli (5 kule + iptal = 6 satır)
constexpr int TM_PANEL_W = 96;                           // Panel genişliği
constexpr int TM_PANEL_H = 96;                           // Panel yüksekliği
constexpr int TM_PANEL_X = (SCR_W - TM_PANEL_W) / 2;     // Panel X (ortalı)
constexpr int TM_PANEL_Y = 18;                           // Panel Y
constexpr int TM_OPTIONS = 6;                            // 5 kule türü + iptal
constexpr int TI_OPTIONS = 3;                            // Kule bilgi paneli: Yukselt / Sat / Kapat

// ------------------------------------------------------------
//  DURUM MAKİNESİ
// ------------------------------------------------------------
enum GameState {
    MENU, PLAYING, PAUSE, GAMEOVER, WAVE_CLEAR, TOWER_MENU, VICTORY, LEVEL_CLEAR,
    TOWER_INFO, LEVEL_SELECT
};

// ------------------------------------------------------------
//  YARDIMCILAR VE İLERİ BİLDİRİMLER
// ------------------------------------------------------------
// RGB565 rengi yarıya karartır (sprite gölgelendirme için)
inline uint16_t dimColor(uint16_t c) { return (c >> 1) & 0x7BEF; }

// RGB565 rengi beyaza doğru açar (sprite üst yüz vurgusu için)
inline uint16_t litColor(uint16_t c) {
    uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    r = (r + 8 > 31) ? 31 : r + 8;
    g = (g + 16 > 63) ? 63 : g + 16;
    b = (b + 8 > 31) ? 31 : b + 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// towerdefense.ino içinde tanımlanır (GameBase osPlaySound sarmalayıcısı)
void playSound(uint16_t freq, uint32_t dur);
