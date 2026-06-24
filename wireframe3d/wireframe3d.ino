// ============================================================
//  E-OS — WIREFRAME 3D
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanli cift tamponlama (Flicker-Free)
//  3D wireframe render + uzay savasi
//
//  Kontroller:
//    JOY_X/Y -> Kamera / Nisan alma
//    BTN_A   -> Ates
//    BTN_B   -> OS Menu
//    Buzzer  -> Ses efektleri
// ============================================================

// --- Donanim kutuphanesi: TFT ekrani icin grafik surucu ---
#include <TFT_eSPI.h>
// --- SPI haberlesmesi (TFT_eSPI icin gerekli) ---
#include <SPI.h>
// --- I2C haberlesmesi (OLED radar icin) ---
#include <Wire.h>
// --- OLED (SH1106) grafik kutuphanesi — radar ekrani ---
#include <U8g2lib.h>
// OLED nesnesi: 128x64 SH1106, donanimsal I2C, donus yok, reset pini yok
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
// --- ESP32 OTA (Over-The-Air) islemleri — OS partition yoneticisi ---
#include <esp_ota_ops.h>
// --- NVFlash (Preferences) — yuksek skor kalici kaydi ---
#include <Preferences.h>
// --- Donanim konfigurasyonu: pin tanimlari (BTN_*, JOY_*, BUZZER, SPI_*) ---
//     Bu dosya oyun klasoru disinda, paylasilan ortak config dosyasi
#include "../hardware_config.h"
// --- Dev tools: screenshot, FPS/RAM HUD, SD kart — paylasilan arac kutuphanesi ---
#include "../dev_tools.h"

// ============ Ekran ============
#define SCR_W    160   // Ekran genisligi (piksel) — yatay (landscape) mod
#define SCR_H    128   // Ekran yuksekligi (piksel)
#define HUD_H    12    // Alt HUD seridinin yuksekligi (skor/can/dalga)

// ============ Renkler ============
// TFT_BGR_ORDER tanimli oldugu icin R ve B'yi yer degistiriyoruz
// (Doom oyunundaki RGB_FIX mantigi ile ayni)
// RGB makro: 8-bit R/G/B degerlerini 16-bit RGB565 formatina cevirir
#define RGB(r,g,b) ((uint16_t)(((b)&0xF8)<<8)|(((g)&0xFC)<<3)|((r)>>3))

#define COL_BG         RGB(2, 2, 8)          // Arka plan (koyu uzay mavisi)
#define COL_STAR       RGB(100, 100, 140)    // Yildiz rengi (soluk mavi-beyaz)
#define COL_WIRE_CYAN  RGB(0, 255, 255)      // Kup dusmanlari (cyan)
#define COL_WIRE_GREEN RGB(0, 255, 80)       // Piramit dusmanlari (yesil)
#define COL_WIRE_RED   RGB(255, 60, 60)      // Elmas dusmanlari / HP bar (kirmizi)
#define COL_WIRE_GOLD  RGB(255, 200, 40)     // Altin vurgu (rekor, elmas)
#define COL_WIRE_PINK  RGB(255, 80, 200)     // Pembe vurgu (yedek)
#define COL_BULLET     RGB(255, 255, 100)    // Mermi (sari)
#define COL_HIT        RGB(255, 100, 100)    // Vurulma efekti (acik kirmizi)
#define COL_CROSS      RGB(0, 255, 0)        // Nisan arti isareti (yesil)
#define COL_HUD_BG     RGB(0, 0, 20)         // HUD arka plani (koyu lacivert)
#define COL_HUD_TXT    RGB(180, 180, 200)    // HUD metin rengi (gri)
#define COL_BOOM       RGB(255, 180, 40)     // Patlama cekirdegi (turuncu-sari)

// ============ Sabitleri ============
#define TARGET_FPS     60  // FPS60: 25'ten 60'a yukseltildi
#define FRAME_MS       (1000 / TARGET_FPS)  // FPS60: artık frame kilidi kullanilmiyor (delta-time)

// Nesne havuz sinirlari (sabit dizi boyutlari — bellek tertemiz)
#define MAX_STARS      30        // Arka plan yildiz sayisi
#define MAX_OBJECTS    8         // Ayni anda max dusman sayisi
#define MAX_BULLETS    6         // Ayni anda ekranda max mermi
#define MAX_EXPLOSIONS 6         // Ayni anda max patlama

// 3D projeksiyon sabitleri
#define FOCAL          80.0f   // Perspektif focal length (ne kadar buyuk = dar lens)
#define NEAR_CLIP      1.0f    // Kamera cok yakin (arkadan) cizimi engelle

// ============ 3D Matematik ============
// Vec3 — 3 boyutlu vektor (x, y, z) — tum 3D hesaplamlarin temeli
struct Vec3 {
    float x, y, z;  // Kartezyen koordinatlar (uzay birimleri)
};

