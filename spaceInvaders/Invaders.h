#pragma once
// ============================================================
//  E-OS SPACE INVADERS — Invaders.h
//  Uzayli ordusu: izgara yonetimi, hareket, animasyon, ates
// ============================================================
#include "Config.h"

struct InvaderGrid {
    bool aliens[ALIEN_ROWS][ALIEN_COLS];
    int count;
    float gridX, gridY;
    int dir;
    float speed;
    bool animFrame;
    unsigned long lastAnimMs;
    unsigned long lastEFireMs;

    void init(int wave) {
        count = ALIEN_ROWS * ALIEN_COLS;
        for (int r = 0; r < ALIEN_ROWS; r++)
            for (int c = 0; c < ALIEN_COLS; c++)
                aliens[r][c] = true;

        int gridW = ALIEN_COLS * (ALIEN_W + ALIEN_GAP_X) - ALIEN_GAP_X;
        gridX = (float)(SCR_W - gridW) / 2.0f;
        gridY = (float)ALIEN_START_Y;
        dir = 1;
        speed = 7.5f + (float)(wave - 1) * 2.4f;
        if (speed > 30.0f) speed = 30.0f;
        animFrame = false;
        lastAnimMs = millis();
        lastEFireMs = millis();
    }

    float alienX(int col) const { return gridX + (float)col * (float)(ALIEN_W + ALIEN_GAP_X); }
    float alienY(int row) const { return gridY + (float)row * (float)(ALIEN_H + ALIEN_GAP_Y); }

    float alienCenterX(int col) const { return alienX(col) + (float)ALIEN_W / 2.0f; }
    float alienBottomY(int row) const { return alienY(row) + (float)ALIEN_H; }

    void getBounds(int &lc, int &rc, int &tr, int &br) const {
        lc = ALIEN_COLS; rc = -1;
        tr = ALIEN_ROWS; br = -1;
        for (int r = 0; r < ALIEN_ROWS; r++) {
            for (int c = 0; c < ALIEN_COLS; c++) {
                if (aliens[r][c]) {
                    if (c < lc) lc = c;
                    if (c > rc) rc = c;
                    if (r < tr) tr = r;
                    if (r > br) br = r;
                }
            }
        }
    }

    void killAt(int row, int col, int wave) {
        aliens[row][col] = false;
        count--;
        recalcSpeed(wave);
    }

    void recalcSpeed(int wave) {
        float baseSpd = 7.5f + (float)(wave - 1) * 2.4f;
        int total = ALIEN_ROWS * ALIEN_COLS;
        float speedMul = 1.0f + (float)(total - count) / (float)total * 2.5f;
        speed = baseSpd * speedMul;
        if (speed > 90.0f) speed = 90.0f;
    }

    void updateMovement(float dt) {
        gridX += speed * (float)dir * dt;
        int lc, rc, tr, br;
        getBounds(lc, rc, tr, br);
        if (rc < 0) return;

        float rightEdge = alienX(rc) + (float)ALIEN_W;
        float leftEdge  = alienX(lc);
        if (rightEdge >= (float)SCR_W - 1.0f && dir > 0) {
            dir = -1;
            gridY += (float)ALIEN_DROP;
        }
        if (leftEdge <= 1.0f && dir < 0) {
            dir = 1;
            gridY += (float)ALIEN_DROP;
        }
    }

    void updateAnimation(unsigned long now) {
        if (now - lastAnimMs > 500) {
            animFrame = !animFrame;
            lastAnimMs = now;
        }
    }

    unsigned long fireInterval(int wave) const {
        unsigned long interval = 2200;
        if (wave > 1) {
            unsigned long reduction = (unsigned long)(wave - 1) * 250UL;
            if (reduction >= 1400) reduction = 1400;
            interval -= reduction;
        }
        return interval;
    }

    bool canFire(int wave) const {
        return (millis() - lastEFireMs >= fireInterval(wave));
    }

    void markFired() {
        lastEFireMs = millis();
    }

    bool reachedPlayer() const {
        int lc, rc, tr, br;
        getBounds(lc, rc, tr, br);
        if (br < 0) return false;
        return alienY(br) + (float)ALIEN_H >= (float)PLAYER_Y - 2.0f;
    }
};
