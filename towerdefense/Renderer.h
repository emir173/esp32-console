#pragma once
// ============================================================
//  towerdefense/Renderer.h — Tüm TFT_eSprite Çizim Fonksiyonları
//
//  Sorumluluk: Harita/tile çizimi (orman paleti, kenar şeritli
//  toprak yol), bayraklı taş kale, mağara spawn, 5 kule ve
//  5 düşman sprite'ı (animasyonlu), mermi/çizgi/halka efektleri,
//  HUD (kale canı + para + bölüm-dalga sayacı + yıldırım ikonu),
//  imleç + menzil halkası, kule seçim paneli ve menü / pause /
//  game over / dalga temiz / bölüm geçiş / zafer ekranları.
// ============================================================

#include "Config.h"
#include "Map.h"
#include "Tower.h"
#include "Enemies.h"
#include "Wave.h"
#include "Combat.h"

// ------------------------------------------------------------
//  Metin yardımcıları (varsayılan font: 6x8 px / karakter)
// ------------------------------------------------------------
constexpr int FONT_W = 6;    // Boyut 1 karakter genişliği
constexpr int FONT_H = 8;    // Boyut 1 karakter yüksekliği

// Kule menüsü etiketleri (indeks = TowerType)
constexpr const char *TOWER_NAME[TOWER_TYPE_COUNT] =
    { "ARCHER", "CANNON", "FROST", "LASER", "TOXIC" };

// Aktif biyom (0..2) — zemin/yol/kaya paleti bundan seçilir.
// drawMapAll ve drawLevelSelect çizimden önce günceller.
int gBiome = 0;

// Yatay ortalanmış metin yaz (Dungeon/Renderer.h ile aynı)
inline void drawCentered(TFT_eSprite &cv, const char *txt, int y,
                         uint16_t color, int size = 1) {
    int w = strlen(txt) * FONT_W * size;
    cv.setTextSize(size);
    cv.setTextColor(color);
    cv.setCursor((SCR_W - w) / 2, y);
    cv.print(txt);
}

// ------------------------------------------------------------
//  TILE YARDIMCILARI
// ------------------------------------------------------------
// Yol ailesi mi? (kenar şeridi çizmemek için komşu kontrolü)
inline bool isPathLike(uint8_t t) {
    return t == TILE_PATH || t == TILE_SPAWN || t == TILE_BASE;
}

// Çimen zemin: dama + deterministik seyrek ot/çiçek serpiştirmesi
inline void drawGrassBase(TFT_eSprite &cv, int sx, int sy, int mx, int my) {
    bool alt = (mx + my) & 1;
    cv.fillRect(sx, sy, TILE_PX, TILE_PX, alt ? BIO_GRASS_ALT[gBiome] : BIO_GRASS[gBiome]);

    // Tile koordinatından sabit "rastgele" desen (her frame aynı, seyrek)
    uint8_t h = (uint8_t)((mx * 31 + my * 17) & 31);
    if (h == 0) {                                   // Ot/kar/köz kümesi (V şekli)
        cv.drawPixel(sx + 2, sy + 5, BIO_GRASS_DEC[gBiome]);
        cv.drawPixel(sx + 3, sy + 4, BIO_GRASS_LT[gBiome]);
        cv.drawPixel(sx + 4, sy + 5, BIO_GRASS_DEC[gBiome]);
    } else if (h == 11) {                           // Tek koyu detay
        cv.drawPixel(sx + 5, sy + 2, BIO_GRASS_DEC[gBiome]);
        cv.drawPixel(sx + 5, sy + 3, BIO_GRASS_DEC[gBiome]);
    } else if (h == 22) {                           // Çiçek / kar parıltısı / köz
        cv.drawPixel(sx + 3, sy + 3, BIO_FLOWER[gBiome]);
        cv.drawPixel(sx + 3, sy + 4, BIO_GRASS_DEC[gBiome]);
    }
}

// Yol zemini: dama + kenar şeritleri + benek (biyoma göre toprak/buz/lav)
inline void drawPathBase(TFT_eSprite &cv, int sx, int sy, int mx, int my) {
    bool alt = (mx + my) & 1;
    cv.fillRect(sx, sy, TILE_PX, TILE_PX, alt ? BIO_PATH_ALT[gBiome] : BIO_PATH[gBiome]);

    // Yolun çimene değen kenarlarına koyu şerit (yolu belirginleştirir)
    uint16_t edge = BIO_PATH_EDGE[gBiome];
    if (!isPathLike(tileAt(mx, my - 1))) cv.drawFastHLine(sx, sy, TILE_PX, edge);
    if (!isPathLike(tileAt(mx, my + 1))) cv.drawFastHLine(sx, sy + TILE_PX - 1, TILE_PX, edge);
    if (!isPathLike(tileAt(mx - 1, my))) cv.drawFastVLine(sx, sy, TILE_PX, edge);
    if (!isPathLike(tileAt(mx + 1, my))) cv.drawFastVLine(sx + TILE_PX - 1, sy, TILE_PX, edge);

    // Seyrek benek (çakıl / buz kırığı / köz)
    if (((mx * 13 + my * 7) & 7) == 0)
        cv.drawPixel(sx + 2 + ((mx * 5 + my) & 3), sy + 2 + ((mx + my * 3) & 3), BIO_PATH_DOT[gBiome]);
}

// ------------------------------------------------------------
//  TEK TILE — (sx,sy) ekran pikseli, (mx,my) harita koordinatı
// ------------------------------------------------------------
inline void drawTile(TFT_eSprite &cv, int sx, int sy, int mx, int my, uint8_t t) {
    switch (t) {
        case TILE_PATH:
        case TILE_BASE:   // Kale sprite'ı ayrı çizilir; altı yol olarak kalır
            drawPathBase(cv, sx, sy, mx, my);
            break;
        case TILE_SPAWN:  // Mağara ağzı (düşmanlar buradan çıkar)
            drawPathBase(cv, sx, sy, mx, my);
            cv.fillRect(sx, sy, 7, TILE_PX, COL_CAVE);           // Kaya kütlesi
            cv.fillRect(sx + 1, sy + 2, 5, 6, COL_CAVE_IN);      // Karanlık ağız
            cv.drawFastHLine(sx, sy, 6, COL_ROCK_HL);            // Üst ışık vurgusu
            cv.drawPixel(sx + 6, sy + 1, COL_ROCK_HL);
            break;
        case TILE_ROCK:
            drawGrassBase(cv, sx, sy, mx, my);
            cv.fillEllipse(sx + 4, sy + 6, 4, 1, COL_SHADOW);         // Zemin gölgesi
            cv.fillRoundRect(sx + 1, sy + 2, 6, 5, 2, BIO_ROCK_SH[gBiome]);  // Koyu taraf/kontur
            cv.fillRoundRect(sx + 1, sy + 2, 5, 4, 2, BIO_ROCK[gBiome]);     // Kaya gövdesi
            cv.drawFastHLine(sx + 2, sy + 2, 3, BIO_ROCK_HL[gBiome]); // Üst ışık
            cv.drawPixel(sx + 2, sy + 3, BIO_ROCK_HL[gBiome]);        // Parıltı
            break;
        default:   // TILE_GRASS ve TILE_TOWER (kule ayrı çizilir, zemin çimen)
            drawGrassBase(cv, sx, sy, mx, my);
            break;
    }
}

