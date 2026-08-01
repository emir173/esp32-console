#pragma once
// ============================================================
//  dungeon/Renderer.h — Tüm TFT_eSprite Çizim Fonksiyonları
//
//  Sorumluluk: Harita/tile çizimi (fog dahil), oyuncu ve düşman
//  sprite'ları, HUD, minimap, efekt katmanları (slash/popup),
//  menü, pause, game over, kat geçişi ve envanter ekranları.
//  Sadece görünen viewport tile'ları çizilir (performans).
// ============================================================

#include "Config.h"
#include "Player.h"
#include "Map.h"
#include "Enemies.h"
#include "Items.h"
#include "Spells.h"
#include "Combat.h"

// ------------------------------------------------------------
//  Metin yardımcıları (varsayılan font: 6x8 px / karakter)
// ------------------------------------------------------------
constexpr int FONT_W = 6;    // Boyut 1 karakter genişliği
constexpr int FONT_H = 8;    // Boyut 1 karakter yüksekliği

// Yatay ortalanmış metin yaz
inline void drawCentered(TFT_eSprite &cv, const char *txt, int y,
                         uint16_t color, int size = 1) {
    int w = strlen(txt) * FONT_W * size;
    cv.setTextSize(size);
    cv.setTextColor(color);
    cv.setCursor((SCR_W - w) / 2, y);
    cv.print(txt);
}

// ------------------------------------------------------------
//  TILE-ARASI KAYMA (v3.0) — grid mantığı değişmez, yalnızca
//  çizim pozisyonu önceki tile'dan yeni tile'a durMs içinde kayar.
// ------------------------------------------------------------
inline int lerpTilePx(int fromTile, int toTile, uint32_t elapsedMs, uint32_t durMs) {
    if (fromTile == toTile || elapsedMs >= durMs) return toTile * TILE_PX;
    int fromPx = fromTile * TILE_PX;
    int toPx   = toTile * TILE_PX;
    return fromPx + (int)(((int32_t)(toPx - fromPx) * (int32_t)elapsedMs) / (int32_t)durMs);
}

// Oyuncunun dünya-piksel pozisyonu (kayma animasyonu dahil)
inline void playerRenderPos(int &px, int &py) {
    uint32_t el = millis() - player.lastMoveMs;
    px = lerpTilePx(player.prevTileX, player.tileX, el, MOVE_ANIM_MS);
    py = lerpTilePx(player.prevTileY, player.tileY, el, MOVE_ANIM_MS);
}

// ------------------------------------------------------------
//  ADIM FAZI (v3.1) — bacak animasyonu hareket İLERLEMESİNE bağlı:
//  kaymanın ilk yarısında bir bacak, ikinci yarısında diğeri öndedir.
//  Tile paritesi eklenir ki ardışık adımlar sol-sağ-sol-sağ görünsün.
//  (Eski sürüm duvar saatine bağlıydı → bacaklar saniyede ~15 kez
//  makasl(ıyor, "titreme" gibi görünüyordu.)
// ------------------------------------------------------------
inline int stepPhase(uint32_t elapsedMs, uint32_t durMs, int tileX, int tileY) {
    int half = (elapsedMs < durMs / 2) ? 0 : 1;
    return half ^ ((tileX + tileY) & 1);
}

// ------------------------------------------------------------
//  VARLIK GÖLGESİ (v3.1) — sprite altındaki zemin piksellerinin
//  kendi rengini karartır; varlıklar zemine "oturur".
//  Sprite'tan ÖNCE çağrılmalı (readPixel zemini okur).
// ------------------------------------------------------------
inline void drawEntityShadow(TFT_eSprite &cv, int sx, int y, int w) {
    if (y < HUD_H || y >= SCR_H) return;
    for (int x = sx; x < sx + w; x++) {
        if (x < 0 || x >= SCR_W) continue;
        cv.drawPixel(x, y, dimColor(cv.readPixel(x, y)));
    }
}

// ------------------------------------------------------------
//  PİKSEL-BAZLI KAMERA (v3.0) — lerp'li oyuncuyu merkezler,
//  harita kenarlarına piksel hassasiyetinde clamp'ler.
//  (Eski computeCamera tile bazlıydı; akıcı takip için değişti.)
// ------------------------------------------------------------
inline void computeCameraPx(int &camPx, int &camPy) {
    int px, py;
    playerRenderPos(px, py);
    constexpr int VIEW_H = SCR_H - HUD_H;                // Oyun alanı yüksekliği
    camPx = px + TILE_PX / 2 - SCR_W / 2;
    camPy = py + TILE_PX / 2 - VIEW_H / 2;
    if (camPx < 0) camPx = 0;
    if (camPy < 0) camPy = 0;
    if (camPx > MAP_W * TILE_PX - SCR_W)  camPx = MAP_W * TILE_PX - SCR_W;
    if (camPy > MAP_H * TILE_PX - VIEW_H) camPy = MAP_H * TILE_PX - VIEW_H;
}

// ------------------------------------------------------------
//  ZEMİN DEKORU (v3.1) — deterministik tile karması ile seyrek
//  görsel çeşitlilik (çakıl / çatlak / biyom aksanı). Salt görsel,
//  yürünebilirlik değişmez; karma (mx,my)'ye bağlı → kare değişse
//  de aynı tile hep aynı dekoru gösterir (titreme yok).
// ------------------------------------------------------------
inline void drawFloorDecor(TFT_eSprite &cv, int sx, int sy, int mx, int my,
                           uint16_t floorCol) {
    uint8_t h = (uint8_t)((mx * 131 + my * 197) ^ (mx * my));
    if (h % DECOR_1_IN != 0) return;
    int px = sx + 1 + ((h >> 3) % 5);            // Tile içi konum x (1..5)
    int py = sy + 2 + ((h >> 5) % 4);            // Tile içi konum y (2..5)
    switch ((h / DECOR_1_IN) % 3) {
        case 0:     // Çakıl taşları (duvar vurgu rengi)
            cv.drawPixel(px, py, BIO_WALL_HL[currentBiome]);
            cv.drawPixel(px + 1, py + 1, BIO_WALL_HL[currentBiome]);
            break;
        case 1:     // Çatlak (zeminin koyusu, L şekli)
            cv.drawPixel(px, py, dimColor(floorCol));
            cv.drawPixel(px, py + 1, dimColor(floorCol));
            cv.drawPixel(px + 1, py + 1, dimColor(floorCol));
            break;
        default:    // Biyom aksanı
            switch (currentBiome) {
                case BIOME_CAVE:    // Küçük kaya çıkıntısı
                    cv.drawPixel(px, py, BIO_WALL[BIOME_CAVE]);
                    cv.drawPixel(px + 1, py, BIO_WALL_HL[BIOME_CAVE]);
                    break;
                case BIOME_FOREST:  // Ot tutamı
                    cv.drawPixel(px, py + 1, dimColor(COL_POTION));
                    cv.drawPixel(px + 1, py, dimColor(COL_POTION));
                    cv.drawPixel(px + 2, py + 1, dimColor(COL_POTION));
                    break;
                case BIOME_HELL:    // Kor parçası
                    cv.drawPixel(px, py, COL_LAVA_DK);
                    cv.drawPixel(px + 1, py, COL_LAVA);
                    break;
                default:            // Zindan: kemik kalıntısı
                    cv.drawPixel(px, py, dimColor(COL_SKELETON));
                    cv.drawPixel(px + 1, py, dimColor(COL_SKELETON));
                    break;
            }
            break;
    }
}

