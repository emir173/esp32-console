#pragma once
// ============================================================
//  dungeon/Map.h — Prosedürel Harita Üretimi
//
//  Sorumluluk: Oda + L-koridor tabanlı kat üretimi, kapı/merdiven/
//  sandık/kilitli kapı yerleşimi, Fog of War ve kamera hesabı.
//  Kilitli kapı yerleştirildiğinde flood-fill ile en az bir
//  sandığın ulaşılabilir olduğu doğrulanır (softlock koruması).
// ============================================================

#include "Config.h"

// ------------------------------------------------------------
//  Oda yapısı
// ------------------------------------------------------------
struct Room {
    int x, y, w, h;                                    // Sol üst köşe + boyut (tile)
    int cx() const { return x + w / 2; }               // Oda merkezi X
    int cy() const { return y + h / 2; }               // Oda merkezi Y
    bool contains(int tx, int ty) const {
        return tx >= x && tx < x + w && ty >= y && ty < y + h;
    }
};

// ------------------------------------------------------------
//  Harita verisi (global, sabit boyutlu — malloc yasak)
// ------------------------------------------------------------
uint8_t tiles[MAP_H][MAP_W];    // Tile türleri
uint8_t fogMap[MAP_H][MAP_W];   // 0=karanlık, 1=keşfedilmiş, 2=görünür
Room    rooms[MAX_ROOMS];
int     roomCount = 0;
int     startTileX = 1, startTileY = 1;   // Oyuncu başlangıç noktası (oda 0 merkezi)
bool    mapHasLockedDoor = false;         // Bu katta kilitli kapı var mı
bool    keyGuaranteePending = false;      // İlk sandık anahtar vermeli (softlock koruması)

// v2.0 kat durumu
BiomeType currentBiome = BIOME_DUNGEON;   // Aktif biyom (renk + spawn dağılımı)
bool    isBossFloor = false;              // Bu kat boss katı mı (kat % 3 == 0)
int     bossStartX = 0, bossStartY = 0;   // Boss'un 2x2 sol üst başlangıç tile'ı
int     currentFloorNum = 1;              // Aktif kat (düşman ölçeklemesi — v2.1)

// ------------------------------------------------------------
//  Temel sorgular
// ------------------------------------------------------------
inline bool inMap(int x, int y) {
    return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H;
}

inline uint8_t tileAt(int x, int y) {
    return inMap(x, y) ? tiles[y][x] : TILE_WALL;
}

// Oyuncu bu tile'a yürüyebilir mi (kilitli kapı/sandık/lav ayrıca işlenir)
inline bool canPlayerWalk(int x, int y) {
    uint8_t t = tileAt(x, y);
    return t == TILE_FLOOR || t == TILE_DOOR || t == TILE_STAIRS ||
           t == TILE_SWAMP || t == TILE_FLOWER;
}

// Düşman bu tile'a yürüyebilir mi (merdiven/sandık kampı ve lav engellenir)
inline bool canEnemyWalk(int x, int y) {
    uint8_t t = tileAt(x, y);
    return t == TILE_FLOOR || t == TILE_DOOR ||
           t == TILE_SWAMP || t == TILE_FLOWER;
}

// Tile herhangi bir odanın içinde mi
inline bool insideAnyRoom(int x, int y) {
    for (int i = 0; i < roomCount; i++) {
        if (rooms[i].contains(x, y)) return true;
    }
    return false;
}

// ------------------------------------------------------------
//  Oda ve koridor kazıma
// ------------------------------------------------------------
// İki oda (marj dahil) çakışıyor mu
inline bool roomsOverlap(const Room &a, const Room &b) {
    return a.x - ROOM_MARGIN < b.x + b.w + ROOM_MARGIN &&
           a.x + a.w + ROOM_MARGIN > b.x - ROOM_MARGIN &&
           a.y - ROOM_MARGIN < b.y + b.h + ROOM_MARGIN &&
           a.y + a.h + ROOM_MARGIN > b.y - ROOM_MARGIN;
}

