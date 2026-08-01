#pragma once
// ============================================================
//  E-OS PLATFORMER — Physics.h
//  Harita sorgulama, fizik, çarpışma, düşman güncelleme ve
//  oyuncu ölümü. Kamera kaydırma mantığı .ino'da uygulanır.
// ============================================================
#include "Player.h"

// ============ Extern Bildirimleri (Physics'in ihtiyacı olan globaller) ============
extern uint8_t mapData[MAP_H][MAP_W];
extern int camX;
extern int curLevel;
extern float gameDT;
extern GameState state;
extern uint32_t stateTimer;
extern int highScore;

// ============ Non-Blocking Bayrak Fanfarı ============
struct FlagFanfare {
    bool active = false;
    uint8_t step = 0;
    uint32_t nextMs = 0;

    void start() {
        active = true;
        step = 0;
        nextMs = millis();
    }

    void update() {
        if (!active) return;
        if (millis() < nextMs) return;
        switch (step) {
            case 0: playSound(NOTE_C5, 50); nextMs = millis() + 60; step++; break;
            case 1: playSound(NOTE_E5, 50); nextMs = millis() + 60; step++; break;
            case 2: playSound(NOTE_G5, 40); active = false; break;
        }
    }
};

extern FlagFanfare fanfare;

// ============ Harita Yardımcıları ============
inline uint8_t getTile(int col, int row) {
    if (col < 0 || col >= MAP_W || row < 0 || row >= MAP_H) return T_GROUND;
    return mapData[row][col];
}

inline bool isSolid(int col, int row) {
    uint8_t t = getTile(col, row);
    return (t == T_GROUND || t == T_BRICK);
}

// ============ Seviye Yükleme ============
inline void loadLevel(int lvl) {
    const uint8_t* src = (const uint8_t*)pgm_read_ptr(&LEVELS[lvl]);
    numEnemies = 0;

    for (int r = 0; r < MAP_H; r++) {
        for (int c = 0; c < MAP_W; c++) {
            uint8_t t = pgm_read_byte(&src[r * MAP_W + c]);
            if (t == T_ENEMY) {
                if (numEnemies < MAX_ENEMIES) {
                    Enemy& e = enemies[numEnemies++];
                    e.x = c * TILE;
                    e.y = r * TILE;
                    e.vx = 0.5f;
                    e.boundL = (c - 3) * TILE;
                    e.boundR = (c + 3) * TILE;
                    e.active = true;
                }
                mapData[r][c] = T_AIR;
            } else {
                mapData[r][c] = t;
            }
        }
    }

    plr.x = 2 * TILE;
    plr.y = 14 * TILE;
    plr.vx = 0; plr.vy = 0;
    plr.grounded = false;
    plr.facingRight = true;
    plr.invincTimer = 60.0f;
    plr.jumpBuf = 0.0f;
    plr.coyoteT = 0.0f;
    camX = 0;
}

inline void resetGame() {
    plr.lives = 3;
    plr.coins = 0;
    plr.score = 0;
    curLevel = 0;
    loadLevel(0);
}

// ============ İleri Bildirim ============
inline void playerDie();

