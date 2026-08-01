#pragma once
// ============================================================
//  dungeon/Enemies.h — Düşman Türleri, Havuz ve AI
//
//  Sorumluluk: Düşman havuzu (sabit 10 slot), tür istatistikleri,
//  kat başına spawn ve hareket AI'sı.
//  - Yarasa: hızlı, rastgele 4 yön
//  - İskelet: orta hız, oyuncuya Manhattan takibi
//  - Goblin: yavaş ama güçlü, 2 tile içinde öfkelenip hızlanır
//  Düşmanlar kendi moveTimer sıklığında hamle yapar (her frame değil).
//  Saldırı uygulaması Combat.h'de tanımlıdır (ileri bildirim).
// ============================================================

#include "Config.h"
#include "Player.h"
#include "Map.h"
#include "Items.h"   // merchant (düşmanlar tüccar tile'ına basmasın)

// ------------------------------------------------------------
//  Tür ve havuz
// ------------------------------------------------------------
enum EnemyType : uint8_t {
    ENEMY_NONE, ENEMY_BAT, ENEMY_SKELETON, ENEMY_GOBLIN,
    ENEMY_BOSS_DRAGON, ENEMY_BOSS_LICH, ENEMY_BOSS_GOLEM
};

struct Enemy {
    bool alive;
    EnemyType type;
    int tileX, tileY;
    int prevX, prevY;     // Önceki tile (v3.0 kayma animasyonu)
    int hp, maxHp;
    int atk, def;
    int xpReward;         // Öldürüldüğünde verilen XP
    int goldReward;       // Öldürüldüğünde verilen altın (v2.0)
    int moveTimer;        // Kaç frame'de bir hamle yapar
    int moveCounter;      // Frame sayacı
    uint32_t slowedUntil; // Buz büyüsü yavaşlatması (v2.0)
    uint32_t lastMoveMs;  // Son hamle zamanı (v3.0 kayma animasyonu)
    uint32_t hitFlashUntil; // Vurulunca beyaz flaş bitişi (v3.0)
    uint32_t lastAttackMs;  // Son saldırı zamanı (v3.2 atılım animasyonu)
    int8_t   atkDir;      // Son saldırı yönü (v3.2)
    uint16_t color;       // Ana renk (çizim + parçacık)
};

Enemy enemies[MAX_ENEMIES];

// ------------------------------------------------------------
//  BOSS (v2.0) — 2x2 tile kaplar, 3 fazlı AI.
//  Faz 2: HP <= %60, Faz 3: HP <= %30. Özel saldırılar Combat.h'de.
// ------------------------------------------------------------
struct Boss {
    bool active;
    EnemyType type;        // ENEMY_BOSS_*
    int tileX, tileY;      // 2x2 alanın sol üst köşesi
    int prevX, prevY;      // Önceki tile (v3.0 kayma animasyonu)
    uint32_t lastMoveMs;   // Son hamle zamanı (v3.0)
    int hp, maxHp;
    int atk, def;
    int xpReward;
    int phase;             // 1, 2, 3 — HP %60/%30'da faz değiştir
    int moveTimer;         // Kaç tick'te bir hamle
    int moveCounter;       // Tick sayacı
    uint32_t specialTimer; // Özel saldırı zamanlayıcısı (millis hedefi)
    bool altSpecial;       // Faz 3'te özel saldırı dönüşümlü
    int patternTurn;       // Boss AI örüntü turu
    uint32_t lastAttackMs; // Son saldırı zamanı (v3.2 atılım animasyonu)
    int8_t   atkDir;       // Son saldırı yönü (v3.2)
    uint16_t color;        // Faza göre güncellenir
};

Boss boss = {};

// (x,y) boss'un 2x2 alanının içinde mi
inline bool bossOccupies(int x, int y) {
    return boss.active &&
           x >= boss.tileX && x < boss.tileX + BOSS_SIZE_TILES &&
           y >= boss.tileY && y < boss.tileY + BOSS_SIZE_TILES;
}

// Boss adı (intro ekranı için)
inline const char *bossName(EnemyType t) {
    switch (t) {
        case ENEMY_BOSS_DRAGON: return "DRAGON";
        case ENEMY_BOSS_LICH:   return "LICH";
        default:                return "GOLEM";
    }
}

