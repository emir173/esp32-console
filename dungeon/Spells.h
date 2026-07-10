#pragma once
// ============================================================
//  dungeon/Spells.h — Büyü Kitabı Verisi (v2.0)
//
//  Sorumluluk: Büyü tanımları (mana/cooldown/seviye kilidi),
//  büyü kitabı durumu, cooldown tick'i ve seçim/hedefleme
//  değişkenleri. Büyülerin ETKİ uygulaması Combat.h'dedir
//  (castSpellAt) — parçacık/hasar sistemlerine ihtiyaç duyar.
//  Cooldown 60 Hz tick bazlıdır (CD_TICKS_PER_S = 1 sn).
// ============================================================

#include "Config.h"
#include "Player.h"

// ------------------------------------------------------------
//  Büyü türleri
// ------------------------------------------------------------
enum SpellType : uint8_t {
    SPELL_NONE = 0,
    SPELL_FIREBOLT,   // Tek hedefe 8 hasar (imleçle hedef seç)
    SPELL_FROST,      // Oyuncu çevresi 3x3 alan hasarı (4) + yavaşlatma
    SPELL_HEAL,       // Can +10
    SPELL_TELEPORT,   // Görünür boş tile'a ışınlanma (imleçle hedef seç)
    SPELL_SHIELD      // 3 sn hasar bağışıklığı
};

struct Spell {
    SpellType type;
    int manaCost;
    int cooldown;       // 60 Hz tick cinsinden (sn * CD_TICKS_PER_S)
    int cooldownLeft;   // Kalan cooldown (tick)
    int unlockLvl;      // Bu seviyeden itibaren kullanılabilir
};

Spell spellbook[MAX_SPELLS];
int  selectedSpell  = 0;        // Büyü menüsü imleci
int  spellsCast     = 0;        // Bu oyunda kullanılan büyü ("Büyü Ustası")
bool spellTargeting = false;    // İmleç modu aktif mi (FIREBOLT/TELEPORT)
int  targetX = 0, targetY = 0;  // Hedef imleci (tile)

// ------------------------------------------------------------
//  Kurulum — yeni oyunda çağrılır
// ------------------------------------------------------------
inline void initSpells() {
    spellbook[0] = { SPELL_FIREBOLT, FIREBOLT_MANA, FIREBOLT_CD_S * CD_TICKS_PER_S, 0, FIREBOLT_LVL };
    spellbook[1] = { SPELL_FROST,    FROST_MANA,    FROST_CD_S    * CD_TICKS_PER_S, 0, FROST_LVL    };
    spellbook[2] = { SPELL_HEAL,     HEAL_MANA,     HEAL_CD_S     * CD_TICKS_PER_S, 0, HEAL_LVL     };
    spellbook[3] = { SPELL_TELEPORT, TP_MANA,       TP_CD_S       * CD_TICKS_PER_S, 0, TP_LVL       };
    spellbook[4] = { SPELL_SHIELD,   SHIELD_MANA,   SHIELD_CD_S   * CD_TICKS_PER_S, 0, SHIELD_LVL   };
    selectedSpell  = 0;
    spellsCast     = 0;
    spellTargeting = false;
}

// ------------------------------------------------------------
//  Sorgular
// ------------------------------------------------------------
inline const char *spellName(SpellType t) {
    switch (t) {
        case SPELL_FIREBOLT: return "Firebolt";
        case SPELL_FROST:    return "Frost Wave";
        case SPELL_HEAL:     return "Heal";
        case SPELL_TELEPORT: return "Teleport";
        case SPELL_SHIELD:   return "Shield";
        default:             return "(none)";
    }
}

inline uint16_t spellColor(SpellType t) {
    switch (t) {
        case SPELL_FIREBOLT: return COL_POWER;    // Kırmızı
        case SPELL_FROST:    return COL_SHIELD;   // Mavi
        case SPELL_HEAL:     return COL_POTION;   // Yeşil
        case SPELL_TELEPORT: return COL_PLAYER;   // Cyan
        case SPELL_SHIELD:   return COL_MANA;     // Açık mavi
        default:             return COL_HUD_TEXT;
    }
}

// Bu büyü imleçle hedef seçtirir mi
inline bool spellNeedsTarget(SpellType t) {
    return t == SPELL_FIREBOLT || t == SPELL_TELEPORT;
}

// Kalan cooldown saniyesi (yukarı yuvarlanmış, UI için)
inline int spellCooldownSec(const Spell &sp) {
    return (sp.cooldownLeft + CD_TICKS_PER_S - 1) / CD_TICKS_PER_S;
}

// ------------------------------------------------------------
//  Cooldown tick'i — 60 Hz sabit tick döngüsünde çağrılır
// ------------------------------------------------------------
inline void tickSpellCooldowns() {
    for (int i = 0; i < MAX_SPELLS; i++) {
        if (spellbook[i].cooldownLeft > 0) spellbook[i].cooldownLeft--;
    }
}
