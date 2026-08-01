#pragma once
// ============================================================
//  dungeon/Combat.h — Savaş Mekaniği ve Efekt Sistemleri
//
//  Sorumluluk: Hasar formülü, oyuncu/düşman saldırı akışı,
//  parçacık sistemi, ekran sarsıntısı, hasar popup'ları,
//  bıçak izi efekti ve oyun içi mesaj sistemi.
//  Ölüm/öldürme sayaçları ve drop mantığı da buradadır.
// ============================================================

#include <Preferences.h>   // Başarım bitmask'i NVS'e yazılır
#include "Config.h"
#include "Player.h"
#include "Map.h"
#include "../SharedParticles.h"
#include "Enemies.h"
#include "Items.h"
#include "Spells.h"

// ------------------------------------------------------------
//  Global savaş durumu
// ------------------------------------------------------------
int  killsTotal       = 0;      // Bu oyunda öldürülen düşman
int  floorKills       = 0;      // Bu katta öldürülen düşman (ach_dungeon)
int  floorDamageTaken = 0;      // Bu katta alınan hasar ("Ölümsüz" başarımı)
bool playerDied       = false;  // dungeon.ino her frame kontrol eder
bool bossDefeated     = false;

uint32_t lastKillTimeMs = 0;
int comboCount = 0;
uint32_t flashUntil = 0;     // Faz geçişi: boss sprite'ı bu zamana dek beyaz yanar

// ------------------------------------------------------------
//  Oyun içi mesaj sistemi ("ENVANTER DOLU!" vb.)
// ------------------------------------------------------------
constexpr int MSG_BUF_LEN = 24;
char     gameMsg[MSG_BUF_LEN] = "";
uint32_t gameMsgUntil = 0;

void showMessage(const char *txt) {
    strncpy(gameMsg, txt, MSG_BUF_LEN - 1);
    gameMsg[MSG_BUF_LEN - 1] = '\0';
    gameMsgUntil = millis() + MSG_DURATION_MS;
}

// ------------------------------------------------------------
//  BAŞARIM SİSTEMİ (v2.0) — NVS "ach_dungeon" bitmask, kalıcı.
//  Bildirim non-blocking: alt-orta banner (Renderer çizer).
// ------------------------------------------------------------
uint32_t achMask = 0;                 // Açılmış başarımlar (setup'ta yüklenir)
char     achText[MSG_BUF_LEN] = "";   // Aktif bildirim metni
uint32_t achUntil = 0;                // Bildirim bitiş zamanı

inline void loadAchievements() {
    achMask = (uint32_t)osLoadHighScore("ach_dungeon", 0);
}

void unlockAchievement(int idx) {
    if (idx < 0 || idx >= ACH_COUNT) return;
    if (achMask & (1u << idx)) return;        // Zaten açık
    achMask |= (1u << idx);
    osSaveHighScore("ach_dungeon", (int)achMask);
    snprintf(achText, MSG_BUF_LEN, "ACHIEVEMENT: %s", ACH_NAMES[idx]);
    achUntil = millis() + ACH_SHOW_MS;
    playSound(NOTE_E5, SND_ACH_MS);
}

// ------------------------------------------------------------
//  ALTIN (v2.0) — tüm kazançlar buradan geçer ("Zengin" kontrolü)
// ------------------------------------------------------------
inline void addGold(int g) {
    player.gold += g;
    if (player.gold >= ACH_RICH_GOLD) unlockAchievement(ACH_RICH);
}

// ------------------------------------------------------------
//  Hasar formülü: ATK - DEF ± 1, minimum 1
// ------------------------------------------------------------
inline int calcDamage(int attackerATK, int defenderDEF) {
    int dmg = attackerATK - defenderDEF + (int)random(DMG_VAR_MIN, DMG_VAR_MAX);
    return (dmg < 1) ? 1 : dmg;
}

// ------------------------------------------------------------
//  Parçacık sistemi (radyal saçılma + yerçekimi)
// ------------------------------------------------------------
struct ParticleSystem : public SharedParticleSystem<MAX_PARTICLES> {