// Kat numarasına göre boss başlat (3→Ejderha, 6→Lich, 9→Golem, rotasyon)
inline void initBoss(int floorNum) {
    int idx = (floorNum / BOSS_EVERY_N_FLOORS - 1) % 3;
    boss.active = true;
    boss.tileX = bossStartX;
    boss.tileY = bossStartY;
    boss.prevX = bossStartX;
    boss.prevY = bossStartY;
    boss.lastMoveMs = 0;
    boss.phase = 1;
    boss.moveCounter = 0;
    boss.specialTimer = millis() + BOSS_SPECIAL_MS;
    boss.altSpecial = false;
    boss.patternTurn = 0;
    boss.lastAttackMs = 0;
    boss.atkDir = -1;
    switch (idx) {
        case 0:
            boss.type = ENEMY_BOSS_DRAGON;
            boss.hp = DRG_HP; boss.maxHp = DRG_HP;
            boss.atk = DRG_ATK; boss.def = DRG_DEF;
            boss.moveTimer = DRG_SPEED; boss.xpReward = DRG_XP;
            boss.color = COL_DRAGON;
            break;
        case 1:
            boss.type = ENEMY_BOSS_LICH;
            boss.hp = LCH_HP; boss.maxHp = LCH_HP;
            boss.atk = LCH_ATK; boss.def = LCH_DEF;
            boss.moveTimer = LCH_SPEED; boss.xpReward = LCH_XP;
            boss.color = COL_LICH;
            break;
        default:
            boss.type = ENEMY_BOSS_GOLEM;
            boss.hp = GLM_HP; boss.maxHp = GLM_HP;
            boss.atk = GLM_ATK; boss.def = GLM_DEF;
            boss.moveTimer = GLM_SPEED; boss.xpReward = GLM_XP;
            boss.color = COL_GOLEM;
            break;
    }

    // Boss kat ölçeklemesi (v2.1): ilk karşılaşma (kat 3) taban değerde,
    // sonraki turlarda (kat 6, 9, 12...) HP/ATK/DEF katla büyür
    int over = floorNum - BOSS_EVERY_N_FLOORS;
    if (over > 0) {
        boss.maxHp += (boss.maxHp * over) / BOSS_SCALE_HP_DIV;
        boss.hp = boss.maxHp;
        boss.atk += over / BOSS_SCALE_ATK_EVERY;
        boss.def += over / BOSS_SCALE_DEF_EVERY;
        boss.xpReward += over;
    }
}

// Combat.h içinde tanımlanır: düşmanın oyuncuya saldırısı
void enemyAttackPlayer(Enemy &e);
// Combat.h içinde tanımlanır: boss yakın saldırısı ve faz özel saldırısı
void bossMeleeAttack();
void bossHeavyAttack();
void bossDoSpecial();
// Combat.h içinde tanımlanır: Lich uzak mesafe büyü mermisi (v3.2)
void lichCastBolt();

// ------------------------------------------------------------
//  Sorgular
// ------------------------------------------------------------
// (x,y) tile'ındaki canlı düşmanın indeksi, yoksa -1
inline int enemyIndexAt(int x, int y) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].alive && enemies[i].tileX == x && enemies[i].tileY == y)
            return i;
    }
    return -1;
}

// Düşman için hedef tile bloklu mu (duvar, düşman, oyuncu, boss, tüccar)
inline bool enemyBlocked(int x, int y) {
    if (!canEnemyWalk(x, y)) return true;
    if (enemyIndexAt(x, y) >= 0) return true;
    if (player.tileX == x && player.tileY == y) return true;
    if (bossOccupies(x, y)) return true;
    if (merchant.active && merchant.tileX == x && merchant.tileY == y) return true;
    return false;
}

// ------------------------------------------------------------
//  Tür istatistiklerini yükle
// ------------------------------------------------------------
inline void initEnemy(Enemy &e, EnemyType type, int x, int y) {
    e.alive = true;
    e.type  = type;
    e.tileX = x;
    e.tileY = y;
    e.prevX = x;                            // Spawn'da kayma yok (v3.0)
    e.prevY = y;
    e.lastMoveMs = 0;
    e.hitFlashUntil = 0;
    e.lastAttackMs = 0;
    e.atkDir = -1;
    e.moveCounter = random(0, BAT_SPEED);   // Sürü senkronunu kırmak için faz kaydırma
    e.slowedUntil = 0;
    switch (type) {
        case ENEMY_BAT:
            e.hp = BAT_HP;  e.maxHp = BAT_HP;
            e.atk = BAT_ATK; e.def = BAT_DEF;
            e.moveTimer = BAT_SPEED; e.xpReward = BAT_XP;
            e.goldReward = GOLD_BAT;
            e.color = COL_BAT;
            break;
        case ENEMY_GOBLIN:
            e.hp = GOB_HP;  e.maxHp = GOB_HP;
            e.atk = GOB_ATK; e.def = GOB_DEF;
            e.moveTimer = GOB_SPEED; e.xpReward = GOB_XP;
            e.goldReward = GOLD_GOB;
            e.color = COL_GOBLIN;
            break;
        default: // ENEMY_SKELETON
            e.hp = SKEL_HP;  e.maxHp = SKEL_HP;
            e.atk = SKEL_ATK; e.def = SKEL_DEF;
            e.moveTimer = SKEL_SPEED; e.xpReward = SKEL_XP;
            e.goldReward = GOLD_SKEL;
            e.color = COL_SKELETON;
            break;
    }

    // Kat ölçeklemesi (v2.1): oyuncu seviyeyle güçlenirken düşman da
    // katla güçlenir — ödüller (XP/altın) de birlikte artar
    int f = currentFloorNum;
    e.maxHp += (e.maxHp * f * EN_SCALE_HP_PCT) / 100;
    e.hp = e.maxHp;
    e.atk += f / EN_SCALE_ATK_EVERY;
    e.def += f / EN_SCALE_DEF_EVERY;
    e.xpReward   += f / EN_SCALE_XP_EVERY;
    e.goldReward += f / EN_SCALE_GOLD_EVERY;
}