// ------------------------------------------------------------
//  TEK TILE ÇİZİMİ — (sx,sy) ekran pikseli, (mx,my) harita koordinatı
//  fog: FOG_DARK=çizme, FOG_SEEN=soluk (eşyalar gizli), FOG_VIS=parlak
// ------------------------------------------------------------
inline void drawTile(TFT_eSprite &cv, int sx, int sy, int mx, int my,
                     uint8_t t, uint8_t fog) {
    if (fog == FOG_DARK) return;
    bool dim = (fog == FOG_SEEN);

    // Zemin rengi (dama deseni) — biyom paletinden (v2.0).
    // Hatırlanan (dim) alan: Biyom renginin soluk (dimColor) hali.
    uint16_t fl = ((mx + my) & 1) ? BIO_FLOOR_ALT[currentBiome]
                                  : BIO_FLOOR[currentBiome];
    if (dim) fl = dimColor(fl);

    switch (t) {
        case TILE_WALL: {
            if (dim) {   // Hatırlanan duvar: soluk renk + soluk vurgu (v3.1 —
                         // vurgusuz hali keşfedilmemiş karanlıkla karışıyordu)
                cv.fillRect(sx, sy, TILE_PX, TILE_PX, dimColor(BIO_WALL[currentBiome]));
                cv.drawFastHLine(sx, sy, TILE_PX, dimColor(BIO_WALL_HL[currentBiome]));
                break;
            }
            cv.fillRect(sx, sy, TILE_PX, TILE_PX, BIO_WALL[currentBiome]);
            cv.drawFastHLine(sx, sy, TILE_PX, BIO_WALL_HL[currentBiome]);  // Üst kenar 3D vurgusu
            break;
        }
        case TILE_SWAMP: {          // Bataklık (mağara): bulanık yeşil gölcük
            uint16_t c  = dim ? dimColor(COL_SWAMP)    : COL_SWAMP;
            uint16_t dk = dim ? dimColor(COL_SWAMP_DK) : COL_SWAMP_DK;
            cv.fillRect(sx, sy, TILE_PX, TILE_PX, fl);
            cv.fillRect(sx + 1, sy + 2, 6, 4, c);
            bool bp = ((millis() >> 9) + mx + my) & 1;      // Kabarcıklar süzülür (v3.0)
            cv.drawPixel(sx + 2, sy + 3 + (bp ? 1 : 0), dk);
            cv.drawPixel(sx + 5, sy + 4 - (bp ? 1 : 0), dk);
            break;
        }
        case TILE_FLOWER: {         // Zehirli çiçek (orman): sap + mor taç
            cv.fillRect(sx, sy, TILE_PX, TILE_PX, fl);
            if (dim) break;                                 // Fog'da detay gizli
            cv.drawFastVLine(sx + 3, sy + 4, 3, COL_POTION); // Sap
            cv.drawPixel(sx + 3, sy + 2, COL_MAXHP);        // Taç yaprakları
            cv.drawPixel(sx + 2, sy + 3, COL_MAXHP);
            cv.drawPixel(sx + 4, sy + 3, COL_MAXHP);
            cv.drawPixel(sx + 3, sy + 3, COL_KEY);          // Çiçek merkezi
            break;
        }
        case TILE_LAVA: {           // Lav (cehennem): animasyonlu kabarcıklar
            uint16_t c  = dim ? dimColor(COL_LAVA_DK) : COL_LAVA_DK;
            cv.fillRect(sx, sy, TILE_PX, TILE_PX, c);
            if (dim) break;
            bool ph = ((millis() >> 8) + mx + my) & 1;      // Kaynayan lav fazı
            cv.drawPixel(sx + 2, sy + (ph ? 2 : 4), COL_LAVA);
            cv.drawPixel(sx + 5, sy + (ph ? 5 : 3), COL_LAVA);
            cv.drawFastHLine(sx + 1, sy + (ph ? 6 : 1), 3, COL_LAVA);
            break;
        }
        case TILE_DOOR: {
            // Açık kapı: Sadece zemin olarak çizilir, yürüyüşü engellemez, göze batmaz.
            cv.fillRect(sx, sy, TILE_PX, TILE_PX, fl);
            // Çok hafif bir eşik çizgisi (isteğe bağlı, şimdilik sadece zemin)
            break;
        }
        case TILE_STAIRS: {
            // Merdiven nabız gibi parlar (v3.0) — hedef göze çarpsın
            uint16_t c = dim ? dimColor(COL_STAIRS)
                             : (((millis() >> 8) & 1) ? COL_STAIRS : COL_STAIRS_GLOW);
            cv.fillRect(sx, sy, TILE_PX, TILE_PX, fl);
            cv.fillRect(sx + 1, sy + 5, 6, 2, c);           // Alçalan basamaklar
            cv.fillRect(sx + 2, sy + 3, 4, 2, c);
            cv.fillRect(sx + 3, sy + 1, 2, 2, c);
            break;
        }
        case TILE_CHEST: {
            cv.fillRect(sx, sy, TILE_PX, TILE_PX, fl);
            if (dim) break;                                 // Fog'da eşyalar gizli
            cv.fillRect(sx + 1, sy + 2, 6, 5, COL_CHEST);   // Gövde
            cv.drawFastHLine(sx + 1, sy + 3, 6, COL_BG);    // Kapak çizgisi
            cv.drawPixel(sx + 4, sy + 4, COL_KEY);          // Kilit
            // Köşe pırıltısı (v3.0): ~1 sn'de bir kısa beyaz parlar
            if ((((millis() >> 7) + mx + my) & 7) == 0)
                cv.drawPixel(sx + 2, sy + 2, COL_SLASH);
            break;
        }
        case TILE_PILLAR: {
            // Boss arenası sütunu (v3.2): her biyomda net seçilir —
            // duvar gövdesi + siyah kontur + açık tepe vurgusu.
            // Bitişik sütun tile'ları arasına kontur çizilmez → 2x2
            // grup tek büyük blok olarak okunur.
            uint16_t body = dim ? dimColor(BIO_WALL[currentBiome])
                                : BIO_WALL[currentBiome];
            uint16_t hl   = dim ? dimColor(BIO_WALL_HL[currentBiome])
                                : BIO_WALL_HL[currentBiome];
            cv.fillRect(sx, sy, TILE_PX, TILE_PX, body);
            if (tileAt(mx, my - 1) != TILE_PILLAR) {
                cv.drawFastHLine(sx, sy, TILE_PX, COL_BG);
                cv.drawFastHLine(sx, sy + 1, TILE_PX, hl);      // Tepe vurgusu
            }
            if (tileAt(mx, my + 1) != TILE_PILLAR)
                cv.drawFastHLine(sx, sy + TILE_PX - 1, TILE_PX, COL_BG);
            if (tileAt(mx - 1, my) != TILE_PILLAR)
                cv.drawFastVLine(sx, sy, TILE_PX, COL_BG);
            if (tileAt(mx + 1, my) != TILE_PILLAR)
                cv.drawFastVLine(sx + TILE_PX - 1, sy, TILE_PX, COL_BG);
            break;
        }
        case TILE_LOCKED: {
            // Kilitli Kapı: Devasa bir asma kilit (Altın renkli)
            cv.fillRect(sx, sy, TILE_PX, TILE_PX, fl);
            uint16_t c = dim ? dimColor(COL_KEY) : COL_KEY;
            // Asma kilit halkası (U şekli)
            cv.drawFastHLine(sx + 3, sy + 1, 2, COL_WALL);
            cv.drawPixel(sx + 2, sy + 2, COL_WALL);
            cv.drawPixel(sx + 5, sy + 2, COL_WALL);
            // Kilit Gövdesi
            cv.fillRect(sx + 2, sy + 3, 4, 4, c);
            // Anahtar deliği
            cv.drawFastVLine(sx + 3, sy + 4, 2, COL_BG);
            cv.drawFastVLine(sx + 4, sy + 4, 2, COL_BG);
            break;
        }
        default: // TILE_FLOOR
            cv.fillRect(sx, sy, TILE_PX, TILE_PX, fl);
            if (!dim) drawFloorDecor(cv, sx, sy, mx, my, fl);  // Seyrek dekor (v3.1)
            break;
    }
}

// ------------------------------------------------------------
//  HARİTA — sadece viewport içindeki tile'lar (+1 sarsıntı marjı)
// ------------------------------------------------------------
inline void drawMap(TFT_eSprite &cv, int camX, int camY, int shX, int shY) {
    for (int r = -1; r <= VIEW_ROWS; r++) {
        int my = camY + r;
        if (my < 0 || my >= MAP_H) continue;
        for (int c = -1; c <= VIEW_COLS; c++) {
            int mx = camX + c;
            if (mx < 0 || mx >= MAP_W) continue;
            int sx = c * TILE_PX + shX;
            int sy = HUD_H + r * TILE_PX + shY;
            drawTile(cv, sx, sy, mx, my, tiles[my][mx], fogMap[my][mx]);
        }
    }
}