// ------------------------------------------------------------
//  KALE — bayraklı taş burç, base tile'ının üstüne taşarak çizilir
// ------------------------------------------------------------
inline void drawCastle(TFT_eSprite &cv) {
    int bx = baseTileX * TILE_PX;
    int by = HUD_H + baseTileY * TILE_PX;

    // Burç gövdesi (üstteki çimen tile'ına taşar: 8x14)
    cv.fillRect(bx, by - 6, TILE_PX, 14, COL_CASTLE);
    cv.drawFastVLine(bx, by - 6, 14, COL_CASTLE_LT);         // Sol ışık kenarı
    cv.drawFastVLine(bx + 7, by - 6, 14, COL_CASTLE_DK);     // Sağ gölge kenarı

    // Mazgal dişleri
    cv.fillRect(bx,     by - 8, 2, 2, COL_CASTLE);
    cv.fillRect(bx + 3, by - 8, 2, 2, COL_CASTLE);
    cv.fillRect(bx + 6, by - 8, 2, 2, COL_CASTLE);

    // Tuğla derz çizgileri
    cv.drawFastHLine(bx + 1, by - 3, 6, COL_CASTLE_DK);
    cv.drawFastHLine(bx + 1, by + 1, 6, COL_CASTLE_DK);

    // Kapı (gelen yola bakar)
    cv.fillRect(bx + 2, by + 3, 3, 5, COL_CAVE_IN);
    cv.drawPixel(bx + 3, by + 2, COL_CAVE_IN);               // Kemer tepesi

    // Bayrak: direk + dalgalanan kırmızı flama
    cv.drawFastVLine(bx + 4, by - 13, 5, COL_CASTLE_LT);
    bool flap = (millis() >> 8) & 1;
    cv.fillRect(bx + 5, by - 13, flap ? 3 : 2, 3, COL_BASE);
}

// ------------------------------------------------------------
//  HARİTA — 20x14 grid tamamı ekrana sığar (kamera yok)
// ------------------------------------------------------------
inline void drawMapAll(TFT_eSprite &cv) {
    gBiome = biomeOf(currentLevel);   // Zemin/yol/kaya paleti aktif bölümün biyomundan
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            drawTile(cv, x * TILE_PX, HUD_H + y * TILE_PX, x, y, tiles[y][x]);
    drawCastle(cv);
}

// ------------------------------------------------------------
//  KULE SPRITE'LARI — konturlu taş kaide + tile üstüne taşan tepe
//  (gölge + koyu dış kontur + ışık/gölge tonlama)
// ------------------------------------------------------------
inline void drawTowerSprite(TFT_eSprite &cv, const Tower &t) {
    int sx = t.tileX * TILE_PX;
    int sy = HUD_H + t.tileY * TILE_PX;
    bool flap = (millis() >> 7) & 1;
    const uint16_t OL = COL_OUTLINE;

    // Zemin gölgesi + konturlu masonlu taş kaide (rows 4-7)
    cv.fillEllipse(sx + 4, sy + 7, 4, 1, COL_SHADOW);
    cv.fillRoundRect(sx, sy + 4, 8, 4, 1, OL);                    // Kaide konturu
    cv.fillRect(sx + 1, sy + 5, 6, 2, COL_TOWER_BASE);           // Taş gövde
    cv.drawFastHLine(sx + 1, sy + 5, 6, COL_CASTLE_LT);          // Üst ışık
    cv.drawPixel(sx + 2, sy + 6, COL_CASTLE);                    // Tuğla derzleri
    cv.drawPixel(sx + 5, sy + 6, COL_CASTLE_DK);

    switch (t.type) {
        case TOWER_ARROW: {
            // Balista: konturlu ahşap kundak + yay kolları + parlak kurulu ok
            uint16_t c = COL_TOWER_ARROW, dk = dimColor(c), lt = litColor(c);
            cv.fillRect(sx + 3, sy - 1, 2, 6, OL);                    // Kundak konturu
            cv.fillRect(sx + 3, sy - 1, 2, 5, dk);                    // Dikey kundak
            cv.fillRect(sx, sy, 8, 3, OL);                            // Yay konturu
            cv.drawFastHLine(sx + 1, sy + 1, 6, c);                   // Yay gövdesi
            cv.drawPixel(sx + 1, sy, lt);     cv.drawPixel(sx + 6, sy, lt);      // Üst uçlar
            cv.drawPixel(sx + 1, sy + 2, c);  cv.drawPixel(sx + 6, sy + 2, c);   // Alt uçlar
            cv.fillRect(sx + 3, sy - 3, 2, 2, OL);                    // Ok konturu
            cv.drawFastVLine(sx + 4, sy - 3, 3, COL_RAY);             // Kurulu ok
            cv.drawPixel(sx + 4, sy - 3, 0xFFFF);                     // Ok ucu parıltısı
            break;
        }
        case TOWER_CANNON: {
            // Çelik taret: konturlu kubbe + hedefe dönen kalın namlu
            float cx = sx + TILE_PX / 2.0f;
            float cy = sy + 1.5f;
            float dx = t.aimX - towerCenterX(t);
            float dy = t.aimY - towerCenterY(t);
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1.0f) { dx = 1.0f; dy = 0.0f; len = 1.0f; }
            int bx = (int)(cx + dx / len * 5.0f);
            int by = (int)(cy + dy / len * 5.0f);
            cv.drawLine((int)cx, (int)cy - 1, bx, by - 1, OL);        // Namlu konturu
            cv.drawLine((int)cx, (int)cy + 2, bx, by + 2, OL);
            cv.drawLine((int)cx, (int)cy, bx, by, COL_CASTLE_LT);     // Namlu (çelik)
            cv.drawLine((int)cx, (int)cy + 1, bx, by + 1, COL_CASTLE_DK);
            cv.drawPixel(bx, by, COL_SHOT_CAN);                       // Namlu ağzı
            cv.fillCircle((int)cx, (int)cy, 4, OL);                   // Kubbe konturu
            cv.fillCircle((int)cx, (int)cy, 3, dimColor(COL_TOWER_CANNON)); // Koyu jant
            cv.fillCircle((int)cx, (int)cy, 2, COL_TOWER_CANNON);     // Kırmızı kubbe
            cv.drawPixel((int)cx - 1, (int)cy - 1, COL_SHOT_CAN);     // Işık yansıması
            break;
        }
        case TOWER_LASER: {
            // Lazer dikilitaşı: konturlu direk + ışıklı kenar + parlayan emitör
            uint16_t c = COL_TOWER_LASER, dk = dimColor(c);
            cv.fillRoundRect(sx + 1, sy - 3, 6, 8, 2, OL);            // Dikilitaş konturu
            cv.fillRoundRect(sx + 2, sy - 2, 4, 6, 1, dk);            // Gövde
            cv.drawFastVLine(sx + 2, sy - 2, 6, c);                   // Sol ışık kenarı
            cv.fillRect(sx + 3, sy - 1, 2, 2, c);                     // Enerji bandı
            cv.drawPixel(sx + 3, sy, COL_RAY);
            cv.fillRect(sx + 3, sy - 5, 2, 2, OL);                    // Emitör konturu
            cv.drawPixel(sx + 4, sy - 4, c);                          // Emitör
            cv.drawPixel(sx + 3, sy - 4, COL_RAY);                    // Emitör parıltısı
            break;
        }
        case TOWER_POISON: {
            // Zehir kazanı: konturlu pota + parlak asit yüzeyi + kabarcıklar
            uint16_t c = COL_TOWER_POISON, dk = dimColor(c);
            cv.fillRoundRect(sx, sy, 8, 6, 2, OL);                    // Pota konturu
            cv.fillRoundRect(sx + 1, sy + 1, 6, 4, 2, dk);            // Pota gövdesi
            cv.drawFastHLine(sx + 1, sy + 1, 6, c);                   // Kenar/ağız
            cv.fillRect(sx + 2, sy + 1, 4, 2, c);                     // Asit yüzeyi (parlak)
            cv.drawPixel(sx + 3, sy + 1, litColor(c));                // Yüzey parıltısı
            cv.drawPixel(sx + 3, sy - 1 - (flap ? 1 : 0), c);         // Yükselen kabarcıklar
            cv.drawPixel(sx + 5, sy - 1 - (flap ? 0 : 1), litColor(c));
            break;
        }
        default: {   // TOWER_FROST — konturlu parlak buz kristali
            uint16_t c = COL_TOWER_FROST;
            cv.fillTriangle(sx + 4, sy - 3, sx, sy + 2, sx + 8, sy + 2, OL);      // Üst kontur
            cv.fillTriangle(sx + 4, sy + 6, sx, sy + 2, sx + 8, sy + 2, OL);      // Alt kontur
            cv.fillTriangle(sx + 4, sy - 2, sx + 1, sy + 2, sx + 7, sy + 2, c);   // Üst yüz
            cv.fillTriangle(sx + 4, sy + 5, sx + 1, sy + 2, sx + 7, sy + 2, c);   // Alt yüz
            cv.drawFastVLine(sx + 4, sy - 1, 5, COL_SHOT_FRO);       // Çekirdek ışığı
            cv.drawPixel(sx + 3, sy, COL_RAY);                       // Işık kıvılcımı
            cv.drawPixel(sx + 5, sy + 2, dimColor(c));               // Gölgeli yüz
            break;
        }
    }

    // Seviye pipleri: seviye 2+ için platformda altın noktalar
    for (int l = 1; l < t.level; l++)
        cv.drawPixel(sx + 1 + l * 2, sy + 7, COL_MONEY);
}

