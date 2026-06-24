// ============================================================
//  E-OS — MODE7 RACING
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Double buffer + pushImage (TFT_eSprite yerine)
//  Mode7 pseudo-3D rendering
//
//  Kontroller:
//    JOY_X  -> Direksiyon
//    BTN_A  -> Gaz
//    BTN_B  -> Fren / OS Menu
//    Buzzer -> Motor sesi
// ============================================================

// --- Donanim kutuphanesi: TFT ekrani icin grafik surucu ---
#include <TFT_eSPI.h>
// --- SPI haberlesmesi (TFT icin gerekli) ---
#include <SPI.h>
// --- I2C haberlesmesi (OLED radar icin) ---
#include <Wire.h>
// --- OLED (SH1106) grafik kutuphanesi — radar/mini harita ---
#include <U8g2lib.h>
// OLED nesnesi: 128x64 SH1106, donanimsal I2C, donus yok, reset pini yok
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
// --- ESP32 OTA (Over-The-Air) — OS partition yoneticisi ---
#include <esp_ota_ops.h>
// --- NVFlash (Preferences) — en iyi tur suresi kalici kaydi ---
#include <Preferences.h>
// --- ESP32 heap kapabilite API'si — PSRAM tahsisi icin ---
#include <esp_heap_caps.h>
// --- Donanim konfigurasyonu: pin tanimlari (BTN_*, JOY_*, BUZZER, SPI_*) ---
//     Bu dosya oyun klasoru disinda, paylasilan ortak config dosyasi
#include "../hardware_config.h"
// --- Dev tools: screenshot, SD kart — paylasilan arac kutuphanesi ---
#include "../dev_tools.h"

// ============ Ekran ============
#define SW       160     // Ekran genisligi (piksel) — yatay (landscape) mod
#define SH       128     // Ekran yuksekligi (piksel)
#define ROAD_H   106   // Yol render yuksekligi (106), ekran HUD'i 22px yapildi
#define HUD_H    (SH - ROAD_H)  // 22px HUD (alt bilgi seridi)
#define HORIZON  38    // Ufuk cizgisi (gokyuzu/yol ayrimi Y koordinati)

// ============ Renkler ============
// RGB makro: 8-bit R/G/B -> 16-bit RGB565 (BGR yer degistirmesi yok, dogru sirada)
#define RGB(r,g,b) ((uint16_t)(((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))

#define COL_SKY_TOP   RGB(100, 180, 255)   // Gokyuzu ust (acik mavi)
#define COL_SKY_BOT   RGB(60, 130, 220)    // Gokyuzu alt (koyu mavi / sis rengi)
#define COL_ROAD_A    RGB(90, 90, 100)     // Yol serit rengi A (gri)
#define COL_ROAD_B    RGB(80, 80, 90)      // Yol serit rengi B (koyu gri — stripe)
#define COL_GRASS_A   RGB(40, 160, 50)     // Cim kare rengi A (yesil)
#define COL_GRASS_B   RGB(35, 140, 42)     // Cim kare rengi B (koyu yesil — checker)
#define COL_LINE      RGB(255, 255, 255)   // Yol cizgisi (beyaz)
#define COL_EDGE_A    RGB(255, 60, 60)     // Yol kenari A (kirmizi)
#define COL_EDGE_B    RGB(255, 255, 255)   // Yol kenari B (beyaz)
#define COL_CAR_BODY  RGB(255, 40, 40)     // Oyuncu arac govdesi (kirmizi)
#define COL_CAR_WIND  RGB(40, 80, 160)     // Arac cami (mavi)
#define COL_HUD_BG    RGB(20, 20, 30)      // HUD arka plani (koyu)
#define COL_HUD_TXT   RGB(200, 200, 200)   // HUD metin rengi (gri)

// ============ Pist Sabitleri ============
#define MAP_W    200       // Pist haritasi genisligi (hucresel)
#define MAP_H    200       // Pist haritasi yuksekligi (hucresel)
#define ROAD_W   11        // Yol yaricapi (hucre) — yol genisligi
// Pist dalgalanmasi (yuvarlak yerine virajli kapali halka)
// TRK_WOB: aciya gore yaricap degisimi (sinus) -> virajli oval pist
#define TRK_WOB(rad)  (1.0f + 0.15f * sinf(3.0f * (rad)))

// ============ Fizik ============
// FPS60: tum degerler per-second (saniye bazli) — delta-time ile carpilarak kullanilir
// Eskiden per-frame @20fps idi; *fps veya *fps^2 ile per-second'a cevrildi
#define TARGET_FPS   60   // FPS60: Hedef FPS 60'a yukseltildi (delta-time ile sinirsiz)
#define FRAME_MS     (1000 / TARGET_FPS)  // FPS60: Artik kullanilmiyor

#define ACCEL        16.0f  // FPS60: Per-second (was 0.04 per frame at 20fps -> *fps^2 = 0.04*400 = 16/s^2)
#define BRAKE_FORCE  32.0f  // FPS60: Per-second (was 0.08 per frame at 20fps -> *fps^2 = 0.08*400 = 32/s^2)
#define DRAG         4.0f   // FPS60: Per-second (was 0.01 per frame at 20fps -> *fps^2 = 0.01*400 = 4/s^2)
#define STEER_RATE   1.2f   // FPS60: Per-second (was 0.06 per frame at 20fps -> *fps = 0.06*20 = 1.2 rad/s)
#define MAX_SPEED    30.0f  // FPS60: Units per second (was 1.5 per frame at 20fps -> *fps = 1.5*20 = 30 u/s)
#define GRASS_SLOW   12.0f  // FPS60: Per-second (was 0.03 per frame at 20fps -> *fps^2 = 0.03*400 = 12/s^2)

// ============ Yaris ============
#define TOTAL_LAPS   3            // Toplam tur sayisi (bitis icin)
#define NUM_CHECKPOINTS 8         // Pist cevresindeki checkpoint sayisi

// ============ Nesneler ============
TFT_eSPI tft = TFT_eSPI();          // TFT ekran surucu nesnesi
TFT_eSprite canvas = TFT_eSprite(&tft);  // Cift tamponlama sprite'i
uint16_t* fb = NULL; // Sprite'in iç buffer'ına işaretçi (direkt piksel yazma icin)

// Pist haritasi
uint8_t* trackMap = NULL;  // MAP_H * MAP_W = 16384 bytes (1=yol, 0=cim)

// ============ Ses ============
bool soundEnabled = true;           // Ses acik/kapali (Preferences'tan okunur)
// playSound — buzzer'a ton uretir (sadece ses aciksa)
// freq: frekans (Hz), dur: sure (ms)
void playSound(uint16_t freq, uint32_t dur) {
    if (soundEnabled) tone(BUZZER, freq, dur);
}

// returnToOS — oyunu OS menuye dondur (ESP restart ile)
void returnToOS() {
    noTone(BUZZER);            // Motor sesini kes
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(20, 60);
    tft.print("Ana Menuye Donuluyor...");
    delay(500);                // Kullanici mesaji gorsun diye kisa bekle
    ESP.restart();             // Cihazi yeniden baslat -> OS boot partition
}

// ============ Durum ============
// State — oyun durum makinesi (state machine)
// ST_COUNTDOWN: geri sayim, ST_RACING: yaris, ST_FINISH: bitis, ST_PAUSE: duraklat
enum State { ST_TITLE, ST_COUNTDOWN, ST_RACING, ST_FINISH, ST_PAUSE };
State state = ST_TITLE;   // Su anki durum (baslangicta baslik ekrani)

// ============ Oyuncu ============
// Racer — bir yariscinin durumu (oyuncu ve AI icin kullanilir)
struct Racer {
    float x, y;       // Pist haritasinda konum
    float angle;      // Bakis/hareket acisi (radyan)
    float speed;      // Mevcut hiz (birim/saniye)
};
Racer player;         // Oyuncu araci

