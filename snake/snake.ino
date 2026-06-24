// ============================================================
//  EMÄ°R OS â€” MODERN SNAKE (YILAN)
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanlÄ± Ã§ift tamponlama (Flicker-Free)
//
//  Kontroller:
//    JOY_X/Y -> YÄ±lanÄ±n yÃ¶nÃ¼nÃ¼ belirle
//    BTN_A   -> Oyunu baÅŸlat / Yeniden baÅŸla
//    BTN_C   -> OS Launcher'a DÃ¶n (Rezerve)
//    Buzzer  -> Ses efektleri
// ============================================================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
#include <Preferences.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"

// ============ Ekran BoyutlarÄ± (Landscape) ============
#define SCR_W  160
#define SCR_H  128

// ============ Izgara (Grid) Sabitleri ============
#define GRID       8       // Her hÃ¼cre 8x8 piksel
#define COLS       20      // 160 / 8 = 20 sÃ¼tun
#define ROWS       14      // 14 satÄ±r oyun alanÄ±
#define OFFSET_Y   11      // Oyun alanÄ± Y baÅŸlangÄ±cÄ± (HUD altÄ±)
#define HUD_H      10      // HUD yÃ¼ksekliÄŸi

// ============ YÄ±lan Sabitleri ============
#define MAX_SNAKE  200     // Maksimum yÄ±lan uzunluÄŸu
#define INIT_LEN   5       // BaÅŸlangÄ±Ã§ uzunluÄŸu
#define BASE_SPEED 150     // BaÅŸlangÄ±Ã§ hareket aralÄ±ÄŸÄ± (ms)
#define MIN_SPEED  55      // Minimum hareket aralÄ±ÄŸÄ± (ms)
#define SPEED_DEC  2       // Her yem iÃ§in hÄ±z artÄ±ÅŸÄ± (ms azalmasÄ±)

// ============ YÃ¶n Sabitleri ============
#define DIR_UP    0
#define DIR_RIGHT 1
#define DIR_DOWN  2
#define DIR_LEFT  3

// ============ ParÃ§acÄ±k Sabitleri ============
#define MAX_PARTICLES 12

// ============ Genel Sabitler ============
#define JOY_THRESHOLD 500   // Joystick sapma eÅŸiÄŸi
#define TARGET_FPS    60 // FPS60: 30â†’60, double frame rate for smoother rendering
#define FRAME_MS      (1000 / TARGET_FPS)

// ============ Ã–zel Renkler (RGB565) ============
// Arka plan (checkerboard)
#define COL_BG_A      0x0000  // Siyah
#define COL_BG_B      0x0841  // Ã‡ok koyu gri

// YÄ±lan
#define COL_HEAD      0x07C0  // Parlak yeÅŸil kafa
#define COL_HEAD_DK   0x0580  // Koyu yeÅŸil kafa kenarlÄ±k
#define COL_BODY_A    0x04A0  // GÃ¶vde renk A
#define COL_BODY_B    0x0560  // GÃ¶vde renk B (biraz aÃ§Ä±k)
#define COL_BODY_BRD  0x0320  // GÃ¶vde kenarlÄ±k

// Yem (elma)
#define COL_FOOD      0xF800  // KÄ±rmÄ±zÄ±
#define COL_FOOD_DIM  0xC000  // Koyu kÄ±rmÄ±zÄ± (pulse karanlÄ±k faz)
#define COL_FOOD_HL   0xFBE0  // Turuncu parlaklÄ±k
#define COL_FOOD_STEM 0x04A0  // Sap rengi (yeÅŸil)
#define COL_FOOD_LEAF 0x07E0  // Yaprak rengi

// HUD
#define COL_HUD_LINE  0x2104
#define COL_HUD_TEXT  0xBDF7

// Oyun alanÄ± kenarlÄ±k
#define COL_BORDER    0x2945
#define COL_RED_DARK  0x8000  // Koyu kırmızı (game over panel iç çerçeve)