inline void drawTowers(TFT_eSprite &cv) {
    for (int i = 0; i < MAX_TOWERS; i++)
        if (towers[i].active) drawTowerSprite(cv, towers[i]);
}

// ------------------------------------------------------------
//  DÜŞMAN SPRITE'LARI — tile'a taşan (~10-14px) konturlu karakterler
//  Gölge (fillEllipse) + koyu dış kontur + üst ışık/alt gölge tonlama
//  + 2 kareli yürüme/çırpma animasyonu. Mantık hâlâ tile bazlı;
//  sprite yalnızca görsel olarak büyür (feet tile tabanında kalır).
// ------------------------------------------------------------
inline void drawEnemySprite(TFT_eSprite &cv, const Enemy &e) {
    int sx = (int)e.x;
    int sy = HUD_H + (int)e.y;
    bool flap = (millis() >> 7) & 1;    // 2 karelik animasyon fazı
    uint16_t col = e.color;
    uint16_t dk  = dimColor(col);
    uint16_t lt  = litColor(col);
    const uint16_t OL = COL_OUTLINE;

    // Overlay (frost/zehir/HP barı) sınır kutusu — her tür kendine göre ayarlar
    int oX = sx - 1, oY = sy - 2, oW = 10, oH = 9;

    switch (e.type) {
        case ENEMY_RUNNER: {  // Sarı çevik böcek: yuvarlak kabuk + iri gözler + anten
            cv.fillEllipse(sx + 4, sy + 7, 4, 1, COL_SHADOW);        // Zemin gölgesi
            cv.fillRoundRect(sx - 1, sy, 10, 7, 3, OL);              // Kontur
            cv.fillRoundRect(sx, sy + 1, 8, 5, 2, col);              // Kabuk
            cv.drawFastHLine(sx + 1, sy + 1, 5, lt);                 // Üst ışık
            cv.drawFastVLine(sx + 4, sy + 1, 4, dk);                 // Kabuk derzi
            cv.fillRect(sx + 1, sy + 2, 2, 2, 0xFFFF);              // Gözler (ak)
            cv.fillRect(sx + 5, sy + 2, 2, 2, 0xFFFF);
            cv.drawPixel(sx + 2, sy + 3, OL);                        // Göz bebekleri
            cv.drawPixel(sx + 6, sy + 3, OL);
            cv.drawPixel(sx + 2, sy - 1, OL);                        // Antenler kımıldar
            cv.drawPixel(sx + 2 - flap, sy - 2, OL);
            cv.drawPixel(sx + 5, sy - 1, OL);
            cv.drawPixel(sx + 5 + flap, sy - 2, OL);
            cv.drawPixel(sx + (flap ? 0 : 2), sy + 6, OL);           // Bacaklar
            cv.drawPixel(sx + (flap ? 7 : 5), sy + 6, OL);
            break;
        }

        case ENEMY_TANK: {   // Ağır yeşil zırh: kubbe gövde + taret/namlu + palet
            oX = sx - 1; oY = sy - 2; oW = 11; oH = 10;
            cv.fillEllipse(sx + 4, sy + 7, 5, 2, COL_SHADOW);        // Geniş gölge
            cv.fillRect(sx - 1, sy + 4, 11, 3, OL);                  // Palet konturu
            for (int i = -1; i < 10; i++)                            // Palet dişleri
                cv.drawPixel(sx + i, sy + 5, ((i + flap) & 1) ? COL_CASTLE : OL);
            cv.fillRoundRect(sx, sy, 8, 5, 2, OL);                   // Gövde konturu
            cv.fillRoundRect(sx + 1, sy + 1, 6, 3, 1, col);          // Zırh gövdesi
            cv.drawFastHLine(sx + 1, sy + 1, 6, lt);                 // Üst ışık
            cv.fillRect(sx + 3, sy - 2, 3, 3, OL);                   // Taret konturu
            cv.fillRect(sx + 3, sy - 1, 2, 1, dk);                   // Taret
            cv.drawFastHLine(sx + 5, sy - 1, 3, OL);                 // Namlu (ileri)
            cv.drawPixel(sx + 2, sy + 2, COL_HP_LOW);                // Kızıl vizör
            cv.drawPixel(sx + 5, sy + 2, COL_HP_LOW);
            break;
        }

        case ENEMY_FLYER: {  // Gök mavisi yarasa: çırpan üçgen kanatlar + parlak göz
            oX = sx - 1; oY = sy - 1; oW = 11; oH = 8;
            int bob = flap ? -1 : 0;
            cv.fillEllipse(sx + 4, sy + 7, 3, 1, COL_SHADOW);        // Küçük yer gölgesi
            int wy = sy + 2 + bob;
            if (flap) {                                              // Kanatlar aşağı
                cv.fillTriangle(sx - 1, wy - 1, sx + 3, wy, sx + 3, wy + 3, OL);
                cv.fillTriangle(sx + 9, wy - 1, sx + 5, wy, sx + 5, wy + 3, OL);
                cv.fillTriangle(sx, wy, sx + 3, wy + 1, sx + 3, wy + 2, col);
                cv.fillTriangle(sx + 8, wy, sx + 5, wy + 1, sx + 5, wy + 2, col);
            } else {                                                 // Kanatlar yukarı
                cv.fillTriangle(sx - 1, wy + 3, sx + 3, wy - 2, sx + 3, wy + 1, OL);
                cv.fillTriangle(sx + 9, wy + 3, sx + 5, wy - 2, sx + 5, wy + 1, OL);
                cv.fillTriangle(sx, wy + 2, sx + 3, wy - 1, sx + 3, wy, col);
                cv.fillTriangle(sx + 8, wy + 2, sx + 5, wy - 1, sx + 5, wy, col);
            }
            cv.fillRoundRect(sx + 2, wy - 1, 4, 5, 1, OL);           // Gövde konturu
            cv.fillRect(sx + 3, wy, 2, 3, dk);                       // Gövde
            cv.drawPixel(sx + 3, wy, COL_HP_LOW);                    // Parlak gözler
            cv.drawPixel(sx + 4, wy, COL_HP_LOW);
            cv.drawPixel(sx + 3, wy + 3, OL);                        // Kulak/kuyruk
            cv.drawPixel(sx + 4, wy + 3, OL);
            break;
        }

        case ENEMY_BOSS: {   // İri iblis: boynuz + altın taç + akkor gözler (biyom tonlu)
            oX = sx - 1; oY = sy - 6; oW = 11; oH = 13;
            cv.fillEllipse(sx + 4, sy + 7, 5, 2, COL_SHADOW);        // Gölge (v4.0: daralt)
            cv.fillRoundRect(sx - 1, sy - 1, 10, 8, 3, OL);          // Gövde konturu (daha dar)
            cv.fillRoundRect(sx, sy, 8, 6, 3, col);                  // İri gövde
            cv.drawFastHLine(sx + 1, sy, 6, lt);                     // Üst ışık
            cv.fillTriangle(sx - 1, sy, sx, sy - 4, sx + 2, sy, OL);       // Sol boynuz (içe çekildi)
            cv.fillTriangle(sx + 8, sy, sx + 7, sy - 4, sx + 5, sy, OL);   // Sağ boynuz
            cv.drawPixel(sx, sy - 3, COL_CASTLE_LT);                 // Boynuz uçları
            cv.drawPixel(sx + 7, sy - 3, COL_CASTLE_LT);
            cv.fillRect(sx + 1, sy - 3, 6, 2, COL_MONEY);            // Altın taç
            cv.drawPixel(sx + 1, sy - 4, COL_MONEY);                 // Taç uçları
            cv.drawPixel(sx + 4, sy - 5, COL_MONEY);
            cv.drawPixel(sx + 6, sy - 4, COL_MONEY);
            cv.fillRect(sx + 1, sy + 1, 2, 2, COL_RAY);              // Akkor gözler
            cv.fillRect(sx + 5, sy + 1, 2, 2, COL_RAY);
            cv.drawPixel(sx + 2, sy + 2, COL_HP_LOW);                // Göz bebekleri
            cv.drawPixel(sx + 5, sy + 2, COL_HP_LOW);
            cv.drawFastHLine(sx + 1, sy + 4, 6, OL);                 // Ağız
            cv.drawPixel(sx + 2, sy + 4, 0xFFFF);                    // Azı dişleri
            cv.drawPixel(sx + 5, sy + 4, 0xFFFF);
            cv.drawPixel(sx + (flap ? 0 : 7), sy + 6, OL);           // Ayak vuruşu
            break;
        }

        case ENEMY_ARMORED: {   // Çelik zırhlı: plakalı gövde + iri ön kalkan
            oX = sx - 1; oY = sy - 2; oW = 10; oH = 10;
            cv.fillEllipse(sx + 4, sy + 7, 4, 1, COL_SHADOW);        // Gölge
            cv.fillRoundRect(sx + 1, sy, 6, 7, 2, OL);              // Gövde konturu
            cv.fillRoundRect(sx + 2, sy + 1, 4, 5, 1, col);         // Çelik gövde
            cv.drawFastHLine(sx + 2, sy + 1, 4, lt);                // Üst ışık
            cv.drawFastHLine(sx + 2, sy + 3, 4, dk);               // Plaka derzi
            cv.fillRoundRect(sx - 1, sy + 1, 3, 6, 1, OL);         // İri kalkan konturu
            cv.fillRect(sx, sy + 2, 2, 4, COL_CASTLE_LT);          // Kalkan yüzü
            cv.drawFastVLine(sx, sy + 2, 4, lt);                    // Kalkan ışığı
            cv.drawPixel(sx + 1, sy + 3, COL_WAVE);                // Perçin
            cv.drawFastHLine(sx + 3, sy + 2, 2, COL_HP_LOW);       // Miğfer yarığı (kızıl)
            cv.drawPixel(sx + 3 + (flap ? 0 : 1), sy + 7, OL);     // Ayak
            break;
        }

        case ENEMY_HEALER: {    // Cüppeli şifacı: kızıl haç + süzülen + iyileşme kıvılcımı
            oX = sx - 1; oY = sy - 3; oW = 10; oH = 11;
            int bob = flap ? -1 : 0;
            cv.fillEllipse(sx + 4, sy + 7, 3, 1, COL_SHADOW);       // Küçük gölge
            cv.fillTriangle(sx + 4, sy - 2 + bob, sx, sy + 6, sx + 8, sy + 6, OL);       // Cüppe konturu
            cv.fillTriangle(sx + 4, sy - 1 + bob, sx + 1, sy + 5, sx + 7, sy + 5, col);  // Beyaz cüppe
            cv.fillCircle(sx + 4, sy - 1 + bob, 2, OL);             // Kukuleta konturu
            cv.fillCircle(sx + 4, sy - 1 + bob, 1, col);           // Baş
            cv.drawFastVLine(sx + 4, sy + 1, 3, COL_HP_LOW);       // Kızıl haç (dikey)
            cv.drawFastHLine(sx + 3, sy + 2, 3, COL_HP_LOW);       // Kızıl haç (yatay)
            cv.drawPixel(sx + (flap ? 1 : 6), sy - 2, litColor(COL_HP_MID));   // İyileşme kıvılcımı
            break;
        }

        case ENEMY_SWARM: {     // Küçük kehribar böcek: kalabalık akın, minik gövde
            oX = sx + 1; oY = sy + 1; oW = 6; oH = 6;
            cv.fillEllipse(sx + 4, sy + 6, 2, 1, COL_SHADOW);       // Minik gölge
            cv.fillRoundRect(sx + 2, sy + 2, 4, 3, 1, OL);         // Gövde konturu
            cv.fillRect(sx + 3, sy + 3, 2, 1, col);               // Kehribar gövde
            cv.drawPixel(sx + 3, sy + 2, lt);                      // Sırt ışığı
            cv.drawPixel(sx + 2 - (flap ? 1 : 0), sy + 5, OL);    // Bacaklar kımıldar
            cv.drawPixel(sx + 5 + (flap ? 1 : 0), sy + 5, OL);
            cv.drawPixel(sx + 2, sy + 1, OL);                     // Antenler
            cv.drawPixel(sx + 5, sy + 1, OL);
            break;
        }

        default: {           // ENEMY_SOLDIER — mor şövalye: tolga + kalkan + mızrak
            oX = sx - 1; oY = sy - 4; oW = 10; oH = 12;               // v4.0: kutu daraltıldı
            cv.fillEllipse(sx + 4, sy + 7, 4, 1, COL_SHADOW);        // Gölge
            cv.fillRoundRect(sx + 1, sy + 2, 6, 5, 2, OL);           // Gövde konturu
            cv.fillRoundRect(sx + 2, sy + 2, 4, 4, 1, col);          // Zırhlı gövde
            cv.drawFastHLine(sx + 2, sy + 2, 4, lt);                 // Göğüs ışığı
            cv.fillRoundRect(sx + 1, sy - 3, 6, 5, 2, OL);           // Tolga konturu
            cv.fillRoundRect(sx + 2, sy - 3, 4, 4, 1, COL_CASTLE_LT);// Tolga (çelik)
            cv.drawFastHLine(sx + 2, sy - 1, 4, COL_HUD_BG);         // Vizör yarığı
            cv.drawFastVLine(sx + 4, sy - 5, 2, COL_HP_LOW);         // Kızıl sorguç
            cv.fillRect(sx - 1, sy + 2, 2, 5, OL);                   // Kalkan konturu (içe çekildi)
            cv.drawPixel(sx, sy + 3, COL_WAVE);                      // Kalkan (cyan yüz)
            cv.drawPixel(sx, sy + 4, 0xFFFF);                        // Kalkan parıltısı
            int spy = sy + (flap ? 1 : 0);                           // Mızrak sallanır (tile içinde)
            cv.drawFastVLine(sx + 7, spy - 4, 9, OL);
            cv.drawFastVLine(sx + 6, spy - 4, 8, COL_CASTLE_LT);
            cv.drawPixel(sx + 6, spy - 5, 0xFFFF);                   // Mızrak ucu
            cv.drawPixel(sx + 2 + (flap ? 0 : 1), sy + 7, OL);       // Bacaklar
            cv.drawPixel(sx + 5 - (flap ? 0 : 1), sy + 7, OL);
            break;
        }
    }

    uint32_t now = millis();

    // Frost etkisi: gövdeyi saran buz mavisi çerçeve
    if (e.slowUntil > now) cv.drawRoundRect(oX, oY, oW, oH, 3, COL_SLOW_MARK);

    // Zehir etkisi: başının üstünde fokurdayan yeşil kabarcık
    if (e.poisonUntil > now) {
        cv.drawPixel(sx + (flap ? 2 : 5), oY - 1, COL_TOWER_POISON);
        cv.drawPixel(sx + (flap ? 3 : 4), oY - 2, litColor(COL_TOWER_POISON));
    }

    // Hasarlı düşmanın üstünde mini can barı
    int barY = oY - 3;
    if (e.hp < e.maxHp && barY >= HUD_H) {
        int fullW = oW;
        int w = (fullW * e.hp) / e.maxHp;
        if (w < 1) w = 1;
        cv.drawFastHLine(oX, barY, fullW, OL);                       // Bar zemini
        cv.drawFastHLine(oX, barY, w, COL_HP_LOW);                   // Kalan can
    }
}

