// ============================================================
//  E-OS — GALACTIC STRIKE
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanli cift tamponlama (Flicker-Free)
//
//  Kontroller:
//    JOY_X/Y -> Gemi hareketi
//    BTN_A   -> Ates
//    BTN_B   -> OS Menu'ye don
//    Buzzer  -> Ses efektleri
// ============================================================
// TFT_eSPI: TFT LCD ekran surucu kutuphanesi (grafik cizim icin)
#include <TFT_eSPI.h>
// SPI: Seri Periferik Arabirim - TFT ile haberlesme icin
#include <SPI.h>
// Wire: I2C haberlesme - OLED ve diger I2C cihazlar icin
#include <Wire.h>
// U8g2lib: OLED ekran (SH1106) icin grafik kutuphanesi
#include <U8g2lib.h>
// OLED ekran nesnesi (128x64, I2C, donanim adresi 0x3C varsayilan)
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
// Preferences: Kalici depolama (NVS) - rekor ve ayar kaydi icin
#include <Preferences.h>
// hardware_config.h: Donanim pin tanimlari (BUZZER, BTN_*, JOY_*, SPI_*)
#include "../hardware_config.h"
// dev_tools.h: Gelistirme araclarlari (ekran goruntusu yakalama, debug)
#include "../dev_tools.h"
// GameBase.h: OS ortak API (ses, ekran, geri donus)
#include "../GameBase.h"

// ============ Ekran ============
// SCR_W: Ekran genisligi (piksel) - yatay (landscape) modda 160
#define SCR_W    160
// SCR_H: Ekran yuksekligi (piksel) - 128
#define SCR_H    128
// HUD_H: Alt bilgi cubugu (skor/can/dalga) yuksekligi (piksel)
#define HUD_H    12

// ============ Renkler ============
// RGB makrosu: 24-bit RGB degerini 16-bit RGB565 formatina cevirir
//  (r)&0xF8 -> kirmizi 5 bit, (g)&0xFC -> yesil 6 bit, (b)>>3 -> mavi 5 bit
#define RGB(r,g,b) ((uint16_t)(((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))

// Arkaplan ve yildiz renkleri
#define COL_BG         RGB(5, 5, 15)      // Koyu uzay mavisi (arkaplan)
#define COL_STAR_DIM   RGB(60, 60, 80)    // Soluk yildiz (uzak katman)
#define COL_STAR_BRI   RGB(180, 180, 220) // Parlak yildiz (orta katman)
#define COL_STAR_GOLD  RGB(255, 220, 100) // Altin yildiz (yakin katman)

// Oyuncu gemisi renkleri
#define COL_SHIP_A     RGB(80, 200, 255)  // Govde dolgu (acik mavi)
#define COL_SHIP_B     RGB(40, 130, 200)  // Govde cerceve/kanat (koyu mavi)
#define COL_SHIP_ENG   RGB(255, 150, 40)  // Motor alevi (turuncu)

// Mermi renkleri
#define COL_BULLET_P   RGB(255, 255, 100) // Oyuncu mermisi (sari)
#define COL_BULLET_E   RGB(255, 80, 80)   // Dusman mermisi (kirmizi)

// Dusman tip renkleri
#define COL_EN_BASIC   RGB(220, 60, 60)   // Temel dusman (kirmizi)
#define COL_EN_FAST    RGB(255, 180, 40)  // Hizli dusman (turuncu)
#define COL_EN_TANK    RGB(100, 200, 100) // Tank dusman (yesil)
#define COL_EN_BOSS    RGB(200, 60, 200)  // Boss dusman (mor)

// Patlama renkleri (yumusak gecis: sari -> turuncu -> kirmizi)
#define COL_BOOM_A     RGB(255, 200, 60)  // Patlama baslangic (sari)
#define COL_BOOM_B     RGB(255, 100, 40)  // Patlama orta (turuncu)
#define COL_BOOM_C     RGB(255, 60, 20)   // Patlama son (kirmizi)

// Power-up renkleri
#define COL_PWR_TRIPLE RGB(255, 255, 100) // Triple shot (sari)
#define COL_PWR_SHIELD RGB(80, 200, 255)  // Kalkan (mavi)
#define COL_PWR_LIFE   RGB(255, 80, 80)   // Ek can (kirmizi)

// HUD (alt bilgi cubugu) renkleri
#define COL_HUD_BG     RGB(0, 0, 30)      // HUD arkaplan (koyu lacivert)
#define COL_HUD_TXT    RGB(180, 180, 200) // HUD metin rengi (gri-mavi)

// ============ Oyun Sabitleri ============
// FPS60: Frame kilidi kaldirildi, Delta-Time sistemine gecildi
#define MAX_P_BULLETS   16  // Ayni anda maksimum oyuncu mermisi sayisi
#define MAX_E_BULLETS   24  // Ayni anda maksimum dusman mermisi sayisi
#define MAX_ENEMIES     12  // Ekranda ayni anda bulunabilecek dusman sayisi
#define MAX_STARS       40  // Arkaplan yildiz parcacik sayisi
#define MAX_EXPLOSIONS  8   // Ayni anda calisabilecek patlama sayisi
#define MAX_POWERUPS    3   // Ekranda ayni anda bulunabilecek power-up sayisi

#define SHIP_W          9   // Oyuncu gemisi genisligi (carpisma hesabi icin)
#define SHIP_H          10  // Oyuncu gemisi yuksekligi (carpisma hesabi icin)

// FPS60: Ates bekleme suresi frame-tabanli -> saniye-tabanli (8 frame / 30 FPS = 0.267s)
#define SHOOT_COOLDOWN  0.267f

// ============ Nesneler ============
// tft: Donanim TFT ekran nesnesi (gercek cikis icin)
TFT_eSPI tft = TFT_eSPI();
// canvas: Cift tamponlama icin sprite (flicker-free cizim)
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Ses ============
// soundEnabled: Ses efektleri acik/kapali durumu (OS ayarindan okunur)
bool soundEnabled = true;
// playSound — GameBase.h osPlaySound wrapper (eski API uyumu)
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// returnToOS — GameBase.h osReturnToOS wrapper (eski API uyumu)
void returnToOS() {
    osReturnToOS(tft);
}

// ============ Durum ============
// Oyun durum makinesi (state machine) - su anki ekran/mantik durumunu belirler
//  ST_TITLE:    Baslik ekrani
//  ST_PLAY:     Aktif oyun
//  ST_GAMEOVER: Oyun bitti ekrani
//  ST_PAUSE:    Duraklatilmis oyun
enum State { ST_TITLE, ST_PLAY, ST_GAMEOVER, ST_PAUSE };
State state = ST_TITLE;