// ============ Nesneler ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Oyun DurumlarÄ± ============
enum GameState { MENU, PLAYING, GAMEOVER, PAUSE };
GameState state = MENU;

// ============ YÄ±lan Verileri ============
int snakeX[MAX_SNAKE], snakeY[MAX_SNAKE];
int snakeLen;
int dir, nextDir;    // Mevcut ve tamponlanmÄ±ÅŸ yÃ¶n

// ============ Yem ============
int foodX, foodY;

// ============ ParÃ§acÄ±k Efekti ============
struct Particle {
    float x, y, vx, vy;
    uint16_t color;
    float life;  // FPS60: intâ†’float for delta-time based lifetime (seconds)
    bool active;
};
Particle particles[MAX_PARTICLES];

// ============ Skor Popup ============
int popupX, popupY;
float popupTimer;    // FPS60: intâ†’float, delta-time tabanlÄ± geri sayÄ±m (30FPS eÅŸdeÄŸer frame)

// ============ Skor & Oyun ============
int score, highScore;
bool newRecord;

// ============ V2.1: Ses Ayarı ============
bool soundEnabled = true;
// playSound — GameBase.h osPlaySound wrapper (eski API uyumu)
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// ============ Zamanlama ============
unsigned long lastFrameMs;
unsigned long lastMoveMs;
unsigned long gameOverMs;

// ============ FPS SayacÄ± ============
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;

// ============ Joystick ============
int joyCenterX, joyCenterY;

// ============ Buton ============
int prevBtnA = HIGH;

// ==========================================
//  OS'a Dönüş Fonksiyonu — GameBase.h wrapper
// ==========================================
void returnToOS() {
    osReturnToOS(tft);
}

// ==========================================
//  YÄ±lanÄ±n Ãœzerinde Mi? (Yem kontrolÃ¼ iÃ§in)
// ==========================================
bool isOnSnake(int gx, int gy) {
    for (int i = 0; i < snakeLen; i++)
        if (snakeX[i] == gx && snakeY[i] == gy) return true;
    return false;
}

// ==========================================
//  Yemi Rastgele Konuma YerleÅŸtir
// ==========================================
void spawnFood() {
    if (snakeLen >= COLS * ROWS - 1) return;  // Izgara dolu
    int tries = 0;
    do {
        foodX = random(0, COLS);
        foodY = random(0, ROWS);
        tries++;
    } while (isOnSnake(foodX, foodY) && tries < 1000);
}

// ==========================================
//  YÄ±lanÄ± BaÅŸlat
// ==========================================
void initSnake() {
    snakeLen = INIT_LEN;
    dir = DIR_RIGHT;
    nextDir = DIR_RIGHT;
    // YÄ±lanÄ± ortada baÅŸlat, saÄŸa bakacak ÅŸekilde
    for (int i = 0; i < snakeLen; i++) {
        snakeX[i] = COLS / 2 - i;
        snakeY[i] = ROWS / 2;
    }
}

// ==========================================
//  ParÃ§acÄ±klarÄ± Temizle
// ==========================================
void clearParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++)
        particles[i].active = false;
}

// ==========================================
//  ParÃ§acÄ±k OluÅŸtur
// ==========================================
void spawnParticles(float px, float py, uint16_t color, int count) {
    for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
        if (!particles[i].active) {
            particles[i].x = px;
            particles[i].y = py;
            // FPS60: hÄ±zlar px/frame@30FPS â†’ px/s cinsine Ã§evrildi (Ã—30)
            particles[i].vx = random(-75, 73);       // was random(-25,25)/10â†’-2.5..2.4 px/fr *30 = -75..72 px/s
            particles[i].vy = random(-75, 28);       // was random(-25,10)/10â†’-2.5..0.9 px/fr *30 = -75..27 px/s
            particles[i].color = color;
            particles[i].life = random(8, 16) / 30.0f; // FPS60: frameâ†’saniye (8-16fr / 30FPS = 0.27-0.53s)
            particles[i].active = true;
            count--;
        }
    }
}

