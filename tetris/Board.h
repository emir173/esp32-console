#pragma once

#include <string.h>
#include "Config.h"
#include "Tetromino.h"

// ============================================================
//  BOARD — 10x20 oyun alani
//
//  cells[y][x]: 0 = bos, 1-7 = kilitlenmis blok (parcaTipi + 1)
//  Grid'in USTUNDEKI satirlar (y < 0) sanal bostur: parca
//  spawn'da kismen gorunmez bolgede olabilir. Kilitleme aninda
//  hala y < 0'da hucre varsa oyun biter (top-out).
// ============================================================
struct Board {
    uint8_t cells[BOARD_ROWS][BOARD_COLS];

    // Tum alani bosalt (yeni oyun)
    void clear() {
        memset(cells, 0, sizeof(cells));
    }

    // Parca (type,rot) konumunda (px,py) carpisir mi?
    // Duvarlar, taban ve kilitli bloklar kontrol edilir.
    // y < 0 (tavan ustu) serbest bolgedir.
    bool collides(uint8_t type, uint8_t rot, int px, int py) const {
        uint16_t m = PIECE_SHAPES[type][rot];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (!(m & (1u << (i * 4 + j)))) continue;
                int cx = px + j;
                int cy = py + i;
                if (cx < 0 || cx >= BOARD_COLS || cy >= BOARD_ROWS) return true;
                if (cy >= 0 && cells[cy][cx]) return true;
            }
        }
        return false;
    }

    // Parcayi bulundugu yere kilitle.
    // false donerse parca tavanin ustunde kaldi -> oyun bitti.
    bool lockPiece(uint8_t type, uint8_t rot, int px, int py) {
        bool ok = true;
        uint16_t m = PIECE_SHAPES[type][rot];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (!(m & (1u << (i * 4 + j)))) continue;
                int cx = px + j;
                int cy = py + i;
                if (cy < 0) { ok = false; continue; }   // tavan ustu -> top-out
                cells[cy][cx] = type + 1;
            }
        }
        return ok;
    }

    // Tamamlanan satirlari bul (en fazla 4 olabilir).
    // rows[] dizisine satir indekslerini (kucukten buyuge) yazar,
    // bulunan satir sayisini dondurur.
    int findFullRows(uint8_t rows[4]) const {
        int n = 0;
        for (int y = 0; y < BOARD_ROWS && n < 4; y++) {
            bool full = true;
            for (int x = 0; x < BOARD_COLS; x++) {
                if (!cells[y][x]) { full = false; break; }
            }
            if (full) rows[n++] = (uint8_t)y;
        }
        return n;
    }

    // Dolu satirlari sil, ustundeki bloklari asagi kaydir (yercekimi).
    // rows[] kucukten buyuge sirali olmali (findFullRows oyle verir):
    // ustteki bir satirin silinmesi alttaki satirlarin indeksini bozmaz.
    void collapseRows(const uint8_t *rows, int n) {
        for (int i = 0; i < n; i++) {
            int ry = rows[i];
            for (int y = ry; y > 0; y--) {
                memcpy(cells[y], cells[y - 1], BOARD_COLS);
            }
            memset(cells[0], 0, BOARD_COLS);
        }
    }
};