// v3add — Iki vektorun toplamini dondurur (a + b)
Vec3 v3add(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
// v3sub — Iki vektorun farkini dondurur (a - b)
Vec3 v3sub(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
// v3mul — Vektoru skaler ile carp (a * s)
Vec3 v3mul(Vec3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
// v3dot — Skaler carpim (iki vektorun aci/uzunluk iliskisi, dist2 icin)
float v3dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
// v3len — Vektorun uzunlugu (norm) — sqrt(x^2+y^2+z^2)
float v3len(Vec3 a) { return sqrtf(a.x*a.x + a.y*a.y + a.z*a.z); }
// v3norm — Birim vektor (yön) dondurur; sifira yakin ise +Z varsay
Vec3 v3norm(Vec3 a) { float l = v3len(a); if(l<0.001f) return {0,0,1}; return v3mul(a, 1.0f/l); }

// Y ekseni etrafinda donus
// angle: radyan cinsinden donus acisi (yaw — saga/sola bakis)
Vec3 rotateY(Vec3 v, float angle) {
    float c = cosf(angle), s = sinf(angle);
    return { v.x*c + v.z*s, v.y, -v.x*s + v.z*c };  // Y sabit, X/Z doner
}

// X ekseni etrafinda donus
// angle: radyan cinsinden donus acisi (pitch — yukari/asagi bakis)
Vec3 rotateX(Vec3 v, float angle) {
    float c = cosf(angle), s = sinf(angle);
    return { v.x, v.y*c - v.z*s, v.y*s + v.z*c };  // X sabit, Y/Z doner
}

// project — 3D dunya noktasini 2D ekran koordinatlarina donustur (perspektif)
// p: kamera uzayindaki nokta (rotateX/Y uygulanmis)
// sx, sy: cikis ekran koordinatlari (referans)
// return: true = goruntunun cizilebilir (near clip ve ekran sinirlari icinde)
bool project(Vec3 p, int &sx, int &sy) {
    if (p.z < NEAR_CLIP) return false;  // Kamera arkasinda / cok yakin: atla
    // Perspektif bolme: focal length / derinlik -> ekran olcegi
    sx = SCR_W/2 + (int)(p.x * FOCAL / p.z);                // X ekran + merkez kaydir
    sy = (SCR_H - HUD_H)/2 + (int)(p.y * FOCAL / p.z);      // Y ekran (HUD disinda) + merkez
    // Ekran disinda bir miktar tolerans birak (-50..+50) kenar cizgileri icin
    return (sx >= -50 && sx < SCR_W+50 && sy >= -50 && sy < SCR_H+50);
}

// ============ Mesh Tanimlari ============
// Mesh = kose (vertex) listesi + kenar (edge) listesi (cifti)
// Kup (8 kose, 12 kenar)
#define CUBE_VERTS 8       // Kup kose sayisi
#define CUBE_EDGES 12      // Kup kenar sayisi
const Vec3 cubeVerts[CUBE_VERTS] = {
    {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},   // On yuzun 4 kosesi
    {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}        // Arka yuzun 4 kosesi
};
const uint8_t cubeEdges[CUBE_EDGES][2] = {
    {0,1},{1,2},{2,3},{3,0}, // On yuz
    {4,5},{5,6},{6,7},{7,4}, // Arka yuz
    {0,4},{1,5},{2,6},{3,7}  // Baglantilar
};

// Piramit (5 kose, 8 kenar)
#define PYR_VERTS 5        // Piramit kose sayisi
#define PYR_EDGES 8        // Piramit kenar sayisi
const Vec3 pyrVerts[PYR_VERTS] = {
    {0,-1.5f,0},             // Tepe
    {-1,0.5f,-1},{1,0.5f,-1},{1,0.5f,1},{-1,0.5f,1}  // Taban
};
const uint8_t pyrEdges[PYR_EDGES][2] = {
    {0,1},{0,2},{0,3},{0,4}, // Tepe baglantilari
    {1,2},{2,3},{3,4},{4,1}  // Taban
};

// Elmas (6 kose, 12 kenar)
#define DIAM_VERTS 6       // Elmas kose sayisi
#define DIAM_EDGES 12      // Elmas kenar sayisi
const Vec3 diamVerts[DIAM_VERTS] = {
    {0,-1.5f,0},{0,1.5f,0},  // Ust ve alt uc
    {-1,0,-1},{1,0,-1},{1,0,1},{-1,0,1}  // Orta
};
const uint8_t diamEdges[DIAM_EDGES][2] = {
    {0,2},{0,3},{0,4},{0,5}, // Ust baglantilari
    {1,2},{1,3},{1,4},{1,5}, // Alt baglantilari
    {2,3},{3,4},{4,5},{5,2}  // Orta halka
};

// ============ Nesneler ============
TFT_eSPI tft = TFT_eSPI();          // TFT ekran surucu nesnesi
TFT_eSprite canvas = TFT_eSprite(&tft);  // Cift tamponlama (flicker-free) sprite'i

// ============ Ses ============
bool soundEnabled = true;           // Ses acik/kapali (Preferences'tan okunur)
// playSound — buzzer'a ton uretir (sadece ses aciksa)
// freq: frekans (Hz), dur: sure (ms)
void playSound(uint16_t freq, uint32_t dur) {
    if (soundEnabled) tone(BUZZER, freq, dur);
}

// returnToOS — oyunu OS menuye dondur (ESP restart ile)
void returnToOS() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(20, 60);
    tft.print("Ana Menuye Donuluyor...");
    delay(500);         // Kullanici mesaji gorsun diye kisa bekle
    ESP.restart();      // Cihazi yeniden baslat -> OS boot partition
}

// ============ Durum ============
// State — oyun durum makinesi (state machine)
enum State { ST_TITLE, ST_PLAY, ST_GAMEOVER, ST_PAUSE };
State state = ST_TITLE;   // Su anki durum (baslangicta baslik ekrani)

// ============ Yildizlar ============
// Star — arka plan yildizi (ekran koordinatlarinda, parlaklik ile)
struct Star { int x, y; uint8_t bright; };
Star stars[MAX_STARS];    // Yildiz havuzu

// ============ 3D Nesne ============
// ObjType — dusman turu (her birinin mesh'i ve HP'si farkli)
enum ObjType { OBJ_CUBE, OBJ_PYRAMID, OBJ_DIAMOND };
// Object3D — uzaydaki bir dusman (3D cisim) — havuzdan alinir
struct Object3D {
    Vec3 pos;          // Dunya koordinatlarinda pozisyon
    Vec3 vel;           // Hareket yonu ve hizi
    float rotAngle;     // Kendi etrafinda donus
    float rotSpeed;     // FPS60: radyan/saniye cinsinden
    float scale;        // Cisim olcegi (boyut)
    ObjType type;       // Kup/Piramit/Elmas
    int hp;             // Kalan can (vurus sayisi)
    float hitTimer;     // FPS60: saniye cinsinden (frame sayaci yerine)
    bool active;        // Havuz slotu aktif mi (kullaniliyor mu)
};
Object3D objects[MAX_OBJECTS];   // Dusman havuzu

// ============ Mermiler ============
// Bullet3D — oyuncunun attigi mermi (kamera yonunde ilerler)
struct Bullet3D {
    Vec3 pos;        // Dunya pozisyonu
    Vec3 vel;          // Hiz vektoru (kamera yonu * hiz)
    float life;         // FPS60: saniye cinsinden kalan omur (frame sayaci yerine)
    bool active;        // Havuz slotu aktif mi
};
Bullet3D bullets[MAX_BULLETS];   // Mermi havuzu

// ============ Patlamalar ============
// Boom — patlama efekti (genisleyen halka, rengi zamanla degisir)
struct Boom { Vec3 pos; float frame; bool active; };  // FPS60: frame -> float saniye
Boom booms[MAX_EXPLOSIONS];      // Patlama havuzu

// ============ Kamera ============
// Kamera acilari (oyuncunun bakis yonu) — radyan
float camYaw = 0;    // Sag/sol donus
float camPitch = 0;  // Yukari/asagi donus

// ============ Oyuncu ============
int score = 0;            // Toplam skor
int hp = 5;               // Oyuncunun kalan cani
int kills = 0;            // Vurulan dusman sayisi
int wave = 0;             // Su anki dalga (zorluk seviyesi)
float spawnTimer = 0;  // FPS60: saniye cinsinden (frame sayaci yerine)
float shootCD = 0;     // FPS60: saniye cinsinden (frame sayaci yerine)

// ============ Genel ============
int joyCenterX, joyCenterY;        // Joystick kalibrasyon merkezi (0 konumu)
uint32_t lastFrameMs = 0;          // Delta-time hesabi icin onceki frame zamani
uint32_t animTick = 0;             // Animasyon sayaci (sweep, stripe vb.)
uint32_t stateTimer = 0;           // Durum gecisleri icin zamanlayici
int highScore = 0;                 // Kalici yuksek skor (Preferences)

// ============================================================
//  BASLANGIC FONKSIYONLARI
// ============================================================

// initStars — yildiz havuzunu rastgele ekran koordinatlarina yerlestir
// Parlaklik 40-160 arasi (soluk yildiz efekti)
void initStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(0, SCR_W);              // Rastgele X
        stars[i].y = random(0, SCR_H - HUD_H);      // Rastgele Y (HUD disinda)
        stars[i].bright = random(40, 160);          // Parlaklik (soluk)
    }
}