inline void drawEnemies(TFT_eSprite &cv) {
    // Önce yerdekiler, sonra uçanlar (uçan üstte görünsün)
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive && enemies[i].type != ENEMY_FLYER)
            drawEnemySprite(cv, enemies[i]);
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive && enemies[i].type == ENEMY_FLYER)
            drawEnemySprite(cv, enemies[i]);
}

// ------------------------------------------------------------
//  ATIŞ ÇİZGİLERİ, MERMİLER VE HALKALAR
// ------------------------------------------------------------
inline void drawRays(TFT_eSprite &cv) {
    for (int i = 0; i < MAX_RAYS; i++) {
        const Ray &r = rays[i];
        if (!r.active) continue;
        cv.drawLine((int)r.x0, HUD_H + (int)r.y0,
                    (int)r.x1, HUD_H + (int)r.y1, r.color);
        if (r.thick)   // Kalın ışın: 1 px yana ikinci çizgi
            cv.drawLine((int)r.x0 + 1, HUD_H + (int)r.y0,
                        (int)r.x1 + 1, HUD_H + (int)r.y1, dimColor(r.color));
    }
}

inline void drawShots(TFT_eSprite &cv) {
    for (int i = 0; i < MAX_SHOTS; i++) {
        const Shot &s = shots[i];
        if (!s.active) continue;
        int px = (int)(s.x0 + (s.x1 - s.x0) * s.t);
        int py = HUD_H + (int)(s.y0 + (s.y1 - s.y0) * s.t);
        if (s.isCannon) cv.fillCircle(px, py, 2, s.color);     // Top güllesi
        else            cv.fillRect(px - 1, py - 1, 2, 2, s.color);   // Kristal/damla
    }
}

