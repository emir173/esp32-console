// ============================================================
//  EMİR OS — MODERN SNAKE (YILAN)
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanlı çift tamponlama (Flicker-Free)
//
//  Kontroller:
//    JOY_X/Y -> Yılanın yönünü belirle
//    BTN_A   -> Oyunu başlat / Yeniden başla
//    BTN_C   -> OS Launcher'a Dön (Rezerve)
//    Buzzer  -> Ses efektleri
// ============================================================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_ota_ops.h>
#include <Preferences.h>

// ============ Pin Tanımları ============
#define JOY_X    1
#define JOY_Y    2
#define BTN_A    3
#define BTN_B    21
#define BTN_C    4      // OS'a dönüş (her zaman rezerve)
#define BTN_D    6
#define BUZZER   5
#define JOY_SW   18

// ============ SPI Pinleri ============
#define SPI_SCK  12
#define SPI_MOSI 11
#define SPI_MISO 42

// ============ Ekran Boyutları (Landscape) ============
#define SCR_W  160
#define SCR_H  128

// ============ Izgara (Grid) Sabitleri ============
#define GRID       8       // Her hücre 8x8 piksel
#define COLS       20      // 160 / 8 = 20 sütun
#define ROWS       14      // 14 satır oyun alanı
#define OFFSET_Y   11      // Oyun alanı Y başlangıcı (HUD altı)
#define HUD_H      10      // HUD yüksekliği

// ============ Yılan Sabitleri ============
#define MAX_SNAKE  200     // Maksimum yılan uzunluğu
#define INIT_LEN   5       // Başlangıç uzunluğu
#define BASE_SPEED 150     // Başlangıç hareket aralığı (ms)
#define MIN_SPEED  55      // Minimum hareket aralığı (ms)
#define SPEED_DEC  2       // Her yem için hız artışı (ms azalması)

// ============ Yön Sabitleri ============
#define DIR_UP    0
#define DIR_RIGHT 1
#define DIR_DOWN  2
#define DIR_LEFT  3

// ============ Parçacık Sabitleri ============
#define MAX_PARTICLES 12

// ============ Genel Sabitler ============
#define JOY_THRESHOLD 500   // Joystick sapma eşiği
#define TARGET_FPS    30
#define FRAME_MS      (1000 / TARGET_FPS)

// ============ Özel Renkler (RGB565) ============
// Arka plan (checkerboard)
#define COL_BG_A      0x0000  // Siyah
#define COL_BG_B      0x0841  // Çok koyu gri

// Yılan
#define COL_HEAD      0x07C0  // Parlak yeşil kafa
#define COL_HEAD_DK   0x0580  // Koyu yeşil kafa kenarlık
#define COL_BODY_A    0x04A0  // Gövde renk A
#define COL_BODY_B    0x0560  // Gövde renk B (biraz açık)
#define COL_BODY_BRD  0x0320  // Gövde kenarlık

// Yem (elma)
#define COL_FOOD      0xF800  // Kırmızı
#define COL_FOOD_DIM  0xC000  // Koyu kırmızı (pulse karanlık faz)
#define COL_FOOD_HL   0xFBE0  // Turuncu parlaklık
#define COL_FOOD_STEM 0x04A0  // Sap rengi (yeşil)
#define COL_FOOD_LEAF 0x07E0  // Yaprak rengi

// HUD
#define COL_HUD_LINE  0x2104
#define COL_HUD_TEXT  0xBDF7

// Oyun alanı kenarlık
#define COL_BORDER    0x2945

// ============ Nesneler ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Oyun Durumları ============
enum GameState { MENU, PLAYING, GAMEOVER, PAUSE };
GameState state = MENU;

// ============ Yılan Verileri ============
int snakeX[MAX_SNAKE], snakeY[MAX_SNAKE];
int snakeLen;
int dir, nextDir;    // Mevcut ve tamponlanmış yön

// ============ Yem ============
int foodX, foodY;

// ============ Parçacık Efekti ============
struct Particle {
    float x, y, vx, vy;
    uint16_t color;
    int life;
    bool active;
};
Particle particles[MAX_PARTICLES];

// ============ Skor Popup ============
int popupX, popupY;
int popupTimer;      // Kalan kare sayısı

// ============ Skor & Oyun ============
int score, highScore;
bool newRecord;

// ============ V2.1: Ses Ayarı ============
bool soundEnabled = true;
void playSound(uint16_t freq, uint32_t dur) {
    if (soundEnabled) { tone(BUZZER, freq, dur); }
}

