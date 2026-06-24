// ============================================================
//  EMİR OS — ARKANOID (TUĞLA KIRMA)
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanlı çift tamponlama (Flicker-Free)
//
//  Kontroller:
//    JOY_X  -> Çubuğu sağa / sola hareket ettir
//    BTN_A  -> Topu fırlat
//    BTN_C  -> OS Launcher'a Dön (Rezerve)
//    Buzzer -> Ses efektleri
// ============================================================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
#include <math.h>
#include <Preferences.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"

// ============ Ekran Boyutları (Landscape) ============
#define SCR_W  160
#define SCR_H  128

// ============ Tuğla Sabitleri ============
#define BRICK_COLS    8       // Sütun sayısı
#define BRICK_ROWS    5       // Satır sayısı
#define BRICK_W       18      // Tuğla genişliği
#define BRICK_H       6       // Tuğla yüksekliği
#define BRICK_GAP_X   2       // Yatay boşluk
#define BRICK_GAP_Y   2       // Dikey boşluk
#define BRICK_START_X 1       // İlk tuğlanın X başlangıcı
#define BRICK_START_Y 13      // İlk tuğlanın Y başlangıcı (HUD altı)

// ============ Çubuk (Paddle) Sabitleri ============
#define PADDLE_W      28      // Çubuk genişliği
#define PADDLE_H      4       // Çubuk yüksekliği
#define PADDLE_Y      (SCR_H - 10)  // Çubuk Y konumu (118)

// ============ Top Sabitleri ============
#define BALL_R        3       // Top yarıçapı
// FPS60: Top hızları piksel/saniye cinsine çevrildi (orijinal piksel/kare × 30)
#define BASE_BALL_SPD 69.0f   // Başlangıç top hızı (2.3 × 30 = 69 piksel/sn)
#define MAX_BALL_SPD  114.0f  // Maksimum top hızı (3.8 × 30 = 114 piksel/sn)

// ============ Genel Sabitler ============
#define HUD_H         10      // Üst bilgi çubuğu yüksekliği
#define DEADZONE      300     // Joystick ölü bölge
#define START_LIVES   3       // Başlangıç canı
#define MAX_PARTICLES 20      // Parçacık efekti havuzu
// FPS60: Delta-Time sistemine geçildi, frame kilidi kaldırıldı
#define TARGET_FPS    60
#define FRAME_MS      (1000 / TARGET_FPS)
#define MAX_DT        0.05f   // FPS60: dt üst sınırı (lag spike koruması)

// ============ Özel Renkler (RGB565) ============
#define COL_BG        0x0008  // Koyu lacivert arka plan
#define COL_PADDLE    0x07E0  // Yeşil çubuk
#define COL_PADDLE_HL 0x47F0  // Açık yeşil vurgu
#define COL_PADDLE_DK 0x03E0  // Koyu yeşil gölge
#define COL_BALL      0xFFFF  // Beyaz top
#define COL_BALL_GLOW 0xBDF7  // Top parıltısı
#define COL_HUD_LINE  0x2104  // HUD ayırıcı çizgi
#define COL_HUD_TEXT  0xBDF7  // HUD metin rengi
#define COL_WALL      0x2945  // Duvar çerçeve rengi
#define COL_TITLE_SHADOW 0x0010  // Başlık gölge rengi
#define COL_RED_DARK     0x8000  // Koyu kırmızı (game over panel iç çerçeve)

// Tuğla renkleri (satır bazlı — üstten alta)
const uint16_t BRICK_COLORS[] = {
    0xF800,   // Kırmızı  (üst satır)
    0xFBE0,   // Turuncu
    0xFFE0,   // Sarı
    0x07E0,   // Yeşil
    0x07FF    // Cyan     (alt satır)
};

// Tuğla vurgu renkleri (3D efekti için)
const uint16_t BRICK_HL[] = {
    0xFACB,   // Açık kırmızı
    0xFDE0,   // Açık turuncu
    0xFFF5,   // Açık sarı
    0x47F0,   // Açık yeşil
    0x5FFF    // Açık cyan
};

// Satır bazlı puanlar
const int BRICK_POINTS[] = {50, 40, 30, 20, 10};

// ============ Nesneler ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Oyun Durumları ============
enum GameState { MENU, PLAYING, WAVE_CLEAR, GAMEOVER, PAUSE };
GameState state = MENU;

// ============ Tuğlalar ============
bool bricks[BRICK_ROWS][BRICK_COLS];
int brickCount;

