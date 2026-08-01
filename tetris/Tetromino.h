#pragma once

#include <Arduino.h>
#include "Config.h"

// ============================================================
//  TETROMINO — 7 standart parca + SRS dondurme sistemi
//
//  Her parca 4x4 bit maskesi olarak saklanir:
//    bit (satir*4 + sutun), satir 0 = ust, sutun 0 = sol
//  4 rotasyon da onceden hesaplanmis tablodadir (calisma
//  zamaninda matris dondurme maliyeti yok).
//
//  Rotasyonlar SRS (Super Rotation System) standardina uygun:
//    rot 0 = spawn, rot 1 = CW, rot 2 = 180, rot 3 = CCW
// ============================================================

constexpr uint16_t PIECE_SHAPES[7][4] = {
    // I — cyan (4x4 kutunun 2. satiri / sutunlari)
    { 0x00F0, 0x4444, 0x0F00, 0x2222 },
    // O — sari (donme etkisiz, 4 rotasyon ayni)
    { 0x0066, 0x0066, 0x0066, 0x0066 },
    // T — mor
    { 0x0072, 0x0262, 0x0270, 0x0232 },
    // S — yesil
    { 0x0036, 0x0462, 0x0360, 0x0231 },
    // Z — kirmizi
    { 0x0063, 0x0264, 0x0630, 0x0132 },
    // J — mavi
    { 0x0071, 0x0226, 0x0470, 0x0322 },
    // L — turuncu
    { 0x0074, 0x0622, 0x0170, 0x0223 },
};

// Parcanin (i,j) hucresinde blok var mi? (i=satir, j=sutun, 0-3)
inline bool pieceCell(uint8_t type, uint8_t rot, int i, int j) {
    return PIECE_SHAPES[type][rot] & (1u << (i * 4 + j));
}

// ============================================================
//  SRS WALL KICK TABLOLARI
//
//  Standart SRS tablosu +y'yi YUKARI kabul eder; bizim ekran
//  koordinatimizda y ASAGI arttigi icin y offsetleri burada
//  onceden ters cevrilmistir (dogrudan curY'ye eklenebilir).
//
//  Indeksleme: [mevcutRot][0=CW 1=CCW][test 0-4][0=dx 1=dy]
// ============================================================

// J, L, S, T, Z parcalari icin
constexpr int8_t KICK_JLSTZ[4][2][5][2] = {
    {   // rot 0
        { {0,0}, {-1,0}, {-1,-1}, {0, 2}, {-1, 2} },   // 0 -> 1 (CW)
        { {0,0}, { 1,0}, { 1,-1}, {0, 2}, { 1, 2} },   // 0 -> 3 (CCW)
    },
    {   // rot 1
        { {0,0}, { 1,0}, { 1, 1}, {0,-2}, { 1,-2} },   // 1 -> 2 (CW)
        { {0,0}, { 1,0}, { 1, 1}, {0,-2}, { 1,-2} },   // 1 -> 0 (CCW)
    },
    {   // rot 2
        { {0,0}, { 1,0}, { 1,-1}, {0, 2}, { 1, 2} },   // 2 -> 3 (CW)
        { {0,0}, {-1,0}, {-1,-1}, {0, 2}, {-1, 2} },   // 2 -> 1 (CCW)
    },
    {   // rot 3
        { {0,0}, {-1,0}, {-1, 1}, {0,-2}, {-1,-2} },   // 3 -> 0 (CW)
        { {0,0}, {-1,0}, {-1, 1}, {0,-2}, {-1,-2} },   // 3 -> 2 (CCW)
    },
};

// I parcasi icin (farkli kick seti)
constexpr int8_t KICK_I[4][2][5][2] = {
    {   // rot 0
        { {0,0}, {-2,0}, { 1,0}, {-2, 1}, { 1,-2} },   // 0 -> 1 (CW)
        { {0,0}, {-1,0}, { 2,0}, {-1,-2}, { 2, 1} },   // 0 -> 3 (CCW)
    },
    {   // rot 1
        { {0,0}, {-1,0}, { 2,0}, {-1,-2}, { 2, 1} },   // 1 -> 2 (CW)
        { {0,0}, { 2,0}, {-1,0}, { 2,-1}, {-1, 2} },   // 1 -> 0 (CCW)
    },
    {   // rot 2
        { {0,0}, { 2,0}, {-1,0}, { 2,-1}, {-1, 2} },   // 2 -> 3 (CW)
        { {0,0}, { 1,0}, {-2,0}, { 1, 2}, {-2,-1} },   // 2 -> 1 (CCW)
    },
    {   // rot 3
        { {0,0}, { 1,0}, {-2,0}, { 1, 2}, {-2,-1} },   // 3 -> 0 (CW)
        { {0,0}, {-2,0}, { 1,0}, {-2, 1}, { 1,-2} },   // 3 -> 2 (CCW)
    },
};

// ============================================================
//  7-BAG RANDOMIZER
//  7 parcanin tamami bir "torbaya" konur, karistirilir ve
//  sirayla cekilir. Torba bitince yeniden doldurulur.
//  Boylece ayni parca en fazla 2 kez ust uste gelebilir ve
//  her 7 parcada mutlaka birer I,O,T,S,Z,J,L cikar.
// ============================================================
struct PieceBag {
    uint8_t order[7];
    uint8_t idx = 7;   // 7 = torba bos, ilk next() doldurur

    // Torbayi yeniden doldur + Fisher-Yates karistirma
    void refill() {
        for (uint8_t i = 0; i < 7; i++) order[i] = i;
        for (int i = 6; i > 0; i--) {
            int j = random(i + 1);
            uint8_t tmp = order[i];
            order[i] = order[j];
            order[j] = tmp;
        }
        idx = 0;
    }

    // Torbadan siradaki parcayi cek
    uint8_t next() {
        if (idx >= 7) refill();
        return order[idx++];
    }

    // Yeni oyun: torbayi sifirla (ilk cekiste taze karistirma)
    void reset() {
        idx = 7;
    }
};
