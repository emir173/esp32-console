#pragma once
// ============================================================
//  towerdefense/Combat.h — Kule Ateşi, Hasar ve Efekt Sistemleri
//
//  Sorumluluk: Kule hedef seçimi ve atış akışı (okçu/top/buz/
//  lazer/zehir), yıldırım yeteneği, düşman hasarı ve ölüm ödülü,
//  kaleye ulaşma hasarı, parçacık sistemi, ekran sarsıntısı,
//  hasar popup'ları, atış çizgileri (ray), görsel mermiler (shot)
//  ve genişleyen halka efektleri (ring).
//  (Dungeon/Combat.h efekt sistemleri bire bir uyarlanmıştır.)
// ============================================================

#include "Config.h"
#include "Map.h"
#include "Tower.h"
#include "Enemies.h"
#include "Wave.h"
#include "../SharedParticles.h"

// ------------------------------------------------------------
//  Global savaş durumu
// ------------------------------------------------------------
int   killsTotal    = 0;      // Bu oyunda öldürülen düşman
bool  baseDestroyed = false;  // towerdefense.ino her frame kontrol eder
float lightningCd   = 0.0f;   // Yıldırım yeteneği kalan bekleme (sn)

// ------------------------------------------------------------
//  Parçacık sistemi (radyal saçılma + yerçekimi)
// ------------------------------------------------------------
struct ParticleSystem : public SharedParticleSystem<MAX_PARTICLES> {

    // (px,py) pikselinden radyal saçılma
    void emit(float px, float py, uint16_t color, int count) {
        int emitted = 0;
        for (int i = 0; i < MAX_PARTICLES && emitted < count; i++) {
            Particle &p = particles[i];
            if (p.active) continue;
            float ang = random(0, 360) * DEG_TO_RAD;
            float spd = PART_SPEED_MIN + random(0, (int)(PART_SPEED_MAX - PART_SPEED_MIN));
            p.x = px; p.y = py;
            p.vx = cosf(ang) * spd;
            p.vy = sinf(ang) * spd;
            p.color = color;
            p.life = PART_LIFE_MIN + random(0, (int)((PART_LIFE_MAX - PART_LIFE_MIN) * 100)) / 100.0f;
            p.active = true;
            emitted++;
        }
    }

    void update(float dt) {
        SharedParticleSystem<MAX_PARTICLES>::update(dt, PARTICLE_GRAVITY, 9999.0f);
    }

    // Oyun alanı → ekran: y'ye HUD_H eklenir
    void draw(TFT_eSprite &canvas) const {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            const Particle &p = particles[i];
            if (!p.active) continue;
            int sx = (int)p.x;
            int sy = (int)p.y + HUD_H;
            if (sx < 0 || sx >= SCR_W || sy < HUD_H || sy >= SCR_H) continue;
            if (p.life > PART_BIG_LIFE) canvas.fillRect(sx, sy, 2, 2, p.color);
            else                        canvas.drawPixel(sx, sy, p.color);
        }
    }
};

ParticleSystem particles;

// ------------------------------------------------------------
//  Ekran sarsıntısı (üstel sönümlenme, büyük tetik kazanır)
// ------------------------------------------------------------
struct ScreenShake {
    float intensity;
    int offsetX, offsetY;

    void reset() { intensity = 0.0f; offsetX = 0; offsetY = 0; }

    void trigger(float mag) {
        if (mag > intensity) intensity = mag;   // Büyük olan kazanır
    }

    void update() {
        if (intensity < SHAKE_MIN) {
            reset();
            return;
        }
        int m = (int)(intensity + 1.0f);
        offsetX = random(-m, m + 1);
        offsetY = random(-m, m + 1);
        intensity *= SHAKE_DECAY;               // Üstel sönümlenme
    }
};

ScreenShake shake;

// ------------------------------------------------------------
//  Hasar popup'ları ("-3" yazısı, yükselip kaybolur)
// ------------------------------------------------------------
struct DmgPopup {
    float x, y;       // Oyun alanı piksel koordinatı
    int val;
    uint16_t color;
    float life;       // Kalan ömür (sn)
    bool active;
};

DmgPopup popups[MAX_POPUPS];

inline void clearPopups() {
    for (int i = 0; i < MAX_POPUPS; i++) popups[i].active = false;
}

inline void spawnPopup(float px, float py, int val, uint16_t color) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (popups[i].active) continue;
        popups[i].x = px;
        popups[i].y = py;
        popups[i].val = val;
        popups[i].color = color;
        popups[i].life = POPUP_LIFE_S;
        popups[i].active = true;
        return;
    }
}

inline void updatePopups(float dt) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!popups[i].active) continue;
        popups[i].y -= POPUP_RISE * dt;   // Yukarı süzülür
        popups[i].life -= dt;
        if (popups[i].life <= 0.0f) popups[i].active = false;
    }
}