// ============ Top ============
float ballX, ballY;
float ballVX, ballVY;
float ballSpeed;
bool ballStuck;     // Top çubukta yapışık mı?

// ============ Çubuk ============
float paddleX;

// ============ Parçacık Efekti ============
struct Particle {
    float x, y, vx, vy;
    uint16_t color;
    float life;     // FPS60: int → float (dt tabanlı azalma için)
    bool active;
};
Particle particles[MAX_PARTICLES];

// ============ Skor & Oyun ============
int score, highScore, wave, lives;
bool newRecord;

// ============ Zamanlama ============
unsigned long lastFrameMs;
unsigned long gameOverMs;
unsigned long waveClearMs;

// ============ FPS Sayacı ============
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;

// ============ Joystick ============
int joyCenterX;

// ============ Buton ============
int prevBtnA = HIGH;

// ============ V2.1: Ses Ayarı ============
bool soundEnabled = true;
// playSound — GameBase.h osPlaySound wrapper (eski API uyumu)
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// ==========================================
//  OS'a Dönüş Fonksiyonu — GameBase.h wrapper
// ==========================================
void returnToOS() {
    osReturnToOS(tft);
}

// ==========================================
//  Tuğlaları Başlat
// ==========================================
void initBricks() {
    brickCount = BRICK_ROWS * BRICK_COLS;
    for (int r = 0; r < BRICK_ROWS; r++)
        for (int c = 0; c < BRICK_COLS; c++)
            bricks[r][c] = true;
}

// ==========================================
//  Parçacıkları Temizle
// ==========================================
void clearParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++)
        particles[i].active = false;
}

// ==========================================
//  Topu Sıfırla (Çubuğa Yapıştır)
// ==========================================
void resetBall() {
    ballStuck = true;
    // FPS60: Dalga başına hız artışı piksel/sn cinsine çevrildi (0.15 × 30 = 4.5)
    ballSpeed = BASE_BALL_SPD + (wave - 1) * 4.5f;
    if (ballSpeed > MAX_BALL_SPD) ballSpeed = MAX_BALL_SPD;
    ballX = paddleX;
    ballY = PADDLE_Y - BALL_R - 1;
    ballVX = 0;
    ballVY = 0;
}

// ==========================================
//  Oyunu Sıfırla
// ==========================================
void resetGame() {
    paddleX = SCR_W / 2.0f;
    lives = START_LIVES;
    score = 0;
    wave = 1;
    newRecord = false;
    initBricks();
    clearParticles();
    resetBall();
    state = PLAYING;
}

// ==========================================
//  Yeni Dalga Başlat
// ==========================================
void startNewWave() {
    wave++;
    initBricks();
    clearParticles();
    paddleX = SCR_W / 2.0f;
    resetBall();
}

// ==========================================
//  Tuğla Pozisyon Yardımcıları
// ==========================================
int brickX(int col) { return BRICK_START_X + col * (BRICK_W + BRICK_GAP_X); }
int brickY(int row) { return BRICK_START_Y + row * (BRICK_H + BRICK_GAP_Y); }

// ==========================================
//  Parçacık Efekti: Oluştur
// ==========================================
void spawnParticles(float x, float y, uint16_t color, int count) {
    for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
        if (!particles[i].active) {
            particles[i].x = x;
            particles[i].y = y;
            // FPS60: Hızlar piksel/saniye cinsine çevrildi (orijinal × 30)
            particles[i].vx = random(-25, 25) * 3.0f;   // -75 ~ 72 piksel/sn
            particles[i].vy = random(-20, 8) * 3.0f;    // -60 ~ 21 piksel/sn
            particles[i].color = color;
            particles[i].life = (float)random(8, 18);   // FPS60: float cast (dt tabanlı azalma)
            particles[i].active = true;
            count--;
        }
    }
}

// ==========================================
//  Parçacık Efekti: Güncelle (FPS60: dt parametresi eklendi)
// ==========================================
void updateParticles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;
        // FPS60: Tüm hareketler dt ile çarpıldı
        particles[i].x += particles[i].vx * dt;
        particles[i].y += particles[i].vy * dt;
        particles[i].vy += 4.5f * dt;       // FPS60: Yerçekimi piksel/sn² (0.15 × 30 = 4.5)
        particles[i].life -= 30.0f * dt;    // FPS60: Zaman tabanlı ömür (30fps mantıksal kare)
        if (particles[i].life <= 0.0f ||
            particles[i].x < 0 || particles[i].x >= SCR_W ||
            particles[i].y >= SCR_H) {
            particles[i].active = false;
        }
    }
}