    // (px,py) dünya pikselinden radyal saçılma
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

    // offX/offY: dünya→ekran dönüşümü (kamera + sarsıntı + HUD)
    void draw(TFT_eSprite &canvas, int offX, int offY) const {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            const Particle &p = particles[i];
            if (!p.active) continue;
            int sx = (int)p.x + offX;
            int sy = (int)p.y + offY;
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
            intensity = 0.0f;
            offsetX = 0; offsetY = 0;
            return;
        }
        int m = (int)(intensity + 1.0f);
        offsetX = random(-m, m + 1);
        offsetY = random(-m, m + 1);
        intensity *= SHAKE_DECAY;               // Üstel sönümlenme
    }
};

ScreenShake shake;

// Golem adım efekti (v3.3): küçük sarsıntı + alçak "güm" —
// Renderer'daki adım-senkron bacak kareleriyle birlikte 2x2 taş
// devinin ağırlığını hissettirir. bossChaseStep'ten çağrılır.
void golemStompFx() {
    shake.trigger(SHAKE_STOMP);
    playSound(GLM_STOMP_FREQ, GLM_STOMP_MS);
}

// ------------------------------------------------------------
//  Hasar popup'ları ("-3" yazısı, yükselip kaybolur)
// ------------------------------------------------------------
struct DmgPopup {
    float x, y;       // Dünya piksel koordinatı
    int val;
    uint16_t color;
    float life;       // Kalan ömür (sn)
    bool active;
};

DmgPopup popups[MAX_POPUPS];

inline void clearPopups() {
    for (int i = 0; i < MAX_POPUPS; i++) popups[i].active = false;
}

inline void spawnPopup(int tileX, int tileY, int val, uint16_t color) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (popups[i].active) continue;
        popups[i].x = tileX * TILE_PX + TILE_PX / 2;
        popups[i].y = tileY * TILE_PX;
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
//  Bıçak izi efekti (saldırı yönünde kısa parlak çizgi)
// ------------------------------------------------------------
struct Slash {
    int tileX, tileY;   // Hedef tile
    int dir;            // Saldırı yönü
    float life;         // Kalan ömür (sn)
    bool active;
};

Slash slashes[MAX_SLASHES];

inline void clearSlashes() {
    for (int i = 0; i < MAX_SLASHES; i++) slashes[i].active = false;
}

inline void spawnSlash(int tileX, int tileY, int dir) {
    for (int i = 0; i < MAX_SLASHES; i++) {
        if (slashes[i].active) continue;
        slashes[i].tileX = tileX;
        slashes[i].tileY = tileY;
        slashes[i].dir = dir;
        slashes[i].life = SLASH_LIFE_S;
        slashes[i].active = true;
        return;
    }
}

inline void updateSlashes(float dt) {
    for (int i = 0; i < MAX_SLASHES; i++) {
        if (!slashes[i].active) continue;
        slashes[i].life -= dt;
        if (slashes[i].life <= 0.0f) slashes[i].active = false;
    }
}

// ------------------------------------------------------------
//  TELEGRAF SİSTEMİ (v2.0) — boss saldırı uyarısı: tile'lar
//  yanıp söner, süre dolunca üstündeki oyuncuya hasar uygulanır.
//  damage=0 varyantı sadece görsel (buz kristali efekti).
// ------------------------------------------------------------
constexpr int MAX_TELEGRAPHS  = 2;
constexpr int TELE_MAX_TILES  = 9;

struct Telegraph {
    bool active;
    uint8_t count;
    uint8_t tx[TELE_MAX_TILES], ty[TELE_MAX_TILES];
    uint32_t until;      // Patlama zamanı
    uint16_t color;      // Uyarı rengi
    int damage;          // 0 = sadece görsel
};

Telegraph telegraphs[MAX_TELEGRAPHS];

inline void clearTelegraphs() {
    for (int i = 0; i < MAX_TELEGRAPHS; i++) telegraphs[i].active = false;
}

