#pragma once
// ============================================================
//  Songs.h — Rhythm Beats sarki verileri
//  3 sarki: nota chart'lari (SongNote) + buzzer melodileri (MelNote).
//  Tum diziler sabit (malloc YOK). Zamanlar sarki basindan ms.
// ============================================================

#include "Config.h"

// Chart notasi: ne zaman, hangi serit
struct SongNote {
    uint16_t timeMs;   // sarki basindan ms — hit line'a ulasma ani
    uint8_t  lane;     // 0-3 (D/A/C/B seridi)
};

// Melodi notasi: dongulu arka plan melodisi icin
struct MelNote {
    uint16_t timeMs;   // dongu basindan ms
    uint16_t freq;     // Hz (165-784 bandinda)
    uint8_t  dur;      // ms (30-80)
};

// Sarki tanimi — menu + oynatici tum bilgiyi buradan alir
struct SongInfo {
    const char     *name;       // menu adi
    const SongNote *notes;      // chart dizisi
    uint16_t        noteCount;
    const MelNote  *melody;     // dongulu melodi frazi
    uint8_t         melCount;
    uint16_t        melLoopMs;  // melodi dongu suresi
    uint16_t        beatMs;     // beat suresi (hit line nabiz efekti)
    uint8_t         stars;      // zorluk 1-3
    float           speed;      // nota dusme hizi (px/sn)
};

// ============================================================
//  SARKI 1 — "First Steps" (BPM 80, beat 750 ms, 64 nota, ~49 sn)
//  Tek notalar, ogrenme telasi yok. 8'lik bloklar halinde desenler.
// ============================================================
constexpr SongNote SONG1_NOTES[] = {
    // Blok A — yukselen merdiven (x2)
    { 2000, 0}, { 2750, 1}, { 3500, 2}, { 4250, 3},
    { 5000, 0}, { 5750, 1}, { 6500, 2}, { 7250, 3},
    // Blok B — inen merdiven (x2)
    { 8000, 3}, { 8750, 2}, { 9500, 1}, {10250, 0},
    {11000, 3}, {11750, 2}, {12500, 1}, {13250, 0},
    // Blok C — zigzag
    {14000, 0}, {14750, 2}, {15500, 1}, {16250, 3},
    {17000, 0}, {17750, 2}, {18500, 1}, {19250, 3},
    // Blok D — cift tekrarlar
    {20000, 1}, {20750, 1}, {21500, 2}, {22250, 2},
    {23000, 3}, {23750, 3}, {24500, 0}, {25250, 0},
    // Blok E — capraz gecisler
    {26000, 0}, {26750, 3}, {27500, 1}, {28250, 2},
    {29000, 2}, {29750, 1}, {30500, 3}, {31250, 0},
    // Blok F — sol capa (0 seridi merkezli)
    {32000, 0}, {32750, 1}, {33500, 0}, {34250, 2},
    {35000, 0}, {35750, 3}, {36500, 0}, {37250, 1},
    // Blok G — sag-orta capa (2 seridi merkezli)
    {38000, 2}, {38750, 3}, {39500, 2}, {40250, 1},
    {41000, 2}, {41750, 0}, {42500, 2}, {43250, 3},
    // Blok H — final: cikis + inis
    {44000, 0}, {44750, 1}, {45500, 2}, {46250, 3},
    {47000, 3}, {47750, 2}, {48500, 1}, {49250, 0},
};

// Melodi 1 — sakin Do majör arpej (6 sn dongu)
constexpr MelNote SONG1_MELODY[] = {
    {   0, NOTE_C4, 60}, { 750, NOTE_E4, 60}, {1500, NOTE_G4, 60}, {2250, NOTE_C5, 50},
    {3000, NOTE_G4, 60}, {3750, NOTE_E4, 60}, {4500, NOTE_C4, 60}, {5250, NOTE_G3, 80},
};

// ============================================================
//  SARKI 2 — "Neon Pulse" (BPM 120, beat 500 ms, 80 nota, ~34 sn)
//  Her olcu: 2 dortluk + sekizlik cifti + dortluk (5 nota/olcu).
// ============================================================
constexpr SongNote SONG2_NOTES[] = {
    // Olcu 1-4
    { 2000, 0}, { 2500, 1}, { 3000, 2}, { 3250, 2}, { 3500, 3},
    { 4000, 3}, { 4500, 2}, { 5000, 1}, { 5250, 1}, { 5500, 0},
    { 6000, 0}, { 6500, 2}, { 7000, 1}, { 7250, 3}, { 7500, 2},
    { 8000, 1}, { 8500, 3}, { 9000, 0}, { 9250, 0}, { 9500, 2},
    // Olcu 5-8
    {10000, 2}, {10500, 0}, {11000, 3}, {11250, 3}, {11500, 1},
    {12000, 3}, {12500, 1}, {13000, 2}, {13250, 0}, {13500, 0},
    {14000, 0}, {14500, 0}, {15000, 1}, {15250, 2}, {15500, 3},
    {16000, 2}, {16500, 3}, {17000, 1}, {17250, 1}, {17500, 0},
    // Olcu 9-12
    {18000, 0}, {18500, 1}, {19000, 3}, {19250, 3}, {19500, 2},
    {20000, 1}, {20500, 2}, {21000, 0}, {21250, 2}, {21500, 3},
    {22000, 3}, {22500, 0}, {23000, 2}, {23250, 2}, {23500, 1},
    {24000, 2}, {24500, 1}, {25000, 3}, {25250, 0}, {25500, 0},
    // Olcu 13-16
    {26000, 0}, {26500, 3}, {27000, 1}, {27250, 1}, {27500, 2},
    {28000, 1}, {28500, 0}, {29000, 2}, {29250, 3}, {29500, 3},
    {30000, 3}, {30500, 2}, {31000, 0}, {31250, 0}, {31500, 1},
    {32000, 0}, {32500, 1}, {33000, 2}, {33250, 3}, {33500, 3},
};