// ==========================================
//  Parçacık Efekti: Çiz
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
//  Çizim: Tuğla (3D efektli)
// ==========================================
void drawBrick(int row, int col) {
    int bx = brickX(col);
    int by = brickY(row);
    uint16_t color = BRICK_COLORS[row % 5];
    uint16_t hl = BRICK_HL[row % 5];

    // Ana gövde
    canvas.fillRect(bx, by, BRICK_W, BRICK_H, color);
    // Üst ve sol kenar vurgusu (3D)
    canvas.drawFastHLine(bx, by, BRICK_W - 1, hl);
    canvas.drawFastVLine(bx, by, BRICK_H - 1, hl);
    // Alt ve sağ kenar gölge
    canvas.drawFastHLine(bx + 1, by + BRICK_H - 1, BRICK_W - 1, TFT_BLACK);
    canvas.drawFastVLine(bx + BRICK_W - 1, by + 1, BRICK_H - 1, TFT_BLACK);
}

// ==========================================
//  Çizim: Çubuk (Paddle)
// ==========================================
void drawPaddle() {
    int px = (int)paddleX - PADDLE_W / 2;
    int py = PADDLE_Y;
    // Ana gövde
    canvas.fillRect(px, py, PADDLE_W, PADDLE_H, COL_PADDLE);
    // Üst vurgu
    canvas.drawFastHLine(px, py, PADDLE_W, COL_PADDLE_HL);
    // Alt gölge
    canvas.drawFastHLine(px, py + PADDLE_H - 1, PADDLE_W, COL_PADDLE_DK);
    // Orta dekorasyon
    canvas.drawFastVLine((int)paddleX, py + 1, PADDLE_H - 2, COL_PADDLE_HL);
}

// ==========================================
//  Çizim: Top
// ==========================================
void drawBall() {
    int bx = (int)ballX;
    int by = (int)ballY;
    canvas.fillCircle(bx, by, BALL_R, COL_BALL);
    // Parlaklık noktası (sol üst)
    canvas.drawPixel(bx - 1, by - 1, COL_BALL_GLOW);
}

// ==========================================
//  Çizim: HUD (Skor, Dalga, Canlar)
// ==========================================
void drawHUD() {
    canvas.setTextSize(1);

    // Skor (sol taraf)
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(2, 1);
    canvas.print("S:");
    canvas.setTextColor(TFT_WHITE);
    canvas.print(score);

    // Dalga numarası (orta)
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(SCR_W / 2 - 8, 1);
    canvas.print("W:");
    canvas.setTextColor(TFT_YELLOW);
    canvas.print(wave);

    // FPS Sayacı (Canlar'dan hemen önce, sağa dayalı)
    char fpsStr[10];
    sprintf(fpsStr, "FPS:%d", currentFPS);
    int fpsWidth = strlen(fpsStr) * 6;
    
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(128 - fpsWidth, 1);
    canvas.print("FPS:");
    canvas.setTextColor(TFT_GREEN);
    canvas.print(currentFPS);

    // Canlar (sağ taraf — küçük toplar)
    for (int i = 0; i < lives; i++) {
        canvas.fillCircle(SCR_W - 6 - i * 10, 5, 3, COL_BALL);
    }

    // Ayırıcı çizgi
    canvas.drawFastHLine(0, HUD_H, SCR_W, COL_HUD_LINE);
}

// ==========================================
//  Çizim: Duvar Çerçeveleri (sol, sağ, üst)
// ==========================================
void drawWalls() {
    // İnce duvar çizgileri
    canvas.drawFastVLine(0, HUD_H, SCR_H - HUD_H, COL_WALL);
    canvas.drawFastVLine(SCR_W - 1, HUD_H, SCR_H - HUD_H, COL_WALL);
    canvas.drawFastHLine(0, HUD_H, SCR_W, COL_WALL);
}

