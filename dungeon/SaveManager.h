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

inline void saveGameState(int floorNum, int killsTotal) {
    Preferences prefs;
    prefs.begin("dun_save", false);
    prefs.putBool("has_save", true);
    prefs.putInt("ver", DUN_SAVE_VERSION);
    prefs.putInt("floor", floorNum);
    prefs.putInt("kills", killsTotal);
    prefs.putInt("spellsCast", spellsCast);
    prefs.putBytes("player", &player, sizeof(Player));
    prefs.putBytes("inv", &inventory, sizeof(Inventory));
    prefs.end();
}

inline bool hasSaveGame() {
    Preferences prefs;
    prefs.begin("dun_save", true);
    bool has = prefs.getBool("has_save", false) &&
               prefs.getInt("ver", 0) == DUN_SAVE_VERSION;
    prefs.end();
    return has;
}

inline void loadGameState(int& floorNum, int& killsTotal) {
    Preferences prefs;
    prefs.begin("dun_save", true);
    floorNum = prefs.getInt("floor", 1);
    killsTotal = prefs.getInt("kills", 0);
    spellsCast = prefs.getInt("spellsCast", 0);
    prefs.getBytes("player", &player, sizeof(Player));
    prefs.getBytes("inv", &inventory, sizeof(Inventory));
    prefs.end();
    
    // Sync keys with HUD
    syncPlayerKeys();
}

inline void clearSaveGame() {
    Preferences prefs;
    prefs.begin("dun_save", false);
    prefs.putBool("has_save", false);
    prefs.end();
}