// ============ Yildizlar ============
// FPS60: speed artik piksel/saniye cinsinden (orijinal px/frame * 30)
// Star: Arkaplan kayan yildiz parcacigi (parallax katmanli)
//  x, y:    Konum
//  speed:   Asagiya kayma hizi (px/s)
//  layer:   Derinlik katmani (0-2; 2 en yakin/parlak)
struct Star { float x, y, speed; uint8_t layer; };
Star stars[MAX_STARS];

// ============ Mermiler ============
// FPS60: vx,vy artik piksel/saniye cinsinden
// Bullet: Bir mermi parcacigi (oyuncu veya dusman)
//  x, y:   Konum
//  vx, vy: Hiz vektoru (px/s)
//  active: Mermi su an aktif mi (havuz mantigi)
struct Bullet { float x, y, vx, vy; bool active; };
Bullet pBullets[MAX_P_BULLETS];  // Oyuncu mermi havuzu
Bullet eBullets[MAX_E_BULLETS];  // Dusman mermi havuzu

// ============ Dusmanlar ============
// EnemyType: Dusman turleri - her biri farkli davranis ve guce sahip
//  EN_BASIC: Standart, yavas, 1 can
//  EN_FAST:  Hizli, az ates, 1 can
//  EN_TANK:  Yavas, dayanikli, 3 can
//  EN_BOSS:  Boss, yelpaze ates, 15 can (her 5. dalga)
enum EnemyType { EN_BASIC, EN_FAST, EN_TANK, EN_BOSS };
// Enemy: Bir dusman birimi
//  x, y:           Konum
//  vx, vy:         Hiz vektoru (px/s)
//  hp, maxHp:      Mevcut / maksimum can
//  shootTimer:     Sonraki atese kalan sure (s)
//  shootInterval:  Atisler arasi bekleme suresi (s)
//  type:           Dusman turu
//  active:         Dusman su an aktif mi (havuz mantigi)
struct Enemy {
    float x, y;
    float vx, vy;
    int hp, maxHp;
    // FPS60: Zamanlayicilar frame-tabanli -> saniye-tabanli
    float shootTimer;
    float shootInterval;
    EnemyType type;
    bool active;
};
Enemy enemies[MAX_ENEMIES];

// Manuel prototip (Arduino IDE otomatik prototip olusturamaz)
// spawnEnemy fonksiyonu ileride tanimlandigi icin prototipi burada verilir
void spawnEnemy(EnemyType type);

// ============ Patlamalar ============
// FPS60: frame(int) -> elapsed(float), saniye cinsinden gecen sure
// Explosion: Bir patlama efekti (genisleyen daire animasyonu)
//  x, y:      Konum
//  elapsed:   Patlama baslangicindan beri gecen sure (s)
//  active:    Patlama su an aktif mi
struct Explosion { float x, y; float elapsed; bool active; };
Explosion explosions[MAX_EXPLOSIONS];

// ============ Power-up ============
// PwrType: Toplanabilir guc-yukseltme turleri
//  PWR_TRIPLE: Triple shot (3'lü ates) - 10 saniye
//  PWR_SHIELD: Kalkan (hasar emer) - 10 saniye
//  PWR_LIFE:   Ek can (+1 hp)
enum PwrType { PWR_TRIPLE, PWR_SHIELD, PWR_LIFE };
// PowerUp: Dusmandan dusen toplabilir guc-yükseltme
//  x, y:   Konum (asagiya kayar)
//  type:   Power-up turu
//  active: Su an aktif mi
struct PowerUp { float x, y; PwrType type; bool active; };
PowerUp powerUps[MAX_POWERUPS];

// ============ Oyuncu ============
// Ship: Oyuncu uzay gemisi durumu
//  x, y:          Konum
//  hp, maxHp:     Mevcut / maksimum can
//  shootCD:       Ates bekleme suresi sayaci (s, 0 olunca ates edebilir)
//  tripleTimer:   Triple shot kalan sure (s)
//  shieldTimer:   Kalkan kalan sure (s)
//  invincTimer:   Dokunulmazlik kalan sure (s) - hasar alinca aktif
//  score:         Oyuncu skoru
struct Ship {
    float x, y;
    int hp, maxHp;
    // FPS60: Zamanlayicilar frame-tabanli -> saniye-tabanli
    float shootCD;
    float tripleTimer;   // Triple shot suresi
    float shieldTimer;   // Kalkan suresi
    float invincTimer;   // Dokunulmazlik
    int score;
};
Ship ship;

// ============ Dalga Sistemi ============
int curWave = 0;                // Su anki dalga numarasi (0-tabanli)
// FPS60: Zamanlayici frame-tabanli -> saniye-tabanli
float waveSpawnTimer = 0.0f;    // Sonraki dusman dogusuna kalan sure (s)
int waveEnemiesSpawned = 0;     // Bu dalgada su ana kadar dogmus dusman sayisi
int waveEnemiesPerWave = 5;     // Bu dalgada toplam dogacak dusman sayisi (Artarak artar)
bool waveActive = false;        // Dalga su an aktif mi

// ============ Genel ============
int joyCenterX, joyCenterY;     // Joystick kalibrasyon merkez degerleri
uint32_t lastFrameMs = 0;       // Delta-Time: Onceki karenin millis() degeri
// FPS60: animTick(frame) -> animFrame(sanal 30fps kare sayaci) + animTime(saniye)
float animTime = 0.0f;          // Toplam animasyon suresi (s) - kare hesabi icin
int animFrame = 0;              // Sanal 30fps kare sayaci (animasyon tick'i)
uint32_t stateTimer = 0;        // Durum degisim zaman damgasi (oyun bitti suresi vb.)
int highScore = 0;              // Kalici olarak saklanan en yuksek skor

uint32_t fpsFrameCount = 0;     // FPS olcumu: bu saniyedeki kare sayaci
uint32_t fpsStartTime = 0;      // FPS olcumu: saniye baslangic zamani
int currentFPS = 0;             // Guncel FPS degeri (HUD'da gosterilir)

// ============================================================
//  BASLANGIC AYARLARI
// ============================================================
// initStars — Yildiz parlaksini rastgele konumla baslatir
//  Her yildiza rastgele x/y, katman (0-2) ve katmana bagli hiz verir
void initStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(0, SCR_W);
        stars[i].y = random(0, SCR_H);
        stars[i].layer = random(0, 3);
        // FPS60: px/frame -> px/s (orijinal 0.3 + layer*0.4, *30)
        // Katman 0: 9 px/s, Katman 1: 21 px/s, Katman 2: 33 px/s
        stars[i].speed = 9.0f + stars[i].layer * 12.0f;
    }
}

// clearBullets — Tum mermileri (oyuncu ve dusman) pasif hale getirir
void clearBullets() {
    for (int i = 0; i < MAX_P_BULLETS; i++) pBullets[i].active = false;
    for (int i = 0; i < MAX_E_BULLETS; i++) eBullets[i].active = false;
}

