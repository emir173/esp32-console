#pragma once
// ============================================================
//  E-OS SPACE INVADERS — Bunker.h
//  Oyuncu kalkanlari: blok-tabanli hasar, kademeli parcalanma
// ============================================================
#include "Config.h"

struct Bunker {
    bool blocks[BUNKER_ROWS][BUNKER_COLS];
    int posX, posY;

    void init(int x, int y) {
        posX = x;
        posY = y;
        for (int r = 0; r < BUNKER_ROWS; r++)
            for (int c = 0; c < BUNKER_COLS; c++)
                blocks[r][c] = true;
    }

    int blockX(int col) const { return posX + col * BUNKER_BLOCK_W; }
    int blockY(int row) const { return posY + row * BUNKER_BLOCK_H; }
    int width()  const { return BUNKER_COLS * BUNKER_BLOCK_W; }
    int height() const { return BUNKER_ROWS * BUNKER_BLOCK_H; }

    int aliveBlocks() const {
        int n = 0;
        for (int r = 0; r < BUNKER_ROWS; r++)
            for (int c = 0; c < BUNKER_COLS; c++)
                if (blocks[r][c]) n++;
        return n;
    }

    bool hitTest(float px, float py, float halfW, float halfH) {
        int c0 = (int)((px - halfW - (float)posX) / (float)BUNKER_BLOCK_W);
        int c1 = (int)((px + halfW - (float)posX) / (float)BUNKER_BLOCK_W);
        int r0 = (int)((py - halfH - (float)posY) / (float)BUNKER_BLOCK_H);
        int r1 = (int)((py + halfH - (float)posY) / (float)BUNKER_BLOCK_H);

        if (c0 < 0) c0 = 0;
        if (r0 < 0) r0 = 0;
        if (c1 >= BUNKER_COLS) c1 = BUNKER_COLS - 1;
        if (r1 >= BUNKER_ROWS) r1 = BUNKER_ROWS - 1;
        if (c0 > c1 || r0 > r1) return false;

        bool hitAny = false;
        for (int r = r0; r <= r1; r++) {
            for (int c = c0; c <= c1; c++) {
                if (blocks[r][c]) {
                    blocks[r][c] = false;
                    hitAny = true;
                }
            }
        }
        return hitAny;
    }
};

struct BunkerManager {
    Bunker bunkers[BUNKER_COUNT];

    void init() {
        for (int i = 0; i < BUNKER_COUNT; i++)
            bunkers[i].init(BUNKER_X[i], BUNKER_Y);
    }

    bool checkBulletHit(float bx, float by) {
        for (int i = 0; i < BUNKER_COUNT; i++) {
            if (bx - 2.0f >= (float)(bunkers[i].posX + bunkers[i].width()))  continue;
            if (bx + 2.0f <= (float)bunkers[i].posX) continue;
            if (by < (float)bunkers[i].posY) continue;
            if (by > (float)(bunkers[i].posY + bunkers[i].height())) continue;
            if (bunkers[i].hitTest(bx, by, 2.0f, 2.0f)) return true;
        }
        return false;
    }

    int totalBlocks() const {
        int n = 0;
        for (int i = 0; i < BUNKER_COUNT; i++) n += bunkers[i].aliveBlocks();
        return n;
    }
};