// ============ Zamanlama ============
unsigned long lastFrameMs;
unsigned long lastMoveMs;
unsigned long gameOverMs;

// ============ Joystick ============
int joyCenterX, joyCenterY;

// ============ Buton ============
int prevBtnA = HIGH;

// ==========================================
//  OS'a Dönüş Fonksiyonu
// ==========================================
void returnToOS() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 60);
    tft.print("Ana Menuye Donuluyor...");
    delay(500);
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }
    ESP.restart();
}

// ==========================================
//  Yılanın Üzerinde Mi? (Yem kontrolü için)
// ==========================================
bool isOnSnake(int gx, int gy) {
    for (int i = 0; i < snakeLen; i++)
        if (snakeX[i] == gx && snakeY[i] == gy) return true;
    return false;
}

// ==========================================
//  Yemi Rastgele Konuma Yerleştir
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
//  Yılanı Başlat
// ==========================================
void initSnake() {
    snakeLen = INIT_LEN;
    dir = DIR_RIGHT;
    nextDir = DIR_RIGHT;
    // Yılanı ortada başlat, sağa bakacak şekilde
    for (int i = 0; i < snakeLen; i++) {
        snakeX[i] = COLS / 2 - i;
        snakeY[i] = ROWS / 2;
    }
}

// ==========================================
//  Parçacıkları Temizle
// ==========================================
void clearParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++)
        particles[i].active = false;
}

// ==========================================
//  Parçacık Oluştur
// ==========================================
void spawnParticles(float px, float py, uint16_t color, int count) {
    for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
        if (!particles[i].active) {
            particles[i].x = px;
            particles[i].y = py;
            particles[i].vx = random(-25, 25) / 10.0f;
            particles[i].vy = random(-25, 10) / 10.0f;
            particles[i].color = color;
            particles[i].life = random(8, 16);
            particles[i].active = true;
            count--;
        }
    }
}

// ==========================================
//  Parçacık Güncelle
// ==========================================
void updateParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].vy += 0.12f;
        particles[i].life--;
        if (particles[i].life <= 0 ||
            particles[i].x < 0 || particles[i].x >= SCR_W ||
            particles[i].y >= SCR_H) {
            particles[i].active = false;
        }
    }
}

// ==========================================
//  Parçacık Çiz
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
//  Oyunu Sıfırla
// ==========================================
void resetGame() {
    initSnake();
    spawnFood();
    clearParticles();
    score = 0;
    newRecord = false;
    popupTimer = 0;
    state = PLAYING;
    lastMoveMs = millis();
}

// ==========================================
//  Çizim: Checkerboard Arka Plan
// ==========================================
void drawBackground() {
    // HUD alanı siyah
    canvas.fillRect(0, 0, SCR_W, OFFSET_Y, 0x0000);
    // Oyun alanı (checkerboard)
    canvas.fillRect(0, OFFSET_Y, SCR_W, ROWS * GRID, COL_BG_A);
    for (int gy = 0; gy < ROWS; gy++) {
        for (int gx = 0; gx < COLS; gx++) {
            if ((gx + gy) % 2 == 1) {
                canvas.fillRect(gx * GRID, OFFSET_Y + gy * GRID, GRID, GRID, COL_BG_B);
            }
        }
    }
    // Alt boşluk
    int bottomY = OFFSET_Y + ROWS * GRID;
    canvas.fillRect(0, bottomY, SCR_W, SCR_H - bottomY, 0x0000);
    // Oyun alanı kenarlığı
    canvas.drawRect(0, OFFSET_Y - 1, SCR_W, ROWS * GRID + 2, COL_BORDER);
}

// ==========================================
//  Çizim: Yılan Gözleri
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
    // Göz bebekleri (1px siyah nokta, yöne göre)
    int ox = (d == DIR_RIGHT) ? 1 : (d == DIR_LEFT) ? 0 : 0;
    int oy = (d == DIR_DOWN) ? 1 : (d == DIR_UP) ? 0 : 0;
    canvas.drawPixel(ex1 + ox, ey1 + oy, TFT_BLACK);
    canvas.drawPixel(ex2 + ox, ey2 + oy, TFT_BLACK);
}