// clearEnemies — Tum dusmanlari pasif hale getirir
void clearEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
}

// clearExplosions — Tum patlamalari pasif hale getirir
void clearExplosions() {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) explosions[i].active = false;
}

// clearPowerUps — Tum power-up'lari pasif hale getirir
void clearPowerUps() {
    for (int i = 0; i < MAX_POWERUPS; i++) powerUps[i].active = false;
}

// spawnExplosion — Belirtilen konumda yeni bir patlama efekti olusturur
//  x, y: Patlama konumu (piksel)
//  Bos patlama slotunu bulur, elapsed=0 ile aktif eder
void spawnExplosion(float x, float y) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) {
            // FPS60: frame(int) -> elapsed(float), 0.0f baslangic
            explosions[i] = {x, y, 0.0f, true};
            return;
        }
    }
}

// resetGame — Yeni bir oyun baslatmak icin tum oyun durumunu sifirlar
//  Gemi konumu, canlar, skor, dalga, timerlar ve tum nesneleri resetler
void resetGame() {
    ship.x = SCR_W / 2;             // Gemi ekranin ust-orta-altinda baslar
    ship.y = SCR_H - 20;            // 20px alt bosluk
    ship.hp = ship.maxHp = 5;       // Baslangic cani: 5
    ship.shootCD = 0.0f;         // FPS60: float
    ship.tripleTimer = 0.0f;     // FPS60: float
    ship.shieldTimer = 0.0f;     // FPS60: float
    ship.invincTimer = 3.0f;     // FPS60: 90 frame / 30 FPS = 3.0 saniye
    ship.score = 0;
    curWave = 0;
    waveSpawnTimer = 3.0f;       // FPS60: 90 frame / 30 FPS = 3.0 saniye
    waveEnemiesSpawned = 0;
    waveEnemiesPerWave = 4;         // Ilk dalga 4 dusmanla baslar
    waveActive = true;
    animTime = 0.0f;             // FPS60: animasyon zamanini sifirla
    clearBullets();
    clearEnemies();
    clearExplosions();
    clearPowerUps();
}

// ============================================================
//  DUSMAN SPAWN
// ============================================================
// spawnEnemy — Belirtilen tipte yeni bir dusman dogurur
//  type: Dusman turu (EN_BASIC/EN_FAST/EN_TANK/EN_BOSS)
//  Bos dusman slotunu bulur, tipe gore hiz/can/atis araligi atar
void spawnEnemy(EnemyType type) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) continue;  // Dolu slotu atla
        Enemy& e = enemies[i];
        e.active = true;
        e.type = type;
        e.x = random(15, SCR_W - 15);  // X: kenarlardan 15px icte rastgele
        e.y = -10;                      // Y: ekranin ustunde baslar (disaridan girer)
        // FPS60: px/frame -> px/s (0.3f * 30 = 9.0f)
        // Yatay hareket yonu rastgele (sag veya sol)
        e.vx = (random(0, 2) ? 9.0f : -9.0f);

        // Tipe ozel hiz, can ve atis araligi ayarlari
        switch (type) {
            case EN_BASIC:
                // FPS60: vy 0.6f*30=18.0f, shootInterval 90/30=3.0s
                // Temel dusman: orta hiz, 1 can, 3sn'de bir ates
                e.vy = 18.0f; e.hp = e.maxHp = 1;
                e.shootInterval = 3.0f; break;
            case EN_FAST:
                // FPS60: vy 1.2f*30=36.0f, shootInterval 120/30=4.0s
                // Hizli dusman: cok hizli, 1 can, 4sn'de bir ates
                e.vy = 36.0f; e.hp = e.maxHp = 1;
                e.shootInterval = 4.0f; break;
            case EN_TANK:
                // FPS60: vy 0.4f*30=12.0f, shootInterval 60/30=2.0s
                // Tank dusman: yavas, 3 can, 2sn'de bir ates
                e.vy = 12.0f; e.hp = e.maxHp = 3;
                e.shootInterval = 2.0f; break;
            case EN_BOSS:
                // FPS60: vy 0.2f*30=6.0f, shootInterval 30/30=1.0s
                // Boss: cok yavas, 15 can, 1sn'de yelpaze ates, ekran ortasinda dogar
                e.x = SCR_W / 2;
                e.vy = 6.0f; e.hp = e.maxHp = 15;
                e.shootInterval = 1.0f; break;
        }
        e.shootTimer = e.shootInterval;  // Ilk ates gecikmesi
        return;
    }
}

// ============================================================
//  ATES FONKSIYONLARI
// ============================================================
// firePlayer — Oyuncu gemisinden mermi atesler
//  Ates bekleme suresi (shootCD) kontrol eder; triple shot aktifse
//  ek 2 mermi (sag-sol capraz) daha atesler. Ses efekti calar.
void firePlayer() {
    if (ship.shootCD > 0.0f) return;  // FPS60: float karsilastirma
    ship.shootCD = SHOOT_COOLDOWN;     // FPS60: saniye cinsinden

    // Ana mermi
    for (int i = 0; i < MAX_P_BULLETS; i++) {
        if (!pBullets[i].active) {
            // FPS60: vy -4.0f*30 = -120.0f px/s
            pBullets[i] = {ship.x, ship.y - 5, 0.0f, -120.0f, true};
            break;
        }
    }

    // Triple shot
    if (ship.tripleTimer > 0.0f) {  // FPS60: float karsilastirma
        for (int s = 0; s < 2; s++) {
            // FPS60: vx ±1.5f*30=±45.0f, vy -3.5f*30=-105.0f
            float vxOff = (s == 0) ? -45.0f : 45.0f;
            for (int i = 0; i < MAX_P_BULLETS; i++) {
                if (!pBullets[i].active) {
                    pBullets[i] = {ship.x, ship.y - 3, vxOff, -105.0f, true};
                    break;
                }
            }
        }
    }
    playSound(1500, 10);
}

// fireEnemy — Dusmandan mermi atesler (oyuncuya dogru yonlu)
//  ex, ey: Dusman konumu
//  tvx, tvy: Mermi hiz vektoru (px/s, zaten normalize edilmis yon * hiz)
void fireEnemy(float ex, float ey, float tvx, float tvy) {
    for (int i = 0; i < MAX_E_BULLETS; i++) {
        if (!eBullets[i].active) {
            eBullets[i] = {ex, ey + 5, tvx, tvy, true};
            return;
        }
    }
}