// clearObjects — tum dusmanlari devre disi birak (havuzu temizle)
void clearObjects() {
    for (int i = 0; i < MAX_OBJECTS; i++) objects[i].active = false;
}

// clearBullets — tum mermileri devre disi birak
void clearBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
}

// clearBooms — tum patlamalari devre disi birak
void clearBooms() {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) booms[i].active = false;
}

// spawnObject — havuzdan bos slot bul ve yeni dusman olustur
// Dusman tipi, boyutu, pozisyonu ve hizi rastgele (dalga zorluguna gore)
void spawnObject() {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (objects[i].active) continue;            // Dolu slotu atla
        Object3D& o = objects[i];
        o.active = true;
        o.type = (ObjType)random(0, 3);             // Rastgele tip (kup/piramit/elmas)
        o.scale = 1.0f + random(0, 100) / 100.0f;   // 1.0 - 2.0 arasi olcek
        o.rotAngle = random(0, 628) / 100.0f;       // 0 - 2*PI rastgele baslangic acisi
        o.rotSpeed = 0.5f + random(0, 125) / 100.0f; // FPS60: rad/s (eskiden 0.02+random/1000 @25fps)
        o.hitTimer = 0;                             // Vurulma efekti sifir

        // Rastgele pozisyon (360 derece etrafta, uzayda)
        // dist: oyuncudan uzaklik (30-50 birim arasi)
        float dist = 30.0f + random(0, 200) / 10.0f;
        float hAngle = random(0, 628) / 100.0f; // Tam 360 derece
        // Dikey aci ±30° (yukari/asagi sapma sinirli)
        float vAngle = (random(-30, 30)) * M_PI / 180.0f; // Hafif yukari/asagi
        o.pos.x = sinf(hAngle) * cosf(vAngle) * dist;   // Kure uzerinde X
        o.pos.y = sinf(vAngle) * dist * 0.5f;           // Y (dikey, daraltildi)
        o.pos.z = cosf(hAngle) * cosf(vAngle) * dist;   // Z (ileri)

        // Hareket yonu: Oyuncuya dogru ama rastgele sapma ile
        // Bazi dusmanlar tam uzerine gelir, bazilari yandan gecer
        Vec3 target = {0, 0, 0}; // Oyuncu pozisyonu
        target.x += random(-8, 8); // Sag/sol sapma
        target.y += random(-4, 4); // Yukari/asagi sapma
        target.z += random(-8, 8); // Ileri/geri sapma
        
        Vec3 dir = v3norm(v3sub(target, o.pos));    // Hedefe birim yon vektoru
        // Hiz: 3.75 u/s + rastgele + dalga basina 0.5 (zorluk artisi)
        float spd = 3.75f + random(0, 100) / 20.0f + (wave * 0.5f); // FPS60: birim/saniye (eskiden 0.15+random/500 @25fps)
        o.vel = v3mul(dir, spd);

        // Ture gore HP (elmas saglam, piramit kirilgan)
        switch (o.type) {
            case OBJ_CUBE:    o.hp = 2; break;
            case OBJ_PYRAMID: o.hp = 1; break;
            case OBJ_DIAMOND: o.hp = 3; break;
        }
        return;     // Tek slot doldur ve cik
    }
}

// spawnBoom — havuzdan bos slot bul ve patlama baslat
// pos: patlamanin dunya koordinati
void spawnBoom(Vec3 pos) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!booms[i].active) {
            booms[i] = {pos, 0, true}; // FPS60: 0 float olarak dogru calisir
            return;
        }
    }
}

// resetGame — yeni oyun baslat (tum durum degiskenlerini sifirla)
void resetGame() {
    score = 0;
    hp = 5;                 // Baslangic cani
    kills = 0;
    wave = 0;
    spawnTimer = 2.4f; // FPS60: 60 frame / 25fps = 2.4 saniye
    shootCD = 0;
    camYaw = 0;
    camPitch = 0;
    clearObjects();
    clearBullets();
    clearBooms();
}