// ============ AI Rakipler ============
#define NUM_AI 2                    // AI rakip sayisi
Racer aiCars[NUM_AI];               // AI araclari
uint16_t aiColors[NUM_AI] = { RGB(40, 120, 255), RGB(255, 200, 40) };  // Mavi + sari
int aiCheckpoint[NUM_AI] = {0, 2};  // Her AI'nin sonraki checkpoint'i (farkli baslangic)

// ============ Checkpoint ============
// Checkpoint — pist cevresinde sirayla gecilen kontrol noktasi (tur sayma)
struct Checkpoint {
    float x, y;        // Konum
    float radius;      // Geçiş yaricapi (bu mesafe icine girilince gecildi say)
};
Checkpoint checkpoints[NUM_CHECKPOINTS];  // Checkpoint listesi
int playerNextCP = 0;   // Oyuncunun siradaki checkpoint index'i
int playerLap = 0;      // Oyuncunun tamamladigi tur sayisi
int aiLap[NUM_AI] = {0, 0};  // AI'larin tur sayisi

// ============ Yol kenari direkleri (billboard) ============
#define NUM_POSTS 36                  // Direk sayisi (pist cevresine dagik)
struct Vec2 { float x, y; };          // 2D vektor (dunya konumu)
Vec2 posts[NUM_POSTS];                // Direk konumlari
void buildPosts();                    // On-tanimi (yukarida cagrilabilir)
void renderPosts();

// ============ OLED radar guncelleme sayaci ============
uint8_t radarTick = 0;    // Her ~10 frame'de bir radar guncelle (I2C yavas)

// ============ Genel ============
int joyCenterX, joyCenterY;        // Joystick kalibrasyon merkezi (0 konumu)
uint32_t lastFrameMs = 0;          // Delta-time hesabi icin onceki frame zamani
uint32_t animTick = 0;             // Animasyon sayaci (stripe, motor dalgalanma)
uint32_t stateTimer = 0;           // Durum gecisleri icin zamanlayici
int countdownVal = 3;              // Geri sayim baslangic degeri (3,2,1)
uint32_t lapStartTime = 0;         // Mevcut turun baslangic zamani
uint32_t lastLapTime = 0;          // Son tamamlanan turun suresi
uint32_t bestLapTime = UINT32_MAX; // En iyi tur suresi (baslangicta "yok")

uint32_t fpsFrameCount = 0;        // FPS sayac (1 sn icinde frame say)
uint32_t fpsStartTime = 0;         // FPS olcum penceresi baslangici
int currentFPS = 0;                // Son olculen FPS degeri

// ============================================================
//  PİST OLUSTURMA (oval)
// ============================================================
// buildTrack — trackMap'i olustur: oval/virajli kapali pist halkasi ciz
// Her acida elips uzerinde nokta alip cevresine ROAD_W yaricapi daire koyar
void buildTrack() {
    if (!trackMap) return;
    memset(trackMap, 0, MAP_H * MAP_W);   // Once tum haritayi cim (0) yap

    // Oval pist (elips seklinde)
    float cx = MAP_W / 2.0f;             // Pist merkezi X
    float cy = MAP_H / 2.0f;             // Pist merkezi Y
    float ra = MAP_W / 2.0f - 26;  // Yatay yaricap (kenar payi artirildi)
    float rb = MAP_H / 2.0f - 26;  // Dikey yaricap

    // 360 derece boyunca elips uzerinde nokta al, etrafinda yol koy
    for (int deg = 0; deg < 360; deg++) {
        float rad = deg * M_PI / 180.0f;
        float wob = TRK_WOB(rad);         // Viraj dalgalanmasi (sinus)
        float mx = cx + ra * wob * cosf(rad);   // Elips uzerinde X
        float my = cy + rb * wob * sinf(rad);   // Elips uzerinde Y

        // Yol genisligi — merkez etrafinda ROAD_W yaricapli daireyi 1 (yol) yap
        for (int dy = -ROAD_W; dy <= ROAD_W; dy++) {
            for (int dx = -ROAD_W; dx <= ROAD_W; dx++) {
                int px = (int)(mx + dx);
                int py = (int)(my + dy);
                if (px >= 0 && px < MAP_W && py >= 0 && py < MAP_H) {
                    float dist = sqrtf(dx*dx + dy*dy);   // Merkeze olan mesafe
                    if (dist <= ROAD_W) trackMap[py * MAP_W + px] = 1;  // Daire icinde = yol
                }
            }
        }
    }
}

// buildCheckpoints — pist cevresine esit aralikli checkpoint'leri yerlestir
// Her checkpoint bir oncekiyle ayni elips uzerinde, TRK_WOB dalgalanmasi ile
void buildCheckpoints() {
    float cx = MAP_W / 2.0f;
    float cy = MAP_H / 2.0f;
    float ra = MAP_W / 2.0f - 26;
    float rb = MAP_H / 2.0f - 26;

    for (int i = 0; i < NUM_CHECKPOINTS; i++) {
        // Esit aci araligi (0..2*PI) ile dagit
        float rad = i * 2.0f * M_PI / NUM_CHECKPOINTS;
        float wob = TRK_WOB(rad);
        checkpoints[i].x = cx + ra * wob * cosf(rad);
        checkpoints[i].y = cy + rb * wob * sinf(rad);
        checkpoints[i].radius = ROAD_W + 4;   // Gecis toleransi (yol genisligi + 4)
    }
}

// Pist disina, kenara dizilen direkler (hiz hissi icin)
// buildPosts — yol kenarina disa dogru direkleri yerlestir (billboard render icin)
void buildPosts() {
    float cx = MAP_W / 2.0f, cy = MAP_H / 2.0f;
    float ra = MAP_W / 2.0f - 26, rb = MAP_H / 2.0f - 26;
    float edge = ROAD_W + 3;   // yol merkezinden dis kenara mesafe
    for (int i = 0; i < NUM_POSTS; i++) {
        float rad = i * 2.0f * M_PI / NUM_POSTS;   // Esit aralikla dagit
        float wob = TRK_WOB(rad);
        posts[i].x = cx + (ra * wob + edge) * cosf(rad);   // Yol dis kenari + edge
        posts[i].y = cy + (rb * wob + edge) * sinf(rad);
    }
}