// ============================================================
//  POWER-UP SPAWN
// ============================================================
// spawnPowerUp — Belirtilen konumda (%25 olasilikla) power-up dogurur
//  x, y: Dogus konumu (genelde yok edilen dusmanin konumu)
//  Turu rastgele secilir (TRIPLE/SHIELD/LIFE)
void spawnPowerUp(float x, float y) {
    if (random(0, 100) > 25) return; // %25 sans
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerUps[i].active) {
            PwrType t = (PwrType)random(0, 3);
            powerUps[i] = {x, y, t, true};
            return;
        }
    }
}

// ============================================================
//  GUNCELLEME
// ============================================================
// FPS60: Tum hareket guncellemelerine dt parametresi eklendi
// updateStars — Yildizlari asagiya kaydirir (parallax arkaplan)
//  dt: Gecen sure (s) - delta-time ile cerceve bagimsiz hareket
void updateStars(float dt) {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].y += stars[i].speed * dt;  // FPS60: dt ile carp
        if (stars[i].y > SCR_H) {
            stars[i].y = 0;
            stars[i].x = random(0, SCR_W);
        }
    }
}

// updateBullets — Tum mermileri (oyuncu+dusman) hareket ettirir
//  dt: Gecen sure (s). Ekrandan cikan mermileri pasif yapar.
void updateBullets(float dt) {
    // Oyuncu mermileri
    for (int i = 0; i < MAX_P_BULLETS; i++) {
        if (!pBullets[i].active) continue;
        pBullets[i].x += pBullets[i].vx * dt;  // FPS60: dt ile carp
        pBullets[i].y += pBullets[i].vy * dt;  // FPS60: dt ile carp
        if (pBullets[i].y < -5 || pBullets[i].x < -5 || pBullets[i].x > SCR_W + 5)
            pBullets[i].active = false;
    }
    // Dusman mermileri
    for (int i = 0; i < MAX_E_BULLETS; i++) {
        if (!eBullets[i].active) continue;
        eBullets[i].x += eBullets[i].vx * dt;  // FPS60: dt ile carp
        eBullets[i].y += eBullets[i].vy * dt;  // FPS60: dt ile carp
        if (eBullets[i].y > SCR_H + 5 || eBullets[i].x < -5 || eBullets[i].x > SCR_W + 5)
            eBullets[i].active = false;
    }
}

// updateEnemies — Dusman yapay zekasini (AI) gunceller
//  Hareket, sinir kontrolu, ekran disi cikis ve oyuncuya yonelik atesi yonetir
//  dt: Gecen sure (s)
void updateEnemies(float dt) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        Enemy& e = enemies[i];

        e.x += e.vx * dt;  // FPS60: dt ile carp
        e.y += e.vy * dt;  // FPS60: dt ile carp

        // Yatay sinirlar
        if (e.x < 10 || e.x > SCR_W - 10) e.vx = -e.vx;

        // Boss: Y sinirlama (cok asagi inmesin)
        if (e.type == EN_BOSS && e.y > 40) { e.vy = 0.0f; e.y = 40.0f; }

        // Ekrandan cikti
        if (e.y > SCR_H + 15) { e.active = false; continue; }

        // Ates
        e.shootTimer -= dt;  // FPS60: dt ile azalt
        if (e.shootTimer <= 0.0f) {
            e.shootTimer = e.shootInterval;
            // Oyuncuya dogru ates
            float dx = ship.x - e.x;
            float dy = ship.y - e.y;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 1) { dx /= len; dy /= len; }

            if (e.type == EN_BOSS) {
                // Boss: yelpaze ates
                for (int s = -1; s <= 1; s++) {
                    float a = s * 0.3f;
                    float bvx = dx * cosf(a) - dy * sinf(a);
                    float bvy = dx * sinf(a) + dy * cosf(a);
                    // FPS60: mermi hizi 1.5f*30=45.0f px/s
                    fireEnemy(e.x, e.y, bvx * 45.0f, bvy * 45.0f);
                }
            } else {
                // FPS60: mermi hizi FAKT->1.8f*30=54.0f, diger->1.2f*30=36.0f
                float spd = (e.type == EN_FAST) ? 54.0f : 36.0f;
                fireEnemy(e.x, e.y, dx * spd, dy * spd);
            }
            if (e.type != EN_BOSS) playSound(300, 8);
        }
    }
}

// updateWaveSpawning — Dalga (wave) sistemini yonetir
//  Dalga icinde dusman dogurur, tum dusmanlar olunce sonraki dalgaya gecer
//  Her 5. dalgada Boss dogar. Dalga numarasi arttikca dusman sayisi artar.
//  dt: Gecen sure (s)
void updateWaveSpawning(float dt) {
    if (!waveActive) return;

    waveSpawnTimer -= dt;  // FPS60: dt ile azalt
    if (waveSpawnTimer > 0.0f) return;

    if (waveEnemiesSpawned < waveEnemiesPerWave) {
        // Boss her 5. dalgada
        if (curWave > 0 && curWave % 5 == 0 && waveEnemiesSpawned == 0) {
            spawnEnemy(EN_BOSS);
        } else {
            // Rastgele dusman turu
            int r = random(0, 100);
            if (r < 50)      spawnEnemy(EN_BASIC);
            else if (r < 80) spawnEnemy(EN_FAST);
            else              spawnEnemy(EN_TANK);
        }
        waveEnemiesSpawned++;
        // FPS60: (40+random(0,30)) frame / 30 FPS -> saniye (1.33-2.33s)
        waveSpawnTimer = (40.0f + random(0, 30)) / 30.0f;
    } else {
        // Tum dusmanlar oldu mu?
        bool allDead = true;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) { allDead = false; break; }
        }
        if (allDead) {
            // Sonraki dalga
            curWave++;
            waveEnemiesSpawned = 0;
            waveEnemiesPerWave = min(4 + curWave, 10);
            waveSpawnTimer = 3.0f; // FPS60: 90 frame / 30 FPS = 3.0 saniye
            playSound(880, 80);
        }
    }
}