// Boş telegraf slotu al (yoksa nullptr)
inline Telegraph *allocTelegraph(uint32_t durMs, uint16_t color, int damage) {
    for (int i = 0; i < MAX_TELEGRAPHS; i++) {
        if (telegraphs[i].active) continue;
        telegraphs[i].active = true;
        telegraphs[i].count = 0;
        telegraphs[i].until = millis() + durMs;
        telegraphs[i].color = color;
        telegraphs[i].damage = damage;
        return &telegraphs[i];
    }
    return nullptr;
}

inline void telegraphAddTile(Telegraph *t, int x, int y) {
    if (t == nullptr || t->count >= TELE_MAX_TILES || !inMap(x, y)) return;
    if (tiles[y][x] == TILE_WALL) return;    // Duvara işaret koyma
    t->tx[t->count] = (uint8_t)x;
    t->ty[t->count] = (uint8_t)y;
    t->count++;
}

// ------------------------------------------------------------
//  BÜYÜ MERMİSİ (v3.2) — Lich uzak saldırısı: boss merkezinden
//  çıkan mor küre, atış ANINDA kilitlenen hedef tile'a uçar.
//  Varışta oyuncu hâlâ o tile'daysa hasar alır (kenara kaçılabilir).
// ------------------------------------------------------------
struct BossBolt {
    bool active;
    float x0, y0;        // Çıkış noktası (dünya px)
    uint8_t tx, ty;      // Hedef tile (kilitli)
    uint32_t startMs;
    int damage;
};

BossBolt bolts[MAX_BOLTS];

inline void clearBolts() {
    for (int i = 0; i < MAX_BOLTS; i++) bolts[i].active = false;
}

// ------------------------------------------------------------
//  MERKEZİ OYUNCU HASARI (v2.0) — kalkan bağışıklığı, hasar
//  sayacı ve ölüm bayrağı tek noktada. Tüm hasar kaynakları
//  (düşman, boss, telegraf, lav) bunu kullanır.
// ------------------------------------------------------------
inline void damagePlayer(int dmg) {
    float pcx = player.tileX * TILE_PX + TILE_PX / 2;
    float pcy = player.tileY * TILE_PX + TILE_PX / 2;

    if (status.shielded()) {                 // Kalkan: hasar tamamen emilir
        spawnPopup(player.tileX, player.tileY, 0, COL_SHIELD);
        particles.emit(pcx, pcy, COL_SHIELD, PART_N_HIT);
        playSound(NOTE_G4, SND_SPELL_SHIELD_MS);
        return;
    }

    player.hp -= dmg;
    floorDamageTaken += dmg;
    spawnPopup(player.tileX, player.tileY, dmg, COL_HP_LOW);
    particles.emit(pcx, pcy, COL_HP_LOW, PART_N_HURT);
    shake.trigger(SHAKE_HURT);
    playSound(NOTE_G3, SND_HURT_MS);         // 196 Hz / 80 ms

    if (player.hp <= 0) {
        player.hp = 0;
        playerDied = true;                   // dungeon.ino GAMEOVER'a geçirir
        shake.trigger(SHAKE_DEATH);
    }
}

// Büyü mermisi varışları — her frame çağrılır (v3.2)
inline void updateBolts() {
    for (int i = 0; i < MAX_BOLTS; i++) {
        BossBolt &b = bolts[i];
        if (!b.active || millis() - b.startMs < LICH_BOLT_MS) continue;
        b.active = false;
        particles.emit(b.tx * TILE_PX + TILE_PX / 2,
                       b.ty * TILE_PX + TILE_PX / 2,
                       COL_LICH, PART_N_HIT);
        if (player.tileX == b.tx && player.tileY == b.ty)
            damagePlayer(b.damage);
    }
}