inline void carveRoom(const Room &r) {
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            tiles[y][x] = TILE_FLOOR;
}

// L-şekilli koridor: önce yatay, sonra dikey
inline void carveCorridor(int x1, int y1, int x2, int y2) {
    int x = x1, y = y1;
    while (x != x2) {
        tiles[y][x] = (tiles[y][x] == TILE_WALL) ? TILE_FLOOR : tiles[y][x];
        x += (x2 > x) ? 1 : -1;
    }
    while (y != y2) {
        tiles[y][x] = (tiles[y][x] == TILE_WALL) ? TILE_FLOOR : tiles[y][x];
        y += (y2 > y) ? 1 : -1;
    }
    tiles[y][x] = (tiles[y][x] == TILE_WALL) ? TILE_FLOOR : tiles[y][x];
}

// ------------------------------------------------------------
//  Kapı yerleşimi: oda dışındaki dar koridor tile'ı bir odaya
//  bitişikse orası oda girişidir → TILE_DOOR yapılır.
// ------------------------------------------------------------
inline void placeDoors() {
    for (int y = 1; y < MAP_H - 1; y++) {
        for (int x = 1; x < MAP_W - 1; x++) {
            if (tiles[y][x] != TILE_FLOOR || insideAnyRoom(x, y)) continue;
            // Dar geçit mi: yatay VEYA dikey eksende iki yanı duvar
            bool narrowH = (tiles[y][x - 1] == TILE_WALL && tiles[y][x + 1] == TILE_WALL);
            bool narrowV = (tiles[y - 1][x] == TILE_WALL && tiles[y + 1][x] == TILE_WALL);
            if (!narrowH && !narrowV) continue;
            // Bir odanın hemen yanında mı
            if (insideAnyRoom(x - 1, y) || insideAnyRoom(x + 1, y) ||
                insideAnyRoom(x, y - 1) || insideAnyRoom(x, y + 1)) {
                tiles[y][x] = TILE_DOOR;
            }
        }
    }
}

// ------------------------------------------------------------
//  Flood-fill ulaşılabilirlik testi (statik yığın, malloc yok)
//  blockLocked=true ise kilitli kapılar duvar sayılır.
//  Hedef tile türüne (targetTile) ulaşılabiliyorsa true döner.
// ------------------------------------------------------------
inline bool floodCanReach(int sx, int sy, uint8_t targetTile, bool blockLocked) {
    static uint8_t  visited[MAP_H][MAP_W];
    static uint16_t stack[MAP_W * MAP_H];
    memset(visited, 0, sizeof(visited));
    int top = 0;
    stack[top++] = (uint16_t)(sy * MAP_W + sx);
    visited[sy][sx] = 1;

    while (top > 0) {
        uint16_t idx = stack[--top];
        int x = idx % MAP_W, y = idx / MAP_W;
        for (int d = 0; d < 4; d++) {
            int nx = x + DIR_DX[d], ny = y + DIR_DY[d];
            if (!inMap(nx, ny) || visited[ny][nx]) continue;
            uint8_t t = tiles[ny][nx];
            if (t == targetTile) return true;
            if (t == TILE_WALL) continue;
            if (t == TILE_PILLAR) continue;              // Sütun geçilmez (v3.2)
            if (t == TILE_LAVA) continue;                // Lav oyuncuyu bloklar (v2.0)
            if (t == TILE_LOCKED && blockLocked) continue;
            if (t == TILE_CHEST) continue;               // Sandık geçilmez ama hedef olabilir
            visited[ny][nx] = 1;
            stack[top++] = (uint16_t)(ny * MAP_W + nx);
        }
    }
    return false;
}