// ==========================================
//  ParÃ§acÄ±k GÃ¼ncelle
// ==========================================
void updateParticles(float dt) { // FPS60: delta-time parametresi eklendi
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;
        particles[i].x += particles[i].vx * dt;       // FPS60: dt ile Ã¶lÃ§ekle
        particles[i].y += particles[i].vy * dt;       // FPS60: dt ile Ã¶lÃ§ekle
        particles[i].vy += 3.6f * dt;                 // FPS60: yerÃ§ekimi 0.12Ã—30=3.6 px/sÂ², dt ile Ã¶lÃ§ekle
        particles[i].life -= dt;                      // FPS60: saniye cinsinden geri sayÄ±m
        if (particles[i].life <= 0.0f ||
            particles[i].x < 0 || particles[i].x >= SCR_W ||
            particles[i].y >= SCR_H) {
            particles[i].active = false;
        }
    }
}

// ==========================================
//  ParÃ§acÄ±k Ã‡iz
// ==========================================
void drawParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;
        int px = (int)particles[i].x;
        int py = (int)particles[i].y;
        if (px >= 0 && px < SCR_W - 1 && py >= 0 && py < SCR_H - 1) {
            canvas.fillRect(px, py, 2, 2, particles[i].color);
        }
    }
}

// ==========================================
//  Oyunu SÄ±fÄ±rla
// ==========================================
void resetGame() {
    initSnake();
    spawnFood();
    clearParticles();
    score = 0;
    newRecord = false;
    popupTimer = 0.0f; // FPS60: float baÅŸlatma
    state = PLAYING;
    lastMoveMs = millis();
}

// ==========================================
//  Ã‡izim: Checkerboard Arka Plan
// ==========================================
void drawBackground() {
    // HUD alanÄ± siyah
    canvas.fillRect(0, 0, SCR_W, OFFSET_Y, COL_BG_A);
    // Oyun alanÄ± (checkerboard)
    canvas.fillRect(0, OFFSET_Y, SCR_W, ROWS * GRID, COL_BG_A);
    for (int gy = 0; gy < ROWS; gy++) {
        for (int gx = 0; gx < COLS; gx++) {
            if ((gx + gy) % 2 == 1) {
                canvas.fillRect(gx * GRID, OFFSET_Y + gy * GRID, GRID, GRID, COL_BG_B);
            }
        }
    }
    // Alt boÅŸluk
    int bottomY = OFFSET_Y + ROWS * GRID;
    canvas.fillRect(0, bottomY, SCR_W, SCR_H - bottomY, COL_BG_A);
    // Oyun alanÄ± kenarlÄ±ÄŸÄ±
    canvas.drawRect(0, OFFSET_Y - 1, SCR_W, ROWS * GRID + 2, COL_BORDER);
}

// ==========================================
//  Ã‡izim: YÄ±lan GÃ¶zleri
// ==========================================
void drawEyes(int px, int py, int d) {
    int ex1, ey1, ex2, ey2;
    switch (d) {
        case DIR_UP:
            ex1 = px + 1; ey1 = py + 1;
            ex2 = px + 5; ey2 = py + 1;
            break;
        case DIR_RIGHT:
            ex1 = px + 5; ey1 = py + 1;
            ex2 = px + 5; ey2 = py + 5;
            break;
        case DIR_DOWN:
            ex1 = px + 1; ey1 = py + 5;
            ex2 = px + 5; ey2 = py + 5;
            break;
        case DIR_LEFT:
            ex1 = px + 1; ey1 = py + 1;
            ex2 = px + 1; ey2 = py + 5;
            break;
        default:
            return;
    }
    canvas.fillRect(ex1, ey1, 2, 2, TFT_WHITE);
    canvas.fillRect(ex2, ey2, 2, 2, TFT_WHITE);
    // GÃ¶z bebekleri (1px siyah nokta, yÃ¶ne gÃ¶re)
    int ox = (d == DIR_RIGHT) ? 1 : (d == DIR_LEFT) ? 0 : 0;
    int oy = (d == DIR_DOWN) ? 1 : (d == DIR_UP) ? 0 : 0;
    canvas.drawPixel(ex1 + ox, ey1 + oy, TFT_BLACK);
    canvas.drawPixel(ex2 + ox, ey2 + oy, TFT_BLACK);
}

