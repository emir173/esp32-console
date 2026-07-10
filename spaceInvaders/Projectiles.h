#pragma once
// ============================================================
//  E-OS SPACE INVADERS — Projectiles.h
//  Mermi havuzu yonetimi (oyuncu + dusman)
// ============================================================
#include "Config.h"

struct ProjectileManager {
    Bullet pBullets[MAX_PBULLETS];
    Bullet eBullets[MAX_EBULLETS];

    void clear() {
        for (int i = 0; i < MAX_PBULLETS; i++) pBullets[i].active = false;
        for (int i = 0; i < MAX_EBULLETS; i++) eBullets[i].active = false;
    }

    int activePB() const {
        int n = 0;
        for (int i = 0; i < MAX_PBULLETS; i++) if (pBullets[i].active) n++;
        return n;
    }

    int activeEB() const {
        int n = 0;
        for (int i = 0; i < MAX_EBULLETS; i++) if (eBullets[i].active) n++;
        return n;
    }

    bool addPlayerBullet(float x, float y) {
        for (int i = 0; i < MAX_PBULLETS; i++) {
            if (!pBullets[i].active) {
                pBullets[i].x = x;
                pBullets[i].y = y;
                pBullets[i].active = true;
                return true;
            }
        }
        return false;
    }

    bool addEnemyBullet(float x, float y, uint16_t color = COL_EBULLET) {
        for (int i = 0; i < MAX_EBULLETS; i++) {
            if (!eBullets[i].active) {
                eBullets[i].x = x;
                eBullets[i].y = y;
                eBullets[i].color = color;
                eBullets[i].active = true;
                return true;
            }
        }
        return false;
    }

    void updatePlayerBullets(float dt) {
        for (int i = 0; i < MAX_PBULLETS; i++) {
            if (!pBullets[i].active) continue;
            pBullets[i].y -= PBULLET_SPD * dt;
            if (pBullets[i].y < (float)HUD_H) pBullets[i].active = false;
        }
    }

    void updateEnemyBullets(float dt) {
        for (int i = 0; i < MAX_EBULLETS; i++) {
            if (!eBullets[i].active) continue;
            eBullets[i].y += EBULLET_SPD * dt;
            if (eBullets[i].y > (float)SCR_H) eBullets[i].active = false;
        }
    }

    void deactivatePB(int idx) { if (idx >= 0 && idx < MAX_PBULLETS) pBullets[idx].active = false; }
    void deactivateEB(int idx) { if (idx >= 0 && idx < MAX_EBULLETS) eBullets[idx].active = false; }
};
