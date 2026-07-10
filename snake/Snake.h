#pragma once

#include "Config.h"

struct Snake {
    int x[MAX_SNAKE], y[MAX_SNAKE];
    int len, dir;
    int dirQueue[2];
    int qCount;

    int oldTailX, oldTailY;
    int oldHeadX, oldHeadY;
    bool grew;
    bool shrank;
    int extraEraseX, extraEraseY;
    bool extraErase;

    // Trail (kafa izi — ring buffer, combo'ya gore 4/8)
    int trailX[8], trailY[8];
    int trailHead;
    int trailLen;

    void init(int level) {
        len = INIT_LEN;
        dir = DIR_RIGHT;
        clearQueue();

        int startX = COLS / 2;
        int startY = ROWS / 2;

        // Shift down if obstacle is in the way (level >= 2 centre block)
        if (level >= 2) {
            startY += 3;
            if (startY >= ROWS - INIT_LEN) startY = ROWS - INIT_LEN - 1;
        }

        for (int i = 0; i < len; i++) {
            x[i] = startX - i;
            y[i] = startY;
        }

        oldTailX = x[len - 1];
        oldTailY = y[len - 1];
        oldHeadX = x[0];
        oldHeadY = y[0];
        grew    = false;
        shrank  = false;
        extraErase = false;
        trailHead = 0;
        trailLen = 0;
    }

    void clearQueue() { qCount = 0; }

    void queueDirection(int newDir) {
        if (newDir == -1) return;
        int lastDir = (qCount > 0) ? dirQueue[qCount - 1] : dir;

        if (newDir == DIR_UP && lastDir == DIR_DOWN) return;
        if (newDir == DIR_DOWN && lastDir == DIR_UP) return;
        if (newDir == DIR_LEFT && lastDir == DIR_RIGHT) return;
        if (newDir == DIR_RIGHT && lastDir == DIR_LEFT) return;
        if (newDir == lastDir) return;

        if (qCount < 2) {
            dirQueue[qCount++] = newDir;
        }
    }

    void popDirection() {
        while (qCount > 0) {
            int next = dirQueue[0];
            // Safety net: reject 180-degree turns just before applying
            if ((next == DIR_UP    && dir == DIR_DOWN)  ||
                (next == DIR_DOWN  && dir == DIR_UP)    ||
                (next == DIR_LEFT  && dir == DIR_RIGHT) ||
                (next == DIR_RIGHT && dir == DIR_LEFT)) {
                // Skip this invalid entry, shift queue
                if (qCount > 1) dirQueue[0] = dirQueue[1];
                qCount--;
                continue;  // try next queued direction
            }
            // Valid direction found
            dir = next;
            if (qCount > 1) dirQueue[0] = dirQueue[1];
            qCount--;
            return;
        }
        // Queue empty — keep current direction
    }

    bool isOnCell(int gx, int gy) const {
        for (int i = 0; i < len; i++)
            if (x[i] == gx && y[i] == gy) return true;
        return false;
    }

    uint16_t bodyColor(int i) const {
        return ((x[i] + y[i]) & 1) ? COL_BODY_B : COL_BODY_A;
    }

    // Returns true if movement handled normally (no self-collision)
    // Returns false and sets selfHit=true if snake hits itself
    bool move(int newX, int newY, bool ate, bool atePoison, bool ghostActive) {
        // Save old head/tail for partial redraw
        oldHeadX = x[0];
        oldHeadY = y[0];
        oldTailX = x[len - 1];
        oldTailY = y[len - 1];
        extraErase = false;

        // Trail: kafa pozisyonunu ilerlemeden once kaydet
        trailX[trailHead] = oldHeadX;
        trailY[trailHead] = oldHeadY;
        trailHead = (trailHead + 1) % 8;
        if (trailLen < 8) trailLen++;

        // Self-collision check (ghost aktifken atla)
        if (!ghostActive) {
            int checkLen = (ate || atePoison) ? len : len - 1;
            for (int i = 0; i < checkLen; i++) {
                if (x[i] == newX && y[i] == newY) return false;
            }
        }

        // Grow/shrink
        grew   = false;
        shrank = false;
        if (ate) {
            if (len < MAX_SNAKE) { len++; grew = true; }
        } else if (atePoison) {
            if (len > INIT_LEN) {
                // Save extra cell to erase (the one that disappears)
                extraEraseX = x[len - 2];
                extraEraseY = y[len - 2];
                extraErase = true;
                len--;
                shrank = true;
            }
        }

        // Shift segments back
        for (int i = len - 1; i > 0; i--) {
            x[i] = x[i - 1];
            y[i] = y[i - 1];
        }

        // Place new head
        x[0] = newX;
        y[0] = newY;

        return true;
    }
};