// ============================================================
//  3D MESH CİZİMİ
// ============================================================
// drawMesh — bir 3D mesh'i (kup/piramit/elmas) wireframe olarak canvas'a ciz
// verts: kose listesi, nVerts: kose sayisi, edges: kenar ciftleri, nEdges: kenar sayisi
// worldPos: dunya koordinati, rot: kendi donus acisi, scale: boyut, color: cizgi rengi
void drawMesh(const Vec3* verts, int nVerts, const uint8_t edges[][2], int nEdges,
              Vec3 worldPos, float rot, float scale, uint16_t color) {
    // Donusturulmus koseler
    int sx[12], sy[12]; // Max 12 kose
    bool visible[12];   // Kose ekranda mi (kenar cizim kontrolu icin)

    // 1. Adim: her koseyi dunya -> kamera -> ekran uzayina donustur
    for (int i = 0; i < nVerts && i < 12; i++) {
        Vec3 v = v3mul(verts[i], scale);  // Olcekle buyut
        v = rotateY(v, rot);              // Kendi etrafinda dondur
        v = v3add(v, worldPos);           // Dunya konumuna tasi

        // Kamera donusumu (kamera ters yonde doner ki cisim kameraya gore dursun)
        v = rotateY(v, -camYaw);
        v = rotateX(v, -camPitch);

        visible[i] = project(v, sx[i], sy[i]);  // 3D -> 2D projeksiyon
    }

    // 2. Adim: gorunur koseleri birlestiren kenarlari ciz
    for (int i = 0; i < nEdges; i++) {
        int a = edges[i][0], b = edges[i][1];
        if (visible[a] && visible[b]) {
            canvas.drawLine(sx[a], sy[a], sx[b], sy[b], color);
        }
    }
}

// ============================================================
//  GUNCELLEME  (FPS60: tum fonksiyonlar dt parametresi alir)
//  dt = bir onceki frame'den bu yana gecen saniye (delta-time)
//  Bu sayede hareket FPS'ten bagimsiz (60FPS'te de 30FPS'te de ayni hiz)
// ============================================================
// updateObjects — dusmanlari hareket ettir, carpisma/hasar kontrolu yap
// dt: delta-time (saniye)
void updateObjects(float dt) {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) continue;
        Object3D& o = objects[i];
        o.rotAngle += o.rotSpeed * dt; // FPS60: delta-time ile carp
        if (o.hitTimer > 0) { o.hitTimer -= dt; if (o.hitTimer < 0) o.hitTimer = 0; } // FPS60: dt ile azalt

        // Dusmanlar kendi yollerinde ilerlesin (bazi duz gelir, bazi yandan gecer)
        o.pos = v3add(o.pos, v3mul(o.vel, dt)); // FPS60: delta-time ile carp

        // Hasar kontrolu (Carpisma) — oyuncuya cok yaklasirsa
        float dist = v3len(o.pos);              // Oyuncuya (orijin) mesafe
        if (dist < 3.0f) {                       // 3 birimden yakin = carpisma
            o.active = false;
            hp--;                                // Oyuncu can kaybi
            spawnBoom(o.pos);
            playSound(150, 200);
            if (hp <= 0) {
                stateTimer = millis();
                if (score > highScore) {        // Rekor kontrolu
                    highScore = score;
                    Preferences prefs;          // NVFlash'a kaydet
                    prefs.begin("wire3d", false);
                    prefs.putInt("hi", highScore);
                    prefs.end();
                }
                state = ST_GAMEOVER;            // Oyun bitti
            }
            continue;
        }

        // Cok uzaklastiysa sil (uzayin derinligine kacan dusman)
        if (v3len(o.pos) > 60.0f) {
            o.active = false;
        }
    }
}

// updateBullets — mermileri hareket ettir, dusman carpisma kontrolu yap
// dt: delta-time (saniye)
void updateBullets(float dt) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        Bullet3D& b = bullets[i];
        b.pos = v3add(b.pos, v3mul(b.vel, dt)); // FPS60: delta-time ile carp
        b.life -= dt; // FPS60: dt ile azalt
        if (b.life <= 0) { b.active = false; continue; }  // Omru doldu

        // Hedef carpisma kontrolu — her mermi her dusman ile test edilir
        for (int j = 0; j < MAX_OBJECTS; j++) {
            if (!objects[j].active) continue;
            Vec3 d = v3sub(b.pos, objects[j].pos);   // Mesafe vektoru
            float dist2 = v3dot(d, d);               // Kare mesafe (sqrt'tan kacma)
            float hitR = objects[j].scale * 2.0f;    // Carpisma yaricapi (olcekle)

            if (dist2 < hitR * hitR) {               // Carpisma!
                b.active = false;
                objects[j].hp--;
                objects[j].hitTimer = 0.24f; // FPS60: 6 frame / 25fps = 0.24 saniye

                if (objects[j].hp <= 0) {            // Dusman yok edildi
                    objects[j].active = false;
                    spawnBoom(objects[j].pos);
                    // Skor: ture gore (elmas en degerli)
                    int pts = objects[j].type == OBJ_DIAMOND ? 150 :
                              objects[j].type == OBJ_CUBE ? 100 : 75;
                    score += pts;
                    kills++;
                    playSound(600, 40);
                } else {                              // Hasar aldi ama hayatta
                    playSound(900, 15);
                }
                break;                               // Mermi bir hedefe carpti, cik
            }
        }
    }
}

// updateSpawning — dusman uretme zamanlayicisi + dalga ilerlemesi
// dt: delta-time (saniye)
void updateSpawning(float dt) {
    spawnTimer -= dt; // FPS60: dt ile azalt
    if (spawnTimer <= 0) {
        // Aktif dusman sayisi
        int activeCount = 0;
        for (int i = 0; i < MAX_OBJECTS; i++)
            if (objects[i].active) activeCount++;

        // Max aktif = 3 + dalga (zorluk artisi)
        int maxActive = min(3 + wave, MAX_OBJECTS);
        if (activeCount < maxActive) {
            spawnObject();
        }

        // Sonraki spawn araligi: dalga arttikca kisalir (min 0.6sn)
        spawnTimer = max(1.6f - wave * 0.12f, 0.6f); // FPS60: saniye cinsinden (eskiden max(40-w*3,15) frame @25fps)

        // Her 10 kill = yeni dalga (daha hizli/daha cok dusman)
        if (kills > 0 && kills % 10 == 0 && kills / 10 > wave) {
            wave = kills / 10;
            playSound(880, 80);     // Dalga yukselme sesi
        }
    }
}

// updateBooms — patlamalari ilerlet (genisleyen halka, sure doldugunda sil)
// dt: delta-time (saniye)
void updateBooms(float dt) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!booms[i].active) continue;
        booms[i].frame += dt; // FPS60: dt ile artir
        if (booms[i].frame > 0.6f) booms[i].active = false; // FPS60: 15 frame / 25fps = 0.6 saniye
    }
}

