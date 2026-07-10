#pragma once
// ============================================================
//  dungeon/Player.h — Oyuncu Sınıfı
//
//  Sorumluluk: Oyuncu verisi (pozisyon, can, saldırı, savunma),
//  XP/seviye sistemi ve seviye atlama bonusları.
//  Grid hareketi ve girdi işleme dungeon.ino'da yapılır
//  (harita/düşman bağımlılığı gerektirdiği için).
// ============================================================

#include "Config.h"

// Seviye atlama halka efekti başlangıcı (v3.0) — Renderer okur
uint32_t levelUpFxStart = 0;

struct Player {
    int tileX, tileY;     // Grid pozisyonu (tile koordinatı)
    int prevTileX, prevTileY; // Önceki tile (v3.0 kayma animasyonu)
    int hp, maxHp;        // Can / maksimum can
    int atk;              // Saldırı gücü
    int def;              // Savunma
    int lvl;              // Seviye
    int xp;               // Mevcut deneyim puanı
    int xpToNext;         // Sonraki seviye için gereken XP
    int dir;              // Son bakılan yön (DIR_UP/RIGHT/DOWN/LEFT)
    int keys;             // Envanterdeki anahtar sayısı (HUD/kapı için ayna)
    int mana, maxMana;    // Büyü kaynağı (v2.0)
    int gold;             // Altın (tüccar ekonomisi, v2.0)
    uint32_t lastMoveMs;  // Son hareket zamanı (v3.0 animasyon)
    uint32_t lastAttackMs;// Son saldırı zamanı (v3.0 kılıç savurma)

    // Yeni oyun başlangıç değerleri
    void init() {
        tileX = 0; tileY = 0;
        prevTileX = 0; prevTileY = 0;
        maxHp = PLAYER_START_HP;
        hp    = maxHp;
        atk   = PLAYER_START_ATK;
        def   = PLAYER_START_DEF;
        lvl   = 1;
        xp    = 0;
        xpToNext = XP_BASE + lvl * XP_PER_LVL;
        dir   = DIR_DOWN;
        keys  = 0;
        maxMana = MANA_START;
        mana    = maxMana;
        gold    = 0;
        lastMoveMs = 0;
        lastAttackMs = 0;
    }

    // Mana kazan (üst sınır maxMana)
    void gainMana(int amount) {
        mana += amount;
        if (mana > maxMana) mana = maxMana;
    }

    // Kat başında pozisyonu ayarla (kayma animasyonu olmadan ışınla)
    void setPosition(int x, int y) {
        tileX = x;
        tileY = y;
        prevTileX = x;
        prevTileY = y;
        lastMoveMs = 0;       // Lerp tamamlanmış sayılır
    }

    // Can yenile (üst sınır maxHp)
    void heal(int amount) {
        hp += amount;
        if (hp > maxHp) hp = maxHp;
    }

    // XP kazan; seviye atlandıysa true döner.
    // Seviye bonusları: maxHp +3, atk +1, her 3 seviyede def +1.
    // Seviye atlayınca can tamamen yenilenir.
    bool gainXP(int amount) {
        xp += amount;
        bool leveled = false;
        while (xp >= xpToNext) {
            xp -= xpToNext;
            lvl++;
            maxHp += LVL_HP_BONUS;
            atk   += LVL_ATK_BONUS;
            if (lvl % LVL_DEF_EVERY == 0) def++;
            hp = maxHp;                             // Tam iyileşme
            maxMana += MANA_PER_LVL;
            mana = maxMana;                         // Mana da tam dolar
            xpToNext = XP_BASE + lvl * XP_PER_LVL;
            leveled = true;
        }
        if (leveled) {
            playSound(NOTE_G5, SND_LEVELUP_MS);     // 784 Hz / 40 ms (tavan nota)
            showMessage("LEVEL UP!");
            levelUpFxStart = millis();              // Genişleyen halka efekti (v3.0)
        }
        return leveled;
    }
};

// Global oyuncu nesnesi (tek TU: tüm başlıklar dungeon.ino'ya dahil edilir)
Player player;

// ------------------------------------------------------------
//  Durum efektleri (v2.0) — zehir/stun/kalkan/yavaşlatma
//  Zehir stack tabanlı (1 sn'de 1 hasar), diğerleri millis() süreli.
// ------------------------------------------------------------
struct StatusEffects {
    int poisonStacks;      // Her durum tick'inde 1 hasar, sonra stacks--
    uint32_t stunUntil;    // Bu zamana kadar hareket/saldırı yok
    uint32_t shieldUntil;  // Bu zamana kadar hasar almaz
    uint32_t slowUntil;    // Bu zamana kadar hareket yarı hız

    void clear() {
        poisonStacks = 0;
        stunUntil = shieldUntil = slowUntil = 0;
    }
    bool stunned()  const { return millis() < stunUntil; }
    bool shielded() const { return millis() < shieldUntil; }
    bool slowed()   const { return millis() < slowUntil; }
};

StatusEffects status;