// ------------------------------------------------------------
//  Spawn — kat başına 3 + kat*2 düşman (max havuz), oda 0 hariç.
//  Kat arttıkça goblin oranı artar, yarasa oranı azalır.
// ------------------------------------------------------------
inline void spawnEnemies(int floorNum) {
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].alive = false;

    // Boss katında normal düşman yok (Lich sonradan iskelet çağırabilir)
    if (isBossFloor) return;

    int want = ENEMIES_BASE + floorNum * ENEMIES_PER_FLOOR;
    if (want > MAX_ENEMIES) want = MAX_ENEMIES;

    // Tür dağılımı: biyoma göre sabit ağırlık, zindanda kat formülü
    int batPct, gobPct;
    switch (currentBiome) {
        case BIOME_CAVE:   batPct = CAVE_BAT_PCT;   gobPct = CAVE_GOB_PCT;   break;
        case BIOME_FOREST: batPct = FOREST_BAT_PCT; gobPct = FOREST_GOB_PCT; break;
        case BIOME_HELL:   batPct = HELL_BAT_PCT;   gobPct = HELL_GOB_PCT;   break;
        default:
            batPct = BAT_PCT_BASE - floorNum * BAT_PCT_DROP;
            if (batPct < BAT_PCT_MIN) batPct = BAT_PCT_MIN;
            gobPct = floorNum * GOB_PCT_PER_FL;
            if (gobPct > GOB_PCT_MAX) gobPct = GOB_PCT_MAX;
            break;
    }

    int placed = 0;
    for (int i = 0; i < want; i++) {
        const Room &r = (roomCount > 1) ? rooms[random(1, roomCount)] : rooms[0];
        int x, y;
        if (!randomFloorInRoom(r, x, y)) continue;

        // CA Haritalarında oyuncunun hemen dibinde doğmalarını engelle (min 3 block mesafe)
        if (roomCount == 1 && abs(x - startTileX) + abs(y - startTileY) < 4) continue;

        if (enemyIndexAt(x, y) >= 0) continue;

        int roll = random(0, PCT_MAX);
        EnemyType t;
        if (roll < batPct)               t = ENEMY_BAT;
        else if (roll < batPct + gobPct) t = ENEMY_GOBLIN;
        else                             t = ENEMY_SKELETON;

        initEnemy(enemies[placed++], t, x, y);
    }
}

// ------------------------------------------------------------
//  AI hamle yardımcıları
// ------------------------------------------------------------
// Rastgele boş bir yöne 1 tile (yarasa / pasif dolaşma)
inline void enemyRandomStep(Enemy &e) {
    int start = random(0, 4);
    for (int i = 0; i < 4; i++) {
        int d = (start + i) % 4;
        int nx = e.tileX + DIR_DX[d], ny = e.tileY + DIR_DY[d];
        if (!enemyBlocked(nx, ny)) {
            e.prevX = e.tileX; e.prevY = e.tileY;   // Kayma animasyonu (v3.0)
            e.tileX = nx; e.tileY = ny;
            e.lastMoveMs = millis();
            return;
        }
    }
}

