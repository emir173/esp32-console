#pragma once
// ============================================================
//  towerdefense/Enemies.h — Düşman Havuzu, Spawn ve Hareket
//
//  Sorumluluk: Düşman yapısı, sabit havuz (24 slot), dalga
//  ölçekli spawn (HP + hız artışı) ve iki hareket modeli:
//   - Yürüyenler: genişletilmiş yol (pathTiles) üzerinde lineer
//   - Uçanlar: yolu YOK SAYAR, spawn'dan kaleye kuş uçuşu gider
//  Zehir tik hasarı ve kaleye ulaşma bildirimi Combat.h'de
//  tanımlıdır (ileri bildirimler).
// ============================================================

#include "Config.h"
#include "Map.h"
#include "Tower.h"

// ------------------------------------------------------------
//  Düşman yapısı
// ------------------------------------------------------------
struct Enemy {
    bool alive;
    EnemyType type;
    uint8_t biome;          // Spawn edildiği biyom (0..2) — frost/poison direnci, hız
    int pathIdx;            // Mevcut waypoint (pathTiles) indeksi (uçanlar kullanmaz)
    float x, y;             // Piksel koordinat (tile sol üst köşesi, interpole)
    int hp, maxHp;
    float speed;            // tile/sn (dalga ölçekli)
    int reward;             // Öldürünce verilen para
    uint16_t color;
    uint32_t slowUntil;     // Frost yavaşlatma bitiş zamanı (millis)
    uint32_t poisonUntil;   // Zehir etkisi bitiş zamanı (millis)
    uint32_t nextPoisonMs;  // Bir sonraki zehir tik zamanı (millis)
    uint32_t nextHealMs;    // Şifacı: bir sonraki iyileştirme zamanı (millis)
};

Enemy enemies[MAX_ENEMIES];

// Combat.h içinde tanımlanır (ileri bildirimler)
void baseReached(Enemy &e);       // Düşman kaleye ulaştığında hasar akışı
void poisonTick(Enemy &e);        // Zehir tik hasarı

// ------------------------------------------------------------
//  Sorgular
// ------------------------------------------------------------
inline int aliveEnemyCount() {
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive) n++;
    return n;
}

// Düşman merkezinin piksel koordinatı
inline float enemyCenterX(const Enemy &e) { return e.x + TILE_PX / 2.0f; }
inline float enemyCenterY(const Enemy &e) { return e.y + TILE_PX / 2.0f; }

// Yol ilerleme metriği: hedef seçiminde "en ilerlemiş" düşman kazanır.
// Yürüyenler: pathIdx + sonraki tile'a tamamlanma oranı.
// Uçanlar: kaleye kalan kuş uçuşu mesafeden eşdeğer ilerleme.
inline float pathProgressOf(const Enemy &e) {
    if (e.type == ENEMY_FLYER) {
        float dx = baseTileX * (float)TILE_PX - e.x;
        float dy = baseTileY * (float)TILE_PX - e.y;
        float d = sqrtf(dx * dx + dy * dy);
        return pathLen * (1.0f - d / flyDistPx);
    }
    if (e.pathIdx >= pathLen - 1) return (float)pathLen;
    float tx = pathTiles[e.pathIdx + 1].x * (float)TILE_PX;
    float ty = pathTiles[e.pathIdx + 1].y * (float)TILE_PX;
    float dist = fabsf(tx - e.x) + fabsf(ty - e.y);   // Eksen hizalı yol
    return e.pathIdx + (1.0f - dist / (float)TILE_PX);
}

// ------------------------------------------------------------
//  Havuzu temizle (yeni oyun / bölüm geçişi)
// ------------------------------------------------------------
inline void clearEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].alive = false;
}