// updateCollisions — Tum carpisma (collision) kontrollerini yapar
//  1) Oyuncu mermisi vs dusman (hasar/puan/powerup)
//  2) Dusman mermisi vs oyuncu (kalkan/can/oyun bitti)
//  3) Dusman vs oyuncu (carpisma hasari)
//  4) Power-up toplama
//  dt: Gecen sure (s) - power-up hareketi icin
void updateCollisions(float dt) {
    // Oyuncu mermileri vs dusmanlar
    for (int b = 0; b < MAX_P_BULLETS; b++) {
        if (!pBullets[b].active) continue;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!enemies[e].active) continue;
            float ew = (enemies[e].type == EN_BOSS) ? 16 : 8;
            float eh = (enemies[e].type == EN_BOSS) ? 12 : 8;
            if (fabsf(pBullets[b].x - enemies[e].x) < ew/2 &&
                fabsf(pBullets[b].y - enemies[e].y) < eh/2) {
                pBullets[b].active = false;
                enemies[e].hp--;
                if (enemies[e].hp <= 0) {
                    enemies[e].active = false;
                    spawnExplosion(enemies[e].x, enemies[e].y);
                    int pts = (enemies[e].type == EN_BOSS) ? 500 :
                              (enemies[e].type == EN_TANK) ? 100 :
                              (enemies[e].type == EN_FAST) ? 75 : 50;
                    ship.score += pts;
                    playSound(600, 40);
                    spawnPowerUp(enemies[e].x, enemies[e].y);
                } else {
                    playSound(900, 10);
                }
                break;
            }
        }
    }

    // Dusman mermileri vs oyuncu
    if (ship.invincTimer <= 0.0f) {  // FPS60: float karsilastirma
        for (int b = 0; b < MAX_E_BULLETS; b++) {
            if (!eBullets[b].active) continue;
            if (fabsf(eBullets[b].x - ship.x) < SHIP_W/2 + 2 &&
                fabsf(eBullets[b].y - ship.y) < SHIP_H/2 + 2) {
                eBullets[b].active = false;
                if (ship.shieldTimer > 0.0f) {  // FPS60: float karsilastirma
                    ship.shieldTimer -= 1.0f;    // FPS60: 30 frame / 30 FPS = 1.0 saniye azalt
                    playSound(1200, 20);
                } else {
                    ship.hp--;
                    ship.invincTimer = 3.0f;     // FPS60: 90 frame / 30 FPS = 3.0 saniye
                    spawnExplosion(ship.x, ship.y);
                    playSound(150, 200);
                    if (ship.hp <= 0) {
                        state = ST_GAMEOVER;
                        stateTimer = millis();
                        if (ship.score > highScore) {
                            highScore = ship.score;
                            Preferences prefs;
                            prefs.begin("gstrike", false);
                            prefs.putInt("hi", highScore);
                            prefs.end();
                        }
                    }
                }
                break;
            }
        }
    }

    // Dusmanlar vs oyuncu (carpma)
    if (ship.invincTimer <= 0.0f) {  // FPS60: float karsilastirma
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!enemies[e].active) continue;
            float ew = (enemies[e].type == EN_BOSS) ? 16 : 8;
            if (fabsf(enemies[e].x - ship.x) < ew/2 + SHIP_W/2 &&
                fabsf(enemies[e].y - ship.y) < 8) {
                ship.hp--;
                ship.invincTimer = 3.0f;  // FPS60: 90 frame / 30 FPS = 3.0 saniye
                spawnExplosion(ship.x, ship.y);
                playSound(150, 200);
                if (ship.hp <= 0) {
                    state = ST_GAMEOVER;
                    stateTimer = millis();
                    if (ship.score > highScore) {
                        highScore = ship.score;
                        Preferences prefs;
                        prefs.begin("gstrike", false);
                        prefs.putInt("hi", highScore);
                        prefs.end();
                    }
                }
                break;
            }
        }
    }

    // Power-up toplama
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerUps[i].active) continue;
        powerUps[i].y += 24.0f * dt;  // FPS60: 0.8f*30=24.0f px/s, dt ile carp
        if (powerUps[i].y > SCR_H + 10) { powerUps[i].active = false; continue; }

        if (fabsf(powerUps[i].x - ship.x) < 8 && fabsf(powerUps[i].y - ship.y) < 8) {
            powerUps[i].active = false;
            switch (powerUps[i].type) {
                // FPS60: 300 frame / 30 FPS = 10.0 saniye
                case PWR_TRIPLE: ship.tripleTimer = 10.0f; playSound(1000, 40); break;
                case PWR_SHIELD: ship.shieldTimer = 10.0f; playSound(800, 40); break;
                case PWR_LIFE:
                    if (ship.hp < ship.maxHp) ship.hp++;
                    playSound(600, 40); break;
            }
        }
    }
}

// updateExplosions — Patlama efektlerini gunceller (yasam suresi)
//  dt: Gecen sure (s). 0.4 saniye sonra patlamayi pasif yapar.
void updateExplosions(float dt) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) continue;
        explosions[i].elapsed += dt;  // FPS60: dt ile artir
        // FPS60: 12 frame / 30 FPS = 0.4 saniye omur
        if (explosions[i].elapsed > 0.4f) explosions[i].active = false;
    }
}

// ============================================================
//  CİZİM
// ============================================================
// drawStars — Arkaplan yildizlarini canvas'a cizer (katmana gore renk)
void drawStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        uint16_t c = (stars[i].layer == 0) ? COL_STAR_DIM :
                     (stars[i].layer == 1) ? COL_STAR_BRI : COL_STAR_GOLD;
        canvas.drawPixel((int)stars[i].x, (int)stars[i].y, c);
    }
}

// ============================================================
//  drawShip — Oyuncu uzay gemisini canvas'a çiz
//  Hareket animasyonu, motor parlaması, kalkan efekti içerir
//  Dokunulmazlik sirasinda yanip soner (her 0.1s'de toggle)
// ============================================================
void drawShip() {
    int sx = (int)ship.x;
    int sy = (int)ship.y;

    // Yanip sonme
    // FPS60: invincTimer artik saniye cinsinden, (invincTimer*10)%2 her 0.1s'de bir toggle eder
    if (ship.invincTimer > 0.0f && ((int)(ship.invincTimer * 10.0f) % 2)) return;

    // Govde (uc kenari)
    canvas.fillTriangle(sx, sy - 5, sx - 4, sy + 4, sx + 4, sy + 4, COL_SHIP_A);
    canvas.drawTriangle(sx, sy - 5, sx - 4, sy + 4, sx + 4, sy + 4, COL_SHIP_B);

    // Kanatlar
    canvas.fillRect(sx - 5, sy + 1, 2, 4, COL_SHIP_B);
    canvas.fillRect(sx + 4, sy + 1, 2, 4, COL_SHIP_B);

    // Motor alevi (animasyonlu)
    // FPS60: animFrame sanal 30fps kare sayaci
    if (animFrame % 3 != 0) {
        int flameH = 2 + (animFrame % 2);
        canvas.fillRect(sx - 1, sy + 5, 3, flameH, COL_SHIP_ENG);
    }

    // Kalkan gosterim
    // FPS60: shieldTimer artik saniye cinsinden, float karsilastirma
    if (ship.shieldTimer > 0.0f) {
        canvas.drawCircle(sx, sy, 8, RGB(60, 160, 255));
        if (animFrame % 4 < 2) canvas.drawCircle(sx, sy, 9, RGB(30, 100, 200));
    }
}