// Oyuncuya en çok yaklaştıran boş yöne 1 tile (iskelet/goblin takibi)
inline void enemyChaseStep(Enemy &e) {
    int dx = player.tileX - e.tileX;
    int dy = player.tileY - e.tileY;
    // Önce baskın ekseni dene, tıkalıysa diğerini
    int firstDir  = (abs(dx) >= abs(dy)) ? (dx > 0 ? DIR_RIGHT : DIR_LEFT)
                                         : (dy > 0 ? DIR_DOWN  : DIR_UP);
    int secondDir = (abs(dx) >= abs(dy)) ? (dy > 0 ? DIR_DOWN  : DIR_UP)
                                         : (dx > 0 ? DIR_RIGHT : DIR_LEFT);
    int nx = e.tileX + DIR_DX[firstDir], ny = e.tileY + DIR_DY[firstDir];
    if (!enemyBlocked(nx, ny)) {
        e.prevX = e.tileX; e.prevY = e.tileY;       // Kayma animasyonu (v3.0)
        e.tileX = nx; e.tileY = ny;
        e.lastMoveMs = millis();
        return;
    }
    nx = e.tileX + DIR_DX[secondDir]; ny = e.tileY + DIR_DY[secondDir];
    if (!enemyBlocked(nx, ny)) {
        e.prevX = e.tileX; e.prevY = e.tileY;
        e.tileX = nx; e.tileY = ny;
        e.lastMoveMs = millis();
    }
}

// ------------------------------------------------------------
//  Düşman turu — 60 Hz sabit tick olarak çağrılır (dt biriktirme
//  dungeon.ino'da). Her düşman moveTimer dolunca hamle yapar:
//  bitişikse saldırır, değilse türüne göre yürür.
// ------------------------------------------------------------
uint32_t enemyTickCount = 0;   // Yavaşlatma paritesi için global tick sayacı

inline void tickEnemies() {
    enemyTickCount++;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy &e = enemies[i];
        if (!e.alive) continue;

        int dist = abs(player.tileX - e.tileX) + abs(player.tileY - e.tileY);

        // Goblin yakın mesafede öfkelenir: sayaç 2x hızlı dolar
        int inc = (e.type == ENEMY_GOBLIN && dist <= GOBLIN_RAGE_RANGE) ? 2 : 1;
        // Buz yavaşlatması: sayaç yarı hızda dolar (tek tick'lerde durur)
        if (millis() < e.slowedUntil && (enemyTickCount & 1)) inc = 0;
        e.moveCounter += inc;
        if (e.moveCounter < e.moveTimer) continue;
        e.moveCounter = 0;

        if (dist == 1) {
            enemyAttackPlayer(e);          // Bitişik → otomatik saldırı
        } else if (e.type == ENEMY_BAT || dist > AGGRO_RANGE) {
            enemyRandomStep(e);            // Yarasa veya menzil dışı → rastgele
        } else {
            enemyChaseStep(e);             // İskelet/goblin → takip
        }
    }
}

// ------------------------------------------------------------
//  BOSS AI (v2.0) — tickEnemies pattern'inde ayrı tick fonksiyonu.
//  Hamle: bitişikse vur, değilse 2x2 gövdeyle oyuncuya yürü.
//  Faz 2+: BOSS_SPECIAL_MS aralığıyla özel saldırı (Combat.h).
// ------------------------------------------------------------
// Boş düşman havuzu slotu (-1 = dolu)
inline int freeEnemySlot() {
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (!enemies[i].alive) return i;
    return -1;
}

// Lich çağırması: boss odasına n iskelet spawn eder
inline void summonSkeletons(int n) {
    for (int s = 0; s < n; s++) {
        int slot = freeEnemySlot();
        if (slot < 0) return;
        for (int t = 0; t < SPAWN_TRIES; t++) {
            int x, y;
            if (!randomFloorInRoom(rooms[0], x, y)) break;
            if (enemyIndexAt(x, y) >= 0 || bossOccupies(x, y)) continue;
            if (player.tileX == x && player.tileY == y) continue;
            initEnemy(enemies[slot], ENEMY_SKELETON, x, y);
            break;
        }
    }
}

// Boss'un 2x2 gövdesi (nx,ny) sol üst köşesine sığar mı
inline bool bossBlocked(int nx, int ny) {
    for (int dy = 0; dy < BOSS_SIZE_TILES; dy++) {
        for (int dx = 0; dx < BOSS_SIZE_TILES; dx++) {
            int x = nx + dx, y = ny + dy;
            if (!canEnemyWalk(x, y)) return true;
            if (enemyIndexAt(x, y) >= 0) return true;
            if (player.tileX == x && player.tileY == y) return true;
        }
    }
    return false;
}