// Lich büyü mermisi — Enemies.h'deki ileri bildirimin tanımı
void lichCastBolt() {
    for (int i = 0; i < MAX_BOLTS; i++) {
        BossBolt &b = bolts[i];
        if (b.active) continue;
        b.active = true;
        b.x0 = (boss.tileX + 1) * TILE_PX;   // 2x2 gövde merkezi
        b.y0 = (boss.tileY + 1) * TILE_PX;
        b.tx = (uint8_t)player.tileX;
        b.ty = (uint8_t)player.tileY;
        b.startMs = millis();
        b.damage = calcDamage(boss.atk, player.def);
        playSound(NOTE_A3, SND_SPELL_FIRE_MS);
        return;
    }
}

// Telegraf patlamaları — her frame çağrılır
inline void updateTelegraphs() {
    for (int i = 0; i < MAX_TELEGRAPHS; i++) {
        Telegraph &t = telegraphs[i];
        if (!t.active || millis() < t.until) continue;
        t.active = false;
        bool hitPlayer = false;
        for (int k = 0; k < t.count; k++) {
            particles.emit(t.tx[k] * TILE_PX + TILE_PX / 2,
                           t.ty[k] * TILE_PX + TILE_PX / 2,
                           t.color, PART_N_HIT);
            if (player.tileX == t.tx[k] && player.tileY == t.ty[k]) hitPlayer = true;
        }
        if (hitPlayer && t.damage > 0) damagePlayer(t.damage);
    }
}

// ------------------------------------------------------------
//  DURUM EFEKTİ TICK'İ (v2.0) — 1 sn'de bir çağrılır (dt
//  biriktirme dungeon.ino'da). Zehir: stack başına 1 hasar.
// ------------------------------------------------------------
inline void tickStatus1Hz() {
    if (status.poisonStacks <= 0) return;
    status.poisonStacks--;

    float pcx = player.tileX * TILE_PX + TILE_PX / 2;
    float pcy = player.tileY * TILE_PX + TILE_PX / 2;
    particles.emit(pcx, pcy, COL_POTION, PART_N_POISON);

    if (status.shielded()) return;           // Kalkan zehri de emer

    player.hp -= POISON_TICK_DMG;
    floorDamageTaken += POISON_TICK_DMG;
    spawnPopup(player.tileX, player.tileY, POISON_TICK_DMG, COL_POTION);
    playSound(NOTE_G3, SND_ATK_MS);
    if (player.hp <= 0) {
        player.hp = 0;
        playerDied = true;
        shake.trigger(SHAKE_DEATH);
    }
}

// ------------------------------------------------------------
//  Tüm efektleri temizle (kat geçişi / yeni oyun)
// ------------------------------------------------------------
inline void clearEffects() {
    particles.clear();
    clearPopups();
    clearSlashes();
    clearTelegraphs();
    clearBolts();
    shake.reset();
    status.clear();
    playerDied = false;
    bossDefeated = false;
    flashUntil = 0;
    gameMsgUntil = 0;
}