// ------------------------------------------------------------
//  OYUNCU SPRITE — "Savaşçı" tasarımı (Kılıç, Zırh ve Miğfer)
//  Yürüme animasyonu: Adım atınca bacaklar sallanır.
// ------------------------------------------------------------
inline void drawPlayerSprite(TFT_eSprite &cv, int sx, int sy, int dir) {
    uint32_t ms = millis();
    // Yürüme + adım fazı (v3.1): kayma animasyonu sürdüğü sürece yürür,
    // bacaklar adım İLERLEMESİNE bağlı değişir (adım başına bir makas)
    uint32_t moveEl = ms - player.lastMoveMs;
    bool walking = moveEl < MOVE_ANIM_MS &&
                   (player.prevTileX != player.tileX ||
                    player.prevTileY != player.tileY);
    int wf = stepPhase(moveEl, MOVE_ANIM_MS, player.tileX, player.tileY);

    // Saldırı atılımı (v3.0): vururken kısa süre yöne doğru öne kayar
    uint32_t lungeEl = ms - player.lastAttackMs;
    if (lungeEl < ATTACK_LUNGE_MS) {
        sx += DIR_DX[dir] * ATTACK_LUNGE_PX;
        sy += DIR_DY[dir] * ATTACK_LUNGE_PX;
    }

    // Gölge (v3.1): sprite'tan önce, ayakların altındaki zemini karartır
    drawEntityShadow(cv, sx + 2, sy + TILE_PX - 1, SHADOW_W_SMALL);

    // Boşta nefes alma (v3.0): dururken gövde ~0.5 sn'de bir 1px iner
    int bob = (!walking && ((ms >> 9) & 1)) ? 1 : 0;

    int by = sy + 1 + bob; // Gövde yüksekliği (bacaklar zeminde sabit kalır)

    // Baş (Miğfer)
    cv.fillRect(sx + 3, by, 2, 2, COL_HUD_TEXT);
    // Gövde (Tunika)
    cv.fillRect(sx + 2, by + 2, 4, 3, COL_PLAYER);

    // Yön Detayları (Kılıç, Kollar, Göz)
    int atkFrame = 0;
    uint32_t atkElapsed = ms - player.lastAttackMs;
    if (atkElapsed < 50) atkFrame = 1;
    else if (atkElapsed < 120) atkFrame = 2;

    if (dir == DIR_DOWN) {
        cv.drawPixel(sx + 3, by + 1, COL_BG); // Sol Göz
        cv.drawPixel(sx + 4, by + 1, COL_BG); // Sağ Göz
        cv.drawFastVLine(sx + 1, by + 2, 2, COL_WALL_HL); // Sol Kol
        cv.drawFastVLine(sx + 6, by + 2, 2, COL_WALL_HL); // Sağ Kol
        if (atkFrame == 1)      cv.drawFastVLine(sx + 6, by + 4, 3, COL_HUD_TEXT); // Kılıç kalkıyor
        else if (atkFrame == 2) cv.drawFastHLine(sx + 2, by + 6, 6, COL_HUD_TEXT); // Yatay savurma
        else                    cv.drawFastVLine(sx + 6, by + 4, 2, COL_HUD_TEXT); // Dinlenme
    } else if (dir == DIR_UP) {
        cv.drawFastVLine(sx + 1, by + 2, 2, COL_WALL_HL); // Sol Kol
        cv.drawFastVLine(sx + 6, by + 2, 2, COL_WALL_HL); // Sağ Kol
        if (atkFrame == 1)      cv.drawFastVLine(sx + 6, by - 2, 3, COL_HUD_TEXT); 
        else if (atkFrame == 2) cv.drawFastHLine(sx, by - 2, 6, COL_HUD_TEXT); 
        else                    cv.drawFastVLine(sx + 6, by - 1, 2, COL_HUD_TEXT); 
    } else if (dir == DIR_RIGHT) {
        cv.drawPixel(sx + 4, by + 1, COL_BG); // Sağ Göz
        cv.drawFastVLine(sx + 3, by + 2, 2, COL_WALL_HL); // Kol
        if (atkFrame == 1)      cv.drawFastHLine(sx + 4, by + 1, 3, COL_HUD_TEXT);
        else if (atkFrame == 2) cv.drawFastVLine(sx + 7, by, 6, COL_HUD_TEXT);
        else                    cv.drawFastHLine(sx + 4, by + 3, 3, COL_HUD_TEXT); 
    } else if (dir == DIR_LEFT) {
        cv.drawPixel(sx + 3, by + 1, COL_BG); // Sol Göz
        cv.drawFastVLine(sx + 4, by + 2, 2, COL_WALL_HL); // Kol
        if (atkFrame == 1)      cv.drawFastHLine(sx + 1, by + 1, 3, COL_HUD_TEXT);
        else if (atkFrame == 2) cv.drawFastVLine(sx - 1, by, 6, COL_HUD_TEXT);
        else                    cv.drawFastHLine(sx + 1, by + 3, 3, COL_HUD_TEXT); 
    }

    // Bacaklar (Zemine sabit) — yürürken faz adım ilerlemesinden gelir (v3.1)
    int ly = sy + 6;
    if (walking) {
        if (dir == DIR_LEFT || dir == DIR_RIGHT) {
            // Yan yürüme: bacaklar makas gibi açılır
            int legX = (dir == DIR_LEFT) ? sx + 3 : sx + 4;
            if (wf == 0) {
                cv.drawPixel(legX - 1, ly, COL_WALL_HL);
                cv.drawPixel(legX + 1, ly + 1, COL_WALL_HL);
            } else {
                cv.drawPixel(legX + 1, ly, COL_WALL_HL);
                cv.drawPixel(legX - 1, ly + 1, COL_WALL_HL);
            }
        } else {
            // Aşağı/Yukarı yürüme
            if (wf == 0) {
                cv.drawFastVLine(sx + 2, ly, 2, COL_WALL_HL);
                cv.drawPixel(sx + 5, ly, COL_WALL_HL);
            } else {
                cv.drawPixel(sx + 2, ly, COL_WALL_HL);
                cv.drawFastVLine(sx + 5, ly, 2, COL_WALL_HL);
            }
        }
    } else {
        cv.drawPixel(sx + 2, ly, COL_WALL_HL);
        cv.drawPixel(sx + 5, ly, COL_WALL_HL);
    }
}

// ------------------------------------------------------------
//  BİTMAP SPRITE SİSTEMİ (v3.1) — karakter haritalı piksel sanat.
//  Harf → renk: '.'=şeffaf, B=gövde (dinamik), D=gövde koyusu,
//  K=kontur (siyah), E=kırmızı göz, W=beyaz, S=kemik/kafatası,
//  G=yeşil göz, C=asa/ahşap, P=küre (cyan), O=turuncu göz.
//  Vuruş/faz flaşında şeffaf olmayan TÜM pikseller beyaz çizilir.
// ------------------------------------------------------------
inline void drawCharmap(TFT_eSprite &cv, int sx, int sy,
                        const char *const *rows, int w, int h,
                        uint16_t body, uint16_t dark, bool whiteFlash) {
    for (int ry = 0; ry < h; ry++) {
        const char *row = rows[ry];
        for (int rx = 0; rx < w; rx++) {
            char ch = row[rx];
            if (ch == '.') continue;
            uint16_t col;
            if (whiteFlash) col = COL_SLASH;
            else switch (ch) {
                case 'B': col = body;         break;
                case 'D': col = dark;         break;
                case 'K': col = COL_BG;       break;
                case 'E': col = COL_HP_LOW;   break;
                case 'W': col = COL_SLASH;    break;
                case 'S': col = COL_SKELETON; break;
                case 'G': col = COL_POTION;   break;
                case 'C': col = COL_CHEST;    break;
                case 'P': col = COL_PLAYER;   break;
                case 'O': col = COL_LAVA;     break;
                default:  continue;
            }
            cv.drawPixel(sx + rx, sy + ry, col);
        }
    }
}

// --- Düşman gövde bitmapleri (8 sütun x 6 satır; bacaklar ayrı) ---
// Yarasa: 2 kare kanat çırpışı (yüksek / gövde hizası), kırmızı gözler
static const char *const SPR_BAT_F0[SPR_BODY_ROWS] = {
    "........",
    ".B....B.",
    "BBB..BBB",
    ".BBEEBB.",
    "..BBBB..",
    "...BB...",
};
static const char *const SPR_BAT_F1[SPR_BODY_ROWS] = {
    "........",
    "........",
    "........",
    "BBBEEBBB",
    ".BBBBBB.",
    "...BB...",
};
// Goblin: sivri kulaklar, siyah kontur (zeminden ayrışır), koyu peştamal
static const char *const SPR_GOBLIN[SPR_BODY_ROWS] = {
    ".B....B.",
    "KBBBBBBK",
    "KBEBBEBK",
    "KBBBBBBK",
    ".KBDDBK.",
    ".KBBBBK.",
};
// İskelet: kafatası + göz oyukları (kontur), çene, omurga, kaburga
static const char *const SPR_SKELETON[SPR_BODY_ROWS] = {
    ".BBBBBB.",
    ".BKBBKB.",
    "..BBBB..",
    "...BB...",
    ".BBBBBB.",
    ".BDBBDB.",
};

// Bacaklar (goblin/iskelet ortak): satır 6-7. Boşta iki bacak sabit
// durur, yürürken adım fazına göre biri uzanır (v3.1).
inline void drawEnemyLegs(TFT_eSprite &cv, int sx, int sy, uint16_t c,
                          bool moving, int phase) {
    int ly = sy + SPR_BODY_ROWS;
    if (!moving) {
        cv.drawFastVLine(sx + 2, ly, 2, c);
        cv.drawFastVLine(sx + 5, ly, 2, c);
    } else if (phase == 0) {
        cv.drawFastVLine(sx + 2, ly, 2, c);
        cv.drawPixel(sx + 5, ly, c);
    } else {
        cv.drawPixel(sx + 2, ly, c);
        cv.drawFastVLine(sx + 5, ly, 2, c);
    }
}

// ------------------------------------------------------------
//  DÜŞMAN KAYMA SÜRESİ (v3.2) — hamle ARALIĞINA eşitlenir: düşman
//  bir sonraki hamlesine dek sürekli yürür (eski 130 ms'lik ani
//  sıçrama + uzun donma "2-3 blok birden geliyor" hissi veriyordu).
//  Öfkeli goblin aralığı yarıya iner → kayma da yarıya iner.
// ------------------------------------------------------------
inline uint32_t enemyAnimMs(const Enemy &e) {
    uint32_t d = (uint32_t)e.moveTimer * MS_PER_TICK;
    if (e.type == ENEMY_GOBLIN &&
        abs(player.tileX - e.tileX) + abs(player.tileY - e.tileY) <= GOBLIN_RAGE_RANGE)
        d /= 2;
    if (d < ENEMY_ANIM_MS) d = ENEMY_ANIM_MS;
    return d;
}

// Boss kayması da hamle aralığına eşitlenir (v3.2 — golem "yürümüyor"
// görünüyordu: seyrek hamle + kısa kayma = yerinde duran blok)
inline uint32_t bossAnimMs() {
    uint32_t d = (uint32_t)boss.moveTimer * MS_PER_TICK;
    if (d < ENEMY_ANIM_MS) d = ENEMY_ANIM_MS;
    return d;
}

