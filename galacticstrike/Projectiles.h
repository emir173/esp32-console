#pragma once
// ============================================================
//  Projectiles.h — Mermi, patlama, power-up ve carpisma mantigi
// ============================================================
#include "Config.h"

// clearBullets — Tum mermileri (oyuncu ve dusman) pasif hale getirir
inline void clearBullets() {
    for (int i = 0; i < MAX_P_BULLETS; i++) pBullets[i].active = false;
    for (int i = 0; i < MAX_E_BULLETS; i++) eBullets[i].active = false;
}

// clearExplosions — Tum patlamalari pasif hale getirir
inline void clearExplosions() {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) explosions[i].active = false;
}

// clearPowerUps — Tum power-up'lari pasif hale getirir
inline void clearPowerUps() {
    for (int i = 0; i < MAX_POWERUPS; i++) powerUps[i].active = false;
}

// spawnExplosion — Belirtilen konumda yeni bir patlama efekti olusturur
inline void spawnExplosion(float x, float y) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) {
            explosions[i] = {x, y, 0.0f, true};
            return;
        }
    }
}

// spawnPowerUp — Belirtilen konumda (%25 olasilikla) power-up dogurur
inline void spawnPowerUp(float x, float y) {
    if (random(0, 100) > 25) return;
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerUps[i].active) {
            PwrType t = (PwrType)random(0, 3);
            powerUps[i] = {x, y, t, true};
            return;
        }
    }
}

// fireEnemy — Dusmandan mermi atesler (oyuncuya dogru yonlu)
inline void fireEnemy(float ex, float ey, float tvx, float tvy) {
    for (int i = 0; i < MAX_E_BULLETS; i++) {
        if (!eBullets[i].active) {
            eBullets[i] = {ex, ey + 5, tvx, tvy, true};
            return;
        }
    }
}

// updateBullets — Tum mermileri hareket ettirir
//  dt: Gecen sure (s)
inline void updateBullets(float dt) {
    for (int i = 0; i < MAX_P_BULLETS; i++) {
        if (!pBullets[i].active) continue;
        pBullets[i].x += pBullets[i].vx * dt;
        pBullets[i].y += pBullets[i].vy * dt;
        if (pBullets[i].y < -5 || pBullets[i].x < -5 || pBullets[i].x > SCR_W + 5)
            pBullets[i].active = false;
    }
    for (int i = 0; i < MAX_E_BULLETS; i++) {
        if (!eBullets[i].active) continue;
        eBullets[i].x += eBullets[i].vx * dt;
        eBullets[i].y += eBullets[i].vy * dt;
        if (eBullets[i].y > SCR_H + 5 || eBullets[i].x < -5 || eBullets[i].x > SCR_W + 5)
            eBullets[i].active = false;
    }
}

// updateCollisions — Tum carpisma kontrollerini yapar
//  Oyuncu mermisi vs dusman, Dusman mermisi vs oyuncu,
//  Dusman vs oyuncu, Power-up toplama
//  dt: Gecen sure (s)
inline void updateCollisions(float dt) {
    for (int b = 0; b < MAX_P_BULLETS; b++) {
        if (!pBullets[b].active) continue;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!enemies[e].active) continue;
            float ew = (enemies[e].type == EN_BOSS) ? 16 : 8;
            float eh = (enemies[e].type == EN_BOSS) ? 12 : 8;
            if (fabsf(pBullets[b].x - enemies[e].x) < ew/2 &&
                fabsf(pBullets[b].y - enemies[e].y) < eh/2) {
                pBullets[b].active = false;
                enemies[e].hp--;
                if (enemies[e].hp <= 0) {
                    enemies[e].active = false;
                    spawnExplosion(enemies[e].x, enemies[e].y);
                    int pts = (enemies[e].type == EN_BOSS) ? 500 :
                              (enemies[e].type == EN_TANK) ? 100 :
                              (enemies[e].type == EN_FAST) ? 75 : 50;
                    ship.score += pts;
                    playSound(NOTE_D5, 40);
                    spawnPowerUp(enemies[e].x, enemies[e].y);
                } else {
                    playSound(NOTE_A4, 20);
                }
                break;
            }
        }
    }

    if (ship.invincTimer <= 0.0f) {
        for (int b = 0; b < MAX_E_BULLETS; b++) {
            if (!eBullets[b].active) continue;
            if (fabsf(eBullets[b].x - ship.x) < SHIP_W/2 + 2 &&
                fabsf(eBullets[b].y - ship.y) < SHIP_H/2 + 2) {
                eBullets[b].active = false;
                if (ship.shieldTimer > 0.0f) {
                    ship.shieldTimer -= 1.0f;
                    playSound(NOTE_G4, 30);
                } else {
                    ship.hp--;
                    ship.invincTimer = 3.0f;
                    spawnExplosion(ship.x, ship.y);
                    playSound(NOTE_G3, 100);
                    if (ship.hp <= 0) {
                        state = ST_GAMEOVER;
                        stateTimer = millis();
                        if (ship.score > highScore) {
                            highScore = ship.score;
                            osSaveHighScore("hs_galacticstrike", highScore);
                        }
                    }
                }
                break;
            }
        }
    }

    if (ship.invincTimer <= 0.0f) {
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!enemies[e].active) continue;
            float ew = (enemies[e].type == EN_BOSS) ? 16 : 8;
            if (fabsf(enemies[e].x - ship.x) < ew/2 + SHIP_W/2 &&
                fabsf(enemies[e].y - ship.y) < 8) {
                ship.hp--;
                ship.invincTimer = 3.0f;
                spawnExplosion(ship.x, ship.y);
                playSound(NOTE_G3, 100);
                if (ship.hp <= 0) {
                    state = ST_GAMEOVER;
                    stateTimer = millis();
                    if (ship.score > highScore) {
                        highScore = ship.score;
                        osSaveHighScore("hs_galacticstrike", highScore);
                    }
                }
                break;
            }
        }
    }

    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerUps[i].active) continue;
        powerUps[i].y += 24.0f * dt;
        if (powerUps[i].y > SCR_H + 10) { powerUps[i].active = false; continue; }

        if (fabsf(powerUps[i].x - ship.x) < 8 && fabsf(powerUps[i].y - ship.y) < 8) {
            powerUps[i].active = false;
            switch (powerUps[i].type) {
                case PWR_TRIPLE: ship.tripleTimer = 10.0f; playSound(NOTE_E5, 40); break;
                case PWR_SHIELD: ship.shieldTimer = 10.0f; playSound(NOTE_D5, 40); break;
                case PWR_LIFE:
                    if (ship.hp < ship.maxHp) ship.hp++;
                    playSound(NOTE_G5, 40); break;
            }
        }
    }
}

// updateExplosions — Patlama efektlerini gunceller (yasam suresi)
//  dt: Gecen sure (s)
inline void updateExplosions(float dt) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) continue;
        explosions[i].elapsed += dt;
        if (explosions[i].elapsed > 0.4f) explosions[i].active = false;
    }
}