// ------------------------------------------------------------
//  MERKEZİ DÜŞMAN HASARI (v2.0) — silah ve büyü saldırıları
//  aynı ödül akışını kullanır: altın + mana + XP + drop + başarım.
// ------------------------------------------------------------
inline void hurtEnemy(int ei, int dmg) {
    Enemy &e = enemies[ei];
    float cx = e.tileX * TILE_PX + TILE_PX / 2;
    float cy = e.tileY * TILE_PX + TILE_PX / 2;

    e.hp -= dmg;
    e.hitFlashUntil = millis() + HIT_FLASH_MS;   // Beyaz hasar flaşı (v3.0)
    spawnPopup(e.tileX, e.tileY, dmg, COL_HUD_TEXT);
    particles.emit(cx, cy, e.color, PART_N_HIT);

    if (e.hp <= 0) {
        // Düşman öldü
        e.alive = false;
        killsTotal++;
        floorKills++;
        playSound(NOTE_C5, SND_KILL_MS);         // 523 Hz / 40 ms
        shake.trigger(SHAKE_KILL);
        particles.emit(cx, cy, e.color, PART_N_KILL);
        
        // Combo Sistemi
        uint32_t now = millis();
        if (now - lastKillTimeMs <= 3000) {
            comboCount++;
        } else {
            comboCount = 1;
        }
        lastKillTimeMs = now;

        int totalGold = e.goldReward;
        if (comboCount > 1) {
            totalGold += comboCount * 2; // Kombo bonusu
            char buf[MSG_BUF_LEN];
            snprintf(buf, sizeof(buf), "COMBO x%d! +%dg", comboCount, totalGold);
            showMessage(buf);
        }
        addGold(totalGold);

        player.gainMana(MANA_PER_KILL);
        unlockAchievement(ACH_FIRST_KILL);
        // %25 şansla iksir düşürür (envanter doluysa kaybolur)
        if (random(0, PCT_MAX) < DROP_POTION_PCT) {
            if (comboCount == 1 && inventory.addItem(ITEM_POTION)) showMessage("POTION DROPPED!");
            else inventory.addItem(ITEM_POTION); // Mesajı combo ile çakışmaması için gizle
        }
        player.gainXP(e.xpReward);               // Seviye sesi/mesajı gainXP içinde
    } else {
        playSound(NOTE_E4, SND_HIT_MS);          // 330 Hz / 30 ms
    }
}

// ------------------------------------------------------------
//  BOSS HASARI (v2.0) — faz geçişleri (%60/%30) ve ölüm akışı.
//  (hitX,hitY): popup/parçacığın çıkacağı tile.
// ------------------------------------------------------------
inline void damageBoss(int dmg, int hitX, int hitY) {
    if (!boss.active) return;
    float cx = hitX * TILE_PX + TILE_PX / 2;
    float cy = hitY * TILE_PX + TILE_PX / 2;

    boss.hp -= dmg;
    flashUntil = millis() + HIT_FLASH_MS;        // Beyaz hasar flaşı (v3.0);
                                                 // faz geçişi daha uzun flaşla ezer
    spawnPopup(hitX, hitY, dmg, COL_HUD_TEXT);
    particles.emit(cx, cy, boss.color, PART_N_HIT);

    if (boss.hp <= 0) {
        // Boss öldü — merdiven belirir, fanfar (non-blocking)
        boss.hp = 0;
        boss.active = false;
        killsTotal++;
        floorKills++;
        shake.trigger(SHAKE_DEATH);
        particles.emit(boss.tileX * TILE_PX + TILE_PX,
                       boss.tileY * TILE_PX + TILE_PX,
                       boss.color, PART_N_KILL);
        addGold(GOLD_BOSS);
        player.gainMana(MANA_PER_KILL);
        unlockAchievement(ACH_BOSS);
        player.gainXP(boss.xpReward);
        
        // Merdiveni boss'un tam öldüğü yer yerine odanın merkezine koy
        // Böylece oyuncu saldırı tuşuna basılı tutuyorsa yanlışlıkla hemen bölüme geçmez.
        tiles[MAP_H / 2][MAP_W / 2] = TILE_STAIRS;
        
        startBossWinJingle();                          // 165 Hz ölüm + zafer triadı
        showMessage("STAIRS APPEARED!");
        bossDefeated = true;                           // dungeon.ino state'i çevirir
        return;
    }

    playSound(NOTE_E4, SND_BOSS_HIT_MS);         // 330 Hz / 40 ms

    // Faz geçişi: HP %60 → faz 2, %30 → faz 3 (kızarma + flash + sarsıntı)
    int pct = boss.hp * 100 / boss.maxHp;
    int wantPhase = (pct <= BOSS_PHASE3_PCT) ? 3
                  : (pct <= BOSS_PHASE2_PCT) ? 2 : 1;
    if (wantPhase > boss.phase) {
        boss.phase = wantPhase;
        boss.color = (wantPhase >= 3) ? COL_BOSS_P3 : COL_BOSS_P2;
        boss.specialTimer = millis() + BOSS_SPECIAL_MS / 2;  // İlk özel yakında
        playSound(NOTE_B3, SND_BOSS_PHASE_MS);   // 247 Hz / 80 ms (ağır darbe)
        shake.trigger(SHAKE_KILL);
        flashUntil = millis() + PHASE_FLASH_MS;  // Boss sprite hit-flash (Renderer)
        showMessage("BOSS ENRAGED!");
    }
}