// ==========================================
//  Ekran: Ana Menü
// ==========================================
void drawMenu() {
    canvas.fillSprite(COL_BG);

    // Başlık gölgesi
    canvas.setTextSize(2);
    canvas.setTextColor(COL_TITLE_SHADOW);
    canvas.setCursor(33, 7);
    canvas.print("ARKANOID");
    // Başlık
    canvas.setTextColor(TFT_CYAN);
    canvas.setCursor(32, 6);
    canvas.print("ARKANOID");

    // Dekoratif tuğlalar
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 6; c++) {
            int dx = 20 + c * 20;
            int dy = 30 + r * 8;
            uint16_t col = BRICK_COLORS[(r + c) % 5];
            canvas.fillRect(dx, dy, 18, 6, col);
            canvas.drawFastHLine(dx, dy, 17, BRICK_HL[(r + c) % 5]);
        }
    }

    // Dekoratif çubuk ve top
    int demoPX = SCR_W / 2;
    int demoPY = 64;
    canvas.fillRect(demoPX - PADDLE_W / 2, demoPY, PADDLE_W, PADDLE_H, COL_PADDLE);
    canvas.drawFastHLine(demoPX - PADDLE_W / 2, demoPY, PADDLE_W, COL_PADDLE_HL);
    // Top (animasyonlu yukarı-aşağı — millis() tabanlı, dt'den bağımsız)
    float demoBy = 58.0f + sin(millis() / 200.0f) * 3.0f;
    canvas.fillCircle(demoPX, (int)demoBy, BALL_R, COL_BALL);

    // Alt Menuler (Iki kolon, derli toplu)
    canvas.setTextSize(1);
    
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(10, 90);
    canvas.print("[A] Basla");

    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(90, 90);
    canvas.print("[B] OS Menu");

    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(10, 106);
    canvas.print("[JOY] Hareket");

    // En yüksek skor
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(92, 106);
    canvas.printf("Rekor: %d", highScore);

    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}

// ==========================================
//  Ekran: Dalga Temizlendi
// ==========================================
void drawWaveClear() {
    canvas.fillSprite(COL_BG);

    canvas.setTextSize(2);
    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(22, 30);
    canvas.print("DALGA ");
    canvas.print(wave);

    canvas.setTextColor(TFT_CYAN);
    canvas.setCursor(20, 55);
    canvas.print("TEMIZ!");

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(45, 85);
    canvas.print("Skor: ");
    canvas.print(score);

    // Rengarenk tuğla parçacıkları (kutlama)
    for (int i = 0; i < 15; i++) {
        int px = random(10, SCR_W - 10);
        int py = random(20, SCR_H - 20);
        uint16_t pc = BRICK_COLORS[random(0, 5)];
        canvas.fillRect(px, py, 3, 3, pc);
    }

    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}

