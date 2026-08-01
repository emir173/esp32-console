#pragma once

#include <Arduino.h>
#include "Config.h"

// ============================================================
//  Board2048 — 4x4 grid mantigi
//  grid degerleri US (exponent) olarak tutulur: 0=bos, 1=2, 11=2048.
//  slide() hem yeni tahtayi hem de animasyon listesini uretir;
//  .ino animasyon bitince yeni tahtayi uygular.
// ============================================================

// Tek bir karonun kayma animasyonu (from -> to hucre)
struct AnimTile {
    uint8_t v;       // us (kaynak deger)
    int8_t  fr, fc;  // kaynak hucre
    int8_t  tr, tc;  // hedef hucre
    bool    merge;   // true = hedefte birlesiyor
};

struct Board2048 {
    uint8_t grid[BOARD_N][BOARD_N];

    void clear() { memset(grid, 0, sizeof(grid)); }

    int emptyCount() const {
        int n = 0;
        for (int r = 0; r < BOARD_N; r++)
            for (int c = 0; c < BOARD_N; c++)
                if (!grid[r][c]) n++;
        return n;
    }

    // Rastgele bos hucreye yeni karo koy (%90 -> 2, %10 -> 4)
    bool spawn(uint8_t &outR, uint8_t &outC) {
        int n = emptyCount();
        if (n == 0) return false;
        int pick = random(n);
        for (int r = 0; r < BOARD_N; r++) {
            for (int c = 0; c < BOARD_N; c++) {
                if (grid[r][c]) continue;
                if (pick-- == 0) {
                    grid[r][c] = (random(10) == 0) ? 2 : 1;
                    outR = r; outC = c;
                    return true;
                }
            }
        }
        return false;   // ulasilmaz
    }

    uint8_t maxExp() const {
        uint8_t m = 0;
        for (int r = 0; r < BOARD_N; r++)
            for (int c = 0; c < BOARD_N; c++)
                if (grid[r][c] > m) m = grid[r][c];
        return m;
    }

    // Hamle kaldi mi? (bos hucre veya bitisik esit karo)
    bool canMove() const {
        for (int r = 0; r < BOARD_N; r++) {
            for (int c = 0; c < BOARD_N; c++) {
                uint8_t v = grid[r][c];
                if (!v) return true;
                if (c + 1 < BOARD_N && grid[r][c + 1] == v) return true;
                if (r + 1 < BOARD_N && grid[r + 1][c] == v) return true;
            }
        }
        return false;
    }

    // dir yonunde i. serit, k. pozisyon -> (r,c).
    // k=0 = karolarin kaydigi kenar (LEFT'te sol sutun vb.)
    static void cellAt(int dir, int i, int k, int &r, int &c) {
        switch (dir) {
            case DIR_LEFT:  r = i;              c = k;              break;
            case DIR_RIGHT: r = i;              c = BOARD_N - 1 - k; break;
            case DIR_UP:    r = k;              c = i;              break;
            default:        r = BOARD_N - 1 - k; c = i;             break; // DIR_DOWN
        }
    }

    // Kaydir + birlestir. out = yeni tahta, anims = her kaynak karonun
    // hedefi, gain = bu hamlede kazanilan puan (birlesen karo degerleri).
    // Donus: en az bir karo hareket etti / birlesti mi.
    bool slide(int dir, uint8_t out[BOARD_N][BOARD_N],
               AnimTile *anims, uint8_t &animCount, int &gain) {
        memset(out, 0, BOARD_N * BOARD_N);
        animCount = 0;
        gain = 0;
        bool moved = false;

        for (int i = 0; i < BOARD_N; i++) {
            int wp = 0;             // seritte siradaki yazma pozisyonu
            int lastPos = -1;       // son yerlesen karonun pozisyonu
            uint8_t lastVal = 0;    // son yerlesen karonun degeri
            bool lastMerged = false;

            for (int k = 0; k < BOARD_N; k++) {
                int r, c;
                cellAt(dir, i, k, r, c);
                uint8_t v = grid[r][c];
                if (!v) continue;

                if (lastPos >= 0 && lastVal == v && !lastMerged) {
                    // Bir onceki karoyla birles
                    int tr, tc;
                    cellAt(dir, i, lastPos, tr, tc);
                    out[tr][tc] = v + 1;
                    gain += (1 << (v + 1));
                    anims[animCount++] = { v, (int8_t)r, (int8_t)c,
                                           (int8_t)tr, (int8_t)tc, true };
                    lastVal = 0;        // birlesmis karo tekrar birlesemez
                    lastMerged = true;
                    moved = true;
                } else {
                    int tr, tc;
                    cellAt(dir, i, wp, tr, tc);
                    out[tr][tc] = v;
                    anims[animCount++] = { v, (int8_t)r, (int8_t)c,
                                           (int8_t)tr, (int8_t)tc, false };
                    if (tr != r || tc != c) moved = true;
                    lastPos = wp;
                    lastVal = v;
                    lastMerged = false;
                    wp++;
                }
            }
        }
        return moved;
    }
};