// ==========================================
//  Ã‡izim: YÄ±lan
// ==========================================
void drawSnake() {
    // Kuyruktan kafaya doÄŸru Ã§iz (kafa en Ã¼stte)
    for (int i = snakeLen - 1; i >= 0; i--) {
        int px = snakeX[i] * GRID;
        int py = OFFSET_Y + snakeY[i] * GRID;

        if (i == 0) {
            // === KAFA ===
            canvas.fillRect(px + 1, py + 1, GRID - 2, GRID - 2, COL_HEAD);
            canvas.drawRect(px, py, GRID, GRID, COL_HEAD_DK);
            drawEyes(px, py, dir);
        } else {
            // === GÃ–VDE ===
            uint16_t col = (i % 2 == 0) ? COL_BODY_A : COL_BODY_B;
            canvas.fillRect(px + 1, py + 1, GRID - 2, GRID - 2, col);
            canvas.drawRect(px, py, GRID, GRID, COL_BODY_BRD);
        }
    }
}

// ==========================================
//  Ã‡izim: Yem (Elma)
// ==========================================
void drawFood() {
    int cx = foodX * GRID + GRID / 2;
    int cy = OFFSET_Y + foodY * GRID + GRID / 2;

    // NabÄ±z efekti (parlak â†” koyu geÃ§iÅŸ)
    bool bright = (millis() / 300) % 2 == 0;
    uint16_t mainCol = bright ? COL_FOOD : COL_FOOD_DIM;

    // Elma gÃ¶vdesi (daire)
    canvas.fillCircle(cx, cy, 3, mainCol);
    // ParlaklÄ±k noktasÄ± (sol Ã¼st)
    canvas.drawPixel(cx - 1, cy - 1, COL_FOOD_HL);
    // Sap
    canvas.drawFastVLine(cx, cy - 4, 2, COL_FOOD_STEM);
    // Yaprak
    canvas.drawPixel(cx + 1, cy - 4, COL_FOOD_LEAF);
    canvas.drawPixel(cx + 2, cy - 3, COL_FOOD_LEAF);
}

// ==========================================
//  Ã‡izim: Skor Popup (+10 yÃ¼zen metin)
// ==========================================
void drawPopup(float dt) { // FPS60: delta-time parametresi eklendi
    if (popupTimer <= 0.0f) return;
    int floatY = popupY - (int)(15.0f - popupTimer);  // FPS60: popupTimer 30FPS eÅŸdeÄŸer frame sayar
    if (floatY < HUD_H) floatY = HUD_H;
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(popupX - 6, floatY);
    canvas.print("+10");
    popupTimer -= 30.0f * dt; // FPS60: 30FPS eÅŸdeÄŸer azaltma (dt=1/30 iken 1 azalÄ±r)
}

// ==========================================
//  Ã‡izim: HUD (Skor & Rekor)
// ==========================================
void drawHUD() {
    canvas.setTextSize(1);

    // Skor (sol taraf)
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(2, 1);
    canvas.print("SKOR:");
    canvas.setTextColor(TFT_WHITE);
    canvas.print(score);

    // FPS SayacÄ± (saÄŸ taraf)
    char fpsStr[10];
    sprintf(fpsStr, "FPS:%d", currentFPS);
    int fpsWidth = strlen(fpsStr) * 6; // Her harf/rakam 6 piksel
    
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(SCR_W - fpsWidth - 2, 1);
    canvas.print("FPS:");
    canvas.setTextColor(TFT_GREEN);
    canvas.print(currentFPS);

    // AyÄ±rÄ±cÄ± Ã§izgi
    canvas.drawFastHLine(0, HUD_H, SCR_W, COL_HUD_LINE);
}