// drawBullets — Tum aktif mermileri canvas'a cizer
//  Oyuncu mermisi: sari dikdortgen, Dusman mermisi: kirmizi daire
void drawBullets() {
    for (int i = 0; i < MAX_P_BULLETS; i++) {
        if (!pBullets[i].active) continue;
        canvas.fillRect((int)pBullets[i].x - 1, (int)pBullets[i].y - 1, 2, 3, COL_BULLET_P);
    }
    for (int i = 0; i < MAX_E_BULLETS; i++) {
        if (!eBullets[i].active) continue;
        canvas.fillCircle((int)eBullets[i].x, (int)eBullets[i].y, 2, COL_BULLET_E);
    }
}

// drawEnemies — Tum aktif dusmanlari tiplerine gore canvas'a cizer
//  Her tip farkli sekil: BASIC/FAST (ucgen), TANK (kare), BOSS (blok+can cubugu)
void drawEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        Enemy& e = enemies[i];
        int sx = (int)e.x;
        int sy = (int)e.y;

        switch (e.type) {
            case EN_BASIC:
                canvas.fillTriangle(sx, sy + 4, sx - 4, sy - 3, sx + 4, sy - 3, COL_EN_BASIC);
                canvas.drawPixel(sx - 1, sy, 0xFFFF);
                canvas.drawPixel(sx + 1, sy, 0xFFFF);
                break;
            case EN_FAST:
                canvas.fillTriangle(sx, sy + 3, sx - 3, sy - 3, sx + 3, sy - 3, COL_EN_FAST);
                canvas.drawPixel(sx, sy, 0xFFFF);
                break;
            case EN_TANK:
                canvas.fillRect(sx - 4, sy - 4, 9, 8, COL_EN_TANK);
                canvas.drawRect(sx - 4, sy - 4, 9, 8, RGB(60, 150, 60));
                canvas.fillRect(sx - 1, sy + 2, 3, 3, RGB(200, 200, 200));
                break;
            case EN_BOSS:
                canvas.fillRect(sx - 8, sy - 6, 17, 12, COL_EN_BOSS);
                canvas.drawRect(sx - 8, sy - 6, 17, 12, RGB(150, 40, 150));
                // Boss gozler
                canvas.fillRect(sx - 4, sy - 3, 3, 3, RGB(255, 80, 80));
                canvas.fillRect(sx + 2, sy - 3, 3, 3, RGB(255, 80, 80));
                // Can cubugu (Boss ustunde, max 16px, hp'ye gore kisalir)
                int bw = 16 * e.hp / e.maxHp;  // Can oranina gore cubuk genisligi
                canvas.fillRect(sx - 8, sy - 9, bw, 2, RGB(255, 60, 60));
                canvas.drawRect(sx - 8, sy - 9, 16, 2, RGB(100, 100, 100));
                break;
        }
    }
}

// drawExplosions — Patlama efektlerini canvas'a cizer
//  Zamanla genisleyen daire; renk yumusak gecis yapar (sari->turuncu->kirmizi)
void drawExplosions() {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) continue;
        // FPS60: elapsed(saniye) -> sanal kare (30fps esdegeri)
        int f = (int)(explosions[i].elapsed * 30.0f);
        int r = 3 + f;  // Patlama yaricapi zamanla buyur (3 pikselden baslar)
        // Renk gecisi: f<4 sari, f<8 turuncu, sonrasi kirmizi
        uint16_t c = (f < 4) ? COL_BOOM_A : (f < 8) ? COL_BOOM_B : COL_BOOM_C;
        canvas.drawCircle((int)explosions[i].x, (int)explosions[i].y, r, c);
        // Ilk 6 karede icini doldur (daha parlak patlama etkisi)
        if (f < 6) canvas.fillCircle((int)explosions[i].x, (int)explosions[i].y, r/2, c);
    }
}

// drawPowerUps — Power-up'ları canvas'a cizer (renkli kare + harf etiketi)
//  T=Triple(sari), S=Shield(mavi), +=Life(kirmizi)
void drawPowerUps() {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerUps[i].active) continue;
        int sx = (int)powerUps[i].x;
        int sy = (int)powerUps[i].y;
        uint16_t c;
        char label;
        switch (powerUps[i].type) {
            case PWR_TRIPLE: c = COL_PWR_TRIPLE; label = 'T'; break;
            case PWR_SHIELD: c = COL_PWR_SHIELD; label = 'S'; break;
            case PWR_LIFE:   c = COL_PWR_LIFE;   label = '+'; break;
            default:         c = 0xFFFF;         label = '?'; break;
        }
        canvas.fillRect(sx - 4, sy - 4, 9, 9, c);
        canvas.drawRect(sx - 4, sy - 4, 9, 9, 0xFFFF);
        canvas.setTextColor(0x0000);
        canvas.setCursor(sx - 2, sy - 3);
        canvas.print(label);
    }
}

// drawHUD — Alt bilgi cubugunu (HUD) canvas'a cizer
//  Icerik: Skor, Can (kalpler), Dalga no, FPS, Triple/Shield gostergeleri
void drawHUD() {
    canvas.fillRect(0, SCR_H - HUD_H, SCR_W, HUD_H, COL_HUD_BG);
    canvas.drawFastHLine(0, SCR_H - HUD_H, SCR_W, RGB(40, 40, 80));

    canvas.setTextSize(1);
    canvas.setTextColor(COL_HUD_TXT);

    // Skor
    canvas.setCursor(2, SCR_H - HUD_H + 2);
    canvas.printf("SKR:%d", ship.score);

    // Can (kalpler) - Biraz daha bitisik cizelim (7 px aralik)
    for (int i = 0; i < ship.hp; i++) {
        canvas.fillRect(50 + i * 7, SCR_H - HUD_H + 4, 5, 5, RGB(255, 60, 60));
    }

    // Dalga (En saga yasli)
    char wStr[16];
    snprintf(wStr, sizeof(wStr), "W:%d", curWave + 1);
    int wLen = strlen(wStr) * 6;
    canvas.setTextColor(RGB(100, 255, 100));
    canvas.setCursor(158 - wLen, SCR_H - HUD_H + 2);
    canvas.print(wStr);

    // FPS (Dalganin soluna hizali)
    char fpsStr[16];
    snprintf(fpsStr, sizeof(fpsStr), "FPS:%d", currentFPS);
    int fpsW = strlen(fpsStr) * 6;
    int fpsX = 158 - wLen - fpsW - 4; // Dalga ile arasina 4 px bosluk
    canvas.setTextColor(RGB(100, 255, 100));
    canvas.setCursor(fpsX, SCR_H - HUD_H + 2);
    canvas.print(fpsStr);

    // Triple/Shield gostergesi (Can barinin bitimine sabitlendi)
    // FPS60: float karsilastirma
    if (ship.tripleTimer > 0.0f) {
        canvas.fillRect(88, SCR_H - HUD_H + 4, 3, 5, COL_PWR_TRIPLE);
    }
    if (ship.shieldTimer > 0.0f) {
        canvas.fillRect(93, SCR_H - HUD_H + 4, 3, 5, COL_PWR_SHIELD);
    }
}