// ============================================================
//  MODE7 RENDER
// ============================================================
// renderMode7 — pseudo-3D yaris goruntusu olustur (gokyuzu + tepeler + yol)
// Mode7: her ekran satirini bir dunya derinligine esler, perspektif verir
// Direkt frame buffer'a (fb) yazar — hizli piksel erisimi
void renderMode7() {
    // fb: sprite buffer'ina direkt pointer (ilk frame'de al)
    if (!fb) { fb = (uint16_t*)canvas.getPointer(); if (!fb) return; }
    // Oyuncu yonu icin onceden hesapla (her pikselde tekrar etmesin)
    float cosA = cosf(player.angle);
    float sinA = sinf(player.angle);

    // Gokyuzu — ufuk cizgisine kadar dikey gradyan (ust acik, alt koyu)
    for (int y = 0; y < HORIZON; y++) {
        float t = (float)y / HORIZON;          // 0 (ust) -> 1 (ufuk)
        uint16_t skyC = RGB(
            (int)(100 - t * 40),               // R: 100 -> 60
            (int)(180 - t * 50),               // G: 180 -> 130
            255                                // B: sabit (mavi)
        );
        for (int x = 0; x < SW; x++) fb[y * SW + x] = skyC;
    }

    // Uzak tepeler (panorama — direksiyona gore yatay kayar, dunya hissi)
    // HILL_FOV: tepelerin ne kadar genis aciya yayilacagi (radyan)
    const float HILL_FOV = 1.4f;
    for (int x = 0; x < SW; x++) {
        // Bu ekran sutunu icin dunya yonu (oyuncu acisi + ekran icindeki offset)
        float dir = player.angle + ((float)x / SW - 0.5f) * HILL_FOV;
        // Iki sinus dalgasinin toplami -> dogal gorunen tepe silueti
        float h = sinf(dir * 3.0f) * 4.0f + sinf(dir * 7.0f + 1.3f) * 2.5f;
        int hillH = (int)(7.0f + h);           // Taban yukseklik + dalga
        if (hillH < 1) hillH = 1;              // Min 1 piksel
        int hy = HORIZON - hillH;              // Tepenin ust siniri
        if (hy < 0) hy = 0;
        for (int y = hy; y < HORIZON; y++) fb[y * SW + x] = RGB(70, 95, 130);  // Tepe rengi
    }

    // Yol (Mode7 perspektif)
    // camH: kamera yuksekligi, focal: projeksiyon odak uzakligi
    float camH = 15.0f; // Kamera yuksekligi
    float focal = 40.0f;
    float maxDepth = 120.0f; // Maksimum gorus mesafesi (sis siniri)
    
    // Her ekran satiri (ufuktan yol sonuna) bir dunya derinligine karsilik gelir
    for (int y = HORIZON; y < ROAD_H; y++) {
        // row: ufuktan uzaklik (1 = en uzak, buyudukce yakinlasir)
        float row = (float)(y - HORIZON + 1);
        // Mode7 perspektif: depth = camH * focal / row (kucuk row = buyuk depth)
        float depth = (camH * focal) / row;
        
        bool isFoggy = false;
        if (depth > maxDepth) {                // Sis siniri otesi
            depth = maxDepth;
            isFoggy = true;
        }

        // halfW: bu derinlikteki goruntu yaricap (FOV genisligi)
        float halfW = depth * 1.0f; // FOV biraz daha genisletildi

        // Dunya baslangic noktasi (Ekranin SOL kenari) - Aynalanma hatasi duzeltildi!
        // Kamera yonunde ileri (cosA/sinA * depth) + yatay kayma (sinA/cosA * halfW)
        float wx = player.x + cosA * depth + sinA * halfW;
        float wy = player.y + sinA * depth - cosA * halfW;

        // Adim vektoru (Soldan saga dogru) — her pikselde dunya artisi
        float dx = (-sinA * halfW * 2.0f) / (float)SW;
        float dy = ( cosA * halfW * 2.0f) / (float)SW;

        // Stripe (serit efekti): derinlik + hareket ile degisen A/B rengi
        // Hizla ilerleyince seritler hareket eder (yol hizi hissi)
        bool stripe = ((int)(depth * 0.2f + animTick * player.speed * 0.025f)) % 2; // FPS60: Stripe factor recalibrated (0.5/20=0.025)

        for (int x = 0; x < SW; x++) {
            if (isFoggy) {
                // Ufuk cizgisinde sonsuzluk hissi (gokyuzu ile kaynasma)
                fb[y * SW + x] = COL_SKY_BOT;
            } else {
                // Harita disina cikmayi onle (Sonsuz tekrar eden yollari kaldir)
                bool outOfBounds = (wx < 0 || wx >= MAP_W || wy < 0 || wy >= MAP_H);
                int mapX = (int)wx;
                int mapY = (int)wy;

                uint16_t col;
                if (outOfBounds) {
                    // Harita disi: sonsuz cim (checker deseni, +10000 negatif fix)
                    bool checker = (((int)(wx + 10000)/4) + ((int)(wy + 10000)/4)) % 2;
                    col = checker ? COL_GRASS_A : COL_GRASS_B;
                } else {
                    bool onRoad = trackMap[mapY * MAP_W + mapX];
                    if (onRoad) {
                        col = stripe ? COL_ROAD_A : COL_ROAD_B;   // Serit rengi
                        // Kenar pikseli mi? 4 komsudan biri cimse kenar = true
                        bool edge = false;
                        int nx1 = constrain(mapX + 1, 0, MAP_W - 1);
                        int nx2 = constrain(mapX - 1, 0, MAP_W - 1);
                        int ny1 = constrain(mapY + 1, 0, MAP_H - 1);
                        int ny2 = constrain(mapY - 1, 0, MAP_H - 1);
                        if (!trackMap[mapY * MAP_W + nx1] || !trackMap[mapY * MAP_W + nx2] ||
                            !trackMap[ny1 * MAP_W + mapX] || !trackMap[ny2 * MAP_W + mapX]) {
                            edge = true;
                        }
                        // Kenarda kirmizi/beyaz serit (yol siniri)
                        if (edge) col = stripe ? COL_EDGE_A : COL_EDGE_B;
                    } else {
                        // Cim: 4x4 hucre checker deseni
                        bool checker = ((mapX / 4) + (mapY / 4)) % 2;
                        col = checker ? COL_GRASS_A : COL_GRASS_B;
                    }
                }

                // Uzaklik sisi (fog) - Keskin pikselleşmeyi engeller
                // 0x7BEF maskesi: her kanaldan 1 bit dusur (yari parlaklik)
                if (depth > 60.0f) col = (col >> 1) & 0x7BEF;   // 60+ -> %50 koyu
                if (depth > 90.0f) col = (col >> 1) & 0x7BEF;   // 90+ -> %25 koyu

                fb[y * SW + x] = col;
            }
            wx += dx;    // Bir sonraki piksel icin dunya X'ini ilerlet
            wy += dy;    // Bir sonraki piksel icin dunya Y'sini ilerlet
        }
    }
}

// ============================================================
//  OYUNCU ARACI SPRITE (alt ortada sabit)
// ============================================================
// renderPlayerCar — oyuncunun arabasini ekran alt-ortasina ciz (arkadan gorunum)
// steer: direksiyon degeri (-1..1) -> arac hafif yana kayar (hiz hissi)
void renderPlayerCar(float steer) {
    int baseY = ROAD_H - 2;                    // Aracin taban Y'si (yol ustu)
    int cx = SW / 2 + (int)(steer * 12.0f);    // Merkez + direksiyon kaymasi (max 12px)
    bool isBraking = !digitalRead(BTN_B); // Fren lambasi icin

    // 1. Arka Lastikler (daha genis) — dis kenarlarda siyah
    for (int y = baseY - 6; y < baseY; y++) {
        if (y < HORIZON || y >= ROAD_H) continue;   // Yol alaninin disinda atla
        for (int x = cx - 12; x <= cx + 12; x++) {
            if (x < 0 || x >= SW) continue;
            if (x < cx - 6 || x > cx + 6) {         // Kenar 6px = lastik
                fb[y * SW + x] = RGB(15, 15, 15); // Siyah tekerlek
            }
        }
    }

    // 2. Alt Govde / Difuzor — orta kisim kirmizi govde
    for (int y = baseY - 5; y < baseY; y++) {
        if (y < HORIZON || y >= ROAD_H) continue;
        for (int x = cx - 8; x <= cx + 8; x++) {
            if (x < 0 || x >= SW) continue;
            fb[y * SW + x] = COL_CAR_BODY;
        }
    }

    // 3. Arka Stop Lambalari — fren yapiliyorsa parlak, degilse koyu kirmizi
    uint16_t tailColor = isBraking ? RGB(255, 50, 50) : RGB(100, 0, 0);
    for (int x = cx - 7; x <= cx - 4; x++) {        // Sol stop
        if (x >= 0 && x < SW) fb[(baseY - 4) * SW + x] = tailColor;
    }
    for (int x = cx + 4; x <= cx + 7; x++) {        // Sag stop
        if (x >= 0 && x < SW) fb[(baseY - 4) * SW + x] = tailColor;
    }

    // 4. Ust Kabin / Pilot Cam — ustte koyu cam, altta govde
    for (int y = baseY - 10; y < baseY - 5; y++) {
        if (y < HORIZON || y >= ROAD_H) continue;
        for (int x = cx - 5; x <= cx + 5; x++) {
            if (x < 0 || x >= SW) continue;
            if (y < baseY - 7) fb[y * SW + x] = RGB(30, 30, 40); // Koyu cam/kask
            else fb[y * SW + x] = COL_CAR_BODY;
        }
    }

    // 5. Spoiler (Arka Kanat) — ustte beyaz/gri kanat + ayaklar
    int spY = baseY - 11;
    if (spY >= HORIZON) {
        for (int x = cx - 10; x <= cx + 10; x++) {
            if (x >= 0 && x < SW) fb[spY * SW + x] = RGB(220, 220, 220); // Beyaz/Gri kanat
        }
        // Kanat ayaklari (sol/sag destekler)
        if (cx - 6 >= 0 && cx - 6 < SW) fb[(spY + 1) * SW + cx - 6] = RGB(100, 100, 100);
        if (cx + 6 >= 0 && cx + 6 < SW) fb[(spY + 1) * SW + cx + 6] = RGB(100, 100, 100);
    }
}