// ------------------------------------------------------------
//  OYUNCU SALDIRISI — baktığı/yürüdüğü yöndeki tile'a
//  Boss/düşman varsa hasar, yoksa boşa sallama.
// ------------------------------------------------------------
inline void playerAttackDir(int dir) {
    player.dir = dir;
    player.lastAttackMs = millis();
    int tx = player.tileX + DIR_DX[dir];
    int ty = player.tileY + DIR_DY[dir];

    spawnSlash(tx, ty, dir);
    shake.trigger(SHAKE_ATTACK);

    if (bossOccupies(tx, ty)) {                  // Boss 2x2 alanına vurma
        damageBoss(calcDamage(player.atk, boss.def), tx, ty);
        return;
    }

    int ei = enemyIndexAt(tx, ty);
    if (ei < 0) {
        playSound(NOTE_A4, SND_ATK_MS);          // Boşa sallama: 440 Hz / 25 ms
        return;
    }
    hurtEnemy(ei, calcDamage(player.atk, enemies[ei].def));
}

// ------------------------------------------------------------
//  DÜŞMAN SALDIRISI — Enemies.h'deki ileri bildirimin tanımı.
//  Bitişik düşman, hamle sırası geldiğinde otomatik vurur.
//  v3.2: oyuncuya doğru atılım animasyonu + savurma izi.
// ------------------------------------------------------------
void enemyAttackPlayer(Enemy &e) {
    int dx = player.tileX - e.tileX, dy = player.tileY - e.tileY;
    e.atkDir = (dx > 0) ? DIR_RIGHT : (dx < 0) ? DIR_LEFT
             : (dy > 0) ? DIR_DOWN  : DIR_UP;
    e.lastAttackMs = millis();
    spawnSlash(player.tileX, player.tileY, e.atkDir);
    damagePlayer(calcDamage(e.atk, player.def));
}

// Boss'un oyuncuya bakan saldırı yönü (2x2 gövdeye dik bitişiklikten)
inline int bossAttackDir() {
    if (player.tileY <  boss.tileY)                   return DIR_UP;
    if (player.tileY >= boss.tileY + BOSS_SIZE_TILES) return DIR_DOWN;
    if (player.tileX <  boss.tileX)                   return DIR_LEFT;
    return DIR_RIGHT;
}

// ------------------------------------------------------------
//  BOSS YAKIN SALDIRISI — Enemies.h'deki ileri bildirimin tanımı.
//  Ejderha faz 3'te çift vurur. v3.2: atılım animasyonu + iz.
// ------------------------------------------------------------
void bossMeleeAttack() {
    boss.atkDir = (int8_t)bossAttackDir();
    boss.lastAttackMs = millis();
    spawnSlash(player.tileX, player.tileY, boss.atkDir);
    int hits = (boss.type == ENEMY_BOSS_DRAGON && boss.phase >= 3) ? 2 : 1;
    for (int h = 0; h < hits; h++) {
        damagePlayer(calcDamage(boss.atk, player.def));
        if (playerDied) return;
    }
}

// ------------------------------------------------------------
//  BOSS AĞIR SALDIRISI (Golem) — v3.2: atılım animasyonu + iz
// ------------------------------------------------------------
void bossHeavyAttack() {
    if (boss.active) {
        boss.atkDir = (int8_t)bossAttackDir();
        boss.lastAttackMs = millis();
        spawnSlash(player.tileX, player.tileY, boss.atkDir);
        damagePlayer(calcDamage(boss.atk * 2, player.def));
    }
}