// ------------------------------------------------------------
//  Fog of War — SADECE oyuncu hareket edince çağrılır (lazy)
//  Manhattan mesafesi FOG_RADIUS içindeki tile'lar görünür olur.
// ------------------------------------------------------------
inline void updateFog(int px, int py) {
    // Boss katında fog of war yok — tüm oda kalıcı görünür
    if (isBossFloor) return;

    // Önce görünür alanları "keşfedilmiş"e indir
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (fogMap[y][x] == FOG_VIS) fogMap[y][x] = FOG_SEEN;

    // Görüş alanını parlat (Dairesel alan + Edge Fade/Dithering)
    int rSq = FOG_RADIUS * FOG_RADIUS;
    int fadeSq = (FOG_RADIUS - 1) * (FOG_RADIUS - 1);
    for (int dy = -FOG_RADIUS; dy <= FOG_RADIUS; dy++) {
        for (int dx = -FOG_RADIUS; dx <= FOG_RADIUS; dx++) {
            int distSq = dx * dx + dy * dy;
            if (distSq > rSq) continue;
            
            int x = px + dx, y = py + dy;
            if (inMap(x, y)) {
                fogMap[y][x] = FOG_VIS;
            }
        }
    }
}

// ------------------------------------------------------------
//  Kamera — oyuncuyu merkezde tutar, harita kenarına clamp'ler
// ------------------------------------------------------------
inline void computeCamera(int px, int py, int &camX, int &camY) {
    camX = px - VIEW_COLS / 2;
    camY = py - VIEW_ROWS / 2;
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
    if (camX > MAP_W - VIEW_COLS) camX = MAP_W - VIEW_COLS;
    if (camY > MAP_H - VIEW_ROWS) camY = MAP_H - VIEW_ROWS;
}

// ------------------------------------------------------------
//  Yerleşim yardımcısı: odada rastgele BOŞ zemin tile'ı bul
//  (başlangıç noktası hariç). Bulunamazsa false.
// ------------------------------------------------------------
inline bool randomFloorInRoom(const Room &r, int &outX, int &outY) {
    for (int t = 0; t < SPAWN_TRIES; t++) {
        int x = r.x + random(0, r.w);
        int y = r.y + random(0, r.h);
        if (tiles[y][x] != TILE_FLOOR) continue;
        if (x == startTileX && y == startTileY) continue;
        outX = x; outY = y;
        return true;
    }
    return false;
}

// ------------------------------------------------------------
//  BOSS ODASI ÜRETİMİ (v2.0) — 11x9 tek oda, 4 köşede sütun.
//  Fog yok, merdiven/sandık/tüccar yok; merdiven boss ölünce belirir.
// ------------------------------------------------------------
inline void generateBossRoom() {
    int rx = (MAP_W - BOSS_ROOM_W) / 2;
    int ry = (MAP_H - BOSS_ROOM_H) / 2;
    Room r = { rx, ry, BOSS_ROOM_W, BOSS_ROOM_H };
    rooms[0] = r;
    roomCount = 1;
    carveRoom(r);

    // Dört köşede 2x2 sütun — Ejderha ve Golem arenalarında.
    // v3.2: TILE_WALL yerine TILE_PILLAR — mağara/orman biyomlarında
    // duvar rengi zemine karışıyor, sütunlar görünmez engel oluyordu.
    // v3.3: LICH arenası AÇIK — Lich yürümez (ışınlanır) ve mermisi
    // sütunun üzerinden geçer, sütunlar ona karşı işlevsizdi; her
    // boss arenası artık farklı hissettirir (idx 1 = Lich, initBoss).
    bool lichArena = ((currentFloorNum / BOSS_EVERY_N_FLOORS - 1) % 3) == 1;
    if (!lichArena) {
        for (int dy = 0; dy < 2; dy++) {
            for (int dx = 0; dx < 2; dx++) {
                tiles[ry + 2 + dy][rx + 2 + dx] = TILE_PILLAR;
                tiles[ry + 2 + dy][rx + BOSS_ROOM_W - 4 + dx] = TILE_PILLAR;
                tiles[ry + BOSS_ROOM_H - 4 + dy][rx + 2 + dx] = TILE_PILLAR;
                tiles[ry + BOSS_ROOM_H - 4 + dy][rx + BOSS_ROOM_W - 4 + dx] = TILE_PILLAR;
            }
        }
    }

    // Oyuncu solda, boss (2x2) sağ merkezde
    startTileX = rx + 1;
    startTileY = ry + BOSS_ROOM_H / 2;
    bossStartX = rx + BOSS_ROOM_W - 5;
    bossStartY = ry + BOSS_ROOM_H / 2 - 1;

    // Fog yok ama SADECE oda + 1 tile duvar çerçevesi görünür;
    // haritanın kalanı karanlık kalır (ekranı gri duvar kaplamasın)
    for (int y = ry - 1; y <= ry + BOSS_ROOM_H; y++) {
        for (int x = rx - 1; x <= rx + BOSS_ROOM_W; x++) {
            if (inMap(x, y)) fogMap[y][x] = FOG_VIS;
        }
    }
}