inline void drawRings(TFT_eSprite &cv) {
    for (int i = 0; i < MAX_RINGS; i++) {
        const Ring &g = rings[i];
        if (!g.active) continue;
        int r = (int)(g.maxR * (1.0f - g.life / RING_LIFE_S)) + 1;
        cv.drawCircle((int)g.x, HUD_H + (int)g.y, r, g.color);
    }
}

// ------------------------------------------------------------
//  HASAR POPUP'LARI — "-3" yazısı yükselerek kaybolur
// ------------------------------------------------------------
inline void drawPopups(TFT_eSprite &cv) {
    cv.setTextSize(1);
    for (int i = 0; i < MAX_POPUPS; i++) {
        const DmgPopup &p = popups[i];
        if (!p.active) continue;
        int sx = (int)p.x - FONT_W;
        int sy = HUD_H + (int)p.y - FONT_H;
        if (sy < HUD_H || sy >= SCR_H - FONT_H) continue;
        cv.setTextColor(p.color);
        cv.setCursor(sx, sy);
        cv.print('-');
        cv.print(p.val);
    }
}

// ------------------------------------------------------------
//  İMLEÇ + MENZİL HALKASI
// ------------------------------------------------------------
inline void drawCursor(TFT_eSprite &cv, int curX, int curY) {
    int sx = curX * TILE_PX;
    int sy = HUD_H + curY * TILE_PX;

    // Kule üzerinde: menzil halkası (soluk daire)
    int ti = towerIndexAt(curX, curY);
    if (ti >= 0) {
        const Tower &t = towers[ti];
        cv.drawCircle(sx + TILE_PX / 2, sy + TILE_PX / 2,
                      t.range * TILE_PX, COL_TOWER_RANGE);
    }

    // Yanıp sönme: 2 karelik faz (Dungeon minimap pattern'i)
    if ((millis() >> 7) & 1) return;
    bool ok = canBuildTower(curX, curY) || ti >= 0;
    cv.drawRect(sx, sy, TILE_PX, TILE_PX, ok ? COL_CURSOR : COL_CURSOR_BAD);
}