// ============================================================
//  RAKİP ARACI SPRITE (perspektifli)
// ============================================================
// renderAICar — AI rakip araci perspektif olcegiyle ekrana ciz (kamera uzayinda)
// idx: aiCars icindeki AI index'i (0..NUM_AI-1)
void renderAICar(int idx) {
    Racer& ai = aiCars[idx];
    float cosA = cosf(player.angle);     // Oyuncu yonu (onceden hesapla)
    float sinA = sinf(player.angle);

    // AI'nin oyuncuya gore dunya farki
    float dx = ai.x - player.x;
    float dy = ai.y - player.y;

    // Kamera uzayina donusum (oyuncu yonunde dondur)
    float tz = cosA * dx + sinA * dy;  // Derinlik (ileri/geri)
    float tx = -sinA * dx + cosA * dy; // Yatay (sol/sag)

    // Cok yakin (<5, kamera icinde) veya cok uzak (>120, sista) -> cizme
    if (tz < 5.0f || tz > 120.0f) return; // Cok yakin/arkada veya sisin icinde gorunmez

    // Projeksiyon sabitleri — renderMode7 ile ayni (tutarli derinlik icin)
    float focal = 40.0f;       // Dikey projeksiyon (renderMode7 camH*focal/row ile ayni)
    float camH = 15.0f;        // renderMode7 ile ayni
    float focalX = SW / 2.0f;  // YATAY projeksiyon: zeminde halfW=depth oldugundan SW/2

    // Ekran konumu ve olcegi (perspektif: tz'ye bol)
    int sx = (int)(SW / 2 + (tx / tz) * focalX); // yatayda zeminle hizali
    float scaleY = focal / tz;   // yukseklikler (dikey olcek)
    float scaleX = focalX / tz;  // genislikler (yatay perspektifle uyumlu)

    // Arac boyutlari (olcege gore) — constrain ile sinirli (yakin = buyuk)
    int cw = (int)(scaleX * 5.0f);     // govde genisligi
    int ch = (int)(scaleY * 3.0f);     // govde yuksekligi
    int tireW = (int)(scaleX * 1.3f);  // lastik genisligi
    int cabW = (int)(scaleX * 2.8f);   // kabin genisligi
    cw = constrain(cw, 3, 18);       // Yakin mesafede devasa olmasini onle
    ch = constrain(ch, 2, 8);
    tireW = constrain(tireW, 1, 4);
    cabW = constrain(cabW, 2, 10);

    // Taban Y'si: zemin temas noktasi (Mode7 ile ayni formül)
    int sy = (int)(HORIZON + (camH * focal) / tz);
    sy = constrain(sy, HORIZON + 5, ROAD_H - 1); // Taban noktasi

    // Lastikler ve Alt Govde (Birlestirildi ki kucukken kaybolmasin)
    for (int py = sy - ch; py <= sy; py++) {
        if (py < HORIZON || py >= ROAD_H) continue;
        for (int px = sx - cw/2; px <= sx + cw/2; px++) {
            if (px < 0 || px >= SW) continue;
            // Sol ve sag kenarlar lastik, orta kisim govde
            if (px < sx - cw/2 + tireW || px > sx + cw/2 - tireW) {
                fb[py * SW + px] = RGB(15, 15, 15); // Siyah Tekerlek
            } else {
                fb[py * SW + px] = aiColors[idx]; // Arac Rengi
            }
        }
    }
    
    // Ust Kabin ve Cam — kabin yuksekligi sinirli (dikey uzama bug fix)
    int cabH = constrain((int)(scaleY * 2.5f), 1, 5);  // yukseklik sinirli (dikey uzama fix)
    int cabTop = sy - ch - cabH;                 // Kabin ust siniri
    for (int py = cabTop; py < sy - ch; py++) {
        if (py < HORIZON || py >= ROAD_H) continue;
        for (int px = sx - cabW/2; px <= sx + cabW/2; px++) {
            if (px < 0 || px >= SW) continue;
            if (py == cabTop) fb[py * SW + px] = aiColors[idx]; // Tavan
            else fb[py * SW + px] = RGB(30, 30, 40); // Siyah cam
        }
    }

    // Spoiler — kabin ustunde kanat + ayaklar (renk yari parlaklikta)
    int spY = cabTop - constrain((int)(scaleY * 1.5f), 1, 6);
    if (spY >= HORIZON && spY < ROAD_H) {
        for (int px = sx - cw/2; px <= sx + cw/2; px++) {
            if (px >= 0 && px < SW) fb[spY * SW + px] = ((aiColors[idx] >> 1) & 0x7BEF); // Kanat
        }
        // Spoiler Ayaklari
        if (spY + 1 >= HORIZON && spY + 1 < ROAD_H) {
            int leg1 = sx - cabW/2;
            int leg2 = sx + cabW/2;
            if (leg1 >= 0 && leg1 < SW) fb[(spY + 1) * SW + leg1] = RGB(100, 100, 100);
            if (leg2 >= 0 && leg2 < SW) fb[(spY + 1) * SW + leg2] = RGB(100, 100, 100);
        }
    }
}

// ============================================================
//  YOL KENARI DIREKLERI (billboard render)
// ============================================================
// renderPosts — yol kenari direklerini perspektif olcegiyle ciz (hiz hissi)
// Her direk kirmizi-beyaz cizgili (kucuk dikey cubuk)
void renderPosts() {
    float cosA = cosf(player.angle);
    float sinA = sinf(player.angle);
    for (int i = 0; i < NUM_POSTS; i++) {
        // Diregin oyuncuya gore dunya farki
        float dx = posts[i].x - player.x;
        float dy = posts[i].y - player.y;
        float tz = cosA * dx + sinA * dy;   // derinlik
        if (tz < 3.0f || tz > 120.0f) continue;   // Cok yakin/uzak atla
        float tx = -sinA * dx + cosA * dy;  // yatay

        // Yatay ekran konumu (zeminle ayni olcek)
        int sx = (int)(SW / 2 + (tx / tz) * (SW / 2.0f)); // zeminle ayni yatay olcek
        if (sx < -4 || sx >= SW + 4) continue;    // Ekranda degilse atla

        // Taban Y: zemin temas noktasi (15.0f*40.0f = camH*focal)
        int baseY = (int)(HORIZON + (15.0f * 40.0f) / tz); // zemin temas noktasi (camH*focal)
        baseY = constrain(baseY, HORIZON, ROAD_H - 1);
        // Direk yuksekligi/genisligi (perspektif — uzakta kuculur)
        int ph = constrain((int)((40.0f / tz) * 7.0f), 2, 32); // direk yuksekligi
        int pw = constrain((int)((SW / 2.0f / tz) * 0.9f), 1, 4);
        int topY = baseY - ph;                 // Direk ust ucu
        if (topY < HORIZON) topY = HORIZON;    // Ufuk ustune cikma

        // Direk govdesi: kirmizi-beyaz cizgili (her 3 pikselde renk degisir)
        for (int y = topY; y <= baseY; y++) {
            uint16_t c = (((y / 3) & 1) ? COL_EDGE_A : COL_EDGE_B); // kirmizi-beyaz cizgili
            for (int x = sx - pw / 2; x <= sx + pw / 2; x++) {
                if (x >= 0 && x < SW) fb[y * SW + x] = c;
            }
        }
    }
}