// ------------------------------------------------------------
//  Atış çizgisi (Ray) — kuleden hedefe 2-3 kare parlak çizgi.
//  Okçu ince beyaz, lazer kalın turuncu, yıldırım dikey sarı.
//  (Dungeon'daki Slash struct'ının uyarlaması)
// ------------------------------------------------------------
struct Ray {
    float x0, y0, x1, y1;   // Oyun alanı pikselleri
    float life;
    uint16_t color;
    bool thick;             // Kalın çizim (lazer/yıldırım)
    bool active;
};

Ray rays[MAX_RAYS];

inline void clearRays() {
    for (int i = 0; i < MAX_RAYS; i++) rays[i].active = false;
}

inline void spawnRay(float x0, float y0, float x1, float y1,
                     uint16_t color = COL_RAY, bool thick = false) {
    for (int i = 0; i < MAX_RAYS; i++) {
        if (rays[i].active) continue;
        rays[i] = { x0, y0, x1, y1, RAY_LIFE_S, color, thick, true };
        return;
    }
}

inline void updateRays(float dt) {
    for (int i = 0; i < MAX_RAYS; i++) {
        if (!rays[i].active) continue;
        rays[i].life -= dt;
        if (rays[i].life <= 0.0f) rays[i].active = false;
    }
}

// ------------------------------------------------------------
//  Görsel mermi (Shot) — kuleden hedefe lerp, kısa ömürlü.
//  Hasar atış anında uygulanır; mermi sadece görseldir.
//  Cannon mermisi varışta patlama halkası + parçacık bırakır.
// ------------------------------------------------------------
struct Shot {
    float x0, y0, x1, y1;   // Başlangıç ve hedef pikselleri
    float t;                // 0..1 ilerleme
    uint16_t color;
    bool isCannon;          // Varışta patlama efekti
    bool active;
};

Shot shots[MAX_SHOTS];

inline void clearShots() {
    for (int i = 0; i < MAX_SHOTS; i++) shots[i].active = false;
}

inline void spawnShot(float x0, float y0, float x1, float y1,
                      uint16_t color, bool isCannon) {
    for (int i = 0; i < MAX_SHOTS; i++) {
        if (shots[i].active) continue;
        shots[i] = { x0, y0, x1, y1, 0.0f, color, isCannon, true };
        return;
    }
}

// ------------------------------------------------------------
//  Genişleyen halka (Ring) — patlama, inşa ve yıldırım vurgusu
// ------------------------------------------------------------
struct Ring {
    float x, y;        // Merkez (oyun alanı pikseli)
    float life;        // Kalan ömür (sn)
    int maxR;          // Ulaşılacak yarıçap
    uint16_t color;
    bool active;
};

Ring rings[MAX_RINGS];

inline void clearRings() {
    for (int i = 0; i < MAX_RINGS; i++) rings[i].active = false;
}

inline void spawnRing(float x, float y, int maxR, uint16_t color) {
    for (int i = 0; i < MAX_RINGS; i++) {
        if (rings[i].active) continue;
        rings[i] = { x, y, RING_LIFE_S, maxR, color, true };
        return;
    }
}

inline void updateRings(float dt) {
    for (int i = 0; i < MAX_RINGS; i++) {
        if (!rings[i].active) continue;
        rings[i].life -= dt;
        if (rings[i].life <= 0.0f) rings[i].active = false;
    }
}

inline void updateShots(float dt) {
    for (int i = 0; i < MAX_SHOTS; i++) {
        Shot &s = shots[i];
        if (!s.active) continue;
        s.t += dt / SHOT_DUR_S;
        if (s.t >= 1.0f) {
            s.active = false;
            if (s.isCannon) {                     // Varış: patlama görseli
                particles.emit(s.x1, s.y1, COL_SHOT_CAN, PART_N_SPLASH);
                spawnRing(s.x1, s.y1, RING_R_SPLASH, COL_SHOT_CAN);
            }
        }
    }
}

// ------------------------------------------------------------
//  Görsel efektleri temizle (yeni oyun / bölüm geçişi)
//  NOT: killsTotal/baseDestroyed towerdefense.ino'da sıfırlanır
//  (bölüm geçişinde istatistikler korunmalı).
// ------------------------------------------------------------
inline void clearEffects() {
    particles.clear();
    clearPopups();
    clearRays();
    clearShots();
    clearRings();
    shake.reset();
}