// ------------------------------------------------------------
//  DÜŞMAN SPRITE'LARI — bitmap gövde + adım-senkron bacaklar (v3.1)
// ------------------------------------------------------------
inline void drawEnemySprite(TFT_eSprite &cv, int sx, int sy, const Enemy &e) {
    uint32_t ms = millis();
    // Hasar flaşı (v3.0): vurulan düşman kısa süre beyaza yanar
    bool flash = (ms < e.hitFlashUntil);
    uint16_t body = flash ? COL_SLASH : e.color;
    uint16_t dark = dimColor(e.color);

    // Saldırı atılımı (v3.2): vururken oyuncuya doğru 2 px atılır
    if (ms - e.lastAttackMs < ENEMY_LUNGE_SHOW_MS && e.atkDir >= 0) {
        sx += DIR_DX[e.atkDir] * ATTACK_LUNGE_PX;
        sy += DIR_DY[e.atkDir] * ATTACK_LUNGE_PX;
    }

    // Gölge: sprite'tan önce (v3.1)
    drawEntityShadow(cv, sx + 2, sy + TILE_PX - 1, SHADOW_W_SMALL);

    // Bacak fazı hareket ilerlemesinden (v3.1); süre hamle aralığı (v3.2)
    uint32_t animMs = enemyAnimMs(e);
    uint32_t mel = ms - e.lastMoveMs;
    bool moving = mel < animMs && (e.prevX != e.tileX || e.prevY != e.tileY);
    int ph = stepPhase(mel, animMs, e.tileX, e.tileY);

    // Boşta nefes (v3.2): dururken gövde ~0.5 sn'de 1 px iner —
    // tile paritesiyle desenkron (sürü aynı anda inip kalkmasın)
    int bob = (!moving && (((ms >> 9) + e.tileX + e.tileY) & 1)) ? 1 : 0;

    switch (e.type) {
        case ENEMY_BAT: {
            bool flap = (ms >> BAT_FLAP_SHIFT) & 1;   // Uçuş: sürekli çırpar
            drawCharmap(cv, sx, sy, flap ? SPR_BAT_F1 : SPR_BAT_F0,
                        TILE_PX, SPR_BODY_ROWS, body, dark, flash);
            break;
        }
        case ENEMY_GOBLIN:
            drawCharmap(cv, sx, sy + bob, SPR_GOBLIN,
                        TILE_PX, SPR_BODY_ROWS, body, dark, flash);
            drawEnemyLegs(cv, sx, sy, body, moving, ph);
            break;
        default: // ENEMY_SKELETON
            drawCharmap(cv, sx, sy + bob, SPR_SKELETON,
                        TILE_PX, SPR_BODY_ROWS, body, dark, flash);
            drawEnemyLegs(cv, sx, sy, body, moving, ph);
            break;
    }
    // Hasarlı düşmanın üstünde mini can barı
    if (e.hp < e.maxHp && sy - 2 >= HUD_H) {
        int w = (TILE_PX * e.hp) / e.maxHp;
        if (w < 1) w = 1;
        cv.drawFastHLine(sx, sy - 2, w, COL_HP_LOW);
    }
}

// ------------------------------------------------------------
//  TÜCCAR SPRITE'I (v2.0) — 8x8 altın cübbeli NPC
// ------------------------------------------------------------
inline void drawMerchantSprite(TFT_eSprite &cv, int sx, int sy) {
    drawEntityShadow(cv, sx + 2, sy + TILE_PX - 1, SHADOW_W_SMALL);  // v3.1
    cv.fillRect(sx + 2, sy + 3, 4, 5, COL_CHEST);           // Cübbe
    cv.fillRect(sx + 1, sy + 1, 6, 1, COL_KEY);             // Şapka kenarı
    cv.drawPixel(sx + 3, sy, COL_KEY);                      // Şapka tepesi
    cv.drawPixel(sx + 4, sy, COL_KEY);
    cv.drawPixel(sx + 3, sy + 2, COL_BG);                   // Gözler
    cv.drawPixel(sx + 5, sy + 2, COL_BG);
    // Yanıp sönen para kesesi (dikkat çekici)
    if ((millis() >> 8) & 1) cv.drawPixel(sx + 7, sy + 5, COL_KEY);
}

// ------------------------------------------------------------
//  BOSS SPRITE'LARI — 16x16 bitmap piksel sanat (v3.1; eski sürüm
//  düz dikdörtgen bloklardı). Gövde rengi faza göre değişir,
//  vuruş/faz flaşında tüm silüet beyaz yanar.
// ------------------------------------------------------------
// Ejderha: boynuzlar, kırmızı gözler, koyu karın plakaları, sola
// kıvrılan kuyruk; 2 kare kanat çırpışı (yukarıda / aşağıda)
static const char *const SPR_DRG_F0[BOSS_SPR_PX] = {
    "....W......W....",
    "....B......B....",
    "....BB....BB....",
    ".....BBBBBB.....",
    ".....BEBBEB.....",
    ".....BBBBBB.....",
    "W.....BDDB.....W",
    "BB....BBBB....BB",
    "BBB..BBBBBB..BBB",
    "BBBB.BBDDBB.BBBB",
    ".DBBBBBDDBBBBBD.",
    "..DDBBBDDBBBDD..",
    ".....BBBBBB.....",
    "......BBBB......",
    ".......BBB......",
    "....WBBB........",
};
static const char *const SPR_DRG_F1[BOSS_SPR_PX] = {
    "....W......W....",
    "....B......B....",
    "....BB....BB....",
    ".....BBBBBB.....",
    ".....BEBBEB.....",
    ".....BBBBBB.....",
    "......BDDB......",
    "......BBBB......",
    ".....BBBBBB.....",
    "BB...BBDDBB...BB",
    "BBB.BBBDDBBB.BBB",
    "WBBBBBBDDBBBBBBW",
    ".DD..BBBBBB..DD.",
    "......BBBB......",
    ".......BBB......",
    "....WBBB........",
};
// Lich: kukuletalı kafatası (yeşil gözler), kıvrımlı cübbe, kemik el
// ve küreli asa; tek harita — kare değişiminde 1 px süzülür (levitasyon)
static const char *const SPR_LICH[BOSS_SPR_PX] = {
    ".............P..",
    "....BBBB.....C..",
    "...BBBBBBBB..C..",
    "...BSSSSSSB..C..",
    "...BSGSSGSB..C..",
    "....BSSSSB...C..",
    "...BBBBBBBB..C..",
    "..BBBBBBBBBB.C..",
    "..BDBBBBBBDBWC..",
    "..BDBBBBBBDB.C..",
    "..BDBBBBBBDB.C..",
    ".BBBBBBBBBBB.C..",
    ".BBBBBBBBBBB.C..",
    ".BDBDBDBDBDB.C..",
    ".............C..",
    "................",
};
// Golem (v3.3 yeniden çizim): TEK bütün silüet — eski çizimde ayrık
// kollar kendi konturlarıyla gövdeyi parçalıyor, "ne olduğu belirsiz
// blok" görünüyordu. Yeni tasarım: kafa omuzlara gömülü, 2 px çift
// turuncu göz, geniş tek gövde, yanlara bitişik kollar, İKİ AYRI
// KALIN BACAK. Yürüyüş kareleri bacak dönüşümlü BASAR (F0 sol yerde
// sağ havada, F1 tersi) — kare seçimi adıma senkron (drawBossSprite),
// dururken heykel gibi sabit. D = taş çatlakları.
static const char *const SPR_GLM_F0[BOSS_SPR_PX] = {
    "....KKKKKKK.....",
    "...KBDBBBDBK....",
    "...KBOOBOOBK....",
    "...KBBBBBBBK....",
    "..KKBBBBBBBKK...",
    ".KBBBBBBBBBBBK..",
    ".KBDBBBDBBBDBK..",
    ".KBBKBBBBBKBBK..",
    ".KBDKBBDBBKBDK..",
    ".KBBKBBBBBKBBK..",
    ".KKK.KBBBK.KKK..",
    "...KBBBBBBBK....",
    "...KBBBKBBBK....",
    "...KBDBKBBBK....",
    "...KBBBKKKKK....",
    "...KKKKK........",
};
static const char *const SPR_GLM_F1[BOSS_SPR_PX] = {
    "....KKKKKKK.....",
    "...KBDBBBDBK....",
    "...KBOOBOOBK....",
    "...KBBBBBBBK....",
    "..KKBBBBBBBKK...",
    ".KBBBBBBBBBBBK..",
    ".KBDBBBDBBBDBK..",
    ".KBDKBBBBBKBDK..",
    ".KBBKBBDBBKBBK..",
    ".KBBKBBBBBKBBK..",
    ".KKK.KBBBK.KKK..",
    "...KBBBBBBBK....",
    "...KBBBKBBBK....",
    "...KBBBKBDBK....",
    "...KKKKKBBBK....",
    ".......KKKKK....",
};