// ============================================================
//  UZAY TOZU (Yon hissi icin)
// ============================================================
// SpaceDust — kamera etrafinda ucusan toz parcaciklari (hiz/yon hissi verir)
struct SpaceDust {
    float x, y, z;  // Dunya koordinatlari (kamera cevresinde)
};
SpaceDust dust[20];      // 20 parcacik
bool dustInit = false;   // Ilk yerlesim yapildi mi

// initDust — toz parcaciklarini kamera cevresinde rastgele dagit
void initDust() {
    for (int i = 0; i < 20; i++) {
        dust[i].x = random(-40, 40);   // Yatay yayilim
        dust[i].y = random(-20, 20);   // Dikey yayilim
        dust[i].z = random(5, 50);     // Derinlik (yakin=buyuk)
    }
    dustInit = true;
}

// drawSpaceDust — tozu kameraya dogru ilerlet ve ciz (hiz hissi)
// dt: delta-time (saniye) — pause'ta bile toz hareket eder
void drawSpaceDust(float dt) { // FPS60: dt parametresi eklendi
    if (!dustInit) initDust();
    
    for (int i = 0; i < 20; i++) {
        // Toz parcaciklari yavasca bize dogru gelsin
        dust[i].z -= 7.5f * dt; // FPS60: birim/saniye (eskiden 0.3 @25fps)
        if (dust[i].z < 1.0f) {                 // Kamerayi gecti -> yeniden uzaga gonder
            dust[i].x = random(-40, 40);
            dust[i].y = random(-20, 20);
            dust[i].z = 40.0f + random(0, 10);
        }
        
        Vec3 p = {dust[i].x, dust[i].y, dust[i].z};
        p = rotateY(p, -camYaw);     // Kamera yonune gore dondur
        p = rotateX(p, -camPitch);
        
        int sx, sy;
        if (project(p, sx, sy) && sx >= 0 && sx < SCR_W && sy >= 0 && sy < SCR_H - HUD_H) {
            // Yakindakiler parlak, uzaktakiler soluk
            uint8_t b = (uint8_t)(120.0f / dust[i].z * 5.0f);  // Parlaklik = 1/derinlik
            b = constrain(b, 10, 80);                          // Sinirla
            canvas.drawPixel(sx, sy, RGB(b/2, b/2, b));        // Mavi ton (R/G kisa)
        }
    }
}

// drawStars — arka plan yildizlarini parallax ile ciz
// Yildizlar kamera acisina gore ters yonde kayar (derinlik illuzyonu)
void drawStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        // Parallax: yildizlar kamera ile zıt yonde hareket eder (Dogru perspektif)
        int sx = (stars[i].x - (int)(camYaw * 30)) % SCR_W;        // Yaw -> yatay kayma
        if (sx < 0) sx += SCR_W;                                   // Mod negatif fix
        int sy = (stars[i].y - (int)(camPitch * 20)) % (SCR_H - HUD_H);  // Pitch -> dikey kayma
        if (sy < 0) sy += (SCR_H - HUD_H);
        uint16_t c = RGB(stars[i].bright, stars[i].bright, stars[i].bright + 30);  // Mavi ton
        canvas.drawPixel(sx, sy, c);
    }
}

// drawObjects — tum aktif dusmanlari canvas'a ciz (renk + mesh + HP bar)
void drawObjects() {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) continue;
        Object3D& o = objects[i];

        uint16_t color;
        if (o.hitTimer > 0) {                  // Vurulma efekti -> kirmizi flash
            color = COL_HIT;
        } else {                               // Normal: ture gore renk
            switch (o.type) {
                case OBJ_CUBE:    color = COL_WIRE_CYAN; break;
                case OBJ_PYRAMID: color = COL_WIRE_GREEN; break;
                case OBJ_DIAMOND: color = COL_WIRE_GOLD; break;
            }
        }

        // Ture gore uygun mesh'i ciz
        switch (o.type) {
            case OBJ_CUBE:
                drawMesh(cubeVerts, CUBE_VERTS, cubeEdges, CUBE_EDGES,
                         o.pos, o.rotAngle, o.scale, color);
                break;
            case OBJ_PYRAMID:
                drawMesh(pyrVerts, PYR_VERTS, pyrEdges, PYR_EDGES,
                         o.pos, o.rotAngle, o.scale, color);
                break;
            case OBJ_DIAMOND:
                drawMesh(diamVerts, DIAM_VERTS, diamEdges, DIAM_EDGES,
                         o.pos, o.rotAngle, o.scale, color);
                break;
        }

        // HP gostergesi (dusman basinda)
        Vec3 hpPos = o.pos;
        hpPos.y -= o.scale * 2.0f;             // Cismin ustune koy
        Vec3 hpCam = rotateY(hpPos, -camYaw);
        hpCam = rotateX(hpCam, -camPitch);
        int hsx, hsy;
        if (project(hpCam, hsx, hsy)) {
            for (int h = 0; h < o.hp; h++) {   // Her can icin kirmizi kare
                canvas.fillRect(hsx - o.hp*2 + h*4, hsy, 3, 2, COL_WIRE_RED);
            }
        }
    }
}

// drawBullets — tum aktif mermileri ekrana ciz (kamera donusumu uygula)
void drawBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        Vec3 p = rotateY(bullets[i].pos, -camYaw);   // Kamera uzayina tasi
        p = rotateX(p, -camPitch);
        int sx, sy;
        if (project(p, sx, sy)) {
            canvas.fillCircle(sx, sy, 2, COL_BULLET);  // 2px sari daire
        }
    }
}

// drawBooms — patlamalari ciz (genisleyen halka, rengi zamanla degisir)
void drawBooms() {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!booms[i].active) continue;
        Vec3 p = rotateY(booms[i].pos, -camYaw);
        p = rotateX(p, -camPitch);
        int sx, sy;
        if (project(p, sx, sy)) {
            // Yari cap: frame (saniye) * 12.5 ile buyur (0-0.6sn -> 0-7.5px)
            int r = 3 + (int)(booms[i].frame * 12.5f); // FPS60: 0-0.6sn -> 0-15 frame esdegeri (15/0.6*0.5=12.5)
            // Renk: ilk 0.24sn parlak turuncu, sonra kirmizi, sonra koyu
            uint16_t c = (booms[i].frame < 0.24f) ? COL_BOOM :  // FPS60: 6 frame / 25fps = 0.24
                         (booms[i].frame < 0.4f) ? COL_WIRE_RED : RGB(100, 40, 20); // FPS60: 10 frame / 25fps = 0.4
            canvas.drawCircle(sx, sy, r, c);
            // Ilk 0.32sn ic cekirdek (turuncu) — daha parlak merkez
            if (booms[i].frame < 0.32f) canvas.drawCircle(sx, sy, r/2, COL_BOOM); // FPS60: 8 frame / 25fps = 0.32
        }
    }
}