// ------------------------------------------------------------
//  DÜŞMANA HASAR — popup + parçacık, ölürse para + ses
// ------------------------------------------------------------
inline void damageEnemy(Enemy &e, int dmg, uint16_t popupColor = COL_HUD_TEXT) {
    // Zırh: her vuruştan sabit azaltma (min 1). Zehir tiki de dahil — zırhı
    // delemeyen düşük hasarlı OKÇU spamı yerine TOP/LAZER teşvik edilir.
    dmg -= ENEMY_ARMOR[e.type];
    if (dmg < 1) dmg = 1;
    e.hp -= dmg;
    spawnPopup(enemyCenterX(e), e.y, dmg, popupColor);
    particles.emit(enemyCenterX(e), enemyCenterY(e), e.color, PART_N_HIT);

    if (e.hp <= 0) {
        e.alive = false;
        killsTotal++;
        money += e.reward;
        playSound(NOTE_A3, SND_KILL_MS);   // 220 Hz: tok darbe
        particles.emit(enemyCenterX(e), enemyCenterY(e), e.color, PART_N_KILL);
    }
}

// ------------------------------------------------------------
//  ZEHİR TİKİ — Enemies.h'deki ileri bildirimin tanımı.
//  Periyodik küçük hasar + yeşil kabarcık parçacığı.
// ------------------------------------------------------------
void poisonTick(Enemy &e) {
    particles.emit(enemyCenterX(e), e.y, COL_TOWER_POISON, 1);
    damageEnemy(e, POISON_TICK_DMG, COL_TOWER_POISON);
}

// ------------------------------------------------------------
//  KALEYE ULAŞMA — Enemies.h'deki ileri bildirimin tanımı.
//  Runner/soldier/uçan -1, tank -3, boss -5; 0'da oyun biter.
// ------------------------------------------------------------
void baseReached(Enemy &e) {
    e.alive = false;
    baseHp -= ENEMY_BASE_DMG[e.type] + BIOME_BASE_DMG_ADD[e.biome];   // Cehennem düşmanı daha yıkıcı
    playSound(NOTE_G3, SND_BASEHIT_MS);    // 196 Hz: ciddi olay
    shake.trigger(SHAKE_BASE_HIT);
    particles.emit(enemyCenterX(e), enemyCenterY(e), COL_BASE, PART_N_BASEHIT);
    spawnRing(enemyCenterX(e), enemyCenterY(e), RING_R_SPLASH, COL_BASE);

    if (baseHp <= 0) {
        baseHp = 0;
        baseDestroyed = true;              // towerdefense.ino GAMEOVER'a geçirir
        shake.trigger(SHAKE_GAMEOVER);
    }
}

// ------------------------------------------------------------
//  YILDIRIM YETENEĞİ — ekrandaki TÜM düşmanlara hasar.
//  Her düşmana gökten dikey sarı ışın + halka; ağır ses + sarsıntı.
//  Maliyet/bekleme kontrolü towerdefense.ino'da yapılır.
// ------------------------------------------------------------
inline void castLightning() {
    playSound(NOTE_B3, SND_BOLT_MS);       // 247 Hz: ağır çarpma
    shake.trigger(SHAKE_LIGHTNING);
    // Hasar düşman HP eğrisiyle aynı ölçeklenir (dalga + biyom) — v4.1
    int dmg = (int)((LIGHTNING_DMG_BASE + (waveNum - 1) * LIGHTNING_DMG_PER_WAVE)
                    * BIOME_HP_MULT[biomeOf(currentLevel)] + 0.5f);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy &e = enemies[i];
        if (!e.alive) continue;
        float cx = enemyCenterX(e), cy = enemyCenterY(e);
        spawnRay(cx, 0.0f, cx, cy, COL_LIGHTNING, true);   // Gökten inen ışın
        spawnRing(cx, cy, RING_R_BOLT, COL_LIGHTNING);
        damageEnemy(e, dmg, COL_MONEY);
    }
}

// ------------------------------------------------------------
//  Hedef seçimi: menzildeki EN İLERLEMİŞ düşman (pathIdx büyük)
// ------------------------------------------------------------
inline int findTarget(const Tower &t) {
    float cx = towerCenterX(t), cy = towerCenterY(t);
    float r2 = (float)(t.range * TILE_PX) * (t.range * TILE_PX);
    int best = -1;
    float bestProg = -1.0f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy &e = enemies[i];
        if (!e.alive) continue;
        float dx = enemyCenterX(e) - cx;
        float dy = enemyCenterY(e) - cy;
        if (dx * dx + dy * dy > r2) continue;
        float prog = pathProgressOf(e);
        if (prog > bestProg) { bestProg = prog; best = i; }
    }
    return best;
}