// ------------------------------------------------------------
//  Spawn — dalga numarasına göre HP/hız ölçekli.
//  Havuz doluysa false döner (spawn kuyruğu bir sonraki tick dener).
// ------------------------------------------------------------
inline bool spawnEnemy(EnemyType type, int waveNum, int level) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy &e = enemies[i];
        if (e.alive) continue;
        int biome = biomeOf(level);
        e.alive   = true;
        e.type    = type;
        e.biome   = (uint8_t)biome;
        e.pathIdx = 0;
        e.x       = pathTiles[0].x * (float)TILE_PX;
        e.y       = pathTiles[0].y * (float)TILE_PX;

        // HP ölçekleme: global dalga başına lineer artış + BİYOM çarpanı.
        // Biyom sınırında belirgin duvar (orman×1.0 / donmuş×1.6 / cehennem×2.5).
        int perWave = (type == ENEMY_BOSS) ? BOSS_HP_PER_WAVE : ENEMY_HP_PER_WAVE;
        int baseHp  = ENEMY_HP[type] + (waveNum - 1) * perWave;
        e.maxHp = (int)(baseHp * BIOME_HP_MULT[biome] + 0.5f);
        e.hp    = e.maxHp;

        // Hız ölçekleme: dalga başına +%2 (üst sınırlı) × biyom hız çarpanı
        float scale = 1.0f + (waveNum - 1) * SPEED_SCALE_PER_WAVE;
        if (scale > SPEED_SCALE_MAX) scale = SPEED_SCALE_MAX;
        e.speed = ENEMY_SPEED[type] * scale * BIOME_SPEED_MULT[biome];

        e.reward       = ENEMY_REWARD[type];
        e.slowUntil    = 0;
        e.poisonUntil  = 0;
        e.nextPoisonMs = 0;
        e.nextHealMs   = (type == ENEMY_HEALER) ? millis() + HEAL_INTERVAL_MS : 0;
        switch (type) {
            case ENEMY_RUNNER:  e.color = COL_ENEMY_RUNNER;  break;
            case ENEMY_TANK:    e.color = COL_ENEMY_TANK;    break;
            case ENEMY_FLYER:   e.color = COL_ENEMY_FLYER;   break;
            case ENEMY_BOSS:    e.color = (biome == BIOME_FROST) ? COL_ENEMY_BOSS_FR
                                        : (biome == BIOME_HELL)  ? COL_ENEMY_BOSS_HL
                                        : COL_ENEMY_BOSS;    break;
            case ENEMY_ARMORED: e.color = COL_ENEMY_ARMORED; break;
            case ENEMY_HEALER:  e.color = COL_ENEMY_HEALER;  break;
            case ENEMY_SWARM:   e.color = COL_ENEMY_SWARM;   break;
            default:            e.color = COL_ENEMY_SOLDIER; break;
        }
        return true;
    }
    return false;
}

// ------------------------------------------------------------
//  Hareket turu — 60 Hz sabit tick olarak çağrılır (dt biriktirme
//  towerdefense.ino'da). Zehir tikleri de burada işlenir.
// ------------------------------------------------------------
inline void tickEnemies() {
    uint32_t now = millis();
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy &e = enemies[i];
        if (!e.alive) continue;

        // Zehir etkisi: periyodik tik hasarı (Combat.h uygular)
        if (e.poisonUntil > now && now >= e.nextPoisonMs) {
            e.nextPoisonMs = now + POISON_TICK_MS;
            poisonTick(e);
            if (!e.alive) continue;
        }

        // Şifacı: periyodik olarak menzil içindeki CANLI düşmanları iyileştirir
        if (e.type == ENEMY_HEALER && now >= e.nextHealMs) {
            e.nextHealMs = now + HEAL_INTERVAL_MS;
            float hr2 = (HEAL_RANGE_TILES * TILE_PX) * (HEAL_RANGE_TILES * TILE_PX);
            for (int j = 0; j < MAX_ENEMIES; j++) {
                Enemy &o = enemies[j];
                if (!o.alive || o.hp >= o.maxHp) continue;
                float dx = enemyCenterX(o) - enemyCenterX(e);
                float dy = enemyCenterY(o) - enemyCenterY(e);
                if (dx * dx + dy * dy > hr2) continue;
                o.hp += HEAL_AMOUNT;
                if (o.hp > o.maxHp) o.hp = o.maxHp;
            }
        }

        // Frost yavaşlatma — biyom direnci: donmuşta etki zayıf (BIOME_FROST_POWER)
        float slowF = 1.0f;
        if (e.slowUntil > now)
            slowF = 1.0f - (1.0f - FROST_SLOW_FACTOR) * BIOME_FROST_POWER[e.biome];
        float step  = e.speed * slowF * FRAME_SEC * (float)TILE_PX;   // px/tick

        // --- UÇAN: yolu yok sayar, kaleye kuş uçuşu ---
        if (e.type == ENEMY_FLYER) {
            float tx = baseTileX * (float)TILE_PX;
            float ty = baseTileY * (float)TILE_PX;
            float dx = tx - e.x;
            float dy = ty - e.y;
            float d = sqrtf(dx * dx + dy * dy);
            if (step >= d) {
                baseReached(e);
            } else {
                e.x += dx / d * step;
                e.y += dy / d * step;
            }
            continue;
        }

        // --- YÜRÜYEN: genişletilmiş yol üzerinde lineer ---
        if (e.pathIdx >= pathLen - 1) {
            baseReached(e);       // Son tile'a (kale) varıldı
            continue;
        }

        float tx = pathTiles[e.pathIdx + 1].x * (float)TILE_PX;
        float ty = pathTiles[e.pathIdx + 1].y * (float)TILE_PX;
        float dx = tx - e.x;
        float dy = ty - e.y;
        float dist = fabsf(dx) + fabsf(dy);   // Eksen hizalı: biri sıfır

        if (step >= dist) {
            // Waypoint'e ulaşıldı, sonrakine geç (artan adım ihmal edilir)
            e.x = tx;
            e.y = ty;
            e.pathIdx++;
        } else if (fabsf(dx) > fabsf(dy)) {
            e.x += (dx > 0) ? step : -step;
        } else {
            e.y += (dy > 0) ? step : -step;
        }
    }
}
