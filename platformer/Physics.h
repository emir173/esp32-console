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
extern bool newRecord;

inline void saveHighScoreIfNeeded() {
    if (plr.score > highScore) {
        highScore = plr.score;
        newRecord = true;
        osSaveHighScore("hs_platformer", highScore);
    }
}

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
        if ((int32_t)(millis() - nextMs) < 0) return;
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
    if (row >= MAP_H) return T_AIR; // The lower edge is a pit, not a floor.
    if (col < 0 || col >= MAP_W || row < 0) return T_GROUND;
    return mapData[row][col];
}

inline int tileCoord(float pos) {
    return (int)floorf(pos / TILE);
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
    newRecord = false;
    plr.lives = 3;
    plr.coins = 0;
    plr.score = 0;
    curLevel = 0;
    loadLevel(0);
}

// ============ İleri Bildirim ============
inline void playerDie(bool falling = false);

// ============ Oyuncu Fiziği ve Çarpışma ============
inline void updatePhysics() {
    if (state != ST_PLAY) return;
    // Move at most one pixel per collision query so a slow frame cannot skip
    // a tile. Use fractional hitbox edges rather than truncating away overlap.
    float dx = plr.vx * gameDT;
    int stepsX = max(1, (int)ceilf(fabsf(dx)));
    for (int i = 0; i < stepsX; i++) {
        plr.x += dx / stepsX;
        int col = tileCoord(plr.vx < 0 ? plr.x : plr.x + PW - 0.001f);
        int rowT = tileCoord(plr.y);
        int rowB = tileCoord(plr.y + PH - 0.001f);
        if (plr.vx != 0 && (isSolid(col, rowT) || isSolid(col, rowB))) {
            plr.x = plr.vx < 0 ? (col + 1) * TILE : col * TILE - PW;
            plr.vx = 0;
            break;
        }
    }
    plr.x = max(0.0f, min(plr.x, (float)(MAP_W * TILE - PW)));
    plr.vy += GRAVITY * gameDT;
    if (plr.vy > MAX_FALL) plr.vy = MAX_FALL;
    float dy = plr.vy * gameDT;
    int stepsY = max(1, (int)ceilf(fabsf(dy)));
    plr.grounded = false;
    for (int i = 0; i < stepsY; i++) {
        plr.y += dy / stepsY;
        bool falling = plr.vy >= 0;
        int row = tileCoord(falling ? plr.y + PH : plr.y);
        int colL = tileCoord(plr.x);
        int colR = tileCoord(plr.x + PW - 0.001f);
        if (isSolid(colL, row) || isSolid(colR, row)) {
            plr.y = falling ? row * TILE - PH : (row + 1) * TILE;
            plr.vy = 0;
            plr.grounded = falling;
            if (falling) plr.coyoteT = COYOTE_TIME;
            break;
        }
    }
    if (plr.y > MAP_H * TILE + 4) {
        playerDie(true); // Spawn protection must not keep a fallen player alive.
        return;
    }

    if (!plr.grounded && plr.coyoteT > 0.0f) plr.coyoteT -= gameDT;
}

// ============ Altın / Bayrak / Diken Kontrolü ============
inline void checkTilePickups() {
    if (state != ST_PLAY) return;
    int c1 = max(0, tileCoord(plr.x));
    int c2 = min(MAP_W - 1, tileCoord(plr.x + PW - 0.001f));
    int r1 = max(0, tileCoord(plr.y));
    int r2 = min(MAP_H - 1, tileCoord(plr.y + PH - 0.001f));

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
                return;
            }
            else if (t == T_SPIKE && plr.invincTimer <= 0.0f) {
                playerDie();
                return;
            }
        }
    }
}

// ============ Düşman Güncelleme ============
inline void updateEnemies() {
    if (state != ST_PLAY) return;
    for (int i = 0; i < numEnemies; i++) {
        if (!enemies[i].active) continue;
        Enemy& e = enemies[i];

        e.x += e.vx * gameDT;
        if (e.x <= e.boundL) { e.x = e.boundL; e.vx = fabsf(e.vx); }
        else if (e.x >= e.boundR) { e.x = e.boundR; e.vx = -fabsf(e.vx); }

        bool overlapX = plr.x + PW > e.x && plr.x < e.x + 7;
        bool overlapY = plr.y + PH > e.y && plr.y < e.y + 7;

        if (overlapX && overlapY) {
            if (plr.vy > 0 && plr.y + PH < e.y + 5) {
                e.active = false;
                plr.vy = JUMP_VEL * 0.6f;
                plr.grounded = false;
                plr.coyoteT = 0.0f;
                plr.score += 50;
                playSound(NOTE_E4, 50);
            } else if (plr.invincTimer <= 0.0f) {
                playerDie();
                return;
            }
        }
    }
}

// ============ Oyuncu Ölümü ============
inline void playerDie(bool falling) {
    if (state != ST_PLAY || (!falling && plr.invincTimer > 0.0f)) return;
    plr.lives--;
    playSound(NOTE_G3, 100);

    if (plr.lives <= 0) {
        state = ST_GAMEOVER;
        stateTimer = millis();
        saveHighScoreIfNeeded();
    } else {
        state = ST_DEAD;
        stateTimer = millis();
    }
}