// ------------------------------------------------------------
//  BOSS ÖZEL SALDIRILARI — Enemies.h'deki ileri bildirimin tanımı.
//  Faz 2'den itibaren BOSS_SPECIAL_MS aralığıyla tetiklenir.
//  Faz 3'te faz-2 ve faz-3 saldırıları dönüşümlü kullanılır.
// ------------------------------------------------------------
void bossDoSpecial() {
    uint32_t now = millis();
    switch (boss.type) {

        // EJDERHA: ateş alanı — oyuncunun 3x3 çevresi işaretlenir,
        // 500 ms sonra alandakine tam ATK hasarı (DEF yok sayılır)
        case ENEMY_BOSS_DRAGON: {
            Telegraph *t = allocTelegraph(TELE_FIRE_MS, COL_HP_LOW,
                                          calcDamage(boss.atk, 0));
            if (t == nullptr) return;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    telegraphAddTile(t, player.tileX + dx, player.tileY + dy);
            playSound(NOTE_A3, SND_SPELL_FIRE_MS);
            break;
        }

        // LICH: faz 2 zehir, faz 3'te zehir/iskelet çağırma dönüşümlü
        case ENEMY_BOSS_LICH: {
            bool doSummon = (boss.phase >= 3) && boss.altSpecial;
            boss.altSpecial = !boss.altSpecial;
            if (doSummon) {
                summonSkeletons(LICH_SUMMON_N);
                playSound(NOTE_B3, SND_BOSS_PHASE_MS);
                showMessage("SKELETONS SUMMONED!");
            } else {
                status.poisonStacks = LICH_POISON_STACKS;
                particles.emit(player.tileX * TILE_PX + TILE_PX / 2,
                               player.tileY * TILE_PX + TILE_PX / 2,
                               COL_POTION, PART_N_HURT);
                playSound(NOTE_G3, SND_HURT_MS);
                showMessage("POISONED!");
            }
            break;
        }

        // GOLEM: faz 2 kaya yağmuru, faz 3'te kaya/stun dönüşümlü
        default: {
            bool doStun = (boss.phase >= 3) && boss.altSpecial;
            boss.altSpecial = !boss.altSpecial;
            if (doStun) {
                status.stunUntil = now + GOLEM_STUN_MS;
                particles.emit(player.tileX * TILE_PX + TILE_PX / 2,
                               player.tileY * TILE_PX + TILE_PX / 2,
                               COL_STAIRS, PART_N_HURT);
                playSound(NOTE_B3, SND_BOSS_PHASE_MS);
                showMessage("STUNNED!");
            } else {
                // Kaya yağmuru: oyuncunun tile'ı + çevresinde 2 rastgele tile
                Telegraph *t = allocTelegraph(TELE_ROCK_MS, COL_STAIRS,
                                              calcDamage(boss.atk, 0));
                if (t == nullptr) return;
                telegraphAddTile(t, player.tileX, player.tileY);
                for (int k = 1; k < ROCK_RAIN_TILES; k++) {
                    telegraphAddTile(t, player.tileX + (int)random(-2, 3),
                                        player.tileY + (int)random(-2, 3));
                }
                playSound(NOTE_A3, SND_SPELL_FIRE_MS);
            }
            break;
        }
    }
}