// ============ Oyuncu Fiziği ve Çarpışma ============
inline void updatePhysics() {
    plr.x += plr.vx * gameDT;

    if (plr.vx < 0) {
        int col = (int)plr.x / TILE;
        int rowT = (int)plr.y / TILE;
        int rowB = (int)(plr.y + PH - 1) / TILE;
        if (isSolid(col, rowT) || isSolid(col, rowB)) {
            plr.x = (col + 1) * TILE;
            plr.vx = 0;
        }
    }
    if (plr.vx > 0) {
        int col = (int)(plr.x + PW - 1) / TILE;
        int rowT = (int)plr.y / TILE;
        int rowB = (int)(plr.y + PH - 1) / TILE;
        if (isSolid(col, rowT) || isSolid(col, rowB)) {
            plr.x = col * TILE - PW;
            plr.vx = 0;
        }
    }

    plr.vy += GRAVITY * gameDT;
    if (plr.vy > MAX_FALL) plr.vy = MAX_FALL;
    plr.y += plr.vy * gameDT;

    plr.grounded = false;

    if (plr.vy >= 0) {
        int colL = (int)plr.x / TILE;
        int colR = (int)(plr.x + PW - 1) / TILE;
        int rowBelow = (int)(plr.y + PH) / TILE;
        if (isSolid(colL, rowBelow) || isSolid(colR, rowBelow)) {
            plr.y = rowBelow * TILE - PH;
            plr.vy = 0;
            plr.grounded = true;
            plr.coyoteT = COYOTE_TIME;
        }
    }
    if (plr.vy < 0) {
        int rowT = (int)plr.y / TILE;
        int colL = (int)plr.x / TILE;
        int colR = (int)(plr.x + PW - 1) / TILE;
        if (isSolid(colL, rowT) || isSolid(colR, rowT)) {
            plr.y = (rowT + 1) * TILE;
            plr.vy = 0;
        }
    }

    if (plr.x < 0) { plr.x = 0; plr.vx = 0; }
    if (plr.x > MAP_W * TILE - PW) { plr.x = MAP_W * TILE - PW; plr.vx = 0; }

    if (plr.y > MAP_H * TILE + 4) {
        playerDie();
    }

    if (!plr.grounded && plr.coyoteT > 0.0f) plr.coyoteT -= gameDT;
}

// ============ Altın / Bayrak / Diken Kontrolü ============
inline void checkTilePickups() {
    int c1 = (int)plr.x / TILE;
    int c2 = (int)(plr.x + PW - 1) / TILE;
    int r1 = (int)plr.y / TILE;
    int r2 = (int)(plr.y + PH - 1) / TILE;

    for (int r = r1; r <= r2; r++) {
        for (int c = c1; c <= c2; c++) {
            uint8_t t = getTile(c, r);
            if (t == T_COIN) {
                mapData[r][c] = T_AIR;
                plr.coins++;
                plr.score += 10;
                playSound(NOTE_E5, 30);
            }
            else if (t == T_FLAG) {
                state = ST_LEVELCLEAR;
                stateTimer = millis();
                plr.score += 100;
                fanfare.start();
            }
            else if (t == T_SPIKE && plr.invincTimer <= 0.0f) {
                playerDie();
            }
        }
    }
}

// ============ Düşman Güncelleme ============
inline void updateEnemies() {
    for (int i = 0; i < numEnemies; i++) {
        if (!enemies[i].active) continue;
        Enemy& e = enemies[i];

        e.x += e.vx * gameDT;
        if (e.x <= e.boundL || e.x >= e.boundR) e.vx = -e.vx;

        if (plr.invincTimer > 0.0f) continue;

        bool overlapX = plr.x + PW > e.x && plr.x < e.x + 7;
        bool overlapY = plr.y + PH > e.y && plr.y < e.y + 7;

        if (overlapX && overlapY) {
            if (plr.vy > 0 && plr.y + PH < e.y + 5) {
                e.active = false;
                plr.vy = JUMP_VEL * 0.6f;
                plr.score += 50;
                playSound(NOTE_E4, 50);
            } else {
                playerDie();
            }
        }
    }
}

// ============ Oyuncu Ölümü ============
inline void playerDie() {
    if (plr.invincTimer > 0.0f) return;
    plr.lives--;
    playSound(NOTE_G3, 100);

    if (plr.lives <= 0) {
        state = ST_GAMEOVER;
        stateTimer = millis();
        if (plr.score > highScore) {
            highScore = plr.score;
            osSaveHighScore("hs_platformer", highScore);
        }
    } else {
        state = ST_DEAD;
        stateTimer = millis();
    }
}
