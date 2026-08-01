#pragma once
// ============================================================
//  E-OS PLATFORMER — Player.h
//  Oyuncu ve düşman veri yapıları (state'ler).
// ============================================================
#include "Config.h"

// ============ Oyuncu Struct ============
struct Player {
    float x, y;
    float vx, vy;
    bool grounded;
    bool facingRight;
    int lives;
    int coins;
    int score;
    float invincTimer;
    float jumpBuf;
    float coyoteT;
};

// ============ Düşman Struct ============
struct Enemy {
    float x, y;
    float vx;
    float boundL, boundR;
    bool active;
};

// ============ Extern Bildirimleri ============
extern Player plr;
extern Enemy enemies[MAX_ENEMIES];
extern int numEnemies;