// ==========================================
//  Ekran: Oyun Bitti
// ==========================================
void drawGameOver() {
    canvas.fillSprite(COL_BG);

    // Panel
    canvas.fillRoundRect(15, 6, 130, 120, 5, COL_HUD_LINE);
    canvas.drawRoundRect(15, 6, 130, 120, 5, TFT_RED);
    canvas.drawRoundRect(16, 7, 128, 118, 4, COL_RED_DARK);

    // Başlık
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

    // Dalga
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 60);
    canvas.print("Dalga: ");
    canvas.setTextColor(TFT_CYAN);
    canvas.setTextSize(2);
    canvas.setCursor(75, 56);
    canvas.print(wave);

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
    // OLED Kapatma ve Buzzer Susturma (Hızlı Başlatma için)
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
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

    initDevTools(tft);

    // Ekran başlatma
    tft.init();
    tft.setRotation(1);   // Landscape: 160x128
    // TFT donanimini RGB moduna gecir (standart RGB565 sabitler icin)
    tft.startWrite();
    tft.writecommand(0x36);  // MADCTL
    tft.writedata(0xA0);     // MY|MV, BGR bit kapali (RGB modu)
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    // Sprite tamponu (çift tamponlama / flicker-free)
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // Joystick merkez kalibrasyonu
    joyCenterX = analogRead(JOY_X);

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

    // ---- FPS60: Delta-Time sistemi (frame kilidi kaldırıldı, soft cap 60fps) ----
    unsigned long now = millis();
    unsigned long frameDelta = now - lastFrameMs;
    if (frameDelta < FRAME_MS) {
        delay(1);   // FPS60: FreeRTOS'a yield et, CPU'yu boşa yakma
        return;
    }
    float dt = frameDelta / 1000.0f;
    if (dt > MAX_DT) dt = MAX_DT;   // FPS60: Lag spike koruması (max 50ms)
    lastFrameMs = now;

    // ---- BTN_A: Kenar tespiti ----
    int btnA = digitalRead(BTN_A);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    prevBtnA = btnA;

    // ---- FPS Hesaplama ----
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

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
                delay(100);   // FPS60: Debounce 200→100ms
                returnToOS();
            }
            break;

        // ======================================
        //  OYUN
        // ======================================
        case PLAYING: {

            // === Joystick → Çubuk Hareketi ===
            int rawJx = analogRead(JOY_X);
            int jx = rawJx - joyCenterX;
            if (abs(jx) > DEADZONE) {
                float factor = (float)(abs(jx) - DEADZONE) / (float)(2048 - DEADZONE);
                if (factor > 1.0f) factor = 1.0f;
                // FPS60: Çubuk hızı piksel/sn cinsine çevrildi (1.2~4.2 × 30 = 36~126)
                float spd = 36.0f + factor * 90.0f;   // 36 ~ 126 piksel/sn
                paddleX += (jx > 0 ? spd : -spd) * dt; // FPS60: dt ile çarp
            }
            // Ekran sınırları
            float halfW = PADDLE_W / 2.0f;
            if (paddleX < halfW + 1) paddleX = halfW + 1;
            if (paddleX > SCR_W - halfW - 1) paddleX = SCR_W - halfW - 1;

            // === Top Çubuğa Yapışıkken ===
            if (ballStuck) {
                ballX = paddleX;
                ballY = PADDLE_Y - BALL_R - 1;

                // BTN_A → Topu fırlat
                if (pressA) {
                    ballStuck = false;
                    // Rastgele yöne ~30° açıyla fırlat
                    float dir = (random(0, 2) == 0) ? -1.0f : 1.0f;
                    float angle = 0.45f + random(0, 20) / 100.0f;  // ~26-37°
                    ballVX = dir * ballSpeed * sin(angle);
                    ballVY = -ballSpeed * cos(angle);
                    playSound(600, 40);
                }
            }
            else {
                // === Top Hareketini Güncelle ===
                // FPS60: Hareket dt ile çarpıldı
                ballX += ballVX * dt;
                ballY += ballVY * dt;

                // --- Sol duvar çarpışması ---
                if (ballX - BALL_R <= 1) {
                    ballX = BALL_R + 1;
                    ballVX = fabs(ballVX);
                    playSound(400, 15);
                }
                // --- Sağ duvar çarpışması ---
                if (ballX + BALL_R >= SCR_W - 1) {
                    ballX = SCR_W - 1 - BALL_R;
                    ballVX = -fabs(ballVX);
                    playSound(400, 15);
                }
                // --- Üst duvar çarpışması ---
                if (ballY - BALL_R <= HUD_H + 1) {
                    ballY = HUD_H + 1 + BALL_R;
                    ballVY = fabs(ballVY);
                    playSound(400, 15);
                }

                // --- Çubuk çarpışması ---
                if (ballVY > 0) {   // Sadece top aşağı inerken
                    if (ballY + BALL_R >= PADDLE_Y &&
                        ballY + BALL_R < PADDLE_Y + PADDLE_H + 3 &&
                        ballX >= paddleX - halfW - BALL_R &&
                        ballX <= paddleX + halfW + BALL_R) {
                        // Çarpma pozisyonuna göre sekme açısı hesapla
                        float hitPos = (ballX - paddleX) / (halfW + BALL_R);
                        // Sınırla: -0.9 .. +0.9
                        if (hitPos > 0.9f) hitPos = 0.9f;
                        if (hitPos < -0.9f) hitPos = -0.9f;
                        // Açıya dönüştür (±60° maks)
                        float angle = hitPos * 1.05f;  // ~60° max
                        ballVX = ballSpeed * sin(angle);
                        ballVY = -ballSpeed * cos(angle);
                        // Topu çubuğun üstüne yerleştir (gömülmeyi önle)
                        ballY = PADDLE_Y - BALL_R;
                        playSound(500, 25);
                    }
                }

                // --- Tuğla çarpışması ---
                bool brickHit = false;
                for (int r = 0; r < BRICK_ROWS && !brickHit; r++) {
                    for (int c = 0; c < BRICK_COLS && !brickHit; c++) {
                        if (!bricks[r][c]) continue;

                        int bx = brickX(c);
                        int by = brickY(r);

                        // Daire-dikdörtgen çarpışma testi
                        float closestX = fmax((float)bx, fmin(ballX, (float)(bx + BRICK_W)));
                        float closestY = fmax((float)by, fmin(ballY, (float)(by + BRICK_H)));
                        float dx = ballX - closestX;
                        float dy = ballY - closestY;

                        if (dx * dx + dy * dy < (float)(BALL_R * BALL_R)) {
                            // Tuğla kırıldı!
                            bricks[r][c] = false;
                            brickCount--;
                            score += BRICK_POINTS[r % 5];

                            // Parçacık efekti
                            spawnParticles(bx + BRICK_W / 2, by + BRICK_H / 2,
                                           BRICK_COLORS[r % 5], 4);

                            // Sesefekti (üst satırlar daha tiz)
                            playSound(1000 + (BRICK_ROWS - 1 - r) * 200, 35);

                            // Sekme yönünü belirle (önceki pozisyona bakarak)
                            // FPS60: Önceki konum dt dikkate alınarak hesaplandı
                            float prevBX = ballX - ballVX * dt;
                            float prevBY = ballY - ballVY * dt;
                            bool wasOutV = (prevBY + BALL_R <= by || prevBY - BALL_R >= by + BRICK_H);
                            bool wasOutH = (prevBX + BALL_R <= bx || prevBX - BALL_R >= bx + BRICK_W);

                            if (wasOutV && !wasOutH) {
                                ballVY = -ballVY;
                            } else if (wasOutH && !wasOutV) {
                                ballVX = -ballVX;
                            } else {
                                // Köşe çarpışması — her iki ekseni çevir
                                ballVX = -ballVX;
                                ballVY = -ballVY;
                            }

                            brickHit = true;
                        }
                    }
                }

                // --- Top alt ekrandan düştü mü? ---
                if (ballY - BALL_R > SCR_H) {
                    lives--;
                    playSound(200, 200);
                    delay(80);                  // FPS60: Azaltıldı (100→80ms)
                    playSound(100, 300);

                    if (lives <= 0) {
                        state = GAMEOVER;
                        newRecord = (score > highScore);
                        if (newRecord) highScore = score;
                        gameOverMs = now;
                        // --- V2.1: Yüksek skoru NVS'e kaydet ---
                        { Preferences prefs; prefs.begin("os", false);
                          int32_t hs = prefs.getInt("hs_arkanoid", 0);
                          if (score > hs) prefs.putInt("hs_arkanoid", score);
                          prefs.end(); }
                        break;
                    } else {
                        // Canı azalt, topu sıfırla
                        delay(250);              // FPS60: Azaltıldı (300→250ms)
                        resetBall();
                    }
                }
            }

            // === Tüm tuğlalar kırıldı mı? ===
            if (brickCount <= 0) {
                state = WAVE_CLEAR;
                waveClearMs = now;
                playSound(1047, 80);
                delay(90);
                playSound(1319, 80);
                delay(90);
                playSound(1568, 150);
                break;
            }

            // === Parçacık güncellemesi ===
            updateParticles(dt);   // FPS60: dt parametresi geçildi

            // ============================
            //  SAHNE ÇİZİMİ (Sprite'a)
            // ============================
            canvas.fillSprite(COL_BG);

            // Duvar çerçeveleri
            drawWalls();

            // Tuğlalar
            for (int r = 0; r < BRICK_ROWS; r++)
                for (int c = 0; c < BRICK_COLS; c++)
                    if (bricks[r][c])
                        drawBrick(r, c);

            // Parçacıklar (tuğlaların üstünde)
            drawParticles();

            // Çubuk
            drawPaddle();

            // Top
            drawBall();

            // HUD (en üstte)
            drawHUD();

            // Yapışık topken bilgi metni
            if (ballStuck) {
                canvas.setTextSize(1);
                canvas.setTextColor(0xBDF7);
                canvas.setCursor(47, 85);
                canvas.print("[A] Firlat!");
            }

            // Sprite'ı ekrana bas
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  DALGA TEMİZLENDİ
        // ======================================
        case WAVE_CLEAR:
            drawWaveClear();
            if (now - waveClearMs > 2000) {
                startNewWave();
                state = PLAYING;
            }
            break;

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
            // Arka planı olduğu gibi tutuyoruz (çizim devam ediyor)
            drawHUD();
            drawPaddle();
            drawBall();
            // Tuğlalar
            for (int r = 0; r < BRICK_ROWS; r++)
                for (int c = 0; c < BRICK_COLS; c++)
                    if (bricks[r][c])
                        drawBrick(r, c);
            
            // Üzerine karartma ve menü
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
                delay(100);   // FPS60: Debounce 200→100ms
                state = PLAYING;
                lastFrameMs = millis();
            }
            if (!digitalRead(BTN_B)) {
                playSound(400, 50);
                delay(100);   // FPS60: Debounce 200→100ms
                returnToOS();
            }
            break;
    }
}
