#pragma once
// ============================================================
//  dungeon/Items.h — Eşya Tanımları ve Envanter Sistemi
//
//  Sorumluluk: Eşya türleri, envanter yapısı (8 slot), sandık
//  içerik zarı (ağırlıklı) ve eşya kullanım etkileri.
//  Sandık açma ve düşman drop akışı dungeon.ino/Combat.h'den
//  bu API üzerinden yürütülür.
// ============================================================

#include "Config.h"
#include "Player.h"
#include "Map.h"      // keyGuaranteePending (softlock koruması) için

// ------------------------------------------------------------
//  Eşya türleri
// ------------------------------------------------------------
enum ItemType : uint8_t {
    ITEM_NONE = 0,
    ITEM_POTION,      // Can İksiri: HP +8 yenile (yeşil)
    ITEM_POWER_GEM,   // Güç Taşı: ATK +1 kalıcı (kırmızı)
    ITEM_SHIELD,      // Kalkan Parçası: DEF +1 kalıcı (mavi)
    ITEM_KEY,         // Anahtar: kilitli kapı açar (sarı)
    ITEM_MAX_HP_UP    // Can Artışı: maxHp +5 kalıcı (pembe)
};

// Eşya adı (envanter listesi için, ASCII)
inline const char *itemName(ItemType t) {
    switch (t) {
        case ITEM_POTION:    return "Health Potion";
        case ITEM_POWER_GEM: return "Power Gem";
        case ITEM_SHIELD:    return "Shield Piece";
        case ITEM_KEY:       return "Key";
        case ITEM_MAX_HP_UP: return "Max HP Up";
        default:             return "(empty)";
    }
}

// Eşya ikon/parçacık rengi
inline uint16_t itemColor(ItemType t) {
    switch (t) {
        case ITEM_POTION:    return COL_POTION;
        case ITEM_POWER_GEM: return COL_POWER;
        case ITEM_SHIELD:    return COL_SHIELD;
        case ITEM_KEY:       return COL_KEY;
        case ITEM_MAX_HP_UP: return COL_MAXHP;
        default:             return COL_HUD_TEXT;
    }
}

// ------------------------------------------------------------
//  Envanter (sabit 8 slot, malloc yok)
// ------------------------------------------------------------
struct Inventory {
    ItemType slots[INV_SLOTS];
    int count;      // Dolu slot sayısı
    int cursor;     // Envanter ekranında seçili slot

    void init() {
        for (int i = 0; i < INV_SLOTS; i++) slots[i] = ITEM_NONE;
        count = 0;
        cursor = 0;
    }

    bool isFull() const { return count >= INV_SLOTS; }

    // İlk boş slota ekle; envanter doluysa false
    bool addItem(ItemType item) {
        if (item == ITEM_NONE || isFull()) return false;
        for (int i = 0; i < INV_SLOTS; i++) {
            if (slots[i] == ITEM_NONE) {
                slots[i] = item;
                count++;
                return true;
            }
        }
        return false;
    }

    // Kullanılan eşyayı sil
    void removeAt(int index) {
        if (index < 0 || index >= INV_SLOTS || slots[index] == ITEM_NONE) return;
        slots[index] = ITEM_NONE;
        count--;
    }

    // Belirli türden kaç adet var
    int countOf(ItemType t) const {
        int n = 0;
        for (int i = 0; i < INV_SLOTS; i++)
            if (slots[i] == t) n++;
        return n;
    }

    // Belirli türden ilk slotun indeksi (-1 = yok)
    int firstOf(ItemType t) const {
        for (int i = 0; i < INV_SLOTS; i++)
            if (slots[i] == t) return i;
        return -1;
    }
};

// Global envanter nesnesi
Inventory inventory;

