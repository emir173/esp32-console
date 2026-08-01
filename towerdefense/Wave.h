#pragma once
// ============================================================
//  towerdefense/Wave.h — Dalga Tanımları ve Spawn Kuyruğu
//
//  Sorumluluk: 45 dalgalık (9 bölüm x 5) sürekli dalga sistemi,
//  dalga kompozisyonu (runner/swarm/soldier/uçan/zırhlı/tank/şifacı/boss),
//  spawn kuyruğu zamanlaması (saniye bazlı, pause güvenli),
//  dalga öncesi hazırlık geri sayımı ve temizlik kontrolü.
//  Her bölümün son dalgası BOSS finalidir (boss sayısı biyomla artar).
// ============================================================

#include "Config.h"
#include "Enemies.h"

// ------------------------------------------------------------
//  Spawn grubu: aynı türden ardışık düşmanlar
// ------------------------------------------------------------
struct WaveSpawn {
    EnemyType type;
    uint8_t count;
    uint16_t delayMs;   // Düşmanlar arası spawn aralığı
};

// ------------------------------------------------------------
//  Dalga durumu
// ------------------------------------------------------------
WaveSpawn waveGroups[MAX_WAVE_GROUPS];
int   waveGroupCount = 0;   // Bu dalgadaki grup sayısı
int   waveNum        = 0;   // Mevcut dalga numarası, 1..30 sürekli (skor)
int   currentLevel   = 1;   // Mevcut bölüm (1..LEVEL_COUNT)
bool  waveRunning    = false;  // Dalga aktif mi (spawn/savaş)
float prepLeft       = 0.0f;   // Dalga öncesi geri sayım (sn), 0 = yok
int   spawnGroupIdx  = 0;   // Sıradaki grup
int   spawnLeft      = 0;   // Mevcut grupta kalan düşman
float spawnTimer     = 0.0f;   // Sonraki spawn'a kalan süre (sn)

// ------------------------------------------------------------
//  Bir spawn grubu ekle (havuz sınırını aşmadan)
// ------------------------------------------------------------
inline void addGroup(EnemyType type, int count, uint16_t gapMs) {
    if (count <= 0 || waveGroupCount >= MAX_WAVE_GROUPS) return;
    waveGroups[waveGroupCount++] = { type, (uint8_t)count, gapMs };
}

// ------------------------------------------------------------
//  Dalga kompozisyonunu kur — global dalga n (1..45).
//  Adet bölüm-içi dalgaya (1..5) göre (dalgalar kısa/derli toplu);
//  TÜR karışımı bölüm+biyoma göre; boss her bölümün son dalgasında.
//  Yeni türler: SWARM (sürü), ARMORED (zırhlı), HEALER (şifacı).
// ------------------------------------------------------------
inline void buildWave(int n) {
    waveGroupCount = 0;

    // Spawn aralığı kampanya ilerledikçe kısalır (min sınırlı)
    int gap = SPAWN_GAP_BASE_MS - n * SPAWN_GAP_STEP_MS;
    if (gap < SPAWN_GAP_MIN_MS) gap = SPAWN_GAP_MIN_MS;
    int fgap = gap - SPAWN_GAP_FLYER_SUB;
    if (fgap < SPAWN_GAP_MIN_MS) fgap = SPAWN_GAP_MIN_MS;

    if (n == 1) {                                   // Dalga 1: öğretici — 5 runner
        addGroup(ENEMY_RUNNER, 5, SPAWN_GAP_BASE_MS);
        return;
    }

    int level = (n - 1) / WAVES_PER_LEVEL + 1;      // 1..9
    int wil   = (n - 1) % WAVES_PER_LEVEL + 1;      // Bölüm-içi dalga 1..5
    int biome = biomeOf(level);
    bool bossWave = (wil == WAVES_PER_LEVEL);        // Bölümün son dalgası

    int total = WAVE_BASE_COUNT + wil * 2;          // Ana kadro 6,8,10,12,14

    // Tür adetleri (biyom/dalga eşikli)
    int soldiers = (wil >= 2) ? wil : 0;
    int tanks    = (wil >= 2) ? (wil - 1) / 2 : 0;                  // 0,1,1,2
    int flyers   = (level >= 2 && wil >= 2) ? 1 + wil / 3 : 0;      // 1,2,2,2
    int armored  = (biome >= 1) ? wil / 2 : 0;                      // frost+ : 0,1,1,2
    int healers  = (biome >= 1 && wil >= 2) ? (biome == 2 ? 2 : 1) : 0;

    int runners = total - soldiers - tanks - flyers - armored - healers;
    if (runners < 1) runners = 1;

    // EKSTRA burst gruplar (ana kadroya ek)
    int swarms = (wil >= 3) ? (5 + biome * 2) : 0;                  // sürü akını
    int bosses = bossWave ? (biome + 1) : 0;                        // 1/2/3 boss

    // Spawn sırası: hafif/hızlı önce, ağır ve boss sona
    addGroup(ENEMY_RUNNER,  runners,  (uint16_t)gap);
    addGroup(ENEMY_SWARM,   swarms,   SPAWN_GAP_SWARM_MS);
    addGroup(ENEMY_SOLDIER, soldiers, (uint16_t)(gap + SPAWN_GAP_SOLDIER_ADD));
    addGroup(ENEMY_FLYER,   flyers,   (uint16_t)fgap);
    addGroup(ENEMY_ARMORED, armored,  (uint16_t)(gap + SPAWN_GAP_TANK_ADD));
    addGroup(ENEMY_TANK,    tanks,    (uint16_t)(gap + SPAWN_GAP_TANK_ADD));
    addGroup(ENEMY_HEALER,  healers,  (uint16_t)(gap + SPAWN_GAP_SOLDIER_ADD));
    addGroup(ENEMY_BOSS,    bosses,   SPAWN_GAP_BOSS_MS);
}