// ------------------------------------------------------------
//  HUD — kalp + can barı, para, "B4 18/45" sayacı, yıldırım ikonu
// ------------------------------------------------------------
inline void drawHUD(TFT_eSprite &cv, int fps, bool showFpsFlag) {
    cv.fillRect(0, 0, SCR_W, HUD_H, COL_HUD_BG);
    cv.setTextSize(1);

    // Kalp ikonu (7x6 piksel)
    int hx = HUD_HEART_X, hy = HUD_TEXT_Y + 1;
    cv.fillRect(hx + 1, hy, 2, 2, COL_HP_LOW);
    cv.fillRect(hx + 4, hy, 2, 2, COL_HP_LOW);
    cv.fillRect(hx, hy + 1, 7, 2, COL_HP_LOW);
    cv.fillRect(hx + 1, hy + 3, 5, 1, COL_HP_LOW);
    cv.fillRect(hx + 2, hy + 4, 3, 1, COL_HP_LOW);
    cv.drawPixel(hx + 3, hy + 5, COL_HP_LOW);

    // Kale can barı
    cv.drawRect(HUD_BAR_X, HUD_BAR_Y, HUD_BAR_W, HUD_BAR_H, COL_HUD_LINE);
    int pct = (baseHp * 100) / BASE_HP_MAX;
    uint16_t barCol = (pct > HP_MID_PCT) ? COL_HP_FULL
                     : (pct > HP_LOW_PCT) ? COL_HP_MID : COL_HP_LOW;
    int w = ((HUD_BAR_W - 2) * baseHp) / BASE_HP_MAX;
    if (w > 0) cv.fillRect(HUD_BAR_X + 1, HUD_BAR_Y + 1, w, HUD_BAR_H - 2, barCol);

    // Para (sarı)
    cv.setTextColor(COL_MONEY);
    cv.setCursor(HUD_MONEY_X, HUD_TEXT_Y);
    cv.print('$');
    cv.print(money);

    // Bölüm + dalga sayacı: "B4 18/45" — hazırlıkta sıradaki dalga
    int dispWave = waveRunning ? waveNum
                 : (waveNum < TOTAL_WAVES ? waveNum + 1 : waveNum);
    char wbuf[12];
    snprintf(wbuf, sizeof(wbuf), "L%d %d/%d", currentLevel, dispWave, TOTAL_WAVES);
    cv.setTextColor(COL_WAVE);
    cv.setCursor(HUD_WAVE_X, HUD_TEXT_Y);
    cv.print(wbuf);

    // Yıldırım hazır ikonu (daha belirgin ve hizalı)
    bool boltReady = waveRunning && lightningCd <= 0.0f && money >= LIGHTNING_COST;
    uint16_t bc = boltReady ? COL_MONEY : COL_HUD_LINE;
    int bx = HUD_BOLT_X; // 150
    int by = HUD_TEXT_Y; // 3
    cv.drawPixel(bx + 1, by, bc);
    cv.drawPixel(bx, by + 1, bc);
    cv.drawPixel(bx + 1, by + 1, bc);
    cv.drawPixel(bx, by + 2, bc);
    cv.drawPixel(bx - 1, by + 3, bc);
    cv.drawPixel(bx, by + 3, bc);
    cv.drawPixel(bx + 1, by + 3, bc);
    cv.drawPixel(bx, by + 4, bc);
    cv.drawPixel(bx, by + 5, bc);
    cv.drawPixel(bx, by + 6, bc);

    // FPS (koşullu)
    if (showFpsFlag) {
        cv.setTextColor(COL_HUD_TEXT);
        cv.setCursor(HUD_FPS_X, HUD_TEXT_Y);
        cv.print(fps);
    }

    cv.drawFastHLine(0, HUD_H - 1, SCR_W, COL_HUD_LINE);
}

// ------------------------------------------------------------
//  DALGA HAZIRLIK ŞERİDİ — alt orta, geri sayım + yanıp sönen [D]
// ------------------------------------------------------------
inline void drawPrepBanner(TFT_eSprite &cv) {
    if (waveRunning || prepLeft <= 0.0f) return;
    char buf[26];
    int secs = (int)prepLeft + 1;
    snprintf(buf, sizeof(buf), "L%d WAVE %d: %d", currentLevel, waveNum + 1, secs);
    int y = SCR_H - FONT_H - 3;
    cv.fillRect(0, y - 2, SCR_W, FONT_H + 5, COL_PANEL_BG);
    cv.setTextSize(1);
    cv.setTextColor(COL_WAVE);
    cv.setCursor(4, y);
    cv.print(buf);
    if ((millis() >> 8) & 1) {                    // Yanıp sönen erken başlat ipucu
        const char *h = "[D] START";
        cv.setTextColor(COL_MONEY);
        cv.setCursor(102, y); // Sabit hizalama
        cv.print(h);
    }
}

// ------------------------------------------------------------
//  DALGA İÇİ İPUCU — sonraki dalga erken çağrılabilirse yanıp söner (v4.1)
// ------------------------------------------------------------
inline void drawWaveHints(TFT_eSprite &cv) {
    if (!canCallNextWave() || !((millis() >> 8) & 1)) return;
    const char *h = "[D] NEXT";
    int y = SCR_H - FONT_H - 3;
    cv.setTextSize(1);
    cv.setTextColor(COL_MONEY);
    cv.setCursor(102, y); // START ile aynı hizalama
    cv.print(h);
}

// ------------------------------------------------------------
//  YARI SAYDAM KARARTMA — pause/panel arkası (satır atlama)
// ------------------------------------------------------------
inline void dimOverlay(TFT_eSprite &cv) {
    // Empty body
}

// ------------------------------------------------------------
//  MENÜ EKRANI — başlık, dekoratif kule + düşman görseli, rekor
// ------------------------------------------------------------
inline void drawMenu(TFT_eSprite &cv, int bestWave) {
    cv.fillSprite(COL_BG);
    drawCentered(cv, "TOWER", 8, COL_TOWER_ARROW, 2);
    drawCentered(cv, "DEFENSE", 26, COL_TOWER_ARROW, 2);

    // Dekoratif görsel: yol şeridi + kule + yürüyen düşmanlar
    constexpr int DX = 34, DY = 50;                     // Görsel sol üst köşesi
    cv.fillRect(DX, DY + 6, 92, 8, COL_PATH);           // Yol
    cv.drawFastHLine(DX, DY + 6, 92, COL_PATH_EDGE);    // Yol kenarları
    cv.drawFastHLine(DX, DY + 13, 92, COL_PATH_EDGE);
    cv.fillRect(DX + 24, DY + 6, 8, 8, COL_PATH_ALT);   // Dama karoları
    cv.fillRect(DX + 56, DY + 6, 8, 8, COL_PATH_ALT);
    cv.fillTriangle(DX + 10, DY + 7, DX + 7, DY + 12, DX + 13, DY + 12,
                    COL_ENEMY_RUNNER);                  // Runner
    cv.fillRect(DX + 26, DY + 8, 5, 5, COL_ENEMY_SOLDIER);   // Soldier
    cv.fillRect(DX + 44, DY + 7, 7, 6, COL_ENEMY_TANK);      // Tank
    cv.drawFastHLine(DX + 33, DY - 4, 3, COL_ENEMY_FLYER);   // Uçan (kanatlar)
    cv.drawFastHLine(DX + 38, DY - 4, 3, COL_ENEMY_FLYER);
    cv.fillRect(DX + 36, DY - 5, 2, 3, COL_ENEMY_FLYER);
    cv.fillRect(DX + 66, DY - 4, 6, 3, COL_CASTLE_DK);       // Kule kaidesi
    cv.fillRect(DX + 68, DY - 8, 2, 4, COL_TOWER_ARROW);     // Kule gövdesi
    cv.fillTriangle(DX + 69, DY - 11, DX + 66, DY - 8, DX + 72, DY - 8,
                    COL_TOWER_ARROW);                        // Ok ucu
    cv.drawLine(DX + 69, DY - 6, DX + 48, DY + 9, COL_RAY);  // Atış çizgisi
    cv.fillRect(DX + 88, DY + 2, 6, 12, COL_CASTLE);         // Kale
    cv.fillRect(DX + 89, DY - 1, 1, 3, COL_CASTLE_LT);       // Bayrak direği
    cv.fillRect(DX + 90, DY - 1, 3, 2, COL_BASE);            // Bayrak

    drawCentered(cv, "[A] Start", 74, COL_HUD_TEXT);
    drawCentered(cv, "[B] OS Menu", 84, COL_HUD_TEXT);
    drawCentered(cv, "[C] Level Select", 94, COL_WAVE);
    drawCentered(cv, "A:Tower B:Lightning D:Wave", 104, COL_EMPTY_TXT);

    char buf[32];
    snprintf(buf, sizeof(buf), "Best: Wave %d/%d", bestWave, TOTAL_WAVES);
    drawCentered(cv, buf, 116, COL_MONEY);
}