// Oyuncu boss gövdesine DİK bitişik mi (çapraz vuruş yok — v2.1 denge)
inline bool bossAdjacentToPlayer() {
    bool inCols = player.tileX >= boss.tileX &&
                  player.tileX <  boss.tileX + BOSS_SIZE_TILES;
    bool inRows = player.tileY >= boss.tileY &&
                  player.tileY <  boss.tileY + BOSS_SIZE_TILES;
    if (inCols && (player.tileY == boss.tileY - 1 ||
                   player.tileY == boss.tileY + BOSS_SIZE_TILES)) return true;
    if (inRows && (player.tileX == boss.tileX - 1 ||
                   player.tileX == boss.tileX + BOSS_SIZE_TILES)) return true;
    return false;
}

// Oyuncuya en çok yaklaştıran BOŞ yöne 1 tile (2x2 gövdeyle).
// v3.2: 4 yönün hepsi değerlendirilir ve en iyi boş yön seçilir —
// hiçbiri yaklaştırmıyorsa bile yürünür (sütun arkasından dolaşma).
// Eski sürüm 2 yön deneyip vazgeçiyordu → boss sütunda kilitleniyordu.
inline void bossChaseStep() {
    int bestDir = -1, bestDist = 0x7FFF;
    for (int d = 0; d < 4; d++) {
        int nx = boss.tileX + DIR_DX[d], ny = boss.tileY + DIR_DY[d];
        if (bossBlocked(nx, ny)) continue;
        int nd = abs(player.tileX - nx) + abs(player.tileY - ny);
        if (nd < bestDist) { bestDist = nd; bestDir = d; }
    }
    if (bestDir < 0) return;                 // Tamamen kapalı (çok nadir)
    boss.prevX = boss.tileX; boss.prevY = boss.tileY;   // Kayma animasyonu (v3.0)
    boss.tileX += DIR_DX[bestDir];
    boss.tileY += DIR_DY[bestDir];
    boss.lastMoveMs = millis();
    // v3.3: Golem her adımda yeri sarsar (sarsıntı + alçak güm)
    if (boss.type == ENEMY_BOSS_GOLEM) golemStompFx();
}

// Boss turu — 60 Hz sabit tick (BOSS_FIGHT state'inde çağrılır)
inline void tickBoss() {
    if (!boss.active) return;

    // Faz 2+: zamanı gelen özel saldırı (millis tabanlı)
    if (boss.phase >= 2 && (int32_t)(millis() - boss.specialTimer) >= 0) {
        bossDoSpecial();
        boss.specialTimer = millis() + BOSS_SPECIAL_MS;
    }

    boss.moveCounter++;
    if (boss.moveCounter < boss.moveTimer) return;
    boss.moveCounter = 0;
    boss.patternTurn++;

    // Pattern-Based Boss AI
    if (boss.type == ENEMY_BOSS_DRAGON) {
        if (boss.patternTurn % 4 == 0) {
            bossDoSpecial(); // 1 tur durup lav kusar
            return;
        }
    } else if (boss.type == ENEMY_BOSS_LICH) {
        // v3.2: Lich YÜRÜMEZ — ışınlanan uzak mesafe büyücüsü.
        // (Eski takip AI'ı sütunlara takılıyor, oyuncuya hiç ulaşamıyordu.)
        // Bitişikse asayla vurur; değilse döngü: ışınlan → süzül →
        // büyü mermisi → süzül. Mermi hedefi kilitli → kaçılabilir.
        if (bossAdjacentToPlayer()) { bossMeleeAttack(); return; }
        int turn = boss.patternTurn % 4;
        if (turn == 0) {
            int tx, ty;
            for (int t = 0; t < SPAWN_TRIES; t++) {
                if (!randomFloorInRoom(rooms[0], tx, ty)) continue;
                if (!bossBlocked(tx, ty)) {
                    boss.tileX = tx; boss.tileY = ty;
                    boss.prevX = tx; boss.prevY = ty;   // Işınlanma: kayma yok (v3.0)
                    break;
                }
            }
        } else if (turn == 2) {
            lichCastBolt();
        }
        return;                              // 1 ve 3: yerinde süzülür
    } else if (boss.type == ENEMY_BOSS_GOLEM) {
        if (boss.patternTurn % 4 == 3) {
            boss.color = COL_BOSS_P3; // Kırmızı uyarı (Şarj)
            return;
        } else if (boss.patternTurn % 4 == 0) {
            boss.color = (boss.phase >= 3) ? COL_BOSS_P3 : (boss.phase == 2 ? COL_BOSS_P2 : COL_GOLEM);
            if (bossAdjacentToPlayer()) {
                // Şarjlı vuruş
                bossHeavyAttack();
            } else {
                bossChaseStep();
            }
            return;
        }
    }

    if (bossAdjacentToPlayer()) bossMeleeAttack();
    else                        bossChaseStep();
}