// ==========================================
//  Çizim: Yılan
// ==========================================
void drawSnake() {
    // Kuyruktan kafaya doğru çiz (kafa en üstte)
    for (int i = snakeLen - 1; i >= 0; i--) {
        int px = snakeX[i] * GRID;
        int py = OFFSET_Y + snakeY[i] * GRID;

        if (i == 0) {
            // === KAFA ===
            canvas.fillRect(px + 1, py + 1, GRID - 2, GRID - 2, COL_HEAD);
            canvas.drawRect(px, py, GRID, GRID, COL_HEAD_DK);
            drawEyes(px, py, dir);
        } else {
            // === GÖVDE ===
            uint16_t col = (i % 2 == 0) ? COL_BODY_A : COL_BODY_B;
            canvas.fillRect(px + 1, py + 1, GRID - 2, GRID - 2, col);
            canvas.drawRect(px, py, GRID, GRID, COL_BODY_BRD);
        }
    }
}

// ==========================================
//  Çizim: Yem (Elma)
// ==========================================
void drawFood() {
    int cx = foodX * GRID + GRID / 2;
    int cy = OFFSET_Y + foodY * GRID + GRID / 2;

    // Nabız efekti (parlak ↔ koyu geçiş)
    bool bright = (millis() / 300) % 2 == 0;
    uint16_t mainCol = bright ? COL_FOOD : COL_FOOD_DIM;

    // Elma gövdesi (daire)
    canvas.fillCircle(cx, cy, 3, mainCol);
    // Parlaklık noktası (sol üst)
    canvas.drawPixel(cx - 1, cy - 1, COL_FOOD_HL);
    // Sap
    canvas.drawFastVLine(cx, cy - 4, 2, COL_FOOD_STEM);
    // Yaprak
    canvas.drawPixel(cx + 1, cy - 4, COL_FOOD_LEAF);
    canvas.drawPixel(cx + 2, cy - 3, COL_FOOD_LEAF);
}

// ==========================================
//  Çizim: Skor Popup (+10 yüzen metin)
// ==========================================
void drawPopup() {
    if (popupTimer <= 0) return;
    int floatY = popupY - (15 - popupTimer);  // Yukarı süzül
    if (floatY < HUD_H) floatY = HUD_H;
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(popupX - 6, floatY);
    canvas.print("+10");
    popupTimer--;
}

// ==========================================
//  Çizim: HUD (Skor & Rekor)
// ==========================================
void drawHUD() {
    canvas.setTextSize(1);

    // Skor (sol taraf)
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(2, 1);
    canvas.print("SKOR:");
    canvas.setTextColor(TFT_WHITE);
    canvas.print(score);

    // Rekor (sağ taraf)
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(SCR_W - 55, 1);
    canvas.print("EN:");
    canvas.setTextColor(TFT_YELLOW);
    canvas.print(highScore);

    // Ayırıcı çizgi
    canvas.drawFastHLine(0, HUD_H, SCR_W, COL_HUD_LINE);
}

// ==========================================
//  Ekran: Ana Menü
// ==========================================
void drawMenu() {
    canvas.fillSprite(0x0000);

    // Başlık gölgesi
    canvas.setTextSize(2);
    canvas.setTextColor(0x0320);
    canvas.setCursor(36, 9);
    canvas.print("SNAKE");
    // Başlık
    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(35, 8);
    canvas.print("SNAKE");

    // Alt başlık
    canvas.setTextSize(1);
    canvas.setTextColor(COL_BODY_A);
    canvas.setCursor(38, 28);
    canvas.print("Modern Yilan");

    // Dekoratif yılan (statik)
    int demoY = 48;
    // Gövde segmentleri
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
    // Gözler (sağa bakıyor)
    canvas.fillRect(headX + 5, demoY + 1, 2, 2, TFT_WHITE);
    canvas.fillRect(headX + 5, demoY + 5, 2, 2, TFT_WHITE);

    // Dekoratif elma
    int appleX = 118;
    canvas.fillCircle(appleX, demoY + 4, 3, COL_FOOD);
    canvas.drawPixel(appleX - 1, demoY + 3, COL_FOOD_HL);
    canvas.drawFastVLine(appleX, demoY, 2, COL_FOOD_STEM);
    canvas.drawPixel(appleX + 1, demoY, COL_FOOD_LEAF);

    // Checkerboard önizleme (küçük)
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 16; c++) {
            uint16_t bg = ((r + c) % 2 == 0) ? COL_BG_A : COL_BG_B;
            canvas.fillRect(16 + c * 8, 62 + r * 8, 8, 8, bg);
        }
    }
    canvas.drawRect(16, 62, 128, 24, COL_BORDER);

    // Talimatlar
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(25, 94);
    canvas.print("[JOY] Yon Degistir");
    canvas.setCursor(25, 106);
    canvas.print("[A] Basla");
    canvas.setTextColor(0xBDF7);
    canvas.setCursor(25, 118);
    canvas.print("[B] OS Menu");

    // En yüksek skor
    if (highScore > 0) {
        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(90, 94);
        canvas.print("Rekor:");
        canvas.print(highScore);
    }

    canvas.pushSprite(0, 0);
}