inline void drawBossSprite(TFT_eSprite &cv, int sx, int sy) {
    uint32_t ms = millis();
    bool flash = (ms < flashUntil);
    uint16_t c  = flash ? COL_SLASH : boss.color;
    uint16_t dk = dimColor(boss.color);
    bool frame = (ms >> BOSS_ANIM_SHIFT) & 1;   // ~2 Hz kanat/kol/levitasyon

    // Saldırı atılımı (v3.2): vururken oyuncuya doğru 2 px atılır
    if (ms - boss.lastAttackMs < ENEMY_LUNGE_SHOW_MS && boss.atkDir >= 0) {
        sx += DIR_DX[boss.atkDir] * ATTACK_LUNGE_PX;
        sy += DIR_DY[boss.atkDir] * ATTACK_LUNGE_PX;
    }

    // Gölge: sprite'tan önce (v3.1)
    drawEntityShadow(cv, sx + 3, sy + BOSS_SPR_PX - 1, SHADOW_W_BOSS);

    // Öfke aurası (v3.0): faz 2+ boss'un çevresinde titreşen çerçeve
    if (boss.phase >= 2 && frame)
        cv.drawRect(sx - 1, sy - 1, BOSS_SPR_PX + 2, BOSS_SPR_PX + 2,
                    dimColor(boss.color));

    switch (boss.type) {
        case ENEMY_BOSS_DRAGON:
            drawCharmap(cv, sx, sy, frame ? SPR_DRG_F1 : SPR_DRG_F0,
                        BOSS_SPR_PX, BOSS_SPR_PX, c, dk, flash);
            break;
        case ENEMY_BOSS_LICH:
            drawCharmap(cv, sx, sy + (frame ? 1 : 0), SPR_LICH,
                        BOSS_SPR_PX, BOSS_SPR_PX, c, dk, flash);
            break;
        default: { // ENEMY_BOSS_GOLEM — kare adıma senkron (v3.3)
            // Duvar saati (~2 Hz) yerine hareket ilerlemesi: kayma
            // sırasında bacaklar dönüşümlü basar, dururken F0'da
            // heykel gibi sabit durur (taş devi teması).
            uint32_t mel = ms - boss.lastMoveMs;
            uint32_t bAnim = bossAnimMs();
            bool walking = boss.lastMoveMs != 0 && mel < bAnim &&
                           (boss.prevX != boss.tileX || boss.prevY != boss.tileY);
            int gf = walking ? stepPhase(mel, bAnim, boss.tileX, boss.tileY) : 0;
            drawCharmap(cv, sx, sy, gf ? SPR_GLM_F1 : SPR_GLM_F0,
                        BOSS_SPR_PX, BOSS_SPR_PX, c, dk, flash);
            break;
        }
    }
}

// ------------------------------------------------------------
//  VARLIKLAR — düşmanlar (sadece fog=görünür) + tüccar + boss +
//  oyuncu + durum efekti göstergeleri.
//  v3.0: (offX,offY) dünya-piksel → ekran dönüşümü; tüm varlıklar
//  tile-arası kayma (lerp) pozisyonundan çizilir.
// ------------------------------------------------------------
inline void drawEntities(TFT_eSprite &cv, int offX, int offY) {
    uint32_t ms = millis();

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy &e = enemies[i];
        if (!e.alive) continue;
        if (fogMap[e.tileY][e.tileX] != FOG_VIS) continue;  // Fog'da düşman görünmez
        uint32_t el = ms - e.lastMoveMs;
        uint32_t animMs = enemyAnimMs(e);            // Hamle aralığı boyu kayma (v3.2)
        int sx = lerpTilePx(e.prevX, e.tileX, el, animMs) + offX;
        int sy = lerpTilePx(e.prevY, e.tileY, el, animMs) + offY;
        if (sx < -TILE_PX || sx >= SCR_W || sy < HUD_H - TILE_PX || sy >= SCR_H) continue;
        drawEnemySprite(cv, sx, sy, e);
        // Yavaşlatılmış düşman: üstünde mavi nokta (v2.0)
        if (ms < e.slowedUntil && sy - 4 >= HUD_H)
            cv.drawPixel(sx + TILE_PX / 2, sy - 4, COL_SHIELD);
    }

    // Tüccar (v2.0) — sabit durur, lerp gerekmez
    if (merchant.active && fogMap[merchant.tileY][merchant.tileX] == FOG_VIS) {
        int sx = merchant.tileX * TILE_PX + offX;
        int sy = merchant.tileY * TILE_PX + offY;
        if (sx > -TILE_PX && sx < SCR_W && sy > HUD_H - TILE_PX && sy < SCR_H)
            drawMerchantSprite(cv, sx, sy);
    }

    // Boss (v2.0) — 16x16, kayma animasyonlu (v3.0; süre = hamle aralığı v3.2)
    if (boss.active) {
        uint32_t el = ms - boss.lastMoveMs;
        uint32_t bAnim = bossAnimMs();
        int sx = lerpTilePx(boss.prevX, boss.tileX, el, bAnim) + offX;
        int sy = lerpTilePx(boss.prevY, boss.tileY, el, bAnim) + offY;
        drawBossSprite(cv, sx, sy);
    }

    // Oyuncu — kayma animasyonlu (v3.0)
    int wpx, wpy;
    playerRenderPos(wpx, wpy);
    int px = wpx + offX;
    int py = wpy + offY;
    drawPlayerSprite(cv, px, py, player.dir);

    // Durum efekti göstergeleri (v2.0): oyuncunun üstünde ikonlar
    int ix = px, iy = py - 3;
    if (iy >= HUD_H) {
        if (status.poisonStacks > 0) { cv.fillRect(ix, iy, 2, 2, COL_POTION);   ix += 3; }
        if (status.stunned())        { cv.fillRect(ix, iy, 2, 2, COL_KEY);      ix += 3; }
        if (status.slowed())         { cv.fillRect(ix, iy, 2, 2, COL_EMPTY_SLOT); }
    }
    // Kalkan: oyuncu etrafında mavi halka
    if (status.shielded())
        cv.drawCircle(px + TILE_PX / 2, py + TILE_PX / 2, TILE_PX / 2 + 2, COL_SHIELD);

    // Seviye atlama efekti (v3.0): oyuncudan genişleyen altın halka
    uint32_t lel = ms - levelUpFxStart;
    if (levelUpFxStart != 0 && lel < LEVELUP_FX_MS) {
        int cx = px + TILE_PX / 2, cy = py + TILE_PX / 2;
        int r = 2 + (int)((LEVELUP_FX_R * lel) / LEVELUP_FX_MS);
        cv.drawCircle(cx, cy, r, COL_KEY);
        if (r > 5) cv.drawCircle(cx, cy, r - 4, COL_SLASH);
    }
}

// ------------------------------------------------------------
//  TELEGRAF UYARILARI (v2.0) — boss saldırı alanı yanıp söner
// ------------------------------------------------------------
inline void drawTelegraphs(TFT_eSprite &cv, int offX, int offY) {
    bool blinkOn = (millis() >> 6) & 1;   // ~8 Hz yanıp sönme
    for (int i = 0; i < MAX_TELEGRAPHS; i++) {
        const Telegraph &t = telegraphs[i];
        if (!t.active) continue;
        for (int k = 0; k < t.count; k++) {
            int sx = t.tx[k] * TILE_PX + offX;
            int sy = t.ty[k] * TILE_PX + offY;
            if (sx < -TILE_PX || sx >= SCR_W || sy < HUD_H - TILE_PX || sy >= SCR_H) continue;
            if (blinkOn) cv.fillRect(sx + 1, sy + 1, TILE_PX - 2, TILE_PX - 2, dimColor(t.color));
            cv.drawRect(sx, sy, TILE_PX, TILE_PX, t.color);
        }
    }
}

// ------------------------------------------------------------
//  BÜYÜ MERMİLERİ (v3.2) — Lich'in mor küresi hedefe süzülür;
//  hedef tile yanıp sönen çerçeveyle işaretlenir (kaçış ipucu).
// ------------------------------------------------------------
inline void drawBolts(TFT_eSprite &cv, int offX, int offY) {
    uint32_t ms = millis();
    for (int i = 0; i < MAX_BOLTS; i++) {
        const BossBolt &b = bolts[i];
        if (!b.active) continue;
        uint32_t el = ms - b.startMs;
        if (el > LICH_BOLT_MS) el = LICH_BOLT_MS;

        // Hedef tile uyarısı (~8 Hz yanıp söner)
        int tx = b.tx * TILE_PX + offX;
        int ty = b.ty * TILE_PX + offY;
        if ((ms >> 6) & 1)
            cv.drawRect(tx, ty, TILE_PX, TILE_PX, COL_LICH);

        // Mermi: çıkıştan hedef merkezine doğrusal uçuş
        float t = (float)el / LICH_BOLT_MS;
        float gx = b.tx * TILE_PX + TILE_PX / 2;
        float gy = b.ty * TILE_PX + TILE_PX / 2;
        int bx = (int)(b.x0 + (gx - b.x0) * t) + offX;
        int by = (int)(b.y0 + (gy - b.y0) * t) + offY;
        if (bx < 1 || bx >= SCR_W - 1 || by < HUD_H + 1 || by >= SCR_H - 1) continue;
        cv.fillRect(bx - 1, by - 1, 3, 3, COL_LICH);     // Küre
        cv.drawPixel(bx, by, COL_SLASH);                 // Parlak çekirdek
        int trx = (int)(b.x0 + (gx - b.x0) * (t * 0.8f)) + offX;   // Kuyruk
        int try_ = (int)(b.y0 + (gy - b.y0) * (t * 0.8f)) + offY;
        if (trx >= 0 && trx < SCR_W && try_ >= HUD_H && try_ < SCR_H)
            cv.drawPixel(trx, try_, dimColor(COL_LICH));
    }
}