// ============================================================
//  AI GUNCELLEME
// ============================================================
// updateAI — bir AI rakibinin checkpoint takibini ve hareketini guncelle
// idx: AI index'i, dt: delta-time (saniye)
// AI checkpoint'lere yonelir, rubber-band (lastik) hiz ile oyuncuya yaklasir/uzaklasir
void updateAI(int idx, float dt) {
    Racer& r = aiCars[idx];
    Checkpoint& cp = checkpoints[aiCheckpoint[idx] % NUM_CHECKPOINTS];  // Siradaki CP

    // Checkpoint'e mesafe
    float dx = cp.x - r.x;
    float dy = cp.y - r.y;
    float dist = sqrtf(dx*dx + dy*dy);

    // Checkpoint'e ulasti mi? (yaricap icine girdi)
    if (dist < cp.radius) {
        aiCheckpoint[idx] = (aiCheckpoint[idx] + 1) % NUM_CHECKPOINTS;  // Sonrakine gec
        if (aiCheckpoint[idx] == 0) aiLap[idx]++;   // Tur tamamlandi (0'a donunce)
    }

    // Hedefe don — en kisa aci farkini bul (wrap -PI..PI)
    float targetAngle = atan2f(dy, dx);
    float angleDiff = targetAngle - r.angle;
    while (angleDiff > M_PI)  angleDiff -= 2 * M_PI;   // +PI ustu -> negatif
    while (angleDiff < -M_PI) angleDiff += 2 * M_PI;   // -PI alti -> pozitif

    r.angle += angleDiff * 3.0f * dt;   // Yumusak donus (delta-time)

    // Rubber-band hiz — oyuncuya gore mesafeye ile hedef hiz ayarla
    // (Oyuncu gerideyse AI yavaslar, one gectiyse hizlanir -> denge)
    float targetSpeed = 30.0f;                                   // FPS60: Per-second (was 1.5)
    float pDist = sqrtf((r.x - player.x)*(r.x - player.x) +
                        (r.y - player.y)*(r.y - player.y));
    if (pDist > 15) targetSpeed = 36.0f;                         // FPS60: Per-second (was 1.8) // Oyuncu uzakta = AI hizlanir
    if (pDist < 5) targetSpeed = 24.0f;                          // FPS60: Per-second (was 1.2)  // Oyuncu yakin = AI yavaslar

    // Mevcut hizi hedefe yumusakca yaklastir
    r.speed += (targetSpeed - r.speed) * 2.0f * dt;

    // Hareket (delta-time ile)
    r.x += cosf(r.angle) * r.speed * dt;                         // FPS60: Delta-time
    r.y += sinf(r.angle) * r.speed * dt;                         // FPS60: Delta-time

    // Sinirlar — harita icinde kal (kenardan cikmasin)
    r.x = constrain(r.x, 1.0f, (float)MAP_W - 2);
    r.y = constrain(r.y, 1.0f, (float)MAP_H - 2);
}

// ============================================================
//  HUD CİZİMİ (TFT direkt, fb altinda)
// ============================================================
// drawHUD — alt seride hiz/tur/pozisyon/sure/FPS bilgisini ciz
void drawHUD() {
    // HUD arka plan seridi + ust ayirici cizgi
    canvas.fillRect(0, ROAD_H, SW, HUD_H, COL_HUD_BG);
    canvas.drawFastHLine(0, ROAD_H, SW, RGB(60, 60, 80));

    canvas.setTextSize(1);

    // Hiz (yuzde olarak) — MAX_SPEED'e gore
    int spdPct = (int)(player.speed / MAX_SPEED * 100);
    spdPct = constrain(spdPct, 0, 100);
    canvas.setTextColor(RGB(80, 255, 80), COL_HUD_BG);   // Yesil (saydam arka plan)
    
    // Yazi genisligi hesaplanarak 35px barin ustunde ortalaniyor
    char spdStr[16];
    snprintf(spdStr, sizeof(spdStr), "%d%%", spdPct);
    int spdW = strlen(spdStr) * 6;            // Font genisligi 6px
    int spdX = 4 + (35 - spdW) / 2;           // 35px bar uzerinde ortala
    
    canvas.setCursor(spdX, ROAD_H + 3);
    canvas.print(spdStr);

    // Hiz cubugu (Alt metinlerle ortalandi) — yesil dolu + koyu yesil bos
    int barW = spdPct * 35 / 100;             // 35px bar uzerinden yuzde
    canvas.fillRect(4, ROAD_H + 15, barW, 3, RGB(80, 255, 80));
    canvas.fillRect(4 + barW, ROAD_H + 15, 35 - barW, 3, RGB(30, 50, 30));

    // Tur (mevcut/total) — sari renk
    canvas.setTextColor(RGB(255, 220, 80), COL_HUD_BG);
    canvas.setCursor(44, ROAD_H + 3);
    canvas.printf("TUR:%d/%d", min(playerLap + 1, TOTAL_LAPS), TOTAL_LAPS);

    // Pozisyon — AI'larin tur/CP durumuna gore oyuncunun sirasi
    int pos = 1;
    for (int i = 0; i < NUM_AI; i++) {
        if (aiLap[i] > playerLap) pos++;                                  // AI onde
        else if (aiLap[i] == playerLap && aiCheckpoint[i] > playerNextCP) pos++;  // Ayni tur, AI onde CP
    }
    canvas.setTextColor(pos == 1 ? RGB(255, 220, 0) : RGB(200, 200, 200), COL_HUD_BG);  // 1. = altin
    canvas.setCursor(44, ROAD_H + 13);
    canvas.printf("POZ:%d/%d", pos, NUM_AI + 1);

    // Son tur suresi (Saga dayali, her zaman gosterilir)
    char lapStr[32];
    snprintf(lapStr, sizeof(lapStr), "Lap: %02lu.%02lus", lastLapTime / 1000, (lastLapTime % 1000) / 10);
    int lw = strlen(lapStr) * 6;
    canvas.setTextColor(RGB(180, 180, 180), COL_HUD_BG);
    canvas.setCursor(SW - lw - 2, ROAD_H + 3);
    canvas.print(lapStr);

    // En iyi tur suresi (Saga dayali, her zaman gosterilir)
    char bestStr[32];
    if (bestLapTime != UINT32_MAX) {
        snprintf(bestStr, sizeof(bestStr), "Best:%02lu.%02lus", bestLapTime / 1000, (bestLapTime % 1000) / 10);
    } else {
        snprintf(bestStr, sizeof(bestStr), "Best:00.00s");   // Henuz rekor yok
    }
    int bw = strlen(bestStr) * 6;
    canvas.setTextColor(RGB(255, 100, 255), COL_HUD_BG);    // Magenta
    canvas.setCursor(SW - bw - 2, ROAD_H + 13);
    canvas.print(bestStr);

    // FPS (Oyun alaninda sag uste saydam sekilde, Kirmizi)
    char fpsStr[16];
    snprintf(fpsStr, sizeof(fpsStr), "FPS:%d", currentFPS);
    int fpsW = strlen(fpsStr) * 6;
    canvas.setTextColor(RGB(255, 50, 50)); // Kirmizi (Transparent)
    canvas.setCursor(SW - fpsW - 2, 2);
    canvas.print(fpsStr);
}