// ==========================================
//  Ekran: Oyun Bitti
// ==========================================
void drawGameOver() {
    canvas.fillSprite(0x0000);

    // Checkerboard arkaplan (hafif)
    for (int gy = 0; gy < 16; gy++)
        for (int gx = 0; gx < 20; gx++)
            if ((gx + gy) % 2 == 1)
                canvas.fillRect(gx * 8, gy * 8, 8, 8, 0x0841);

    // Panel
    canvas.fillRoundRect(15, 15, 130, 98, 5, 0x2104);
    canvas.drawRoundRect(15, 15, 130, 98, 5, TFT_RED);
    canvas.drawRoundRect(16, 16, 128, 96, 4, 0x8000);

    // Başlık
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    canvas.setCursor(20, 22);
    canvas.print("OYUN BITTI");

    // Skor
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 48);
    canvas.print("Skor:  ");
    canvas.setTextColor(TFT_YELLOW);
    canvas.setTextSize(2);
    canvas.print(score);

    // Yem sayısı (bilgi)
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 68);
    canvas.print("Yem:   ");
    canvas.setTextColor(COL_FOOD);
    canvas.print(score / 10);

    // Rekor
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 80);
    canvas.print("Rekor: ");
    canvas.setTextColor(TFT_GREEN);
    canvas.setTextSize(2);
    canvas.print(highScore);

    // Yeni rekor bildirimi
    if (newRecord) {
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_MAGENTA);
        canvas.setCursor(28, 94);
        canvas.print("** YENI REKOR! **");
    }

    canvas.setTextSize(1);
    canvas.setTextColor(0xBDF7);
    canvas.setCursor(30, 104);
    canvas.print("[A] Tekrar Oyna");
    
    canvas.setCursor(30, 116);
    canvas.print("[B] OS Menu");

    canvas.pushSprite(0, 0);
}