// ------------------------------------------------------------
//  BIÇAK İZİ — saldırı yönüne dik kısa parlak çizgi
// ------------------------------------------------------------
inline void drawSlashes(TFT_eSprite &cv, int offX, int offY) {
    for (int i = 0; i < MAX_SLASHES; i++) {
        const Slash &s = slashes[i];
        if (!s.active) continue;
        int sx = s.tileX * TILE_PX + offX;
        int sy = s.tileY * TILE_PX + offY;
        if (s.dir == DIR_UP || s.dir == DIR_DOWN) {
            cv.drawFastHLine(sx + 1, sy + TILE_PX / 2, TILE_PX - 2, COL_SLASH);
            cv.drawFastHLine(sx + 2, sy + TILE_PX / 2 + 1, TILE_PX - 4, COL_KEY);
        } else {
            cv.drawFastVLine(sx + TILE_PX / 2, sy + 1, TILE_PX - 2, COL_SLASH);
            cv.drawFastVLine(sx + TILE_PX / 2 + 1, sy + 2, TILE_PX - 4, COL_KEY);
        }
    }
}

// ------------------------------------------------------------
//  HASAR POPUP'LARI — "-3" yazısı yükselerek kaybolur
// ------------------------------------------------------------
inline void drawPopups(TFT_eSprite &cv, int offX, int offY) {
    cv.setTextSize(1);
    for (int i = 0; i < MAX_POPUPS; i++) {
        const DmgPopup &p = popups[i];
        if (!p.active) continue;
        int sx = (int)p.x + offX - FONT_W;
        int sy = (int)p.y + offY - FONT_H;
        if (sy < HUD_H || sy >= SCR_H - FONT_H) continue;
        cv.setTextColor(p.color);
        cv.setCursor(sx, sy);
        cv.print('-');
        cv.print(p.val);
    }
}

// ------------------------------------------------------------
//  HUD — HP barı, kat, seviye, (koşullu) FPS + ayırıcı çizgi
// ------------------------------------------------------------
inline void drawHUD(TFT_eSprite &cv, int floorNum, int fps, bool showFpsFlag) {
    cv.fillRect(0, 0, SCR_W, HUD_H, COL_BG);
    cv.setTextSize(1);

    // HP etiketi + bar (üst) — v2.0: mana barına yer açmak için incelt
    cv.setTextColor(COL_HUD_TEXT);
    cv.setCursor(2, HUD_TEXT_Y);
    cv.print("HP");
    cv.drawRect(HUD_BAR_X, HUD_HP_Y, HUD_BAR_W, HUD_HP_H, COL_HUD_LINE);
    int pct = (player.maxHp > 0) ? (player.hp * 100) / player.maxHp : 0;
    uint16_t barCol = (pct > HP_MID_PCT) ? COL_HP_FULL
                     : (pct > HP_LOW_PCT) ? COL_HP_MID : COL_HP_LOW;
    // Kritik HP nabzı (v3.0): bar ~4 Hz parlak/soluk atar
    if (pct <= HP_LOW_PCT && ((millis() >> 7) & 1))
        barCol = dimColor(COL_HP_LOW);
    int w = ((HUD_BAR_W - 2) * player.hp) / (player.maxHp > 0 ? player.maxHp : 1);
    if (w > 0) cv.fillRect(HUD_BAR_X + 1, HUD_HP_Y + 1, w, HUD_HP_H - 2, barCol);

    // Mana barı (alt, açık mavi) — v2.0
    cv.drawRect(HUD_BAR_X, HUD_MANA_Y, HUD_BAR_W, HUD_MANA_H, COL_HUD_LINE);
    int mw = ((HUD_BAR_W - 2) * player.mana) / (player.maxMana > 0 ? player.maxMana : 1);
    if (mw > 0) cv.fillRect(HUD_BAR_X + 1, HUD_MANA_Y + 1, mw, HUD_MANA_H - 2, COL_MANA);

    // Kat (cyan)
    cv.setTextColor(COL_PLAYER);
    cv.setCursor(HUD_FLOOR_X, HUD_TEXT_Y);
    cv.print("F:");
    cv.print(floorNum);

    // Seviye (sarı)
    cv.setTextColor(COL_KEY);
    cv.setCursor(HUD_LVL_X, HUD_TEXT_Y);
    cv.print("LV:");
    cv.print(player.lvl);

    // Altın (turuncu) — FPS açıkken de görünür
    cv.setTextColor(COL_CHEST);
    cv.setCursor(HUD_GOLD_X, HUD_TEXT_Y);
    cv.print("G:");
    cv.print(player.gold);

    // FPS (koşullu, sağ köşe)
    if (showFpsFlag) {
        cv.setTextColor(COL_HUD_TEXT);
        cv.setCursor(HUD_FPS_X, HUD_TEXT_Y);
        cv.print(fps);
    }

    cv.drawFastHLine(0, HUD_H - 1, SCR_W, COL_HUD_LINE);
}

// ------------------------------------------------------------
//  BOSS HP BARI (v2.0) — oyun alanının üstünde geniş kırmızı bar
// ------------------------------------------------------------
inline void drawBossBar(TFT_eSprite &cv) {
    if (!boss.active) return;
    cv.drawRect(BOSSBAR_X, BOSSBAR_Y, BOSSBAR_W, BOSSBAR_H, COL_HUD_TEXT);
    int w = ((BOSSBAR_W - 2) * boss.hp) / boss.maxHp;
    if (w > 0) cv.fillRect(BOSSBAR_X + 1, BOSSBAR_Y + 1, w, BOSSBAR_H - 2,
                           (boss.phase >= 3) ? COL_BOSS_P3
                         : (boss.phase == 2) ? COL_BOSS_P2 : COL_HP_LOW);
}

// ------------------------------------------------------------
//  MİNİMAP — sağ üst köşe, 1 tile = 1 piksel, sadece keşfedilmiş
// ------------------------------------------------------------
inline void drawMinimap(TFT_eSprite &cv) {
    cv.fillRect(MM_X - 1, MM_Y - 1, MM_W + 2, MM_H + 2, COL_MM_BG);
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (fogMap[y][x] == FOG_DARK) continue;
            uint8_t t = tiles[y][x];
            uint16_t c;
            if (t == TILE_WALL || t == TILE_PILLAR) c = COL_MM_WALL;
            else if (t == TILE_STAIRS) c = COL_STAIRS;
            else if (t == TILE_CHEST)  c = COL_CHEST;
            else                       c = COL_BG;
            cv.drawPixel(MM_X + x, MM_Y + y, c);
        }
    }
    // Görünür düşmanlar (kırmızı)
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].alive) continue;
        if (fogMap[enemies[i].tileY][enemies[i].tileX] != FOG_VIS) continue;
        cv.drawPixel(MM_X + enemies[i].tileX, MM_Y + enemies[i].tileY, COL_HP_LOW);
    }
    // Tüccar (altın piksel) — v2.0
    if (merchant.active && fogMap[merchant.tileY][merchant.tileX] == FOG_VIS)
        cv.drawPixel(MM_X + merchant.tileX, MM_Y + merchant.tileY, COL_CHEST);
    // Boss (2x2 kırmızı blok) — v2.0
    if (boss.active)
        cv.fillRect(MM_X + boss.tileX, MM_Y + boss.tileY,
                    BOSS_SIZE_TILES, BOSS_SIZE_TILES, COL_BOSS_P3);
    // Oyuncu (parlayan beyaz piksel)
    uint16_t pc = ((millis() >> 8) & 1) ? COL_SLASH : COL_PLAYER;
    cv.drawPixel(MM_X + player.tileX, MM_Y + player.tileY, pc);
}

// ------------------------------------------------------------
//  OYUN İÇİ MESAJ — alt orta, süreli
// ------------------------------------------------------------
inline void drawMessage(TFT_eSprite &cv) {
    if (millis() >= gameMsgUntil || gameMsg[0] == '\0') return;
    int w = strlen(gameMsg) * FONT_W + 6;
    int x = (SCR_W - w) / 2;
    int y = SCR_H - FONT_H - 6;
    cv.fillRect(x, y - 2, w, FONT_H + 4, COL_PANEL_BG);
    cv.drawRect(x, y - 2, w, FONT_H + 4, COL_HUD_LINE);
    cv.setTextSize(1);
    cv.setTextColor(COL_KEY);
    cv.setCursor(x + 3, y);
    cv.print(gameMsg);
}

// ------------------------------------------------------------
//  YARI SAYDAM KARARTMA — pause/envanter arkası (v3.0 onarım).
//  Eski sürümler piksel ATLAYIP siyaha boyuyordu → çizgili
//  görüntü. Bu sürüm HER pikselin KENDİ rengini yarıya düşürür
//  (readPixel + dimColor) → gerçek %50 karartma, desen yok.
//  Sadece menü state'lerinde çağrılır; ~19k piksel sprite RAM'inde
//  işlenir, menüde FPS düşüşü kabul edilebilir.
//  HUD'a DOKUNMAZ (y >= HUD_H) — üst bar menülerde net kalır.
// ------------------------------------------------------------
inline void dimOverlay(TFT_eSprite &cv) {
    for (int y = HUD_H; y < SCR_H; y++)
        for (int x = 0; x < SCR_W; x++)
            cv.drawPixel(x, y, dimColor(cv.readPixel(x, y)));
}