// ------------------------------------------------------------
//  BİYOM ÖZEL TILE YERLEŞİMİ (v2.0) — mağara bataklığı, orman
//  zehirli çiçeği, cehennem lavı. Sadece oda içi boş zemine konur
//  (koridorlar temiz kalır → geçit tıkanmaz).
// ------------------------------------------------------------
inline void placeSpecialTiles() {
    uint8_t sp;
    switch (currentBiome) {
        case BIOME_CAVE:   sp = TILE_SWAMP;  break;
        case BIOME_FOREST: sp = TILE_FLOWER; break;
        case BIOME_HELL:   sp = TILE_LAVA;   break;
        default:           return;                   // Zindanda özel tile yok
    }
    int n = random(SPECIAL_TILES_MIN, SPECIAL_TILES_MAX + 1);
    for (int i = 0; i < n && roomCount > 1; i++) {
        const Room &r = rooms[random(1, roomCount)]; // Başlangıç odası temiz
        int x, y;
        if (randomFloorInRoom(r, x, y)) tiles[y][x] = sp;
    }
}

// ------------------------------------------------------------
//  CELLULAR AUTOMATA MAĞARA ÜRETİMİ (Faz 2)
// ------------------------------------------------------------
inline void generateCAMap() {
    // 1. Rastgele doldur (%45 duvar)
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (x == 0 || x == MAP_W - 1 || y == 0 || y == MAP_H - 1) {
                tiles[y][x] = TILE_WALL;
            } else {
                tiles[y][x] = (random(0, 100) < 45) ? TILE_WALL : TILE_FLOOR;
            }
        }
    }

    // 2. CA Adımları (4 iterasyon)
    uint8_t tempMap[MAP_H][MAP_W];
    for (int step = 0; step < 4; step++) {
        for (int y = 1; y < MAP_H - 1; y++) {
            for (int x = 1; x < MAP_W - 1; x++) {
                int wallCount = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        if (tiles[y + dy][x + dx] == TILE_WALL) wallCount++;
                    }
                }
                if (tiles[y][x] == TILE_WALL) {
                    tempMap[y][x] = (wallCount >= 4) ? TILE_WALL : TILE_FLOOR;
                } else {
                    tempMap[y][x] = (wallCount >= 5) ? TILE_WALL : TILE_FLOOR;
                }
            }
        }
        for (int y = 1; y < MAP_H - 1; y++) {
            for (int x = 1; x < MAP_W - 1; x++) {
                tiles[y][x] = tempMap[y][x];
            }
        }
    }

    // 3. Başlangıç noktası bul
    startTileX = MAP_W / 2;
    startTileY = MAP_H / 2;
    while (tiles[startTileY][startTileX] != TILE_FLOOR) {
        startTileX++;
        if (startTileX >= MAP_W - 1) { startTileX = 1; startTileY++; }
        if (startTileY >= MAP_H - 1) { startTileY = 1; startTileX = 1; }
    }

    // 4. Flood fill ile ulaşılabilir alanı işaretle
    static uint8_t visited[MAP_H][MAP_W];
    memset(visited, 0, sizeof(visited));
    static uint16_t stack[MAP_W * MAP_H];
    int top = 0;
    stack[top++] = (uint16_t)(startTileY * MAP_W + startTileX);
    visited[startTileY][startTileX] = 1;

    while (top > 0) {
        uint16_t idx = stack[--top];
        int x = idx % MAP_W, y = idx / MAP_W;
        for (int d = 0; d < 4; d++) {
            int nx = x + DIR_DX[d], ny = y + DIR_DY[d];
            if (!inMap(nx, ny) || visited[ny][nx]) continue;
            if (tiles[ny][nx] == TILE_WALL) continue;
            visited[ny][nx] = 1;
            stack[top++] = (uint16_t)(ny * MAP_W + nx);
        }
    }

    // 5. Ulaşılamayan boşlukları duvar yap (adaları sil)
    for (int y = 1; y < MAP_H - 1; y++) {
        for (int x = 1; x < MAP_W - 1; x++) {
            if (tiles[y][x] == TILE_FLOOR && !visited[y][x]) {
                tiles[y][x] = TILE_WALL;
            }
        }
    }

    // 6. Tüm haritayı tek bir dev oda say (diğer sistemler için)
    roomCount = 1;
    rooms[0] = {1, 1, MAP_W - 2, MAP_H - 2};

    // 7. Merdiveni yerleştir (en uzak ulaşılabilir nokta)
    int bestDist = -1, bestX = startTileX, bestY = startTileY;
    for (int y = 1; y < MAP_H - 1; y++) {
        for (int x = 1; x < MAP_W - 1; x++) {
            if (tiles[y][x] == TILE_FLOOR && visited[y][x]) {
                int d = abs(x - startTileX) + abs(y - startTileY);
                if (d > bestDist) { bestDist = d; bestX = x; bestY = y; }
            }
        }
    }
    tiles[bestY][bestX] = TILE_STAIRS;

    // 8. Sandıklar
    int chestCount = random(CHEST_MIN, CHEST_MAX + 1);
    for (int c = 0; c < chestCount; c++) {
        int cx, cy;
        if (randomFloorInRoom(rooms[0], cx, cy)) tiles[cy][cx] = TILE_CHEST;
    }

    // 9. Özel Biyom Tile'ları (placeSpecialTiles roomCount > 1 istediği için burada manuel yapıyoruz)
    uint8_t sp = (currentBiome == BIOME_CAVE) ? TILE_SWAMP : TILE_LAVA;
    int n = random(SPECIAL_TILES_MIN, SPECIAL_TILES_MAX + 1);
    for (int i = 0; i < n; i++) {
        int cx, cy;
        if (randomFloorInRoom(rooms[0], cx, cy)) tiles[cy][cx] = sp;
    }

    // 10. Başlangıç görüşü
    updateFog(startTileX, startTileY);
}

