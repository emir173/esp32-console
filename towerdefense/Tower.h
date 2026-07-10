#pragma once
// ============================================================
//  towerdefense/Tower.h — Kule Havuzu, Yerleştirme ve Yükseltme
//
//  Sorumluluk: Kule yapısı, sabit havuz (16 slot), tile sorguları,
//  yerleştirme/yükseltme mantığı ve ekonomi durumu (para, kale canı).
//  Hedef seçimi ve ateş etme Combat.h'dedir (düşman bilgisi gerekir).
//  (Dungeon/Enemies.h sabit havuz + alive flag pattern'i referans.)
// ============================================================

#include "Config.h"
#include "Map.h"

// ------------------------------------------------------------
//  Ekonomi ve kale durumu (oyun genelinde paylaşılan)
// ------------------------------------------------------------
int money       = START_MONEY;   // Mevcut para
int baseHp      = BASE_HP_MAX;   // Kale canı
int towersBuilt = 0;             // Bu oyunda inşa edilen kule sayısı (skor paneli)

// ------------------------------------------------------------
//  Kule yapısı
// ------------------------------------------------------------
struct Tower {
    bool active;
    TowerType type;
    int tileX, tileY;
    int level;            // 1..TOWER_MAX_LEVEL
    int damage;           // Seviyeyle artar (x1.5 yuvarlanmış)
    int range;            // Menzil (tile), seviyeyle +1
    uint16_t intervalMs;  // Atış aralığı
    float cooldown;       // Kalan bekleme (sn) — tick bazlı, pause güvenli
    float aimX, aimY;     // Son hedef merkezi (px) — cannon namlusu için
};

Tower towers[MAX_TOWERS];

// ------------------------------------------------------------
//  Sorgular
// ------------------------------------------------------------
// (x,y) tile'ındaki aktif kulenin indeksi, yoksa -1
inline int towerIndexAt(int x, int y) {
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (towers[i].active && towers[i].tileX == x && towers[i].tileY == y)
            return i;
    }
    return -1;
}

// Aktif kule sayısı
inline int towerCount() {
    int n = 0;
    for (int i = 0; i < MAX_TOWERS; i++)
        if (towers[i].active) n++;
    return n;
}

// Kule tile merkezinin piksel koordinatı (oyun alanı uzayı)
inline float towerCenterX(const Tower &t) { return t.tileX * TILE_PX + TILE_PX / 2.0f; }
inline float towerCenterY(const Tower &t) { return t.tileY * TILE_PX + TILE_PX / 2.0f; }

// ------------------------------------------------------------
//  Havuzu temizle (yeni oyun)
// ------------------------------------------------------------
inline void clearTowers() {
    for (int i = 0; i < MAX_TOWERS; i++) towers[i].active = false;
}

// ------------------------------------------------------------
//  Yerleştirme — para ve tile kontrolü çağıran tarafta yapılır.
//  Başarılıysa true döner, parayı düşer, tile'ı TILE_TOWER yapar.
// ------------------------------------------------------------
inline bool placeTower(TowerType type, int x, int y) {
    if (!canBuildTower(x, y)) return false;
    if (money < TOWER_COST[type]) return false;
    for (int i = 0; i < MAX_TOWERS; i++) {
        Tower &t = towers[i];
        if (t.active) continue;
        t.active     = true;
        t.type       = type;
        t.tileX      = x;
        t.tileY      = y;
        t.level      = 1;
        t.damage     = TOWER_DMG[type];
        t.range      = TOWER_RANGE[type];
        t.intervalMs = TOWER_INT_MS[type];
        t.cooldown   = 0.0f;
        t.aimX       = towerCenterX(t) + TILE_PX;   // Varsayılan: sağa bakar
        t.aimY       = towerCenterY(t);
        money -= TOWER_COST[type];
        towersBuilt++;
        tiles[y][x] = TILE_TOWER;
        return true;
    }
    return false;   // Havuz dolu
}

// ------------------------------------------------------------
//  Yükseltme — maliyet = orijinal maliyet x mevcut seviye.
//  Seviye başına: hasar x1.5 (yuvarlanmış), menzil +1 (maks 5).
// ------------------------------------------------------------
inline int upgradeCost(const Tower &t) {
    return TOWER_COST[t.type] * t.level;
}

inline bool canUpgrade(const Tower &t) {
    return t.level < TOWER_MAX_LEVEL && money >= upgradeCost(t);
}

// Başarılıysa true döner ve parayı düşer
inline bool applyUpgrade(Tower &t) {
    if (t.level >= TOWER_MAX_LEVEL) return false;
    int cost = upgradeCost(t);
    if (money < cost) return false;
    money -= cost;
    t.level++;
    t.damage = (t.damage * 3 + 1) / 2;              // x1.5 yuvarlanmış
    if (t.range < TOWER_MAX_RANGE) t.range++;
    return true;
}

// ------------------------------------------------------------
//  Satış — kuleye yatırılan toplam paranın SELL_REFUND_PCT %'si.
//  Yatırım = temel maliyet + ödenen tüm yükseltme maliyetleri
//  (yükseltme L→L+1 maliyeti = temel × L).
// ------------------------------------------------------------
inline int towerInvested(const Tower &t) {
    int base = TOWER_COST[t.type];
    int total = base;                               // İlk yerleştirme
    for (int l = 1; l < t.level; l++) total += base * l;   // Yükseltmeler
    return total;
}

inline int sellValue(const Tower &t) {
    return (towerInvested(t) * SELL_REFUND_PCT) / 100;
}

// Kuleyi sat: parayı iade et, tile'ı çimene döndür, slotu boşalt.
inline int sellTower(Tower &t) {
    int refund = sellValue(t);
    money += refund;
    tiles[t.tileY][t.tileX] = TILE_GRASS;
    t.active = false;
    return refund;
}