// ------------------------------------------------------------
//  Cannon hedef seçimi: menzilde EN KALABALIK küme merkezi
//  (patlama yarıçapı içindeki komşu sayısı; eşitlikte ilerleme)
// ------------------------------------------------------------
inline int findClusterTarget(const Tower &t) {
    float cx = towerCenterX(t), cy = towerCenterY(t);
    float r2 = (float)(t.range * TILE_PX) * (t.range * TILE_PX);
    float s2 = (float)SPLASH_RADIUS_PX * SPLASH_RADIUS_PX;
    int best = -1, bestCount = 0;
    float bestProg = -1.0f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy &e = enemies[i];
        if (!e.alive) continue;
        float dx = enemyCenterX(e) - cx;
        float dy = enemyCenterY(e) - cy;
        if (dx * dx + dy * dy > r2) continue;

        // Bu düşmanın etrafındaki küme büyüklüğü
        int count = 0;
        for (int j = 0; j < MAX_ENEMIES; j++) {
            const Enemy &o = enemies[j];
            if (!o.alive) continue;
            float ox = enemyCenterX(o) - enemyCenterX(e);
            float oy = enemyCenterY(o) - enemyCenterY(e);
            if (ox * ox + oy * oy <= s2) count++;
        }
        float prog = pathProgressOf(e);
        if (count > bestCount || (count == bestCount && prog > bestProg)) {
            bestCount = count;
            bestProg = prog;
            best = i;
        }
    }
    return best;
}

// ------------------------------------------------------------
//  KULE ATIŞI — türe göre efekt ve hasar akışı
// ------------------------------------------------------------
inline void towerFire(Tower &t, Enemy &target) {
    float cx = towerCenterX(t), cy = towerCenterY(t);
    float ex = enemyCenterX(target), ey = enemyCenterY(target);
    t.aimX = ex;
    t.aimY = ey;

    switch (t.type) {
        case TOWER_ARROW:
            // Hızlı tek hedef: anlık ince çizgi + hasar
            spawnRay(cx, cy, ex, ey);
            playSound(NOTE_A4, SND_SHOT_MS);   // 440 Hz: ateş
            damageEnemy(target, t.damage);
            break;

        case TOWER_CANNON: {
            // Alan hasarı: hedef merkezli 1 tile yarıçap
            spawnShot(cx, cy, ex, ey, COL_SHOT_CAN, true);
            playSound(NOTE_B3, SND_KILL_MS);   // 247 Hz: ağır atış
            shake.trigger(SHAKE_CANNON);
            float s2 = (float)SPLASH_RADIUS_PX * SPLASH_RADIUS_PX;
            for (int i = 0; i < MAX_ENEMIES; i++) {
                Enemy &e = enemies[i];
                if (!e.alive) continue;
                float dx = enemyCenterX(e) - ex;
                float dy = enemyCenterY(e) - ey;
                if (dx * dx + dy * dy <= s2) damageEnemy(e, t.damage);
            }
            break;
        }

        case TOWER_FROST:
            // Yavaşlatma + hafif hasar
            spawnShot(cx, cy, ex, ey, COL_SHOT_FRO, false);
            playSound(NOTE_E4, SND_SHOT_MS);   // 330 Hz: buz
            target.slowUntil = millis() + FROST_SLOW_MS;
            damageEnemy(target, t.damage);
            break;

        case TOWER_LASER:
            // Uzun menzil, çok hızlı ışın: kalın turuncu çizgi
            spawnRay(cx, cy, ex, ey, COL_TOWER_LASER, true);
            playSound(NOTE_A4, SND_LASER_MS);  // 440 Hz: kısacık cızırtı
            damageEnemy(target, t.damage);
            break;

        default: {   // TOWER_POISON — zamana yayılı hasar bulutu
            spawnShot(cx, cy, ex, ey, COL_TOWER_POISON, false);
            playSound(NOTE_E4, SND_POISON_MS); // 330 Hz: fışş
            uint32_t now = millis();
            // Zehir süresi biyom direncine göre kısalır (cehennemde ateşli düşman)
            uint32_t pdur = (uint32_t)(POISON_DUR_MS * BIOME_POISON_POWER[target.biome]);
            target.poisonUntil  = now + pdur;
            target.nextPoisonMs = now + POISON_TICK_MS;
            particles.emit(ex, ey, COL_TOWER_POISON, PART_N_POISON);
            damageEnemy(target, t.damage, COL_TOWER_POISON);
            break;
        }
    }
}

// ------------------------------------------------------------
//  Kule turu — 60 Hz sabit tick olarak çağrılır.
//  Bekleme dolunca hedef ara; varsa ateş et.
// ------------------------------------------------------------
inline void tickTowers() {
    for (int i = 0; i < MAX_TOWERS; i++) {
        Tower &t = towers[i];
        if (!t.active) continue;
        if (t.cooldown > 0.0f) {
            t.cooldown -= FRAME_SEC;
            continue;
        }
        int ti = (t.type == TOWER_CANNON) ? findClusterTarget(t) : findTarget(t);
        if (ti < 0) continue;   // Menzilde düşman yok, beklemede kal
        towerFire(t, enemies[ti]);
        t.cooldown = t.intervalMs / 1000.0f;
    }
}