// ============================================================
//  BASLIK EKRANI
// ============================================================
// drawTitle — Baslik (menu) ekranini cizer
//  Logo (golge efektli), demo gemi, kontrol bilgileri ve rekor gosterilir
void drawTitle() {
    canvas.fillSprite(COL_BG);
    drawStars();

    canvas.setTextSize(2);
    const char* t1 = "GALACTIC";
    const char* t2 = "STRIKE";
    int w1 = strlen(t1) * 12;
    int w2 = strlen(t2) * 12;

    // Gölge
    canvas.setTextColor(RGB(20, 60, 120));
    canvas.setCursor((SCR_W - w1) / 2 + 1, 16);
    canvas.print(t1);

    // Metin
    canvas.setTextColor(RGB(80, 200, 255));
    canvas.setCursor((SCR_W - w1) / 2, 15);
    canvas.print(t1);

    canvas.setTextColor(RGB(255, 200, 60));
    canvas.setCursor((SCR_W - w2) / 2, 38);
    canvas.print(t2);

    // Demo gemi (Ortalanmış)
    int dx = SCR_W / 2, dy = 64;
    canvas.fillTriangle(dx, dy - 6, dx - 5, dy + 5, dx + 5, dy + 5, COL_SHIP_A);
    canvas.drawTriangle(dx, dy - 6, dx - 5, dy + 5, dx + 5, dy + 5, COL_SHIP_B);
    // FPS60: animFrame sanal 30fps kare sayaci
    canvas.fillRect(dx - 1, dy + 6, 3, 2 + (animFrame % 2), COL_SHIP_ENG);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(10, 95);
    canvas.print("[A] Basla");

    canvas.setTextColor(0xBDF7); // Açık mavi (Premium Standart)
    canvas.setCursor(90, 95);
    canvas.print("[B] OS Menu");

    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(10, 110);
    canvas.print("[JOY] Hareket");

    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(92, 110);
    canvas.printf("Rekor: %d", highScore);

    // Dev tools: ekran goruntusu yakala
    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}

// ============================================================
//  GAME OVER EKRANI
// ============================================================
// drawGameOver — Oyun bitti ekranini cizer
//  Premium panel icinde: skor, dalga, rekor ve "Yeni Rekor" mesaji
void drawGameOver() {
    canvas.fillSprite(COL_BG);
    drawStars();

    // Premium Panel
    canvas.fillRoundRect(15, 12, 130, 108, 5, 0x2104);
    canvas.drawRoundRect(15, 12, 130, 108, 5, TFT_RED);
    canvas.drawRoundRect(16, 13, 128, 106, 4, 0x8000);

    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    const char* title = "OYUN BITTI";
    int titleW = strlen(title) * 12;
    canvas.setCursor((SCR_W - titleW) / 2, 20);
    canvas.print(title);

    // Skor / Dalga / Rekor tablosu (Sağa dayalı standart)
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 44);
    canvas.print("Skor:  ");
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(80, 44);
    canvas.print(ship.score);

    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 58);
    canvas.print("Dalga: ");
    canvas.setTextColor(TFT_CYAN);
    canvas.setCursor(80, 58);
    canvas.print(curWave + 1);

    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 72);
    canvas.print("Rekor: ");
    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(80, 72);
    canvas.print(highScore);

    // Yeni Rekor?
    if (ship.score >= highScore && ship.score > 0) {
        canvas.setTextColor(TFT_MAGENTA);
        const char* nrStr = "YENI REKOR!";
        int nrW = strlen(nrStr) * 6;
        canvas.setCursor((SCR_W - nrW) / 2, 86);
        canvas.print(nrStr);
    }

    // Premium Menu Alt Kismi
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 98);
    canvas.print("[A] Tekrar Oyna");

    canvas.setTextColor(0xBDF7);
    canvas.setCursor(30, 108);
    canvas.print("[B] OS Menu");

    // Dev tools: ekran goruntusu yakala
    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}

// ============================================================
//  SETUP
// ============================================================
// setup — Donanim ve oyun baslangic ayarlarini yapar (bir kez calisir)
void setup() {
    // --- Ses: Buzzer pini ---
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);

    // --- OLED (I2C) uyku moduna al (0x3C adresine 0xAE komutu) ---
    Wire.begin(8, 9);                       // I2C: SDA=8, SCL=9
    Wire.beginTransmission(0x3C);           // OLED I2C adresi
    Wire.write(0x00);                        // Komut registeri
    Wire.write(0xAE);                        // Display OFF komutu
    Wire.endTransmission();

    // --- OTA: Sonraki guncelleme bolumunu boot bolumu yap (OS'a donus icin) ---
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    // --- Buton pinleri (ic pull-up, basili=LOW) ---
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP);
    pinMode(BTN_D, INPUT_PULLUP);
    pinMode(JOY_SW, INPUT_PULLUP);

    // --- Kalici ayarlari oku (NVS) ---
    // Ses ayari "os" namespace'inden, rekor "gstrike" namespace'inden
    { Preferences prefs; prefs.begin("os", true); soundEnabled = prefs.getBool("sound_en", true); prefs.end(); }
    { Preferences prefs; prefs.begin("gstrike", true); highScore = prefs.getInt("hi", 0); prefs.end(); }

    // --- SPI baslat (TFT haberlesmesi) ---
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    // --- Dev tools baslat (ekran goruntusu yakalama vb.) ---
    initDevTools(tft);

    // --- TFT ekran baslat + yatay (landscape) rotasyon ---
    tft.init();
    tft.setRotation(1);

    // TFT donanimini RGB moduna gecir (renklerin mavi gozukmesini engeller)
    tft.startWrite();
    tft.writecommand(0x36);  // MADCTL
    tft.writedata(0xA0);     // MY|MV, BGR bit kapali (RGB modu)
    tft.endWrite();

    // --- Ekran goruntusu modu (RGB byte swap) + ekran temizle ---
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    // --- Canvas (cift tampon sprite) olustur - flicker-free cizim ---
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // --- Joystick kalibrasyon: baslangic merkez degerlerini al ---
    joyCenterX = analogRead(JOY_X);
    joyCenterY = analogRead(JOY_Y);

    // --- Rastgele sayi tohumu (joystick + micros karisimi) ---
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    // --- Oyun baslangic durumu ---
    initStars();
    state = ST_TITLE;
    lastFrameMs = millis();      // Delta-Time: ilk kare referans zamani
    fpsStartTime = millis();     // FPS olcum baslangici
}