// ------------------------------------------------------------
//  ANA ÜRETİM — her kat için çağrılır
// ------------------------------------------------------------
inline void generateMap(int floorNum) {
    // 0. Biyom ve boss katı tespiti (v2.0)
    currentFloorNum = floorNum;
    currentBiome = biomeForFloor(floorNum);
    isBossFloor  = (floorNum % BOSS_EVERY_N_FLOORS == 0);

    // 1. Her yeri duvar yap, fog'u karart
    memset(tiles, TILE_WALL, sizeof(tiles));
    memset(fogMap, FOG_DARK, sizeof(fogMap));
    mapHasLockedDoor = false;
    keyGuaranteePending = false;

    // Boss katı: normal üretim yerine tek boss odası
    if (isBossFloor) {
        generateBossRoom();
        return;
    }

    // Cellular Automata Mağaraları (Faz 2)
    if (currentBiome == BIOME_CAVE || currentBiome == BIOME_HELL) {
        generateCAMap();
        return;
    }

    // 2. Odaları yerleştir (çakışma kontrolü ile)
    int wanted = random(MIN_ROOMS, MAX_ROOMS + 1);
    roomCount = 0;
    for (int i = 0; i < wanted; i++) {
        for (int t = 0; t < ROOM_PLACE_TRIES; t++) {
            Room r;
            r.w = random(ROOM_MIN_W, ROOM_MAX_W + 1);
            r.h = random(ROOM_MIN_H, ROOM_MAX_H + 1);
            r.x = random(1, MAP_W - r.w - 1);
            r.y = random(1, MAP_H - r.h - 1);
            bool ok = true;
            for (int j = 0; j < roomCount; j++) {
                if (roomsOverlap(r, rooms[j])) { ok = false; break; }
            }
            if (ok) {
                rooms[roomCount++] = r;
                carveRoom(r);
                break;
            }
        }
    }

    // Güvenlik: 2'den az oda yerleşebildiyse (pratikte imkansız) yeniden dene
    if (roomCount < 2) { generateMap(floorNum); return; }

    // 3. Odaları sırayla L-koridorlarla bağla
    for (int i = 1; i < roomCount; i++) {
        carveCorridor(rooms[i - 1].cx(), rooms[i - 1].cy(),
                      rooms[i].cx(),     rooms[i].cy());
    }

    // 4. Oda girişlerine kapı koy
    placeDoors();

    // 5. Oyuncu başlangıcı: oda 0 merkezi
    startTileX = rooms[0].cx();
    startTileY = rooms[0].cy();

    // 6. Merdiven: başlangıçtan en uzak odanın merkezi
    int bestRoom = roomCount - 1, bestDist = -1;
    for (int i = 1; i < roomCount; i++) {
        int d = abs(rooms[i].cx() - startTileX) + abs(rooms[i].cy() - startTileY);
        if (d > bestDist) { bestDist = d; bestRoom = i; }
    }
    tiles[rooms[bestRoom].cy()][rooms[bestRoom].cx()] = TILE_STAIRS;

    // 7. Sandıklar: 1-3 rastgele odaya (oda 0 hariç)
    int chestCount = random(CHEST_MIN, CHEST_MAX + 1);
    for (int c = 0; c < chestCount && roomCount > 1; c++) {
        const Room &r = rooms[random(1, roomCount)];
        int cx, cy;
        if (randomFloorInRoom(r, cx, cy)) tiles[cy][cx] = TILE_CHEST;
    }

    // 7.5 Biyom özel tile'ları (bataklık/çiçek/lav) — v2.0
    placeSpecialTiles();

    // 8. Kilitli kapı: kat >= 2'de %50 şansla mevcut bir kapıyı kilitle.
    //    Softlock koruması: kilit duvarken en az bir sandığa ulaşılabilmeli
    //    (anahtar o sandıktan garanti çıkar). Ulaşılamıyorsa kilit iptal.
    if (floorNum >= LOCKED_MIN_FLOOR && random(0, 2) == 0) {
        // Rastgele bir TILE_DOOR seç
        int doorX = -1, doorY = -1;
        for (int t = 0; t < SPAWN_TRIES; t++) {
            int x = random(1, MAP_W - 1), y = random(1, MAP_H - 1);
            if (tiles[y][x] == TILE_DOOR) { doorX = x; doorY = y; break; }
        }
        if (doorX >= 0) {
            tiles[doorY][doorX] = TILE_LOCKED;
            if (floodCanReach(startTileX, startTileY, TILE_CHEST, true)) {
                mapHasLockedDoor = true;
                keyGuaranteePending = true;     // İlk açılan sandık anahtar verir
            } else {
                tiles[doorY][doorX] = TILE_DOOR; // Sandığa ulaşılamıyor → kilidi kaldır
            }
        }
    }

    // 9. Başlangıç görüş alanını aç
    updateFog(startTileX, startTileY);
}