// drawCrosshair — ekran ortasina nisan arti isareti ciz
void drawCrosshair() {
    int cx = SCR_W / 2;             // Ekran ortasi X
    int cy = (SCR_H - HUD_H) / 2;   // Ekran ortasi Y (HUD disinda)
    canvas.drawFastHLine(cx - 6, cy, 4, COL_CROSS);     // Sol kol
    canvas.drawFastHLine(cx + 3, cy, 4, COL_CROSS);     // Sag kol
    canvas.drawFastVLine(cx, cy - 6, 4, COL_CROSS);     // Ust kol
    canvas.drawFastVLine(cx, cy + 3, 4, COL_CROSS);     // Alt kol
    canvas.drawPixel(cx, cy, COL_CROSS);                // Merkez nokta
}

// drawHUD — alt seride skor/can/dalga/kill bilgisini ciz
void drawHUD() {
    int hy = SCR_H - HUD_H;                       // HUD yuksekligi (ust kenar)
    canvas.fillRect(0, hy, SCR_W, HUD_H, COL_HUD_BG);  // Arka plan seridi
    canvas.drawFastHLine(0, hy, SCR_W, RGB(40, 40, 80));  // Ust ayirici cizgi

    canvas.setTextSize(1);
    canvas.setTextColor(COL_HUD_TXT);

    // Skor
    canvas.setCursor(2, hy + 2);
    canvas.printf("SKR:%d", score);

    // Can (kirmizi kareler)
    for (int i = 0; i < hp; i++) {
        canvas.fillRect(60 + i * 8, hy + 4, 5, 5, COL_WIRE_RED);  // 8px aralikla
    }

    // Dalga
    canvas.setTextColor(RGB(100, 255, 100));      // Yesil
    canvas.setCursor(110, hy + 2);
    canvas.printf("W:%d", wave + 1);              // +1 (0-tabanli)

    // Kill sayisi
    canvas.setTextColor(RGB(180, 180, 180));      // Gri
    canvas.setCursor(136, hy + 2);
    canvas.printf("K:%d", kills);
}

// ============================================================
//  BASLIK EKRANI
// ============================================================
// drawRadar — OLED ikinci ekrana radar ciz (dusman konumlari + tarama cizgisi)
// Kamera hep ileri (yukari) bakiyormus gibi dusmanlarin yonunu gosterir
void drawRadar() {
    oled.clearBuffer();
    
    // Radar dairesi (tam ekran ortasinda)
    // cx/cy: OLED merkezi (128x64), radius: radar yaricapi
    const int cx = 64, cy = 32, radius = 30;
    oled.drawCircle(cx, cy, radius);
    oled.drawCircle(cx, cy, radius / 2); // Ic halka (mesafe referansi)
    oled.drawHLine(cx - radius, cy, radius * 2); // Yatay cizgi
    oled.drawVLine(cx, cy - radius, radius * 2); // Dikey cizgi
    
    // Merkez (Oyuncu - Hep yukari (ileri) bakan ucgen)
    oled.drawTriangle(cx, cy - 4, cx - 4, cy + 3, cx + 4, cy + 3);

    // Dusmanlar
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) continue;
        
        // Kameraya gore dondur (Kamera hep ileri(yukari) bakiyor kabul edilir)
        Vec3 rel = rotateY(objects[i].pos, -camYaw);
        float dist = v3len(objects[i].pos);
        
        // Mesafeyi radar yaricapina olcekle (50 birim = dis halka)
        float rScale = (dist / 50.0f) * radius;
        if (rScale > radius) rScale = radius;        // Cok uzak -> halka kenarinda
        
        Vec3 dirN = v3norm(rel);                     // Yon (birim vektor)
        int rx = cx + (int)(dirN.x * rScale);        // Yatay konum
        int ry = cy - (int)(dirN.z * rScale); // Z ileri = yukari
        
        rx = constrain(rx, cx - radius + 2, cx + radius - 2);   // Halka icinde tut
        ry = constrain(ry, cy - radius + 2, cy + radius - 2);

        // Mesafeye gore nokta boyutu (Yakin olanlar buyuk)
        int boxSize = 1;
        if (dist < 20.0f) boxSize = 5;               // Cok yakin = buyuk kare
        else if (dist < 35.0f) boxSize = 3;          // Orta mesafe = orta kare
        
        oled.drawBox(rx - boxSize/2, ry - boxSize/2, boxSize, boxSize);
    }
    
    // Radar Taramasi (Sweep) - Zaman bazli donen cizgi
    float sweepAngle = (millis() % 3000) / 3000.0f * PI * 2.0f;  // 3 sn'de tam tur
    int sx = cx + (int)(sinf(sweepAngle) * radius);
    int sy = cy - (int)(cosf(sweepAngle) * radius);
    oled.drawLine(cx, cy, sx, sy);
    
    oled.sendBuffer();   // OLED'e aktar
}