// ==========================================
//  Ekran: Ana MenÃ¼
// ==========================================
void drawMenu() {
    canvas.fillSprite(COL_BG_A);

    // BaÅŸlÄ±k gÃ¶lgesi
    canvas.setTextSize(2);
    canvas.setTextColor(COL_BODY_BRD);
    canvas.setCursor(51, 9);
    canvas.print("SNAKE");
    // BaÅŸlÄ±k
    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(50, 8);
    canvas.print("SNAKE");

    // Alt baÅŸlÄ±k
    canvas.setTextSize(1);
    canvas.setTextColor(COL_BODY_A);
    canvas.setCursor(44, 28);

    // Dekoratif yÄ±lan (statik)
    int demoY = 48;
    // GÃ¶vde segmentleri
    for (int i = 6; i >= 1; i--) {
        int dx = 30 + i * 10;
        uint16_t col = (i % 2 == 0) ? COL_BODY_A : COL_BODY_B;
        canvas.fillRect(dx + 1, demoY + 1, GRID - 2, GRID - 2, col);
        canvas.drawRect(dx, demoY, GRID, GRID, COL_BODY_BRD);
    }
    // Kafa
    int headX = 100;
    canvas.fillRect(headX + 1, demoY + 1, GRID - 2, GRID - 2, COL_HEAD);
    canvas.drawRect(headX, demoY, GRID, GRID, COL_HEAD_DK);
    // GÃ¶zler (saÄŸa bakÄ±yor)
    canvas.fillRect(headX + 5, demoY + 1, 2, 2, TFT_WHITE);
    canvas.fillRect(headX + 5, demoY + 5, 2, 2, TFT_WHITE);

    // Dekoratif elma
    int appleX = 118;
    canvas.fillCircle(appleX, demoY + 4, 3, COL_FOOD);
    canvas.drawPixel(appleX - 1, demoY + 3, COL_FOOD_HL);
    canvas.drawFastVLine(appleX, demoY, 2, COL_FOOD_STEM);
    canvas.drawPixel(appleX + 1, demoY, COL_FOOD_LEAF);

    // Alt Menuler (Iki kolon, derli toplu)
    canvas.setTextSize(1);
    
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(10, 95);
    canvas.print("[A] Basla");

    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(85, 95);
    canvas.print("[B] OS Menu");

    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(10, 110);
    canvas.print("[JOY] Yon");

    // En yÃ¼ksek skor
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(87, 110);
    canvas.printf("Rekor: %d", highScore);

    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}


// ==========================================
//  Ekran: Oyun Bitti
// ==========================================
void drawGameOver() {
    canvas.fillSprite(COL_BG_A);

    // Checkerboard arkaplan (hafif)
    for (int gy = 0; gy < 16; gy++)
        for (int gx = 0; gx < 20; gx++)
            if ((gx + gy) % 2 == 1)
                canvas.fillRect(gx * 8, gy * 8, 8, 8, COL_BG_B);

    // Panel
    canvas.fillRoundRect(15, 6, 130, 120, 5, COL_HUD_LINE);
    canvas.drawRoundRect(15, 6, 130, 120, 5, TFT_RED);
    canvas.drawRoundRect(16, 7, 128, 118, 4, COL_RED_DARK);

    // BaÅŸlÄ±k
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    canvas.setCursor(22, 12);
    canvas.print("OYUN BITTI");

    // Skor
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 40);
    canvas.print("Skor:  ");
    canvas.setTextColor(TFT_YELLOW);
    canvas.setTextSize(2);
    canvas.setCursor(75, 36);
    canvas.print(score);

    // Yem sayÄ±sÄ± (bilgi)
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 60);
    canvas.print("Yem:   ");
    canvas.setTextColor(COL_FOOD);
    canvas.setTextSize(2);
    canvas.setCursor(75, 56);
    canvas.print(score / 10);

    // Rekor
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 80);
    canvas.print("Rekor: ");
    canvas.setTextColor(TFT_GREEN);
    canvas.setTextSize(2);
    canvas.setCursor(75, 76);
    canvas.print(highScore);

    // Yeni rekor bildirimi
    if (newRecord) {
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_MAGENTA);
        canvas.setCursor(47, 94);
        canvas.print("YENI REKOR!");
    }

    canvas.setTextSize(1);
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(30, 104);
    canvas.print("[A] Tekrar Oyna");
    
    canvas.setCursor(30, 114);
    canvas.print("[B] OS Menu");

    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}