// ============================================================
//  LOOP
// ============================================================
// loop — Ana oyun dongusu (her karede calisir)
//  Delta-Time hesaplar, girisleri okur, duruma gore guncelle+cizim yapar
void loop() {
    uint32_t now = millis();

    // ---- FPS60: Delta-Time Frame Hesaplama ----
    // Delta-Time: onceki kareden beri gecen sure (saniye). Hareketler bu
    //  degerle carpilir, boylece FPS ne olursa olsun hiz sabit kalir.
    float dt = (now - lastFrameMs) / 1000.0f;
    lastFrameMs = now;
    if (dt <= 0.0f) return;     // FPS60: Sifir dt korumasi (ayni ms'de cift calistirmayi engeller)
    if (dt > 0.05f) dt = 0.05f; // FPS60: Lag spike korumasi (max 50ms = 20 FPS alt sinir)

    // FPS60: Zaman-tabanli animasyon sayaci (30fps sanal kare)
    // animTime suresini biriktirir, animFrame sanal 30fps kare numarasi uretir
    animTime += dt;
    animFrame = (int)(animTime * 30.0f);

    // ---- FPS Hesaplama (her saniye guncellenir) ----
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // ---- JOY_SW: Pause toggle (yalnizca basma aninda) ----
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {           // Yuksaktan dusuka gecis = basildi
        if (state == ST_PLAY) {
            state = ST_PAUSE;
            playSound(400, 50);
        }
    }
    prevJoySw = currJoySw;

    // ---- BTN_B: OS menuye don (baslik/oyun bitti ekranlarinda) ----
    if (!digitalRead(BTN_B) && (state == ST_TITLE || state == ST_GAMEOVER)) {
        returnToOS();
    }

    // ---- BTN_A: Kenar dedeksiyonu (basma anini yakala) ----
    bool btnA = !digitalRead(BTN_A);
    static bool prevA = false;
    bool btnA_pressed = (btnA && !prevA);    // Sadece basildigi ilk karede true
    prevA = btnA;

    // ---- Joystick okuma + oluz bolge (dead zone) ----
    // Merkez sapmasini -1..+1 araligina normalize et (2048 yarim genlik)
    float jx = (analogRead(JOY_X) - joyCenterX) / 2048.0f;
    float jy = (analogRead(JOY_Y) - joyCenterY) / 2048.0f;
    // Kucuk sapmalari sifir yap (dead zone: ±%15) - drift engeller
    if (fabsf(jx) < 0.15f) jx = 0;
    if (fabsf(jy) < 0.15f) jy = 0;

    // Yildizlar her zaman hareket etsin (tum durumlarda arkaplan akar)
    updateStars(dt);  // FPS60: dt parametresi eklendi

    // ---- Durum Makinesi (State Machine) ----
    switch (state) {

    case ST_TITLE:
        // Baslik ekrani ciz; A'ya basilinca yeni oyun baslat
        drawTitle();
        if (btnA_pressed) {
            resetGame();
            state = ST_PLAY;
            playSound(660, 80);
        }
        break;

    case ST_PLAY:
        // --- Gemi hareketi (kosegen hiz normalize) ---
        // Capraz hareketi hiz normalize: vektor buyuklugu 1'i gecerse olcekle
        {
            float mvx = jx, mvy = jy;
            float m = mvx * mvx + mvy * mvy;
            if (m > 1.0f) { float inv = 1.0f / sqrtf(m); mvx *= inv; mvy *= inv; }
            // FPS60: 2.5f*30=75.0f px/s, dt ile carp
            // Gemi hizi: 75 px/s (delta-time ile cerceve bagimsiz)
            ship.x += mvx * 75.0f * dt;
            ship.y += mvy * 75.0f * dt;
        }
        // Gemiyi ekran sinirlari icinde tut (HUD ustune cikmasin)
        ship.x = constrain(ship.x, 6.0f, (float)SCR_W - 6);
        ship.y = constrain(ship.y, 10.0f, (float)SCR_H - HUD_H - 8);

        // Ates: A basiliyken mermi at (cooldown firePlayer icinde)
        if (btnA) firePlayer();
        if (ship.shootCD > 0.0f) ship.shootCD -= dt;       // FPS60: dt ile azalt

        // Timerlar: dokunulmazlik/triple/kalkan suresini azalt
        if (ship.invincTimer > 0.0f) ship.invincTimer -= dt; // FPS60: dt ile azalt
        if (ship.tripleTimer > 0.0f) ship.tripleTimer -= dt; // FPS60: dt ile azalt
        if (ship.shieldTimer > 0.0f) ship.shieldTimer -= dt; // FPS60: dt ile azalt

        // Guncellemeler: tum oyun mantigini dt ile ilerlet
        updateBullets(dt);          // FPS60: dt parametresi
        updateEnemies(dt);          // FPS60: dt parametresi
        updateWaveSpawning(dt);     // FPS60: dt parametresi
        updateCollisions(dt);       // FPS60: dt parametresi
        updateExplosions(dt);       // FPS60: dt parametresi

        // --- Cizim (arkadan one sirayla) ---
        canvas.fillSprite(COL_BG);
        drawStars();
        drawBullets();
        drawEnemies();
        drawShip();
        drawExplosions();
        drawPowerUps();
        drawHUD();
        // Dev tools: ekran goruntusu yakala
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);    // Hazir canvas'i TFT'ye aktar (tek kerede)
        break;

    case ST_GAMEOVER:
        // Oyun bitti ekrani ciz; A'ya basilinca basliga don
        drawGameOver();
        if (btnA_pressed) {
            state = ST_TITLE;
            playSound(440, 60);
        }
        break;

    case ST_PAUSE:
        // Cizime devam et ama mantigi dondur (oyun duraklatildi)
        canvas.fillSprite(COL_BG);
        drawStars();
        drawBullets();
        drawEnemies();
        drawShip();
        drawExplosions();
        drawPowerUps();
        drawHUD();

        // Premium PAUSE Menüsü (yesil panel)
        canvas.fillRoundRect(25, 35, 110, 60, 5, tft.color565(0, 30, 0));
        canvas.drawRoundRect(25, 35, 110, 60, 5, TFT_GREEN);

        canvas.setTextSize(2);
        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(50, 42);
        canvas.print("PAUSE");

        canvas.setTextSize(1);
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(35, 65);
        canvas.print("[A] Devam Et");

        canvas.setTextColor(0xBDF7);
        canvas.setCursor(35, 78);
        canvas.print("[B] OS Menu");

        // Dev tools: ekran goruntusu yakala
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);

        // A: oyuna devam et (kisa bekleme + lastFrameMs sifirla dt sıcmasin)
        if (btnA_pressed) {
            playSound(800, 50);
            delay(200);
            state = ST_PLAY;
            lastFrameMs = millis();   // Pause suresini dt'ye sayma
        }
        // B: OS menuye don
        if (!digitalRead(BTN_B)) {
            playSound(400, 50);
            delay(200);
            returnToOS();
        }
        break;
    }
}