// ------------------------------------------------------------
//  BÖLÜM SEÇİMİ EKRANI — 1..9 arası bölüm + mini harita önizleme.
// ------------------------------------------------------------
constexpr const char *LEVEL_TITLE[LEVEL_COUNT] = {
    "S Path", "Snake", "Vortex", "Ice Corridor", "Double Pendulum",
    "Ice Vortex", "Lava River", "Flame Maze", "Hell Gate"
};

inline void drawLevelSelect(TFT_eSprite &cv, int sel) {
    cv.fillSprite(COL_BG);
    int b = biomeOf(sel);
    uint16_t biomeCol = (b == BIOME_FROST) ? COL_ENEMY_FLYER
                      : (b == BIOME_HELL)  ? COL_TOWER_LASER : COL_HP_FULL;

    drawCentered(cv, "LEVEL SELECT", 4, COL_WAVE);
    char buf[32];
    snprintf(buf, sizeof(buf), "L%d  %s", sel, BIOME_NAME[b]);
    drawCentered(cv, buf, 16, biomeCol);

    for (int i = 0; i < LEVEL_COUNT; i++) {
        int bi = biomeOf(i + 1);
        uint16_t c = (bi == BIOME_FROST) ? COL_ENEMY_FLYER
                   : (bi == BIOME_HELL)  ? COL_TOWER_LASER : COL_HP_FULL;
        int bx = 26 + i * 12 + (bi * 2);
        int by = 26;
        if (i == sel - 1) {
            cv.fillRect(bx - 1, by - 1, 11, 11, c);
            cv.setTextColor(COL_BG);
        } else {
            cv.drawRect(bx, by, 9, 9, c);
            cv.setTextColor(c);
        }
        cv.setTextSize(1);
        cv.setCursor(bx + 2, by + 1);
        cv.print((char)('1' + i));
    }

    gBiome = b;
    constexpr int PVW = MAP_W * 3, PVH = MAP_H * 3;
    constexpr int PVX = (SCR_W - PVW) / 2, PVY = 42;
    cv.drawRect(PVX - 1, PVY - 1, PVW + 2, PVH + 2, COL_HUD_LINE);
    const uint8_t (*m)[MAP_W] = LEVEL_MAPS[sel - 1];
    for (int gy = 0; gy < MAP_H; gy++)
        for (int gx = 0; gx < MAP_W; gx++) {
            uint8_t t = m[gy][gx];
            uint16_t c = (t == TILE_ROCK)  ? BIO_ROCK[b]
                       : (t == TILE_BASE)  ? COL_BASE
                       : (t == TILE_SPAWN) ? COL_CAVE_IN
                       : (t == TILE_PATH)  ? BIO_PATH[b] : BIO_GRASS[b];
            cv.fillRect(PVX + gx * 3, PVY + gy * 3, 3, 3, c);
        }

    drawCentered(cv, LEVEL_TITLE[sel - 1], PVY + PVH + 4, COL_HUD_TEXT);
    drawCentered(cv, "[A] Start   [B] Back", SCR_H - 9, COL_EMPTY_TXT);
}

// ------------------------------------------------------------
//  KULE SEÇİM PANELİ — 5 kule türü + iptal
// ------------------------------------------------------------
inline void drawTowerMenu(TFT_eSprite &cv, int cursor) {
    dimOverlay(cv);
    cv.fillRect(TM_PANEL_X, TM_PANEL_Y, TM_PANEL_W, TM_PANEL_H, COL_PANEL_BG);
    cv.drawRect(TM_PANEL_X, TM_PANEL_Y, TM_PANEL_W, TM_PANEL_H, COL_PANEL_BRD);

    drawCentered(cv, "TOWER SELECT", TM_PANEL_Y + 3, COL_WAVE);

    constexpr int LIST_Y = TM_PANEL_Y + 14;
    constexpr int ROW_H  = 11;
    constexpr uint16_t towerCols[TOWER_TYPE_COUNT] = {
        COL_TOWER_ARROW, COL_TOWER_CANNON, COL_TOWER_FROST,
        COL_TOWER_LASER, COL_TOWER_POISON
    };

    cv.setTextSize(1);
    char buf[16];
    for (int i = 0; i < TM_OPTIONS; i++) {
        int y = LIST_Y + i * ROW_H;

        if (i == cursor) {
            cv.fillRect(TM_PANEL_X + 2, y - 1, TM_PANEL_W - 4, ROW_H, COL_SEL_BOX);
            cv.setTextColor(COL_MONEY);
            cv.setCursor(TM_PANEL_X + 4, y);
            cv.print('>');
        }

        if (i < TOWER_TYPE_COUNT) {
            bool affordable = (money >= TOWER_COST[i]);
            cv.fillRect(TM_PANEL_X + 12, y + 1, 5, 5, towerCols[i]);
            cv.setTextColor(affordable ? COL_HUD_TEXT : COL_EMPTY_TXT);
            snprintf(buf, sizeof(buf), "%-5s $%d", TOWER_NAME[i], TOWER_COST[i]);
            cv.setCursor(TM_PANEL_X + 22, y);
            cv.print(buf);
        } else {
            cv.setTextColor(COL_HUD_TEXT);
            cv.setCursor(TM_PANEL_X + 22, y);
            cv.print("CANCEL");
        }
    }

    snprintf(buf, sizeof(buf), "Money: $%d", money);
    drawCentered(cv, buf, TM_PANEL_Y + TM_PANEL_H - 11, COL_MONEY);
}

// ------------------------------------------------------------
//  KULE BİLGİ / YÖNETİM PANELİ
// ------------------------------------------------------------
inline void drawTowerInfo(TFT_eSprite &cv, const Tower &t, int cursor) {
    dimOverlay(cv);
    constexpr int PW = 108, PH = 96;
    constexpr int PX = (SCR_W - PW) / 2;
    constexpr int PY = 16;
    cv.fillRect(PX, PY, PW, PH, COL_PANEL_BG);
    cv.drawRect(PX, PY, PW, PH, COL_PANEL_BRD);

    constexpr uint16_t towerCols[TOWER_TYPE_COUNT] = {
        COL_TOWER_ARROW, COL_TOWER_CANNON, COL_TOWER_FROST,
        COL_TOWER_LASER, COL_TOWER_POISON
    };

    cv.setTextSize(1);
    char buf[18];

    cv.fillRect(PX + 6, PY + 5, 6, 6, towerCols[t.type]);
    cv.setTextColor(COL_WAVE);
    snprintf(buf, sizeof(buf), "%s  Lv.%d", TOWER_NAME[t.type], t.level);
    cv.setCursor(PX + 16, PY + 5);
    cv.print(buf);
    cv.drawFastHLine(PX + 4, PY + 15, PW - 8, COL_HUD_LINE);

    cv.setTextColor(COL_HUD_TEXT);
    snprintf(buf, sizeof(buf), "Damage: %d", t.damage);
    cv.setCursor(PX + 8, PY + 20); cv.print(buf);
    snprintf(buf, sizeof(buf), "Range : %d", t.range);
    cv.setCursor(PX + 8, PY + 30); cv.print(buf);

    constexpr int LIST_Y = PY + 44;
    constexpr int ROW_H  = 13;
    for (int i = 0; i < TI_OPTIONS; i++) {
        int y = LIST_Y + i * ROW_H;
        if (i == cursor) {
            cv.fillRect(PX + 3, y - 1, PW - 6, ROW_H, COL_SEL_BOX);
            cv.setTextColor(COL_MONEY);
            cv.setCursor(PX + 5, y + 1);
            cv.print('>');
        }
        cv.setCursor(PX + 14, y + 1);
        if (i == 0) {
            if (t.level >= TOWER_MAX_LEVEL) {
                cv.setTextColor(COL_EMPTY_TXT);
                cv.print("UPGRADE: MAX");
            } else {
                int uc = upgradeCost(t);
                cv.setTextColor(money >= uc ? COL_HUD_TEXT : COL_EMPTY_TXT);
                snprintf(buf, sizeof(buf), "UPGRADE: $%d", uc);
                cv.print(buf);
            }
        } else if (i == 1) {
            cv.setTextColor(COL_WAVE);
            snprintf(buf, sizeof(buf), "SELL: +$%d", sellValue(t));
            cv.print(buf);
        } else {
            cv.setTextColor(COL_EMPTY_TXT);
            cv.print("CLOSE");
        }
    }
}