// ==========================================
//  SETUP
// ==========================================
void setup() {
    // OLED Kapatma ve Buzzer Susturma (HÄ±zlÄ± BaÅŸlatma iÃ§in)
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
    Wire.begin(8, 9);
    Wire.beginTransmission(0x3C);
    Wire.write(0x00);
    Wire.write(0xAE); // Display OFF
    Wire.endTransmission();

    // GÃœVENLÄ°K: Elektrik kesilirse her zaman OS'tan baÅŸla
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    // Pin ayarlarÄ±
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP);
    pinMode(BTN_D, INPUT_PULLUP);
    pinMode(JOY_SW, INPUT_PULLUP);
    pinMode(BUZZER, OUTPUT);

    // --- V2.1: NVS'ten ses ayarÄ±nÄ± oku ---
    { Preferences prefs; prefs.begin("os", true); soundEnabled = prefs.getBool("sound_en", true); prefs.end(); }

    // SPI baÅŸlat
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    initDevTools(tft);

    // Ekran baÅŸlatma
    tft.init();
    tft.setRotation(1);   // Landscape: 160x128
    // TFT donanimini RGB moduna gecir (Snake standart RGB565 hex sabitler kullanir)
    // TFT_BGR_ORDER config'ini sadece bu oyunda devre disi birak
    tft.startWrite();
    tft.writecommand(0x36);  // MADCTL
    tft.writedata(0xA0);     // MY|MV, BGR bit kapali (RGB modu)
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    // Sprite tamponu (Ã§ift tamponlama / flicker-free)
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // Joystick merkez kalibrasyonu
    joyCenterX = analogRead(JOY_X);
    joyCenterY = analogRead(JOY_Y);

    // Rastgele tohum
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    clearParticles();
    state = MENU;
    lastFrameMs = millis();
    fpsStartTime = millis();
}