// ============================================================
// drawTitle — baslik ekranini ciz (demo kup + baslik + menu + rekor)
// ============================================================
void drawTitle() {
    canvas.fillSprite(COL_BG);      // Arka plani temizle
    drawStars();                    // Yildiz arka plan

    // 3D demo kupu — donen kup ile 3D his verir
    float demoRot = millis() * 0.00125f; // FPS60: saniye bazli (eskiden animTick*0.05 @25fps -> 1.25 rad/s)
    Vec3 demoPos = {0, -0.4f, 8.5f};     // Ekranda ortalanmis pozisyon
    drawMesh(cubeVerts, CUBE_VERTS, cubeEdges, CUBE_EDGES,
             demoPos, demoRot, 1.7f, COL_WIRE_CYAN);

    // Baslik (Ortalanmis) — once golge (koyu), sonra ana renk
    canvas.setTextSize(2);
    canvas.setTextColor(RGB(20, 80, 120));   // Golge rengi
    canvas.setCursor(39, 13);
    canvas.print("WIRE 3D");
    canvas.setTextColor(COL_WIRE_CYAN);      // Ana renk (cyan)
    canvas.setCursor(38, 12);                // Golge ustune 1px kaydir
    canvas.print("WIRE 3D");

    canvas.setTextSize(1);
    
    // Alt Menuler (Iki kolon, derli toplu)
    canvas.setTextColor(0xFFFF);             // Beyaz
    canvas.setCursor(5, 95);
    canvas.print("[A] Basla");

    canvas.setTextColor(RGB(180, 180, 180)); // Gri
    canvas.setCursor(87, 95);
    canvas.print("[B] OS Menu");

    canvas.setTextColor(COL_WIRE_GREEN);     // Yesil
    canvas.setCursor(5, 110);
    canvas.print("[JOY] Nisan");

    canvas.setTextColor(COL_WIRE_GOLD);      // Altin
    canvas.setCursor(87, 110);
    canvas.printf("Rekor: %d", highScore);
}

// drawGameOver — oyun bitti ekranini ciz (skor/kill/dalga/rekor + menu)
void drawGameOver() {
    canvas.fillSprite(COL_BG);
    drawStars();

    canvas.setTextSize(2);
    canvas.setTextColor(COL_WIRE_RED);       // Kirmizi baslik
    canvas.setCursor(20, 15);
    canvas.print("OYUN BITTI");

    // Istatistikler (her biri farkli renk)
    canvas.setTextSize(1);
    canvas.setTextColor(0xFFFF);             // Beyaz
    canvas.setCursor(35, 45);
    canvas.printf("Skor:      %d", score);
    canvas.setTextColor(COL_WIRE_CYAN);      // Cyan
    canvas.setCursor(35, 59);
    canvas.printf("Kill:      %d", kills);
    canvas.setTextColor(COL_WIRE_GREEN);     // Yesil
    canvas.setCursor(35, 73);
    canvas.printf("Dalga:     %d", wave + 1);
    canvas.setTextColor(COL_WIRE_GOLD);      // Altin
    canvas.setCursor(35, 87);
    canvas.printf("Rekor:     %d", highScore);

    // Alt menu secenekleri
    canvas.setTextColor(RGB(180, 180, 180));
    canvas.setCursor(35, 106);
    canvas.print("[A] Tekrar Oyna");
    canvas.setCursor(47, 118);
    canvas.print("[B] OS Menu");
}

// ============================================================
//  SETUP
//  Donanim baslatma, kalici veri yukleme, ekran/sprite hazirligi
// ============================================================
void setup() {
    // --- Seri port (debug) ---
    Serial.begin(115200);
    // --- Buzzer pini (ses efektleri) ---
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);

    // --- OLED (ikinci ekran) — I2C pin 8/9 ile baslat ---
    Wire.begin(8, 9);
    oled.begin();
    oled.clearBuffer();
    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(25, 35, "WIRE 3D");   // Acilis mesaji
    oled.sendBuffer();

    // --- OTA boot partition'i OS olarak ayarla (menuye donus icin) ---
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    // --- Buton pinleri (INPUT_PULLUP — basili = LOW) ---
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP);
    pinMode(BTN_D, INPUT_PULLUP);
    pinMode(JOY_SW, INPUT_PULLUP);

    // --- Kalici ayarlari yukle (Preferences/NVFlash) ---
    // Ses acik/kapali (OS ayari) + yuksek skor
    { Preferences prefs; prefs.begin("os", true); soundEnabled = prefs.getBool("sound_en", true); prefs.end(); }
    { Preferences prefs; prefs.begin("wire3d", true); highScore = prefs.getInt("hi", 0); prefs.end(); }

    // --- SPI baslat (TFT icin, custom pinler) ---
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    // --- Dev tools: SD kart + screenshot — TFT.init()'ten ONCE!
    initDevTools(tft); // SD kart + screenshot sistemi — TFT.init()'ten ONCE!

    // --- TFT ekran baslat ---
    tft.init();
    tft.setRotation(1);          // Landscape (yatay) mod
    tft.fillScreen(TFT_BLACK);

    // --- Cift tamponlama sprite'i (flicker-free) ---
    canvas.setColorDepth(16);    // 16-bit RGB565
    canvas.createSprite(SCR_W, SCR_H);  // Ekran boyutu sprite

    // --- Joystick kalibrasyonu (baslangictaki 0 konumu) ---
    joyCenterX = analogRead(JOY_X);
    joyCenterY = analogRead(JOY_Y);

    // --- Rastgele sayi tohumu (joystick + micros karisimi) ---
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    // --- Oyun ilk durumu ---
    initStars();                 // Yildizlari yerlestir
    state = ST_TITLE;            // Baslik ekranindan basla
    lastFrameMs = millis();      // Delta-time baslangic zamani
}