// ============================================================
//  BASLIK EKRANI
// ============================================================
// drawTitleScreen — baslik ekranini ciz (baslik + araba ikonu + menu + rekor)
// A ile yaris baslar, B ile OS menuye don
void drawTitleScreen() {
    canvas.fillSprite(TFT_BLACK);    // Siyah arka plan

    // Başlık Ortalanmış — "MODE 7" (size 2 -> 12px/char)
    canvas.setTextSize(2);
    const char* title = "MODE 7";
    int titleW = strlen(title) * 12;            // Font genisligi 12px
    canvas.setTextColor(RGB(255, 200, 0));      // Altin/sari
    canvas.setCursor((SW - titleW) / 2, 20);    // Yatay ortala
    canvas.print(title);

    // Kucuk araba ikonu (Ortalanmış) — kirmizi ucgen
    int cx = SW / 2;
    canvas.fillTriangle(cx, 45, cx - 7, 58, cx + 7, 58, COL_CAR_BODY);

    // Alt Menü Seçenekleri (3 Parça)
    canvas.setTextSize(1);
    
    canvas.setTextColor(TFT_WHITE);             // Beyaz
    canvas.setCursor(10, 95);
    canvas.print("[A] Basla");

    canvas.setTextColor(0xBDF7);                // Acik mavi (16-bit renk)
    canvas.setCursor(90, 95);
    canvas.print("[B] OS Menu");

    // En iyi tur suresi (ortada, rekor varsa)
    if (bestLapTime != UINT32_MAX) {
        canvas.setTextColor(TFT_YELLOW);
        char bestStr[32];
        snprintf(bestStr, sizeof(bestStr), "Best: %lu.%02lu", bestLapTime / 1000, (bestLapTime % 1000) / 10);
        int bestW = strlen(bestStr) * 6;
        canvas.setCursor((SW - bestW) / 2, 110);
        canvas.print(bestStr);
    } else {
        canvas.setTextColor(TFT_YELLOW);
        const char* bestStr = "Best: --.--";    // Henuz rekor yok
        int bestW = strlen(bestStr) * 6;
        canvas.setCursor((SW - bestW) / 2, 110);
        canvas.print(bestStr);
    }

    // Dev tools: ekran goruntusu yakala (baslik ekraninda SD'e kaydet)
    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);    // TFT'e gonder
}

// ============================================================
//  OLED RADAR (ikinci ekran — 128x64)
// ============================================================
// drawRadar — OLED'e mini pist haritasi + rakip/oyuncu konumu ciz
// Pist 128x50 alana, altta 14px bilgi cubugu (tur/hiz)
void drawRadar() {
    oled.clearBuffer();

    // Pist merkezi ve yaricaplari (buildTrack ile ayni)
    float cx = MAP_W / 2.0f, cy = MAP_H / 2.0f;
    float ra = MAP_W / 2.0f - 26, rb = MAP_H / 2.0f - 26;
    // Pist 128x50 alanina yayilacak (altta 14px bilgi cubugu)
    const int ox = 64, oy = 25;     // radar merkezi (OLED ortasi)
    const float scX = 0.40f;        // yatay olcek (ekrana sigacak)
    const float scY = 0.30f;        // dikey olcek (ekrana sigacak)

    // Pist halkasi — 3 derece adimlarla nokta koy (hizli cizim)
    for (int deg = 0; deg < 360; deg += 3) {
        float rad = deg * M_PI / 180.0f;
        float wob = TRK_WOB(rad);
        int px = ox + (int)((ra * wob * cosf(rad)) * scX);
        int py = oy + (int)((rb * wob * sinf(rad)) * scY);
        if (px >= 0 && px < 128 && py >= 0 && py < 50)
            oled.drawPixel(px, py);
    }
    // Rakipler (halka) — her AI'yi kucuk daire ile goster
    for (int i = 0; i < NUM_AI; i++) {
        int ax = ox + (int)((aiCars[i].x - cx) * scX);
        int ay = oy + (int)((aiCars[i].y - cy) * scY);
        if (ax >= 1 && ax < 127 && ay >= 1 && ay < 49)
            oled.drawCircle(ax, ay, 2);
    }
    // Oyuncu (dolu kare) — 5x5 piksel
    int ppx = ox + (int)((player.x - cx) * scX);
    int ppy = oy + (int)((player.y - cy) * scY);
    oled.drawBox(ppx - 2, ppy - 2, 5, 5);

    // Alt bilgi cubugu (128px genislik, 14px yukseklik)
    oled.drawHLine(0, 51, 128); // ayirici cizgi
    oled.setFont(u8g2_font_6x12_tr);
    int spd = (int)(player.speed / MAX_SPEED * 100);   // Hiz yuzde
    oled.setCursor(2, 63);
    oled.printf("TUR %d/%d", min(playerLap + 1, TOTAL_LAPS), TOTAL_LAPS);
    oled.setCursor(80, 63);
    oled.printf("HIZ %d%%", constrain(spd, 0, 100));

    oled.sendBuffer();   // OLED'e aktar
}

// ============================================================
//  SETUP
//  Donanim baslatma, kalici veri yukleme, ekran/sprite/pist hazirligi
// ============================================================
void setup() {
    // --- Buzzer pini (motor sesi) ---
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);

    // --- OLED (ikinci ekran) — I2C pin 8/9, 400kHz hiz ---
    Wire.begin(8, 9);
    oled.setBusClock(400000);
    oled.begin();
    oled.clearBuffer();
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
    // Ses acik/kapali (OS ayari) + en iyi tur suresi
    { Preferences prefs; prefs.begin("os", true); soundEnabled = prefs.getBool("sound_en", true); prefs.end(); }
    { Preferences prefs; prefs.begin("mode7", true); bestLapTime = prefs.getUInt("best", UINT32_MAX); prefs.end(); }

    // --- SPI baslat (TFT icin, custom pinler) ---
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    // --- Dev tools: SD kart + screenshot ---
    initDevTools(tft);

    // --- TFT ekran baslat ---
    tft.init();
    tft.setRotation(1);          // Landscape (yatay) mod

    // Renklerin mavi olmasını engelleyen düzeltme (RGB mod)
    // ST7735 MADCTL (0x36) komutuna 0xA0 yaz -> RGB sirasi dogru
    tft.startWrite();
    tft.writecommand(0x36);
    tft.writedata(0xA0);
    tft.endWrite();

    setScreenshotMode(SCR_RGB_SWAP);  // Screenshot icin renk kanal swap modu
    tft.fillScreen(TFT_BLACK);
    tft.setTextWrap(false);           // Metin ekrani tasmasin

    // TFT_eSprite: tam ekran sprite (diger calisan oyunlarla ayni mimari)
    canvas.createSprite(SW, SH);
    canvas.setTextWrap(false);
    fb = (uint16_t*)canvas.getPointer(); // Sprite'in ic buffer'ina direkt erisim

    // --- Joystick kalibrasyonu (baslangictaki 0 konumu) ---
    joyCenterX = analogRead(JOY_X);
    joyCenterY = analogRead(JOY_Y);

    // Pist haritasi icin bellek (PSRAM veya internal)
    // PSRAM tercih edilir (16384 byte buyuk), yoksa internal heap
    trackMap = (uint8_t*)heap_caps_malloc(MAP_H * MAP_W, MALLOC_CAP_SPIRAM);
    if (!trackMap) trackMap = (uint8_t*)malloc(MAP_H * MAP_W);
    if (trackMap) memset(trackMap, 0, MAP_H * MAP_W);

    // Pist olustur — harita + checkpoint + direkler
    buildTrack();
    buildCheckpoints();
    buildPosts();

    // Baslangic pozisyonu (oval pistin ust noktasi)
    player.x = MAP_W / 2.0f;   // pistin ust noktasi (virajli halka)
    player.y = 15.0f;
    player.angle = 0.0f; // <--- YOLA PARALEL BAK (onceki gibi asagi degil)
    player.speed = 0;

    // AI — start grid: oyuncunun arkasinda dizili (start'ta dev gorunmesin)
    aiCars[0] = {MAP_W / 2.0f - 8,  15.0f, 0.0f, 0};   // Biraz sol-arka
    aiCars[1] = {MAP_W / 2.0f - 16, 16.0f, 0.0f, 0};  // Daha sol-arka
    aiLap[0] = aiLap[1] = 0;
    aiCheckpoint[0] = 0; aiCheckpoint[1] = 2;          // Farkli CP ile basla (cakisma onle)

    // --- Oyun ilk durumu ---
    state = ST_TITLE;
    drawTitleScreen();           // Baslik ekranini ciz
    lastFrameMs = millis();      // Delta-time baslangic zamani
    fpsStartTime = millis();     // FPS olcum penceresi
}