// ------------------------------------------------------------
//  Yeni dalgayı başlat (waveNum burada artar)
// ------------------------------------------------------------
inline void startWave() {
    waveNum++;
    buildWave(waveNum);
    waveRunning   = true;
    prepLeft      = 0.0f;
    spawnGroupIdx = 0;
    spawnLeft     = waveGroups[0].count;
    spawnTimer    = 0.0f;   // İlk düşman hemen çıkar
}

// ------------------------------------------------------------
//  Dalga sistemini sıfırla (yeni oyun)
// ------------------------------------------------------------
inline void resetWaves() {
    waveNum      = 0;
    currentLevel = 1;
    waveRunning  = false;
    prepLeft     = FIRST_WAVE_PREP_S;   // İlk dalga 3 sn geri sayımla
}

// ------------------------------------------------------------
//  Spawn kuyruğunu ilerlet — 60 Hz tick içinde çağrılır.
//  Havuz doluysa spawn bekletilir (kuyruk kaybolmaz).
// ------------------------------------------------------------
inline void tickWaveSpawns() {
    if (!waveRunning || spawnGroupIdx >= waveGroupCount) return;

    spawnTimer -= FRAME_SEC;
    if (spawnTimer > 0.0f) return;

    WaveSpawn &g = waveGroups[spawnGroupIdx];
    if (spawnEnemy(g.type, waveNum, currentLevel)) {
        spawnLeft--;
        spawnTimer = g.delayMs / 1000.0f;
        if (spawnLeft <= 0) {
            spawnGroupIdx++;
            if (spawnGroupIdx < waveGroupCount)
                spawnLeft = waveGroups[spawnGroupIdx].count;
        }
    }
    // spawnEnemy false ise (havuz dolu) timer 0'da kalır, sonraki tick dener
}

// ------------------------------------------------------------
//  Dalga temizlendi mi? (tüm gruplar çıktı + canlı düşman yok)
// ------------------------------------------------------------
inline bool waveCleared() {
    return waveRunning && spawnGroupIdx >= waveGroupCount && aliveEnemyCount() == 0;
}

// ------------------------------------------------------------
//  Dalga sonu bonusu: 8 + dalga
// ------------------------------------------------------------
inline int waveBonus() {
    return WAVE_BONUS_BASE + waveNum * WAVE_BONUS_PER_N;
}

// ------------------------------------------------------------
//  Sonraki dalga erken çağrılabilir mi? (v4.1: BTN_D)
//  Mevcut dalga tamamen spawn oldu ve canlı düşman ölmeden yeni
//  dalga çağrılabilir. Bölümün SON dalgasında (boss/harita geçişi)
//  izin verilmez — dalga çağırma bölüm içinde kalır.
// ------------------------------------------------------------
inline bool canCallNextWave() {
    return waveRunning
        && spawnGroupIdx >= waveGroupCount          // Mevcut dalganın tüm grupları çıktı
        && waveNum < TOTAL_WAVES
        && (waveNum % WAVES_PER_LEVEL) != 0;        // Bölümün son dalgası değil
}