// ------------------------------------------------------------
//  MENÜ EKRANI — başlık, dekoratif mini zindan, kontroller, rekor
// ------------------------------------------------------------
inline void drawMenu(TFT_eSprite &cv, int bestFloor, bool hasSave, int cursor) {
    cv.fillSprite(COL_BG);
    drawCentered(cv, "DUNGEON", 8, COL_PLAYER, 2);
    drawCentered(cv, "CRAWLER", 26, COL_PLAYER, 2);

    // Dekoratif mini zindan görseli (2 oda + koridor)
    constexpr int DX = 46, DY = 48;                 // Görsel sol üst köşesi
    cv.drawRect(DX, DY, 26, 18, COL_WALL_HL);       // Sol oda duvarı
    cv.fillRect(DX + 1, DY + 1, 24, 16, COL_FLOOR);
    cv.drawRect(DX + 42, DY + 4, 26, 14, COL_WALL_HL);   // Sağ oda duvarı
    cv.fillRect(DX + 43, DY + 5, 24, 12, COL_FLOOR);
    cv.fillRect(DX + 26, DY + 8, 16, 3, COL_FLOOR_ALT);  // Koridor
    cv.fillRect(DX + 6, DY + 6, 5, 6, COL_PLAYER);       // Oyuncu
    cv.fillRect(DX + 54, DY + 8, 5, 5, COL_GOBLIN);      // Goblin
    // Menü animasyonları (v3.0): goblin gözleri kırpar, merdiven parlar
    if ((millis() >> 9) & 1) {
        cv.drawPixel(DX + 55, DY + 9, COL_HP_LOW);       // Goblin gözleri
        cv.drawPixel(DX + 57, DY + 9, COL_HP_LOW);
    }
    uint16_t stc = ((millis() >> 8) & 1) ? COL_STAIRS : COL_STAIRS_GLOW;
    cv.fillRect(DX + 62, DY + 12, 4, 3, stc);            // Merdiven (nabızlı)

    if (hasSave) {
        uint16_t c1 = (cursor == 0) ? COL_KEY : COL_HUD_TEXT;
        uint16_t c2 = (cursor == 1) ? COL_KEY : COL_HUD_TEXT;
        drawCentered(cv, (cursor == 0) ? "> Continue <" : "Continue", 76, c1);
        drawCentered(cv, (cursor == 1) ? "> New Game <" : "New Game", 88, c2);
        drawCentered(cv, "[A] Select [B] OS Menu", 100, COL_EMPTY_SLOT);
    } else {
        drawCentered(cv, "[A] New Game", 76, COL_KEY);
        drawCentered(cv, "[B] OS Menu", 88, COL_HUD_TEXT);
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "Best: Floor %d", bestFloor);
    drawCentered(cv, buf, 116, COL_KEY);
}

// ------------------------------------------------------------
//  GAME OVER PANELİ
// ------------------------------------------------------------
inline void drawGameOver(TFT_eSprite &cv, int floorNum, int kills,
                         int bestFloor, bool newRecord) {
    dimOverlay(cv);   // sahneyi karart (dungeon'a ozel)

    // Ortak OS game-over: 4 satirlik veri-gudumlu tablo (: sutunu hizali).
    // Best satiri rekorda "NEW BEST!" ile yanip soner (eski davranis korundu).
    char fb[6], lb[6], kb[6], bb[16];
    snprintf(fb, sizeof(fb), "%d", floorNum);
    snprintf(lb, sizeof(lb), "%d", player.lvl);
    snprintf(kb, sizeof(kb), "%d", kills);
    if (newRecord && ((millis() >> 8) & 1))
        snprintf(bb, sizeof(bb), "NEW BEST!");
    else
        snprintf(bb, sizeof(bb), "Floor %d", bestFloor);

    OsStat rows[4] = {
        { "Floor", fb, COL_HUD_TEXT, COL_HUD_TEXT },
        { "Level", lb, COL_HUD_TEXT, COL_HUD_TEXT },
        { "Kills", kb, COL_HUD_TEXT, COL_HUD_TEXT },
        { "Best",  bb, COL_KEY,      COL_KEY      },
    };
    osDrawGameOver(cv, false, rows, 4);
}

// ------------------------------------------------------------
//  PAUSE OVERLAY — donmuş sahnenin üstüne
// ------------------------------------------------------------
inline void drawPauseOverlay(TFT_eSprite &cv) {
    dimOverlay(cv);                 // sahneyi karart (dungeon'a ozel)
    osDrawPause(cv, COL_PLAYER);    // ortak OS pause kutusu
}

// ------------------------------------------------------------
//  KAT GEÇİŞ EKRANI
// ------------------------------------------------------------
inline void drawLevelClear(TFT_eSprite &cv, int floorNum, int kills) {
    cv.fillSprite(COL_BG);
    char buf[32];
    snprintf(buf, sizeof(buf), "FLOOR %d CLEAR!", floorNum);
    drawCentered(cv, buf, 40, COL_STAIRS, 1);
    snprintf(buf, sizeof(buf), "Kills: %d", kills);
    drawCentered(cv, buf, 60, COL_HUD_TEXT);
    snprintf(buf, sizeof(buf), "Level: %d", player.lvl);
    drawCentered(cv, buf, 72, COL_HUD_TEXT);

    int nextB = biomeForFloor(floorNum + 1);
    if (nextB != currentBiome) {
        drawCentered(cv, "ENTERING NEW BIOME...", 92, BIO_FLOOR[nextB]);
    } else {
        snprintf(buf, sizeof(buf), "Loading floor %d...", floorNum + 1);
        drawCentered(cv, buf, 92, COL_EMPTY_SLOT);
    }

    // Aşağı iniş animasyonu (v3.0): sırayla parlayan 3 chevron
    int ph = (int)((millis() / 200) % 3);
    for (int i = 0; i < 3; i++) {
        uint16_t c = (i == ph) ? COL_STAIRS : dimColor(COL_STAIRS);
        int cy = 106 + i * 6;
        cv.drawLine(SCR_W / 2 - 6, cy, SCR_W / 2, cy + 3, c);
        cv.drawLine(SCR_W / 2, cy + 3, SCR_W / 2 + 6, cy, c);
    }
}

// ------------------------------------------------------------
//  DİYALOG EKRANI (NPC Sistemi)
// ------------------------------------------------------------
inline void drawDialogueScreen(TFT_eSprite &cv, const char* speaker, const char* text) {
    dimOverlay(cv);
    constexpr int PX = 8, PY = SCR_H - 44, PW = SCR_W - 16, PH = 36;
    cv.fillRect(PX, PY, PW, PH, COL_PANEL_BG);
    cv.drawRect(PX, PY, PW, PH, COL_PANEL_BRD);

    cv.setTextSize(1);
    cv.setTextColor(COL_PLAYER);
    cv.setCursor(PX + 4, PY + 4);
    cv.print(speaker);
    cv.print(":");

    cv.setTextColor(COL_HUD_TEXT);
    int yOffset = PY + 16;
    const char* p = text;
    while (*p) {
        cv.setCursor(PX + 4, yOffset);
        while (*p && *p != '\n') {
            cv.print(*p);
            p++;
        }
        if (*p == '\n') {
            p++;
            yOffset += 10; // Font yüksekliği
        }
    }

    // Kırpışan devam ikonu
    if ((millis() >> 8) & 1) {
        cv.fillTriangle(PX + PW - 10, PY + PH - 8,
                        PX + PW - 4, PY + PH - 8,
                        PX + PW - 7, PY + PH - 4, COL_STAIRS);
    }
}

// ------------------------------------------------------------
//  ENVANTER EKRANI — donmuş sahnenin üstüne panel
// ------------------------------------------------------------
inline void drawInventoryScreen(TFT_eSprite &cv) {
    dimOverlay(cv);
    cv.fillRect(INV_PANEL_X, INV_PANEL_Y, INV_PANEL_W, INV_PANEL_H, COL_PANEL_BG);
    cv.drawRect(INV_PANEL_X, INV_PANEL_Y, INV_PANEL_W, INV_PANEL_H, COL_PANEL_BRD);

    drawCentered(cv, "INVENTORY", INV_PANEL_Y + 4, COL_PLAYER);

    constexpr int LIST_Y = INV_PANEL_Y + 16;    // Slot listesi başlangıcı
    constexpr int ROW_H  = 10;                  // Satır yüksekliği
    cv.setTextSize(1);

    // Envanter boşsa liste yerine bilgi mesajı göster
    if (inventory.count == 0) {
        drawCentered(cv, "Empty!", INV_PANEL_Y + INV_PANEL_H / 2 - FONT_H / 2,
                     COL_EMPTY_SLOT);
        drawCentered(cv, "[C] Close", INV_PANEL_Y + INV_PANEL_H - 10, COL_HUD_TEXT);
        return;
    }

    for (int i = 0; i < INV_SLOTS; i++) {
        int y = LIST_Y + i * ROW_H;
        ItemType t = inventory.slots[i];

        // Seçili slot vurgusu
        if (i == inventory.cursor) {
            cv.fillRect(INV_PANEL_X + 2, y - 1, INV_PANEL_W - 4, ROW_H, COL_HUD_LINE);
            cv.setTextColor(COL_KEY);
            cv.setCursor(INV_PANEL_X + 4, y);
            cv.print('>');
        }

        if (t == ITEM_NONE) {
            cv.setTextColor(COL_EMPTY_SLOT);
            cv.setCursor(INV_PANEL_X + 22, y);
            cv.print("(empty)");
        } else {
            cv.fillRect(INV_PANEL_X + 12, y + 1, 4, 4, itemColor(t));  // Renkli ikon
            cv.setTextColor(COL_HUD_TEXT);
            cv.setCursor(INV_PANEL_X + 22, y);
            cv.print(itemName(t));
        }
    }

    drawCentered(cv, "[A] Use [C] Close", INV_PANEL_Y + INV_PANEL_H - 10, COL_HUD_TEXT);
}