// Melodi 2 — enerjik nabiz (4 sn dongu)
constexpr MelNote SONG2_MELODY[] = {
    {   0, NOTE_E4, 50}, { 500, NOTE_G4, 50}, {1000, NOTE_A4, 40}, {1500, NOTE_C5, 40},
    {2000, NOTE_A4, 40}, {2500, NOTE_G4, 50}, {3000, NOTE_E4, 50}, {3500, NOTE_C5, 40},
};

// ============================================================
//  SARKI 3 — "Chaos Storm" (BPM 160, beat 375 ms, 120 nota, ~32 sn)
//  Her fraz (3 sn): dortlukler + sekizlik kosulari, sonda 4'lu run.
// ============================================================
constexpr SongNote SONG3_NOTES[] = {
    // Fraz 1 (2000+)
    { 2000, 0}, { 2375, 1}, { 2563, 2}, { 2750, 3}, { 3125, 0}, { 3500, 2},
    { 3688, 1}, { 3875, 3}, { 4250, 0}, { 4438, 1}, { 4625, 2}, { 4813, 3},
    // Fraz 2 (5000+)
    { 5000, 3}, { 5375, 2}, { 5563, 1}, { 5750, 0}, { 6125, 2}, { 6500, 1},
    { 6688, 2}, { 6875, 0}, { 7250, 3}, { 7438, 2}, { 7625, 1}, { 7813, 0},
    // Fraz 3 (8000+)
    { 8000, 1}, { 8375, 0}, { 8563, 2}, { 8750, 3}, { 9125, 1}, { 9500, 3},
    { 9688, 2}, { 9875, 0}, {10250, 0}, {10438, 1}, {10625, 2}, {10813, 3},
    // Fraz 4 (11000+)
    {11000, 2}, {11375, 3}, {11563, 1}, {11750, 0}, {12125, 3}, {12500, 0},
    {12688, 1}, {12875, 2}, {13250, 3}, {13438, 2}, {13625, 1}, {13813, 0},
    // Fraz 5 (14000+)
    {14000, 0}, {14375, 2}, {14563, 3}, {14750, 1}, {15125, 0}, {15500, 1},
    {15688, 3}, {15875, 2}, {16250, 0}, {16438, 2}, {16625, 1}, {16813, 3},
    // Fraz 6 (17000+)
    {17000, 3}, {17375, 1}, {17563, 0}, {17750, 2}, {18125, 3}, {18500, 2},
    {18688, 0}, {18875, 1}, {19250, 3}, {19438, 1}, {19625, 2}, {19813, 0},
    // Fraz 7 (20000+)
    {20000, 1}, {20375, 2}, {20563, 0}, {20750, 3}, {21125, 2}, {21500, 3},
    {21688, 1}, {21875, 0}, {22250, 0}, {22438, 1}, {22625, 2}, {22813, 3},
    // Fraz 8 (23000+)
    {23000, 2}, {23375, 0}, {23563, 3}, {23750, 1}, {24125, 1}, {24500, 0},
    {24688, 2}, {24875, 3}, {25250, 3}, {25438, 2}, {25625, 1}, {25813, 0},
    // Fraz 9 (26000+)
    {26000, 0}, {26375, 3}, {26563, 2}, {26750, 1}, {27125, 2}, {27500, 1},
    {27688, 0}, {27875, 3}, {28250, 0}, {28438, 1}, {28625, 2}, {28813, 3},
    // Fraz 10 (29000+) — final inisi
    {29000, 3}, {29375, 0}, {29563, 1}, {29750, 2}, {30125, 0}, {30500, 3},
    {30688, 2}, {30875, 1}, {31250, 3}, {31438, 2}, {31625, 1}, {31813, 0},
};

// Melodi 3 — surukleyici ritim motoru (3 sn dongu)
constexpr MelNote SONG3_MELODY[] = {
    {   0, NOTE_E4, 40}, { 375, NOTE_E4, 40}, { 750, NOTE_G4, 40}, {1125, NOTE_A4, 40},
    {1500, NOTE_E4, 40}, {1875, NOTE_G4, 40}, {2250, NOTE_C5, 30}, {2625, NOTE_A4, 40},
};

// ============================================================
//  Sarki listesi
// ============================================================
constexpr int SONG_COUNT = 3;

constexpr SongInfo SONGS[SONG_COUNT] = {
    { "First Steps", SONG1_NOTES, (uint16_t)(sizeof(SONG1_NOTES) / sizeof(SongNote)),
      SONG1_MELODY, (uint8_t)(sizeof(SONG1_MELODY) / sizeof(MelNote)), 6000, 750, 1,  60.0f },
    { "Neon Pulse",  SONG2_NOTES, (uint16_t)(sizeof(SONG2_NOTES) / sizeof(SongNote)),
      SONG2_MELODY, (uint8_t)(sizeof(SONG2_MELODY) / sizeof(MelNote)), 4000, 500, 2,  80.0f },
    { "Chaos Storm", SONG3_NOTES, (uint16_t)(sizeof(SONG3_NOTES) / sizeof(SongNote)),
      SONG3_MELODY, (uint8_t)(sizeof(SONG3_MELODY) / sizeof(MelNote)), 3000, 375, 3, 120.0f },
};