// ==========================================
//  SETUP
// ==========================================
void setup() {
    // OLED Kapatma ve Buzzer Susturma (Hızlı Başlatma için)
    pinMode(5, OUTPUT);
    digitalWrite(5, LOW);
    Wire.begin(8, 9);
    Wire.beginTransmission(0x3C);
    Wire.write(0x00);
    Wire.write(0xAE); // Display OFF
    Wire.endTransmission();

    // GÜVENLİK: Elektrik kesilirse her zaman OS'tan başla
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    // Pin ayarları
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP);
    pinMode(BTN_D, INPUT_PULLUP);
    pinMode(JOY_SW, INPUT_PULLUP);
    pinMode(BUZZER, OUTPUT);

    // --- V2.1: NVS'ten ses ayarını oku ---
    { Preferences prefs; prefs.begin("os", true); soundEnabled = prefs.getBool("sound_en", true); prefs.end(); }

    // SPI başlat
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    // Ekran başlatma
    tft.init();
    tft.setRotation(1);   // Landscape: 160x128
    tft.fillScreen(TFT_BLACK);

    // Sprite tamponu (çift tamponlama / flicker-free)
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

    // ---- Frame hız kontrolü ----
    unsigned long now = millis();
    if (now - lastFrameMs < FRAME_MS) return;
    lastFrameMs = now;

    // ---- BTN_A: Kenar tespiti ----
    int btnA = digitalRead(BTN_A);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    prevBtnA = btnA;

    // ---- Durum Makinesi ----
    switch (state) {

        // ======================================
        //  ANA MENÜ
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

            // === Joystick → Yön Belirleme ===
            int jx = analogRead(JOY_X) - joyCenterX;
            int jy = analogRead(JOY_Y) - joyCenterY;

            // Büyük ekseni tercih et (çapraz itmeleri engelle)
            if (abs(jx) > abs(jy)) {
                if (jx > JOY_THRESHOLD && dir != DIR_LEFT)  nextDir = DIR_RIGHT;
                if (jx < -JOY_THRESHOLD && dir != DIR_RIGHT) nextDir = DIR_LEFT;
            } else {
                if (jy > JOY_THRESHOLD && dir != DIR_UP)    nextDir = DIR_DOWN;
                if (jy < -JOY_THRESHOLD && dir != DIR_DOWN)  nextDir = DIR_UP;
            }

            // === Hareket Zamanlaması ===
            unsigned long moveInterval = (unsigned long)BASE_SPEED - (unsigned long)(score / 10) * SPEED_DEC;
            if (moveInterval < MIN_SPEED) moveInterval = MIN_SPEED;

            if (now - lastMoveMs >= moveInterval) {
                lastMoveMs = now;

                // Yönü uygula
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

                // --- Duvar çarpışması ---
                if (newX < 0 || newX >= COLS || newY < 0 || newY >= ROWS) {
                    state = GAMEOVER;
                    newRecord = (score > highScore);
                    if (newRecord) highScore = score;
                    gameOverMs = now;
                    // --- V2.1: Yüksek skoru NVS'e kaydet ---
                    { Preferences prefs; prefs.begin("os", false);
                      int32_t hs = prefs.getInt("hs_snake", 0);
                      if (score > hs) prefs.putInt("hs_snake", score);
                      prefs.end(); }
                    // Ölüm parçacıkları
                    spawnParticles(
                        snakeX[0] * GRID + GRID / 2,
                        OFFSET_Y + snakeY[0] * GRID + GRID / 2,
                        COL_HEAD, 6);
                    playSound(200, 200);
                    delay(120);
                    playSound(100, 300);
                    break;
                }

                // Yemi kontrol et (çarpışma öncesi)
                bool ate = (newX == foodX && newY == foodY);

                // --- Kendine çarpışma ---
                // Yem yenmemişse kuyruk kayacak, o yüzden kuyruğu kontrol etme
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
                    // --- V2.1: Yüksek skoru NVS'e kaydet ---
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

                // === Yılanı Hareket Ettir ===
                if (ate && snakeLen < MAX_SNAKE) {
                    snakeLen++;  // Büyü (kuyruk kayma)
                }
                // Tüm segmentleri bir geri kaydır
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
                    popupTimer = 15;
                    // Parçacık efekti
                    spawnParticles(
                        foodX * GRID + GRID / 2,
                        OFFSET_Y + foodY * GRID + GRID / 2,
                        COL_FOOD, 5);
                    // Yeni yem
                    spawnFood();
                    // Neşeli ses
                    playSound(1047, 40);
                } else {
                    // Çok hafif hareket tıkırtısı
                    playSound(2500, 3);
                }
            }

            // === Parçacık Güncelle ===
            updateParticles();

            // ============================
            //  SAHNE ÇİZİMİ (Sprite'a)
            // ============================
            drawBackground();
            drawFood();
            drawSnake();
            drawParticles();
            drawPopup();
            drawHUD();

            // Hız göstergesi (opsiyonel — alt kenarda ince çubuk)
            int speedPct = 100 - (int)((moveInterval - MIN_SPEED) * 100 / (BASE_SPEED - MIN_SPEED));
            if (speedPct < 0) speedPct = 0;
            if (speedPct > 100) speedPct = 100;
            int barW = speedPct * (SCR_W - 4) / 100;
            int barY = OFFSET_Y + ROWS * GRID + 1;
            canvas.fillRect(2, barY, barW, 2, COL_HEAD);
            canvas.drawRect(2, barY, SCR_W - 4, 2, COL_BORDER);

            // Sprite'ı ekrana bas
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  OYUN BİTTİ
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
        //  PAUSE MENÜSÜ
        // ======================================
        case PAUSE:
            // Çizime devam et ama mantığı dondur
            drawBackground();
            drawFood();
            drawSnake();
            drawParticles();
            drawHUD();
            
            // Üzerine karartma ve menü
            canvas.fillRect(30, 40, 100, 50, COL_BG_A);
            canvas.drawRect(30, 40, 100, 50, TFT_WHITE);
            
            canvas.setTextSize(1);
            canvas.setTextColor(TFT_WHITE);
            canvas.setCursor(65, 48); canvas.print("PAUSE");
            canvas.setTextColor(TFT_YELLOW);
            canvas.setCursor(42, 62); canvas.print("[A] Devam Et");
            canvas.setCursor(42, 74); canvas.print("[B] OS Menu");
            
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