// ============================================================
//  v2.0 EKRANLARI
// ============================================================

// ------------------------------------------------------------
//  BÜYÜ MENÜSÜ — INVENTORY benzeri panel (BTN_D ile açılır)
// ------------------------------------------------------------
inline void drawSpellMenu(TFT_eSprite &cv) {
    dimOverlay(cv);
    cv.fillRect(INV_PANEL_X, INV_PANEL_Y, INV_PANEL_W, INV_PANEL_H, COL_PANEL_BG);
    cv.drawRect(INV_PANEL_X, INV_PANEL_Y, INV_PANEL_W, INV_PANEL_H, COL_MANA);

    drawCentered(cv, "SPELLBOOK", INV_PANEL_Y + 4, COL_MANA);

    // Mana durumu
    char buf[22];
    snprintf(buf, sizeof(buf), "Mana: %d/%d", player.mana, player.maxMana);
    drawCentered(cv, buf, INV_PANEL_Y + 15, COL_HUD_TEXT);

    constexpr int LIST_Y = INV_PANEL_Y + 28;    // Büyü listesi başlangıcı
    constexpr int ROW_H  = 12;                  // Satır yüksekliği
    cv.setTextSize(1);

    for (int i = 0; i < MAX_SPELLS; i++) {
        int y = LIST_Y + i * ROW_H;
        const Spell &sp = spellbook[i];
        bool locked = (player.lvl < sp.unlockLvl);

        // Seçili satır vurgusu
        if (i == selectedSpell) {
            cv.fillRect(INV_PANEL_X + 2, y - 1, INV_PANEL_W - 4, ROW_H - 1, COL_HUD_LINE);
            cv.setTextColor(COL_KEY);
            cv.setCursor(INV_PANEL_X + 4, y);
            cv.print('>');
        }

        // Renkli büyü ikonu + isim
        cv.fillRect(INV_PANEL_X + 12, y + 1, 4, 4, locked ? COL_EMPTY_SLOT : spellColor(sp.type));
        cv.setTextColor(locked ? COL_EMPTY_SLOT : COL_HUD_TEXT);
        cv.setCursor(INV_PANEL_X + 20, y);
        cv.print(spellName(sp.type));

        // Sağ sütun: kilit / cooldown / mana bedeli
        cv.setCursor(INV_PANEL_X + INV_PANEL_W - 34, y);
        if (locked) {
            snprintf(buf, sizeof(buf), "Lv%d", sp.unlockLvl);
            cv.setTextColor(COL_EMPTY_SLOT);
        } else if (sp.cooldownLeft > 0) {
            snprintf(buf, sizeof(buf), "%ds", spellCooldownSec(sp));
            cv.setTextColor(COL_HP_LOW);
        } else {
            snprintf(buf, sizeof(buf), "M%d", sp.manaCost);
            cv.setTextColor(COL_MANA);
        }
        cv.print(buf);
    }

    drawCentered(cv, "[A] Cast [B] Close", INV_PANEL_Y + INV_PANEL_H - 10, COL_HUD_TEXT);
}

// ------------------------------------------------------------
//  HEDEF İMLECİ — FIREBOLT/TELEPORT imleç modu (yanıp söner)
// ------------------------------------------------------------
inline void drawTargetCursor(TFT_eSprite &cv, int shX, int shY) {
    // v3.0: sahneyle aynı piksel-bazlı kamerayı kullan (hizalama)
    int camPx, camPy;
    computeCameraPx(camPx, camPy);
    int sx = targetX * TILE_PX - camPx + shX;
    int sy = HUD_H + targetY * TILE_PX - camPy + shY;
    uint16_t c = ((millis() >> 7) & 1) ? COL_SLASH : spellColor(spellbook[selectedSpell].type);
    cv.drawRect(sx, sy, TILE_PX, TILE_PX, c);
    // Köşe vurguları (nişangah hissi)
    cv.drawPixel(sx - 1, sy - 1, c);
    cv.drawPixel(sx + TILE_PX, sy - 1, c);
    cv.drawPixel(sx - 1, sy + TILE_PX, c);
    cv.drawPixel(sx + TILE_PX, sy + TILE_PX, c);
}

// ------------------------------------------------------------
//  TÜCCAR MENÜSÜ — üstte 4 satın alma, altta 4 satış satırı
// ------------------------------------------------------------
inline void drawMerchantScreen(TFT_eSprite &cv) {
    dimOverlay(cv);
    cv.fillRect(MER_PANEL_X, MER_PANEL_Y, MER_PANEL_W, MER_PANEL_H, COL_PANEL_BG);
    cv.drawRect(MER_PANEL_X, MER_PANEL_Y, MER_PANEL_W, MER_PANEL_H, COL_CHEST);

    drawCentered(cv, "MERCHANT", MER_PANEL_Y + 3, COL_CHEST);

    char buf[26];
    snprintf(buf, sizeof(buf), "Altin: %d", player.gold);
    drawCentered(cv, buf, MER_PANEL_Y + 13, COL_KEY);

    constexpr int LIST_Y  = MER_PANEL_Y + 25;   // Satır listesi başlangıcı
    constexpr int ROW_H   = 9;                  // Satır yüksekliği
    constexpr int COL_ACT = MER_PANEL_X + 10;   // "AL"/"SAT" sütunu
    constexpr int COL_NAM = MER_PANEL_X + 34;   // Eşya adı sütunu
    constexpr int COL_PRC = MER_PANEL_X + 120;  // Fiyat sütunu
    cv.setTextSize(1);

    for (int i = 0; i < TRADE_ROWS; i++) {
        int y = LIST_Y + i * ROW_H;
        bool isBuy  = (i < 4);
        ItemType t  = TRADE_ITEMS[isBuy ? i : i - 4];
        int price   = isBuy ? buyPriceOf(t) : sellPriceOf(t);
        int owned   = inventory.countOf(t);

        // Seçili satır vurgusu
        if (i == merchCursor) {
            cv.fillRect(MER_PANEL_X + 2, y - 1, MER_PANEL_W - 4, ROW_H, COL_HUD_LINE);
            cv.setTextColor(COL_KEY);
            cv.setCursor(MER_PANEL_X + 4, y);
            cv.print('>');
        }

        // Sütunlar: işlem | eşya adı | fiyat (yapılamıyorsa soluk)
        bool cant = isBuy ? (player.gold < price) : (owned == 0);
        cv.setTextColor(isBuy ? COL_POTION : COL_CHEST);
        cv.setCursor(COL_ACT, y);
        cv.print(isBuy ? "BUY" : "SELL");
        cv.setTextColor(cant ? COL_EMPTY_SLOT : COL_HUD_TEXT);
        cv.setCursor(COL_NAM, y);
        cv.print(itemName(t));
        cv.setCursor(COL_PRC, y);
        snprintf(buf, sizeof(buf), "%2dg", price);
        cv.print(buf);
    }

    drawCentered(cv, "[A] Trade [C] Close", MER_PANEL_Y + MER_PANEL_H - 9, COL_HUD_TEXT);
}

// ------------------------------------------------------------
//  BOSS INTRO — "BOSS: [isim]" (500 ms, otomatik geçiş)
// ------------------------------------------------------------
inline void drawBossIntro(TFT_eSprite &cv) {
    dimOverlay(cv);
    constexpr int PX = 20, PY = 40, PW = 120, PH = 48;
    cv.fillRect(PX, PY, PW, PH, COL_PANEL_BG);
    cv.drawRect(PX, PY, PW, PH, COL_BOSS_P3);
    drawCentered(cv, "BOSS", PY + 8, COL_BOSS_P3, 2);
    drawCentered(cv, bossName(boss.type), PY + 30, COL_KEY);
}

// ------------------------------------------------------------
//  BAŞARIM BİLDİRİMİ — alt orta, oyunu durdurmaz (2 sn)
//  Oyun mesajının hemen üstünde çizilir (çakışma yok).
// ------------------------------------------------------------
inline void drawAchievementBanner(TFT_eSprite &cv) {
    if (millis() >= achUntil || achText[0] == '\0') return;
    int w = strlen(achText) * FONT_W + 6;
    int x = (SCR_W - w) / 2;
    int y = SCR_H - FONT_H * 2 - 14;             // Mesaj kutusunun üstü
    cv.fillRect(x, y - 2, w, FONT_H + 4, COL_PANEL_BG);
    cv.drawRect(x, y - 2, w, FONT_H + 4, COL_STAIRS);
    cv.setTextSize(1);
    cv.setTextColor(COL_STAIRS);
    cv.setCursor(x + 3, y);
    cv.print(achText);
}