// ============================================================
//  LOOP
//  Delta-time ile FPS'ten bagimsiz hareket; durum makinesi yonetir
// ============================================================
void loop() {
    // --- Delta-time (dt) hesaplama ---
    // dt = bir onceki frame'den bu yana gecen saniye (min 1ms, max 50ms)
    uint32_t now = millis();
    // FPS60: Frame lock removed — delta-time handles timing
    float dt = (now - lastFrameMs) / 1000.0f;
    dt = constrain(dt, 0.001f, 0.05f); // FPS60: Tighter spike protection (min 1ms, max 50ms)
    lastFrameMs = now;
    animTick++;

    // --- Buton okuma + kenar algilama (basildi aninda 1 kez) ---
    bool btnA = !digitalRead(BTN_A);
    bool btnB = !digitalRead(BTN_B);
    static bool prevA = false, prevB = false;
    bool pressA = (btnA && !prevA);    // Dusen kenar (A)
    bool pressB = (btnB && !prevB);    // Dusen kenar (B)
    prevA = btnA; prevB = btnB;

    // ---- FPS Hesaplama ---- (her 1 saniyede frame sayisi)
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // ---- JOY_SW: Pause toggle (yaris sirasinda duraklat) ----
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {
        if (state == ST_RACING) {
            state = ST_PAUSE;
            playSound(400, 50);
            noTone(BUZZER);            // Motor sesini kes
        }
    }
    prevJoySw = currJoySw;

    // --- Direksiyon (joystick X) — normalize + dead-zone ---
    float jx = (analogRead(JOY_X) - joyCenterX) / 2048.0f; // Ekran aynalanmasi duzeltildigi icin eksiyi (-) kaldirdik
    if (fabsf(jx) < 0.15f) jx = 0;    // Dead-zone (titreme onle)

    // --- Durum makinesi (state machine) ---
    switch (state) {

    // Baslik ekrani — A ile geri sayim baslar, B ile OS menuye don
    case ST_TITLE:
        if (pressB) returnToOS();
        if (pressA) {
            // Reset — oyuncu ve AI'lari baslangic pozisyonuna al
            player.x = MAP_W / 2.0f;
            player.y = 15.0f;
            player.angle = 0.0f;
            player.speed = 0;
            playerLap = 0;
            playerNextCP = 0;
            lastLapTime = 0;

            aiCars[0] = {MAP_W / 2.0f - 8,  15.0f, 0.0f, 0};
            aiCars[1] = {MAP_W / 2.0f - 16, 16.0f, 0.0f, 0};
            aiLap[0] = aiLap[1] = 0;
            aiCheckpoint[0] = 0; aiCheckpoint[1] = 2;

            countdownVal = 3;          // 3,2,1 geri sayim
            stateTimer = now;
            state = ST_COUNTDOWN;
            playSound(440, 100);
        }
        break;

    // Geri sayim — 1 sn arayla 3->2->1->Basla
    case ST_COUNTDOWN:
        if (now - stateTimer >= 1000) {
            stateTimer = now;
            countdownVal--;
            if (countdownVal <= 0) {
                state = ST_RACING;     // Yaris baslar
                lapStartTime = now;
                playSound(880, 200);   // Baslama sesi
            } else {
                playSound(440, 100);   // Sayim sesi
            }
        }

        // Render (statik gorunum) — geri sayim sirasinda da sahne cizilir
        renderMode7();
        renderPosts();
        for (int i = 0; i < NUM_AI; i++) renderAICar(i);
        renderPlayerCar(0);

        // Geri sayim overlay — buyuk sayi karesi ekrana ciz
        {
            int numX = SW/2 - 6, numY = ROAD_H/2 - 6;
            // Renk: 1=kirmizi, 2=sari, 3=yesil
            uint16_t nc = countdownVal == 1 ? RGB(255,80,80) :
                          countdownVal == 2 ? RGB(255,200,0) : RGB(80,255,80);
            for (int py = numY; py < numY + 12; py++)
                for (int px = numX; px < numX + 12; px++)
                    if (px >= 0 && px < SW && py >= 0 && py < ROAD_H)
                        fb[py * SW + px] = nc;
        }

        drawHUD();
        // Dev tools: ekran goruntusu yakala (geri sayim ekraninda SD'e kaydet)
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        break;

    case ST_RACING:
        // --- Oyuncu fizigi --- (delta-time ile tum hareket)
        {
            bool gas = btnA;
            float jy = (analogRead(JOY_Y) - joyCenterY) / 2048.0f;
            if (jy < -0.3f) gas = true; // Joystick yukarisi da gaz

            // Hizlanma/yavaslama — delta-time ile (per-second degerler)
            if (gas) player.speed += ACCEL * dt;                // FPS60: Delta-time
            else player.speed -= DRAG * dt;                     // FPS60: Delta-time

            if (btnB) player.speed -= BRAKE_FORCE * dt;         // FPS60: Delta-time

            // Hiz siniri (0..MAX_SPEED)
            player.speed = constrain(player.speed, 0.0f, MAX_SPEED);

            // Direksiyon (hiza bagli) — yavasca doner, hiz arttikca daha keskin
            // +0.3: dururken bile hafif donus izni
            float steerAmount = jx * STEER_RATE * (player.speed / MAX_SPEED + 0.3f);
            player.angle += steerAmount * dt;                    // FPS60: Delta-time

            // Hareket — aci yonunde ileri (delta-time)
            player.x += cosf(player.angle) * player.speed * dt; // FPS60: Delta-time
            player.y += sinf(player.angle) * player.speed * dt; // FPS60: Delta-time

            // Cimde yavaslama — yol disinda ise hiz dustur
            int mx = (int)player.x; if (mx < 0) mx = 0; else if (mx >= MAP_W) mx = MAP_W - 1;
            int my = (int)player.y; if (my < 0) my = 0; else if (my >= MAP_H) my = MAP_H - 1;
            if (!trackMap[my * MAP_W + mx]) {                    // 0 = cim (yol disi)
                player.speed -= GRASS_SLOW * dt;    // FPS60: Delta-time
                if (player.speed < 0) player.speed = 0;
            }

            // Sinirlar — harita icinde kal
            player.x = constrain(player.x, 1.0f, (float)MAP_W - 2);
            player.y = constrain(player.y, 1.0f, (float)MAP_H - 2);

            // Checkpoint kontrolu — siradaki CP yaricapi icine girince gecildi say
            Checkpoint& cp = checkpoints[playerNextCP];
            float cpDx = player.x - cp.x;
            float cpDy = player.y - cp.y;
            if (cpDx*cpDx + cpDy*cpDy < cp.radius * cp.radius) {
                playerNextCP = (playerNextCP + 1) % NUM_CHECKPOINTS;
                if (playerNextCP == 0) {                        // Tur tamamlandi (0'a donunce)
                    lastLapTime = now - lapStartTime;
                    lapStartTime = now;
                    playerLap++;
                    if (lastLapTime < bestLapTime) {            // Yeni rekor!
                        bestLapTime = lastLapTime;
                        Preferences prefs;                      // NVFlash'a kaydet
                        prefs.begin("mode7", false);
                        prefs.putUInt("best", bestLapTime);
                        prefs.end();
                    }
                    playSound(660, 100);                        // Tur tamamlama sesi

                    if (playerLap >= TOTAL_LAPS) {              // Yaris bitti
                        state = ST_FINISH;
                        stateTimer = now;
                        noTone(BUZZER);
                        playSound(523, 150);                    // Bitis melodisi
                        break; // Finish state gecti, döngüden cik
                    }
                }
            }

            // Motor sesi (Rolanti + Gaz tepkisi)
            // Hiz > 1 u/s iken buzzer'da motor homurdanmasi
            if (soundEnabled && player.speed > 1.0f) { // FPS60: Threshold adjusted for per-second speed
                // Eger gaza basiliyorsa motor bagirir, basik degilse rolanti hiriltisi
                int baseFreq = gas ? 50 : 35; 
                int freq = baseFreq + (int)(player.speed / MAX_SPEED * 60);
                
                // Motorun dumduz bir bip sesi cikarmamasi, homurdanmasi icin kucuk dalgalanma (oscillation)
                freq += (animTick % 3) * 8; 
                
                tone(BUZZER, freq);
            } else {
                // Sadece araba tamamen durunca sesi kes
                noTone(BUZZER);
            }
        }

        // AI guncelle — tum rakiplerin checkpoint takibi + hareketi
        for (int i = 0; i < NUM_AI; i++) updateAI(i, dt);

        // --- Arac-arac carpisma (oyuncu vs AI) ---
        // 4 birimden yakin = itme (iki arac da birbirini iter)
        for (int i = 0; i < NUM_AI; i++) {
            float ddx = player.x - aiCars[i].x;
            float ddy = player.y - aiCars[i].y;
            float d2 = ddx * ddx + ddy * ddy;
            if (d2 < 16.0f) {                       // ~4 birim yaricap
                float d = sqrtf(d2); if (d < 0.001f) d = 0.001f;   // 0'a bolme onle
                float push = (4.0f - d);            // Carpisma siddeti
                player.x += (ddx / d) * push;       // oyuncuyu it
                player.y += (ddy / d) * push;
                aiCars[i].x -= (ddx / d) * push * 0.5f; // rakibi de bir miktar it
                aiCars[i].y -= (ddy / d) * push * 0.5f;
                player.speed *= 0.6f;               // carpinca yavasla
                if (radarTick % 4 == 0) playSound(120, 30);   // Carpisma sesi (seyrek)
            }
        }
        // Carpisma sonrasi sinir kontrolu
        player.x = constrain(player.x, 1.0f, (float)MAP_W - 2);
        player.y = constrain(player.y, 1.0f, (float)MAP_H - 2);

        // --- Render --- (arkadan one cizim sirasi)
        renderMode7();
        renderPosts();
        for (int i = 0; i < NUM_AI; i++) renderAICar(i);
        renderPlayerCar(jx);                        // direksiyon kaymasi ile

        drawHUD();
        // Dev tools: ekran goruntusu yakala (yaris sirasinda SD'e kaydet)
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);

        // OLED radar (her ~10 frame'de bir — I2C gecisi yavas oldugu icin)
        if (++radarTick >= 10) { radarTick = 0; drawRadar(); }
        break;

    case ST_FINISH:
        // A ile basliga don, B ile OS menuye
        if (pressB) { returnToOS(); break; }
        if (pressA) { state = ST_TITLE; drawTitleScreen(); break; }

        // Bitis ekrani (ilk frame'de ciz) — 100ms icinde sadece bir kez
        if (now - stateTimer < 100) {
            canvas.fillSprite(TFT_BLACK);
            
            // Premium Panel — yuvarlatilmis kose + cift cerceve
            canvas.fillRoundRect(15, 12, 130, 108, 5, 0x2104);     // Ic dolgu (koyu)
            canvas.drawRoundRect(15, 12, 130, 108, 5, TFT_RED);    // Dis cerceve (kirmizi)
            canvas.drawRoundRect(16, 13, 128, 106, 4, 0x8000);     // Ikinci cerceve (koyu sari)

            // Baslik "BITIS!" (ortalı, kirmizi, size 2)
            canvas.setTextSize(2);
            canvas.setTextColor(TFT_RED);
            const char* title = "BITIS!";
            int titleW = strlen(title) * 12;
            canvas.setCursor((SW - titleW) / 2, 20);
            canvas.print(title);

            // Pozisyon — tur tamamlayan AI sayisina gore siralamayi hesapla
            int pos = 1;
            for (int i = 0; i < NUM_AI; i++) {
                if (aiLap[i] >= TOTAL_LAPS) pos++;   // AI onde bitirdiyse
            }
            
            // Sira (buyuk, sari)
            canvas.setTextSize(1);
            canvas.setTextColor(TFT_WHITE);
            canvas.setCursor(30, 48);
            canvas.print("Sira:   ");
            canvas.setTextColor(TFT_YELLOW);
            canvas.setTextSize(2);
            canvas.setCursor(75, 42);
            canvas.printf("%d/%d", pos, NUM_AI + 1);

            // En iyi tur (buyuk, yesil)
            canvas.setTextSize(1);
            canvas.setTextColor(TFT_WHITE);
            canvas.setCursor(30, 70);
            canvas.print("En Iyi: ");
            canvas.setTextColor(TFT_GREEN);
            canvas.setTextSize(2);
            canvas.setCursor(75, 64);
            if (bestLapTime != UINT32_MAX)
                canvas.printf("%lu.%01lu", bestLapTime/1000, (bestLapTime%1000)/100);
            else
                canvas.print("--.-");               // Rekor yok

            // Alt menuler
            canvas.setTextSize(1);
            canvas.setTextColor(TFT_WHITE);
            canvas.setCursor(30, 98);
            canvas.print("[A] Tekrar");
            canvas.setTextColor(0xBDF7);            // Acik mavi
            canvas.setCursor(30, 108);
            canvas.print("[B] OS Menu");
            
            // Dev tools: ekran goruntusu yakala (bitis ekraninda SD'e kaydet)
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
        }
        break;

    case ST_PAUSE:
        // Premium PAUSE Menüsü — yesil cerceveli panel
        canvas.fillRoundRect(25, 35, 110, 60, 5, RGB(0, 30, 0));   // Ic dolgu (koyu yesil)
        canvas.drawRoundRect(25, 35, 110, 60, 5, RGB(0, 255, 0));  // Dis cerceve (yesil)
        
        canvas.setTextSize(2);
        canvas.setTextColor(RGB(255, 255, 0));   // Sari "PAUSE"
        canvas.setCursor(50, 42); 
        canvas.print("PAUSE");
        
        // Menu secenekleri (yatay ortali)
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_WHITE);
        const char* txtA = "[A] Devam Et";
        canvas.setCursor((SW - strlen(txtA)*6) / 2, 65);   // Ortala
        canvas.print(txtA);
        
        canvas.setTextColor(0xBDF7);                       // Acik mavi
        const char* txtB = "[B] OS Menu";
        canvas.setCursor((SW - strlen(txtB)*6) / 2, 78);   // Ortala
        canvas.print(txtB);
        
        // Dev tools: ekran goruntusu yakala (pause ekraninda SD'e kaydet)
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        
        if (pressA) {                  // Devam et — yarisa don
            playSound(800, 50);
            delay(100); // FPS60: Debounce minimizasyonu (200ms -> 100ms)
            state = ST_RACING;
            lastFrameMs = millis();    // Delta-time'i sifirla (bekleme suresi saymasin)
        }
        if (pressB) {                  // OS menuye don
            playSound(400, 50);
            delay(100); // FPS60: Debounce minimizasyonu (200ms -> 100ms)
            returnToOS();
        }
        break;
    }
}
