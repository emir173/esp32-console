#pragma once
// ============================================================
//  dungeon/SaveManager.h — NVS Checkpoint Sistemi
//
//  Sorumluluk: Kat geçişlerinde oyuncu ve envanter verisini kaydetmek,
//  ana menüde "Continue" seçeneğini aktif etmek.
// ============================================================

#include <Preferences.h>
#include "Config.h"
#include "Player.h"
#include "Items.h"
#include "Spells.h"

// Kayıt sürümü — Player struct'ının BİNARY düzeni değişince arttır!
// (Blob ham byte kaydedilir; eski blob yeni struct'a yüklenirse alanlar
// kayar ve veri bozulur. Sürüm tutmayan kayıt yok sayılır.)
// v3: prevTileX/prevTileY alanları eklendi (kayma animasyonu).
constexpr int DUN_SAVE_VERSION = 3;

inline bool saveGameState(int floorNum, int killsTotal) {
    Preferences prefs;
    if (!prefs.begin("dun_save", false)) return false;
    // Her alan tamamlanmadan Continue görünmesin (yarım yazma / güç kesintisi).
    bool ok = prefs.putBool("has_save", false) == sizeof(bool);
    ok = ok && prefs.putInt("ver", DUN_SAVE_VERSION) == sizeof(int32_t);
    ok = ok && prefs.putInt("floor", floorNum) == sizeof(int32_t);
    ok = ok && prefs.putInt("kills", killsTotal) == sizeof(int32_t);
    ok = ok && prefs.putInt("spellsCast", spellsCast) == sizeof(int32_t);
    ok = ok && prefs.putBytes("player", &player, sizeof(Player)) == sizeof(Player);
    ok = ok && prefs.putBytes("inv", &inventory, sizeof(Inventory)) == sizeof(Inventory);
    if (ok) ok = prefs.putBool("has_save", true) == sizeof(bool);
    prefs.end();
    return ok;
}

// Geçici nesnelere oku; bozuk kayıt çalışan oyunun verisini değiştirmesin.
inline bool readSaveGame(Player &savedPlayer, Inventory &savedInventory,
                         int &savedFloor, int &savedKills, int &savedCasts) {
    Preferences prefs;
    if (!prefs.begin("dun_save", true)) return false;
    bool ok = prefs.getBool("has_save", false) &&
              prefs.getInt("ver", 0) == DUN_SAVE_VERSION &&
              prefs.getBytesLength("player") == sizeof(Player) &&
              prefs.getBytesLength("inv") == sizeof(Inventory);
    if (ok) {
        savedFloor = prefs.getInt("floor", 0);
        savedKills = prefs.getInt("kills", -1);
        savedCasts = prefs.getInt("spellsCast", -1);
        ok = prefs.getBytes("player", &savedPlayer, sizeof(Player)) == sizeof(Player) &&
             prefs.getBytes("inv", &savedInventory, sizeof(Inventory)) == sizeof(Inventory);
    }
    prefs.end();
    if (!ok) return false;

    const Player &p = savedPlayer;
    if (savedFloor < 1 || savedKills < 0 || savedCasts < 0 ||
        p.maxHp < 1 || p.hp < 1 || p.hp > p.maxHp ||
        p.atk < 1 || p.def < 0 || p.lvl < 1 || p.xp < 0 || p.xpToNext < 1 ||
        p.dir < DIR_UP || p.dir > DIR_LEFT || p.maxMana < 1 ||
        p.mana < 0 || p.mana > p.maxMana || p.gold < 0) return false;

    int count = 0;
    for (int i = 0; i < INV_SLOTS; i++) {
        if (savedInventory.slots[i] > ITEM_MAX_HP_UP) return false;
        if (savedInventory.slots[i] != ITEM_NONE) count++;
    }
    return savedInventory.count == count &&
           savedInventory.cursor >= 0 && savedInventory.cursor < INV_SLOTS;
}

inline bool hasSaveGame() {
    Player savedPlayer;
    Inventory savedInventory;
    int savedFloor, savedKills, savedCasts;
    return readSaveGame(savedPlayer, savedInventory, savedFloor, savedKills, savedCasts);
}

inline bool loadGameState(int& floorNum, int& killsTotal) {
    Player savedPlayer;
    Inventory savedInventory;
    int savedFloor, savedKills, savedCasts;
    if (!readSaveGame(savedPlayer, savedInventory, savedFloor, savedKills, savedCasts))
        return false;
    player = savedPlayer;
    inventory = savedInventory;
    floorNum = savedFloor;
    killsTotal = savedKills;
    initSpells();  // Soğuk açılışta büyü tanımları da hazırlanmalı.
    spellsCast = savedCasts;
    syncPlayerKeys();
    return true;
}

inline void clearSaveGame() {
    Preferences prefs;
    prefs.begin("dun_save", false);
    prefs.putBool("has_save", false);
    prefs.end();
}