// ============================================================
//  LOOP  (FPS60: Delta-time sistemi, frame kilidi kaldirildi)
//  Delta-time: gecen sureye gore hareket -> FPS'ten bagimsiz tutarli hiz
// ============================================================
void loop() {
    // --- Delta-time (dt) hesaplama ---
    // dt = bir onceki frame'den bu yana gecen saniye
    uint32_t now = millis();
    float dt = (now - lastFrameMs) / 1000.0f; // FPS60: delta-time hesaplama
    if (dt > 0.05f) dt = 0.05f;               // FPS60: lag spike korumasi (max 50ms)
    lastFrameMs = now;
    animTick++;

    // Screenshot sonrasi TFT yeniden baslatma (SPI reset bozdu)
    devToolsTick();

    // ---- JOY_SW: Pause toggle (oyun/pause arasi gecis) ----
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {            // Dusen kenar (basildi aninda)
        if (state == ST_PLAY) {
            state = ST_PAUSE;
            playSound(400, 50);
        } else if (state == ST_PAUSE) {
            state = ST_PLAY;
            playSound(600, 50);
        }
    }
    prevJoySw = currJoySw;

    // BTN_B — baslik/oyunbitti ekraninda OS menuye don
    if (!digitalRead(BTN_B) && (state == ST_TITLE || state == ST_GAMEOVER)) {
        returnToOS();
    }

    // BTN_A — kenar algilama (basildi aninda 1 kez tetik)
    bool btnA = !digitalRead(BTN_A);
    static bool prevA = false;
    bool pressA = (btnA && !prevA);
    prevA = btnA;

    // Joystick — merkez cikart + normalize (-1.0..1.0)
    float jx = (analogRead(JOY_X) - joyCenterX) / 2048.0f;
    float jy = (analogRead(JOY_Y) - joyCenterY) / 2048.0f;

    // Dead-zone (kucuk sapsamalari sifir yap — titreme onler)
    if (fabsf(jx) < 0.15f) jx = 0;
    if (fabsf(jy) < 0.15f) jy = 0;

    // Screenshot (BTN_C) — switch SONRASINDA, pushSprite oncesi alinir
    // Dev tools: ekran goruntusu yakala (BTN_C basildiginda)
    static bool prevC = false;
    bool btnC = !digitalRead(BTN_C);

    // --- Durum makinesi (state machine) ---
    switch (state) {

    // Baslik ekrani: menu + A ile oyun baslat
    case ST_TITLE:
        drawTitle();
        if (pressA) {
            resetGame();           // Oyunu sifirla
            state = ST_PLAY;
            playSound(660, 80);
        }
        break;

    case ST_PLAY:
    {
        // --- Kamera kontrolu (FPS60: delta-time ile) ---
        // Joystick -> kamera yon degisimi (yaw/pitch)
        camYaw += jx * 1.5f * dt;                    // FPS60: rad/s (eskiden 0.06 @25fps)
        camPitch -= jy * 1.0f * dt;                   // FPS60: rad/s (eskiden 0.04 @25fps)
        camPitch = constrain(camPitch, -0.5f, 0.5f);  // Pitch siniri (yukari/asagi)

        // --- Ates (FPS60: delta-time ile) ---
        if (shootCD > 0) { shootCD -= dt; if (shootCD < 0) shootCD = 0; } // FPS60: dt ile azalt
        if (btnA && shootCD <= 0) {
            shootCD = 0.4f; // FPS60: 10 frame / 25fps = 0.4 saniye (atis bekleme)
            // Mermi kameranin baktigi yone gider (Matematiksel ters donusum sirasi onemli!)
            // Baslangic yonu +Z (ileri), once pitch sonra yaw uygula
            Vec3 dir = {0, 0, 1};
            dir = rotateX(dir, camPitch); // X ONCE
            dir = rotateY(dir, camYaw);   // Y SONRA

            // Havuzdan bos mermi slotu bul ve atesle
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (!bullets[i].active) {
                    bullets[i].pos = {0, 0, 0};      // Kameradan cikar
                    bullets[i].vel = v3mul(dir, 37.5f); // FPS60: birim/saniye (eskiden 1.5 @25fps)
                    bullets[i].life = 1.6f;             // FPS60: 40 frame / 25fps = 1.6 saniye
                    bullets[i].active = true;
                    break;
                }
            }
            // Her 3 atista 1 ses (bullets[0] aktifse ~ her 3. atis)
            if (bullets[0].active) playSound(1000 + random(-100, 100), 20);
        }

        // --- Guncellemeler (FPS60: dt parametresi gecirildi) ---
        // Hareket, carpisma, spawn, patlama — tumu delta-time ile
        updateObjects(dt);
        updateBullets(dt);
        updateSpawning(dt);
        updateBooms(dt);

        // Radar cizimi (OLED) - I2C frame drop onlemek icin zaman bazli guncelleme
        // I2C yavas oldugundan her frame degil, ~6Hz'de guncellenir
        {
            static uint32_t lastRadarMs = 0;
            if (now - lastRadarMs > 160) { // FPS60: ~6.25Hz (eskiden animTick%4 @25fps)
                drawRadar();
                lastRadarMs = now;
            }
        }

        // --- Cizim sirasi (arkadan one) ---
        canvas.fillSprite(COL_BG);     // Temizle
        drawStars();                   // Yildiz arka plan
        drawSpaceDust(dt); // FPS60: dt parametresi gecirildi
        drawObjects();                 // 3D dusmanlar
        drawBullets();                 // Mermiler
        drawBooms();                   // Patlamalar
        drawCrosshair();               // Nisan
        drawHUD();                     // Alt HUD
        drawDevHUD(canvas); // FPS + RAM  // Dev tools HUD (FPS/RAM)
        break;
    }

    // Oyun bitti ekrani — A ile basliga don
    case ST_GAMEOVER:
        drawGameOver();
        if (pressA) {
            state = ST_TITLE;
            playSound(440, 60);
        }
        break;

    // Pause ekranı — oyun dondurulur, overlay gosterilir
    case ST_PAUSE:
        canvas.fillSprite(COL_BG);
        drawStars();
        drawSpaceDust(dt); // FPS60: dt parametresi gecirildi (pause'ta da toz hareket eder)
        drawObjects();
        drawBullets();
        drawBooms();
        drawCrosshair();
        drawHUD();
        
        // Pause Overlay — yar saydam panel + menu
        canvas.fillRect(30, 36, 100, 56, RGB(10, 10, 15));
        canvas.drawRect(30, 36, 100, 56, COL_WIRE_CYAN);
        
        canvas.setTextSize(2);
        canvas.setTextColor(COL_WIRE_GOLD);
        canvas.setCursor(50, 42);
        canvas.print("PAUSE");
        
        canvas.setTextSize(1);
        canvas.setTextColor(0xFFFF);
        canvas.setCursor(42, 64);
        canvas.print("[A] Devam Et");
        canvas.setTextColor(RGB(180, 180, 180));
        canvas.setCursor(42, 78);
        canvas.print("[B] OS Menu");

        drawDevHUD(canvas);
        
        if (pressA) {                  // Devam et
            state = ST_PLAY;
            playSound(600, 50);
        }
        break;
    }

    // === Screenshot (BTN_C) — pushSprite ONCESI, tum state'lerde ===
    // Dev tools: ekran goruntusu yakala (BTN_C kenarinda, SD'e kaydet)
    if (btnC && !prevC) {
        if (takeScreenshot(canvas)) playSound(1200, 30);
    }
    prevC = btnC;

    canvas.pushSprite(0, 0);   // Sprite'i TFT'e gonder (tek seferde — flicker-free)
}