// ------------------------------------------------------------
//  DALGA TEMİZ EKRANI
// ------------------------------------------------------------
inline void drawWaveClear(TFT_eSprite &cv, int clearedWave, int bonus) {
    constexpr int BAN_Y = 48, BAN_H = 34;
    cv.fillRect(0, BAN_Y, SCR_W, BAN_H, COL_PANEL_BG);
    cv.drawFastHLine(0, BAN_Y, SCR_W, COL_PANEL_BRD);
    cv.drawFastHLine(0, BAN_Y + BAN_H - 1, SCR_W, COL_PANEL_BRD);

    char buf[26];
    snprintf(buf, sizeof(buf), "WAVE %d CLEARED!", clearedWave);
    drawCentered(cv, buf, BAN_Y + 7, COL_WAVE);
    snprintf(buf, sizeof(buf), "BONUS: +$%d", bonus);
    drawCentered(cv, buf, BAN_Y + 20, COL_MONEY);
}

// ------------------------------------------------------------
//  BÖLÜM GEÇİŞ EKRANI
// ------------------------------------------------------------
inline void drawLevelClear(TFT_eSprite &cv, int clearedLevel, int bonus) {
    constexpr int BAN_Y = 40, BAN_H = 48;
    cv.fillRect(0, BAN_Y, SCR_W, BAN_H, COL_PANEL_BG);
    cv.drawFastHLine(0, BAN_Y, SCR_W, COL_MONEY);
    cv.drawFastHLine(0, BAN_Y + BAN_H - 1, SCR_W, COL_MONEY);

    char buf[26];
    snprintf(buf, sizeof(buf), "LEVEL %d CLEARED!", clearedLevel);
    drawCentered(cv, buf, BAN_Y + 7, COL_MONEY);
    snprintf(buf, sizeof(buf), "BONUS: +$%d", bonus);
    drawCentered(cv, buf, BAN_Y + 20, COL_HUD_TEXT);

    int nb = biomeOf(clearedLevel + 1);
    if (clearedLevel < LEVEL_COUNT && nb != biomeOf(clearedLevel)) {
        snprintf(buf, sizeof(buf), ">> %s REALM <<", BIOME_NAME[nb]);
        drawCentered(cv, buf, BAN_Y + 33, COL_HP_LOW);
    } else {
        drawCentered(cv, "Loading new map...", BAN_Y + 33, COL_WAVE);
    }
}

// ------------------------------------------------------------
//  PAUSE OVERLAY — donmuş sahnenin üstüne
// ------------------------------------------------------------
inline void drawPauseOverlay(TFT_eSprite &cv) {
    constexpr int PX = 34, PY = 42, PW = 92, PH = 44;
    cv.fillRect(PX, PY, PW, PH, COL_PANEL_BG);
    cv.drawRect(PX, PY, PW, PH, COL_PANEL_BRD);
    drawCentered(cv, "PAUSE", PY + 8, COL_WAVE);
    drawCentered(cv, "[A] Continue", PY + 22, COL_HUD_TEXT);
    drawCentered(cv, "[B] OS Menu", PY + 32, COL_HUD_TEXT);
}

// ------------------------------------------------------------
//  GAME OVER PANELİ
// ------------------------------------------------------------
inline void drawGameOver(TFT_eSprite &cv, int bestWave, bool newRecord) {
    cv.fillSprite(COL_BG);   // v4.1: çizgili dim yerine temiz arka plan
    constexpr int PX = 24, PY = 18, PW = 112, PH = 96;
    cv.fillRect(PX, PY, PW, PH, COL_PANEL_BG);
    cv.drawRect(PX, PY, PW, PH, COL_HP_LOW);

    drawCentered(cv, "CASTLE FELL!", PY + 6, COL_HP_LOW);

    char buf[22];
    cv.setTextColor(COL_HUD_TEXT);
    cv.setTextSize(1);
    snprintf(buf, sizeof(buf), "Wave  : %d/%d", waveNum, TOTAL_WAVES);
    cv.setCursor(PX + 12, PY + 22); cv.print(buf);
    snprintf(buf, sizeof(buf), "Tower : %d", towersBuilt);
    cv.setCursor(PX + 12, PY + 34); cv.print(buf);
    snprintf(buf, sizeof(buf), "Kills : %d", killsTotal);
    cv.setCursor(PX + 12, PY + 46); cv.print(buf);

    if (newRecord && ((millis() >> 8) & 1)) {
        drawCentered(cv, "NEW BEST!", PY + 62, COL_MONEY);
    } else {
        snprintf(buf, sizeof(buf), "Best: Wave %d", bestWave);
        drawCentered(cv, buf, PY + 62, COL_MONEY);
    }

    drawCentered(cv, "[A] Retry [B] OS", PY + 80, COL_HUD_TEXT);
}

// ------------------------------------------------------------
//  ZAFER PANELİ — 9 bölüm / 45 dalganın tamamı savuşturuldu
// ------------------------------------------------------------
inline void drawVictory(TFT_eSprite &cv) {
    cv.fillSprite(COL_BG);   // v4.1: çizgili dim yerine temiz arka plan
    constexpr int PX = 22, PY = 16, PW = 116, PH = 98;
    cv.fillRect(PX, PY, PW, PH, COL_PANEL_BG);
    cv.drawRect(PX, PY, PW, PH, COL_MONEY);

    drawCentered(cv, "VICTORY!", PY + 6, COL_MONEY, 2);
    drawCentered(cv, "Kingdom saved!", PY + 26, COL_WAVE);

    char buf[22];
    cv.setTextColor(COL_HUD_TEXT);
    cv.setTextSize(1);
    snprintf(buf, sizeof(buf), "Wave  : %d/%d", TOTAL_WAVES, TOTAL_WAVES);
    cv.setCursor(PX + 12, PY + 42); cv.print(buf);
    snprintf(buf, sizeof(buf), "Tower : %d", towersBuilt);
    cv.setCursor(PX + 12, PY + 54); cv.print(buf);
    snprintf(buf, sizeof(buf), "Kills : %d", killsTotal);
    cv.setCursor(PX + 12, PY + 66); cv.print(buf);

    drawCentered(cv, "[A] Retry [B] OS", PY + 84, COL_HUD_TEXT);
}
