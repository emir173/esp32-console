#pragma once
// ============================================================
//  Enemies.h — Dusman yapay zekasi, dalga sistemi, spawn mantigi
// ============================================================
#include "Config.h"
#include "Projectiles.h"

// clearEnemies — Tum dusmanlari pasif hale getirir
inline void clearEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
}

// spawnEnemy — Belirtilen tipte yeni bir dusman dogurur
//  type: Dusman turu (EN_BASIC/EN_FAST/EN_TANK/EN_BOSS)
inline void spawnEnemy(EnemyType type) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) continue;
        Enemy& e = enemies[i];
        e.active = true;
        e.type = type;
        e.x = random(15, SCR_W - 15);
        e.y = -10;
        e.vx = (random(0, 2) ? 9.0f : -9.0f);

        switch (type) {
            case EN_BASIC:
                e.vy = 18.0f; e.hp = e.maxHp = 1;
                e.shootInterval = 3.0f; break;
            case EN_FAST:
                e.vy = 36.0f; e.hp = e.maxHp = 1;
                e.shootInterval = 4.0f; break;
            case EN_TANK:
                e.vy = 12.0f; e.hp = e.maxHp = 3;
                e.shootInterval = 2.0f; break;
            case EN_BOSS:
                e.x = SCR_W / 2;
                e.vy = 6.0f; e.hp = e.maxHp = 15;
                e.shootInterval = 1.0f; break;
        }
        e.shootTimer = e.shootInterval;
        return;
    }
}

// updateEnemies — Dusman yapay zekasini gunceller
//  Hareket, sinir kontrolu, ekran disi cikis ve oyuncuya yonelik ates
//  dt: Gecen sure (s)
inline void updateEnemies(float dt) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        Enemy& e = enemies[i];

        e.x += e.vx * dt;
        e.y += e.vy * dt;

        if (e.x < 10 || e.x > SCR_W - 10) e.vx = -e.vx;

        if (e.type == EN_BOSS && e.y > 40) { e.vy = 0.0f; e.y = 40.0f; }

        if (e.y > SCR_H + 15) { e.active = false; continue; }

        e.shootTimer -= dt;
        if (e.shootTimer <= 0.0f) {
            e.shootTimer = e.shootInterval;
            float dx = ship.x - e.x;
            float dy = ship.y - e.y;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 1) { dx /= len; dy /= len; }

            if (e.type == EN_BOSS) {
                for (int s = -1; s <= 1; s++) {
                    float a = s * 0.3f;
                    float bvx = dx * cosf(a) - dy * sinf(a);
                    float bvy = dx * sinf(a) + dy * cosf(a);
                    fireEnemy(e.x, e.y, bvx * 45.0f, bvy * 45.0f);
                }
            } else {
                float spd = (e.type == EN_FAST) ? 54.0f : 36.0f;
                fireEnemy(e.x, e.y, dx * spd, dy * spd);
            }
            if (e.type != EN_BOSS) playSound(NOTE_C4, 25);
        }
    }
}

// updateWaveSpawning — Dalga (wave) sistemini yonetir
//  Dalga icinde dusman dogurur, tum dusmanlar olunce sonraki dalgaya gecer
//  dt: Gecen sure (s)
inline void updateWaveSpawning(float dt) {
    if (!waveActive) return;

    waveSpawnTimer -= dt;
    if (waveSpawnTimer > 0.0f) return;

    if (waveEnemiesSpawned < waveEnemiesPerWave) {
        if (curWave > 0 && curWave % 5 == 0 && waveEnemiesSpawned == 0) {
            spawnEnemy(EN_BOSS);
        } else {
            int r = random(0, 100);
            if (r < 50)      spawnEnemy(EN_BASIC);
            else if (r < 80) spawnEnemy(EN_FAST);
            else              spawnEnemy(EN_TANK);
        }
        waveEnemiesSpawned++;
        waveSpawnTimer = (40.0f + random(0, 30)) / 30.0f;
    } else {
        bool allDead = true;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) { allDead = false; break; }
        }
        if (allDead) {
            curWave++;
            waveEnemiesSpawned = 0;
            waveEnemiesPerWave = min(4 + curWave, 10);
            waveSpawnTimer = 3.0f;
            playSound(NOTE_E5, 40);
        }
    }
}