// ==========================================
//  LOOP
// ==========================================
void loop() {
    // ---- JOY_SW: Pause toggle ----
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {
        if (state == PLAYING) {
            state = PAUSE;
            playSound(400, 50);
        }
    }
    prevJoySw = currJoySw;

    // ---- Frame hÄ±z kontrolÃ¼ ----
    unsigned long now = millis();
    if (now - lastFrameMs < FRAME_MS) return;
    // FPS60: Delta-time hesaplama (saniye, lag-spike korumalÄ±)
    float dt = (now - lastFrameMs) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f; // FPS60: dt > 50ms ise kÄ±rp (spiral of death korumasÄ±)
    lastFrameMs = now;

    // FPS Hesaplama
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // ---- BTN_A: Kenar tespiti ----
    int btnA = digitalRead(BTN_A);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    prevBtnA = btnA;

    // ---- Durum Makinesi ----
    switch (state) {

        // ======================================
        //  ANA MENÃœ
        // ======================================
        case MENU:
            drawMenu();
            if (pressA) {
                playSound(880, 50);
                resetGame();
            }
            if (!digitalRead(BTN_B)) {
                delay(200);
                returnToOS();
            }
            break;

        // ======================================
        //  OYUN
        // ======================================
        case PLAYING: {

            // === Joystick â†’ YÃ¶n Belirleme ===
            int jx = analogRead(JOY_X) - joyCenterX;
            int jy = analogRead(JOY_Y) - joyCenterY;

            // BÃ¼yÃ¼k ekseni tercih et (Ã§apraz itmeleri engelle)
            if (abs(jx) > abs(jy)) {
                if (jx > JOY_THRESHOLD && dir != DIR_LEFT)  nextDir = DIR_RIGHT;
                if (jx < -JOY_THRESHOLD && dir != DIR_RIGHT) nextDir = DIR_LEFT;
            } else {
                if (jy > JOY_THRESHOLD && dir != DIR_UP)    nextDir = DIR_DOWN;
                if (jy < -JOY_THRESHOLD && dir != DIR_DOWN)  nextDir = DIR_UP;
            }

            // === Hareket ZamanlamasÄ± ===
            unsigned long moveInterval = (unsigned long)BASE_SPEED - (unsigned long)(score / 10) * SPEED_DEC;
            if (moveInterval < MIN_SPEED) moveInterval = MIN_SPEED;

            if (now - lastMoveMs >= moveInterval) {
                lastMoveMs = now;

                // YÃ¶nÃ¼ uygula
                dir = nextDir;

                // Yeni kafa pozisyonu hesapla
                int newX = snakeX[0];
                int newY = snakeY[0];
                switch (dir) {
                    case DIR_UP:    newY--; break;
                    case DIR_RIGHT: newX++; break;
                    case DIR_DOWN:  newY++; break;
                    case DIR_LEFT:  newX--; break;
                }

                // --- Duvar Ã§arpÄ±ÅŸmasÄ± ---
                if (newX < 0 || newX >= COLS || newY < 0 || newY >= ROWS) {
                    state = GAMEOVER;
                    newRecord = (score > highScore);
                    if (newRecord) highScore = score;
                    gameOverMs = now;
                    // --- V2.1: YÃ¼ksek skoru NVS'e kaydet ---
                    { Preferences prefs; prefs.begin("os", false);
                      int32_t hs = prefs.getInt("hs_snake", 0);
                      if (score > hs) prefs.putInt("hs_snake", score);
                      prefs.end(); }
                    // Ã–lÃ¼m parÃ§acÄ±klarÄ±
                    spawnParticles(
                        snakeX[0] * GRID + GRID / 2,
                        OFFSET_Y + snakeY[0] * GRID + GRID / 2,
                        COL_HEAD, 6);
                    playSound(200, 200);
                    delay(120);
                    playSound(100, 300);
                    break;
                }

                // Yemi kontrol et (Ã§arpÄ±ÅŸma Ã¶ncesi)
                bool ate = (newX == foodX && newY == foodY);

                // --- Kendine Ã§arpÄ±ÅŸma ---
                // Yem yenmemiÅŸse kuyruk kayacak, o yÃ¼zden kuyruÄŸu kontrol etme
                int checkLen = ate ? snakeLen : snakeLen - 1;
                bool selfHit = false;
                for (int i = 0; i < checkLen; i++) {
                    if (snakeX[i] == newX && snakeY[i] == newY) {
                        selfHit = true;
                        break;
                    }
                }
                if (selfHit) {
                    state = GAMEOVER;
                    newRecord = (score > highScore);
                    if (newRecord) highScore = score;
                    gameOverMs = now;
                    // --- V2.1: YÃ¼ksek skoru NVS'e kaydet ---
                    { Preferences prefs; prefs.begin("os", false);
                      int32_t hs = prefs.getInt("hs_snake", 0);
                      if (score > hs) prefs.putInt("hs_snake", score);
                      prefs.end(); }
                    spawnParticles(
                        newX * GRID + GRID / 2,
                        OFFSET_Y + newY * GRID + GRID / 2,
                        COL_HEAD, 6);
                    playSound(200, 200);
                    delay(120);
                    playSound(100, 300);
                    break;
                }

                // === YÄ±lanÄ± Hareket Ettir ===
                if (ate && snakeLen < MAX_SNAKE) {
                    snakeLen++;  // BÃ¼yÃ¼ (kuyruk kayma)
                }
                // TÃ¼m segmentleri bir geri kaydÄ±r
                for (int i = snakeLen - 1; i > 0; i--) {
                    snakeX[i] = snakeX[i - 1];
                    snakeY[i] = snakeY[i - 1];
                }
                // Yeni kafa pozisyonu
                snakeX[0] = newX;
                snakeY[0] = newY;

                // === Yem Yendi ===
                if (ate) {
                    score += 10;
                    // Skor popup
                    popupX = foodX * GRID + GRID / 2;
                    popupY = OFFSET_Y + foodY * GRID;
                    popupTimer = 15.0f; // FPS60: float baÅŸlatma
                    // ParÃ§acÄ±k efekti
                    spawnParticles(
                        foodX * GRID + GRID / 2,
                        OFFSET_Y + foodY * GRID + GRID / 2,
                        COL_FOOD, 5);
                    // Yeni yem
                    spawnFood();
                    // NeÅŸeli ses
                    playSound(1047, 40);
                } else {
                    // Ã‡ok hafif hareket tÄ±kÄ±rtÄ±sÄ±
                    playSound(2500, 3);
                }
            }

            // === ParÃ§acÄ±k GÃ¼ncelle ===
            updateParticles(dt); // FPS60: delta-time geÃ§irildi

            // ============================
            //  SAHNE Ã‡Ä°ZÄ°MÄ° (Sprite'a)
            // ============================
            drawBackground();
            drawFood();
            drawSnake();
            drawParticles();
            drawPopup(dt); // FPS60: delta-time geÃ§irildi
            drawHUD();

            // HÄ±z gÃ¶stergesi (opsiyonel â€” alt kenarda ince Ã§ubuk)
            int speedPct = 100 - (int)((moveInterval - MIN_SPEED) * 100 / (BASE_SPEED - MIN_SPEED));
            if (speedPct < 0) speedPct = 0;
            if (speedPct > 100) speedPct = 100;
            int barW = speedPct * (SCR_W - 4) / 100;
            int barY = OFFSET_Y + ROWS * GRID + 1;
            canvas.fillRect(2, barY, barW, 2, COL_HEAD);
            canvas.drawRect(2, barY, SCR_W - 4, 2, COL_BORDER);

            // Sprite'Ä± ekrana bas
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  OYUN BÄ°TTÄ°
        // ======================================
        case GAMEOVER:
            drawGameOver();
            if (pressA && now - gameOverMs > 600) {
                playSound(880, 50);
                resetGame();
            }
            if (!digitalRead(BTN_B) && now - gameOverMs > 600) {
                returnToOS();
            }
            break;

        // ======================================
        //  PAUSE MENÃœSÃœ
        // ======================================
        case PAUSE:
            // Ã‡izime devam et ama mantÄ±ÄŸÄ± dondur
            drawBackground();
            drawFood();
            drawSnake();
            drawParticles();
            drawHUD();
            
            // Ãœzerine karartma ve menÃ¼
            canvas.fillRect(30, 36, 100, 56, canvas.color565(10, 10, 15));
            canvas.drawRect(30, 36, 100, 56, TFT_GREEN);
            
            canvas.setTextSize(2);
            canvas.setTextColor(TFT_YELLOW);
            canvas.setCursor(50, 42); canvas.print("PAUSE");
            
            canvas.setTextSize(1);
            canvas.setTextColor(TFT_WHITE);
            canvas.setCursor(44, 64); canvas.print("[A] Devam Et");
            canvas.setTextColor(canvas.color565(180, 180, 180));
            canvas.setCursor(47, 78); canvas.print("[B] OS Menu");
            
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            
            if (!digitalRead(BTN_A)) {
                playSound(800, 50);
                delay(200);
                state = PLAYING;
                lastMoveMs = millis();
            }
            if (!digitalRead(BTN_B)) {
                playSound(400, 50);
                delay(200);
                returnToOS();
            }
            break;
    }
}