// ------------------------------------------------------------
//  BÜYÜ UYGULAMASI (v2.0) — Spells.h verisini kullanır.
//  Mana/cooldown/seviye kontrolleri ÇAĞIRANDA yapılır
//  (trySelectedSpell, dungeon.ino); burada hedef doğrulanır ve
//  etki uygulanır. Başarıda mana düşer, cooldown başlar.
// ------------------------------------------------------------
inline bool castSpellAt(int idx, int tx, int ty) {
    Spell &sp = spellbook[idx];
    float pcx = player.tileX * TILE_PX + TILE_PX / 2;
    float pcy = player.tileY * TILE_PX + TILE_PX / 2;

    switch (sp.type) {

        case SPELL_FIREBOLT: {
            int ei = enemyIndexAt(tx, ty);
            if (ei < 0 && !bossOccupies(tx, ty)) {
                showMessage("NO TARGET!");
                return false;
            }
            playSound(NOTE_A3, SND_SPELL_FIRE_MS);       // 220 Hz / 50 ms
            particles.emit(tx * TILE_PX + TILE_PX / 2,
                           ty * TILE_PX + TILE_PX / 2,
                           COL_POWER, PART_N_KILL);      // Patlama
            if (ei >= 0) hurtEnemy(ei, FIREBOLT_DMG);
            else         damageBoss(FIREBOLT_DMG, tx, ty);
            break;
        }

        case SPELL_FROST: {
            playSound(NOTE_C4, SND_SPELL_FROST_MS);      // 262 Hz / 60 ms
            // Buz kristali görseli: 3x3 mavi işaret (hasarsız telegraf)
            Telegraph *vis = allocTelegraph(FROST_VISUAL_MS, COL_SHIELD, 0);
            bool bossHit = false;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int x = player.tileX + dx, y = player.tileY + dy;
                    telegraphAddTile(vis, x, y);
                    int ei = enemyIndexAt(x, y);
                    if (ei >= 0) {
                        enemies[ei].slowedUntil = millis() + FROST_SLOW_MS;
                        hurtEnemy(ei, FROST_DMG);
                    }
                    if (bossOccupies(x, y) && !bossHit) {
                        bossHit = true;                  // Boss tek kez hasar alır
                        damageBoss(FROST_DMG, x, y);
                    }
                }
            }
            break;
        }

        case SPELL_HEAL:
            if (player.hp >= player.maxHp) {
                showMessage("HP ALREADY FULL!");
                return false;
            }
            player.heal(HEAL_AMOUNT);
            playSound(NOTE_E5, SND_SPELL_HEAL_MS);       // 659 Hz / 40 ms
            particles.emit(pcx, pcy, COL_POTION, PART_N_ITEM);
            showMessage("HP +10!");
            break;

        case SPELL_TELEPORT: {
            bool ok = inMap(tx, ty) &&
                      fogMap[ty][tx] == FOG_VIS &&
                      canPlayerWalk(tx, ty) &&
                      tileAt(tx, ty) != TILE_STAIRS &&   // Kat geçişi yürüyerek yapılır
                      enemyIndexAt(tx, ty) < 0 &&
                      !bossOccupies(tx, ty) &&
                      !(merchant.active && merchant.tileX == tx && merchant.tileY == ty) &&
                      abs(tx - player.tileX) <= SPELL_RANGE &&
                      abs(ty - player.tileY) <= SPELL_RANGE;
            if (!ok) {
                showMessage("CANNOT TELEPORT THERE!");
                return false;
            }
            particles.emit(pcx, pcy, COL_PLAYER, PART_N_ITEM);   // Kaybolma
            player.tileX = tx;
            player.tileY = ty;
            player.prevTileX = tx;               // Işınlanma: kayma animasyonu yok (v3.0)
            player.prevTileY = ty;
            player.dir = DIR_DOWN;
            updateFog(tx, ty);
            particles.emit(tx * TILE_PX + TILE_PX / 2,
                           ty * TILE_PX + TILE_PX / 2,
                           COL_PLAYER, PART_N_ITEM);             // Belirme
            playSound(NOTE_C5, SND_SPELL_TP_MS);         // 523 Hz / 30 ms
            break;
        }

        case SPELL_SHIELD:
            status.shieldUntil = millis() + SHIELD_MS;
            playSound(NOTE_G4, SND_SPELL_SHIELD_MS);     // 392 Hz / 40 ms
            particles.emit(pcx, pcy, COL_SHIELD, PART_N_ITEM);
            showMessage("SHIELD ACTIVE!");
            break;

        default:
            return false;
    }

    // Başarılı büyü: mana düşer, cooldown başlar, başarım sayacı
    player.mana -= sp.manaCost;
    sp.cooldownLeft = sp.cooldown;
    spellsCast++;
    if (spellsCast >= ACH_MAGE_CASTS) unlockAchievement(ACH_MAGE);
    return true;
}