// ------------------------------------------------------------
//  TÜCCAR NPC (v2.0) — kat 2'den itibaren %40 şansla belirir.
//  Tile'ına yürüyünce ticaret menüsü açılır; katta 1 kez konuşulur.
// ------------------------------------------------------------
struct Merchant {
    bool active;
    int tileX, tileY;
    bool talkedThisFloor;   // Bu katta ticaret yapıldı mı
};
Merchant merchant = { false, 0, 0, false };

int merchCursor = 0;        // Ticaret menüsü imleci (0-3 al, 4-7 sat)

// Ticaret menüsündeki eşya sırası (al ve sat bölümlerinde aynı)
constexpr ItemType TRADE_ITEMS[4] = {
    ITEM_POTION, ITEM_POWER_GEM, ITEM_SHIELD, ITEM_KEY
};

// Satın alma fiyatı (altın)
inline int buyPriceOf(ItemType t) {
    switch (t) {
        case ITEM_POTION:    return BUY_POTION;
        case ITEM_POWER_GEM: return BUY_POWER;
        case ITEM_SHIELD:    return BUY_SHIELD;
        case ITEM_KEY:       return BUY_KEY;
        default:             return 0;
    }
}

// Satış fiyatı (altın)
inline int sellPriceOf(ItemType t) {
    switch (t) {
        case ITEM_POTION:    return SELL_POTION;
        case ITEM_POWER_GEM: return SELL_POWER;
        case ITEM_SHIELD:    return SELL_SHIELD;
        case ITEM_KEY:       return SELL_KEY;
        default:             return 0;
    }
}

// player.keys aynasını envanterle senkronla (HUD ve kapı kontrolü için)
inline void syncPlayerKeys() {
    player.keys = inventory.countOf(ITEM_KEY);
}

// ------------------------------------------------------------
//  Sandık içerik zarı (ağırlıklı):
//  %40 iksir, %20 güç, %15 kalkan, %15 anahtar, %10 can artışı.
//  Softlock koruması: katta kilitli kapı varsa ve oyuncuda hiç
//  anahtar yoksa İLK sandık garantili anahtar verir.
// ------------------------------------------------------------
inline ItemType rollChestItem() {
    if (keyGuaranteePending && inventory.countOf(ITEM_KEY) == 0) {
        keyGuaranteePending = false;
        return ITEM_KEY;
    }
    int r = random(0, PCT_MAX);
    if (r < CHEST_POTION_TH) return ITEM_POTION;
    if (r < CHEST_POWER_TH)  return ITEM_POWER_GEM;
    if (r < CHEST_SHIELD_TH) return ITEM_SHIELD;
    if (r < CHEST_KEY_TH)    return ITEM_KEY;
    return ITEM_MAX_HP_UP;
}

// ------------------------------------------------------------
//  Envanterden eşya kullan. Eşya TÜKETİLDİYSE true döner
//  (çağıran taraf removeAt + parçacık efekti tetikler).
// ------------------------------------------------------------
inline bool applyItemEffect(ItemType t) {
    switch (t) {
        case ITEM_POTION:
            if (player.hp >= player.maxHp) {
                showMessage("HP ALREADY FULL!");
                return false;
            }
            player.heal(POTION_HEAL);
            playSound(NOTE_C5, SND_USE_MS);
            showMessage("HP +8!");
            return true;

        case ITEM_POWER_GEM:
            player.atk += 1;
            playSound(NOTE_C5, SND_USE_MS);
            showMessage("ATTACK +1!");
            return true;

        case ITEM_SHIELD:
            player.def += 1;
            playSound(NOTE_C5, SND_USE_MS);
            showMessage("DEFENSE +1!");
            return true;

        case ITEM_KEY:
            // Anahtar menüden kullanılmaz; kilitli kapıda otomatik harcanır
            showMessage("WALK TO LOCKED DOOR");
            return false;

        case ITEM_MAX_HP_UP:
            player.maxHp += MAXHP_BONUS;
            player.hp = player.maxHp;
            playSound(NOTE_C5, SND_USE_MS);
            showMessage("MAX HP +5!");
            return true;

        default:
            return false;
    }
}
