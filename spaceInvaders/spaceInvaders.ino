// ============================================================
//  EMİR OS — SPACE INVADERS (UZAY SAVAŞLARI)
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanlı çift tamponlama (Flicker-Free)
//
//  Kontroller:
//    JOY_X  -> Sağa / Sola hareket
//    BTN_A  -> Ateş et
//    BTN_B  -> Ateş et (alternatif)
//    BTN_C  -> OS Launcher'a Dön (Rezerve)
//    Buzzer -> Ses efektleri
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

// ============ Oyun Sabitleri ============
#define ALIEN_COLS    8       // Sütun sayısı
#define ALIEN_ROWS    4       // Satır sayısı
#define ALIEN_W       9       // Uzaylı genişliği (piksel)
#define ALIEN_H       7       // Uzaylı yüksekliği (piksel)
#define ALIEN_GAP_X   3       // Yatay boşluk
#define ALIEN_GAP_Y   4       // Dikey boşluk
#define ALIEN_START_Y 14      // İlk satırın Y başlangıcı
#define ALIEN_DROP    5       // Kenar dönüşünde düşme miktarı

#define PLAYER_W      14      // Oyuncu genişliği
#define PLAYER_H      10      // Oyuncu yüksekliği
#define PLAYER_Y      (SCR_H - 14)  // Oyuncu Y konumu

#define MAX_PBULLETS  3       // Maks oyuncu mermisi
#define MAX_EBULLETS  3       // Maks düşman mermisi
#define PBULLET_SPD   4       // Oyuncu mermi hızı
#define EBULLET_SPD   2       // Düşman mermi hızı

#define DEADZONE      300     // Joystick ölü bölge
#define NUM_STARS     30      // Arka plan yıldız sayısı
#define HUD_H         10      // Üst bilgi çubuğu yüksekliği
#define START_LIVES   3       // Başlangıç canı
#define INVINCIBLE_MS 1500    // Hasar sonrası dokunulmazlık (ms)
#define FIRE_COOLDOWN 250     // Ateş bekleme süresi (ms)

#define TARGET_FPS    30
#define FRAME_MS      (1000 / TARGET_FPS)

// ============ Özel Renkler (RGB565) ============
#define COL_SPACE     0x0000  // Siyah uzay
#define COL_PLAYER    0x07E0  // Yeşil gemi
#define COL_PLAYER_HL 0x07F0  // Açık yeşil vurgu
#define COL_CABIN     0xB7F0  // Kabin rengi
#define COL_PBULLET   0xFFE0  // Sarı mermi
#define COL_EBULLET   0xF800  // Kırmızı mermi
#define COL_HUD_LINE  0x2104  // HUD ayırıcı çizgi
#define COL_HUD_TEXT  0xBDF7  // HUD metin rengi

// Uzaylı renkleri (satır bazlı - üstten alta)
#define COL_ALIEN_0   0x07FF  // Cyan   (üst satır - 40 puan)
#define COL_ALIEN_1   0xF81F  // Magenta (30 puan)
#define COL_ALIEN_2   0xFFE0  // Sarı    (20 puan)
#define COL_ALIEN_3   0xF800  // Kırmızı (alt satır - 10 puan)

// ============ Nesneler ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Oyun Durumları ============
enum GameState { MENU, PLAYING, WAVE_CLEAR, GAMEOVER, PAUSE };
GameState state = MENU;

// ============ Yapılar ============
struct Bullet {
    float x, y;
    bool active;
};

// ============ Oyuncu ============
float playerX;
int lives;

// ============ Uzaylılar ============
bool aliens[ALIEN_ROWS][ALIEN_COLS];
int alienCount;
float alienGridX;         // Gridin sol üst köşe X'i
float alienGridY;         // Gridin sol üst köşe Y'si
int alienDir;             // Hareket yönü: 1=sağ, -1=sol
float alienSpeed;         // Anlık hız
bool alienAnimFrame;      // Animasyon karesi (bacak/anten toggle)
unsigned long lastAnimMs;

// ============ Mermiler ============
Bullet pBullets[MAX_PBULLETS];
Bullet eBullets[MAX_EBULLETS];

// ============ Yıldızlar ============
int starX[NUM_STARS], starY[NUM_STARS];
uint16_t starColor[NUM_STARS];

// ============ Skor & Oyun ============
int score, highScore, wave;
bool newRecord;

// Satır bazlı puanlar
const int ALIEN_POINTS[] = {40, 30, 20, 10};

// Satır bazlı renkler
const uint16_t ALIEN_COLORS[] = {
    COL_ALIEN_0, COL_ALIEN_1, COL_ALIEN_2, COL_ALIEN_3
};

// ============ Zamanlama ============
unsigned long lastFrameMs;
unsigned long gameOverMs;
unsigned long waveClearMs;
unsigned long lastFireMs;
unsigned long lastEFireMs;
unsigned long hitTime;

// ============ Joystick ============
int joyCenterX;

// ============ Buton Durumları ============
int prevBtnA = HIGH;
int prevBtnB = HIGH;

// ============ V2.1: Ses Ayarı ============
bool soundEnabled = true;
void playSound(uint16_t freq, uint32_t dur) {
    if (soundEnabled) { tone(BUZZER, freq, dur); }
}

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
//  Yıldızları Başlat
// ==========================================
void initStars() {
    // Önceden hesaplanmış gri tonları (parlak -> soluk)
    const uint16_t grays[] = {0x18E3, 0x3186, 0x528A, 0x7BCF, 0xAD75, 0xFFFF};
    for (int i = 0; i < NUM_STARS; i++) {
        starX[i] = random(0, SCR_W);
        starY[i] = random(HUD_H + 2, SCR_H);
        starColor[i] = grays[random(0, 6)];
    }
}

// ==========================================
//  Uzaylıları Başlat
// ==========================================
void initAliens() {
    alienCount = ALIEN_ROWS * ALIEN_COLS;
    for (int r = 0; r < ALIEN_ROWS; r++)
        for (int c = 0; c < ALIEN_COLS; c++)
            aliens[r][c] = true;

    // Gridi yatayda ortala
    int gridW = ALIEN_COLS * (ALIEN_W + ALIEN_GAP_X) - ALIEN_GAP_X;
    alienGridX = (SCR_W - gridW) / 2.0f;
    alienGridY = ALIEN_START_Y;
    alienDir = 1;

    // Dalga bazlı başlangıç hızı
    alienSpeed = 0.25f + (wave - 1) * 0.08f;
    if (alienSpeed > 1.0f) alienSpeed = 1.0f;

    alienAnimFrame = false;
    lastAnimMs = millis();
}

// ==========================================
//  Mermileri Temizle
// ==========================================
void clearBullets() {
    for (int i = 0; i < MAX_PBULLETS; i++) pBullets[i].active = false;
    for (int i = 0; i < MAX_EBULLETS; i++) eBullets[i].active = false;
}

// ==========================================
//  Yeni Dalga Başlat
// ==========================================
void startNewWave() {
    wave++;
    initAliens();
    clearBullets();
}

// ==========================================
//  Oyunu Sıfırla
// ==========================================
void resetGame() {
    playerX = SCR_W / 2.0f;
    lives = START_LIVES;
    score = 0;
    wave = 1;
    newRecord = false;
    hitTime = 0;
    lastFireMs = 0;
    lastEFireMs = millis();
    initAliens();
    clearBullets();
    initStars();
    state = PLAYING;
}

// ==========================================
//  Canlı Uzaylıların Sınırlarını Bul
// ==========================================
void getAlienBounds(int &leftCol, int &rightCol, int &topRow, int &botRow) {
    leftCol = ALIEN_COLS;
    rightCol = -1;
    topRow = ALIEN_ROWS;
    botRow = -1;
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            if (aliens[r][c]) {
                if (c < leftCol) leftCol = c;
                if (c > rightCol) rightCol = c;
                if (r < topRow) topRow = r;
                if (r > botRow) botRow = r;
            }
        }
    }
}

// Uzaylı pozisyon yardımcıları
float getAlienX(int col) { return alienGridX + col * (ALIEN_W + ALIEN_GAP_X); }
float getAlienY(int row) { return alienGridY + row * (ALIEN_H + ALIEN_GAP_Y); }

// ==========================================
//  Çizim: Yıldızlar
// ==========================================
void drawStars() {
    for (int i = 0; i < NUM_STARS; i++) {
        canvas.drawPixel(starX[i], starY[i], starColor[i]);
    }
}

// ==========================================
//  Çizim: Uzaylı (satır tipine göre farklı şekil)
// ==========================================
void drawAlien(int row, int col) {
    int x = (int)getAlienX(col);
    int y = (int)getAlienY(row);
    uint16_t c = ALIEN_COLORS[row % 4];

    int type = row % 3;

    if (type == 0) {
        // === TİP 0: "Kalamar" — üstte dar, altta geniş ===
        canvas.fillRect(x + 3, y, 3, 2, c);          // Baş
        canvas.fillRect(x + 1, y + 2, 7, 3, c);      // Gövde
        canvas.fillRect(x, y + 4, 9, 2, c);           // Alt gövde
        // Gözler
        canvas.drawPixel(x + 3, y + 3, TFT_BLACK);
        canvas.drawPixel(x + 5, y + 3, TFT_BLACK);
        // Bacaklar (animasyonlu)
        if (alienAnimFrame) {
            canvas.drawPixel(x + 1, y + 6, c);
            canvas.drawPixel(x + 7, y + 6, c);
        } else {
            canvas.drawPixel(x, y + 6, c);
            canvas.drawPixel(x + 8, y + 6, c);
        }
    }
    else if (type == 1) {
        // === TİP 1: "Yengeç" — geniş, boynuzlu ===
        canvas.fillRect(x + 1, y, 7, 6, c);           // Gövde
        canvas.fillRect(x, y + 1, 9, 4, c);           // Geniş bölge
        // Gözler
        canvas.fillRect(x + 2, y + 2, 2, 2, TFT_BLACK);
        canvas.fillRect(x + 5, y + 2, 2, 2, TFT_BLACK);
        // Boynuzlar (animasyonlu)
        if (alienAnimFrame) {
            canvas.drawPixel(x, y, c);
            canvas.drawPixel(x + 8, y, c);
        } else {
            canvas.drawPixel(x, y + 5, c);
            canvas.drawPixel(x + 8, y + 5, c);
            canvas.drawPixel(x, y + 6, c);
            canvas.drawPixel(x + 8, y + 6, c);
        }
    }
    else {
        // === TİP 2: "Ahtapot" — yuvarlak ===
        canvas.fillRect(x + 2, y, 5, 2, c);           // Üst
        canvas.fillRect(x + 1, y + 2, 7, 2, c);       // Orta
        canvas.fillRect(x, y + 3, 9, 2, c);           // Geniş orta
        canvas.fillRect(x + 1, y + 5, 7, 1, c);       // Alt
        // Gözler
        canvas.drawPixel(x + 3, y + 3, TFT_BLACK);
        canvas.drawPixel(x + 5, y + 3, TFT_BLACK);
        // Dokunaçlar (animasyonlu)
        if (alienAnimFrame) {
            canvas.drawPixel(x + 2, y + 6, c);
            canvas.drawPixel(x + 4, y + 6, c);
            canvas.drawPixel(x + 6, y + 6, c);
        } else {
            canvas.drawPixel(x + 1, y + 6, c);
            canvas.drawPixel(x + 3, y + 6, c);
            canvas.drawPixel(x + 5, y + 6, c);
            canvas.drawPixel(x + 7, y + 6, c);
        }
    }
}

// ==========================================
//  Çizim: Oyuncu Gemisi
// ==========================================
void drawPlayer() {
    // Dokunulmazlık sırasında yanıp söner
    bool invincible = (millis() - hitTime < INVINCIBLE_MS);
    if (invincible && (millis() / 100) % 2 == 0) return;

    int x = (int)playerX;
    int y = PLAYER_Y;

    // Gövde (yeşil trapez)
    canvas.fillRect(x - PLAYER_W / 2, y + 4, PLAYER_W, PLAYER_H - 4, COL_PLAYER);
    canvas.fillRect(x - PLAYER_W / 2 + 2, y + 2, PLAYER_W - 4, 3, COL_PLAYER);
    // Namlu (top)
    canvas.fillRect(x - 1, y, 3, 3, COL_PLAYER);
    // Kabin parlaklığı
    canvas.drawFastHLine(x - PLAYER_W / 2 + 1, y + 5, PLAYER_W - 2, COL_PLAYER_HL);
    canvas.fillRect(x - 2, y + 3, 5, 2, COL_CABIN);
}

// ==========================================
//  Çizim: HUD (Skor, Dalga, Canlar)
// ==========================================
void drawHUD() {
    canvas.setTextSize(1);
    canvas.setTextColor(COL_HUD_TEXT);

    // Skor (sol taraf)
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

    // Canlar (sağ taraf — küçük üçgen ikonlar)
    for (int i = 0; i < lives; i++) {
        int lx = SCR_W - 10 - i * 12;
        canvas.fillTriangle(lx, 8, lx + 4, 1, lx + 8, 8, COL_PLAYER);
    }

    // Ayırıcı çizgi
    canvas.drawFastHLine(0, HUD_H, SCR_W, COL_HUD_LINE);
}

// ==========================================
//  Ekran: Ana Menü
// ==========================================
void drawMenu() {
    canvas.fillSprite(COL_SPACE);
    drawStars();

    // Başlık gölgesi
    canvas.setTextSize(2);
    canvas.setTextColor(0x0010);
    canvas.setCursor(19, 7);
    canvas.print("SPACE");
    canvas.setCursor(7, 25);
    canvas.print("INVADERS");
    // Başlık
    canvas.setTextColor(TFT_CYAN);
    canvas.setCursor(18, 6);
    canvas.print("SPACE");
    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(6, 24);
    canvas.print("INVADERS");

    // Dekoratif uzaylılar (menü için basit çizim)
    const uint16_t ac[] = {COL_ALIEN_0, COL_ALIEN_1, COL_ALIEN_2, COL_ALIEN_3};
    bool animF = (millis() / 500) % 2;
    for (int i = 0; i < 4; i++) {
        int ax = 28 + i * 26;
        int ay = 50;
        canvas.fillRect(ax + 1, ay, 7, 6, ac[i]);
        canvas.fillRect(ax, ay + 1, 9, 4, ac[i]);
        canvas.drawPixel(ax + 2, ay + 2, TFT_BLACK);
        canvas.drawPixel(ax + 6, ay + 2, TFT_BLACK);
        if (animF) {
            canvas.drawPixel(ax, ay + 6, ac[i]);
            canvas.drawPixel(ax + 8, ay + 6, ac[i]);
        }
    }

    // Talimatlar
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(22, 72);
    canvas.print("[A/B] Ates Et");
    canvas.setCursor(22, 84);
    canvas.print("[JOY] Hareket");
    canvas.setTextColor(0xBDF7);
    canvas.setCursor(22, 100);
    canvas.print("[B] OS Menu");

    // En yüksek skor
    if (highScore > 0) {
        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(22, 114);
        canvas.print("Rekor: ");
        canvas.print(highScore);
    }

    canvas.pushSprite(0, 0);
}

// ==========================================
//  Ekran: Dalga Temizlendi
// ==========================================
void drawWaveClear() {
    canvas.fillSprite(COL_SPACE);
    drawStars();

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

    canvas.pushSprite(0, 0);
}

// ==========================================
//  Ekran: Oyun Bitti
// ==========================================
void drawGameOver() {
    canvas.fillSprite(COL_SPACE);
    drawStars();

    // Panel
    canvas.fillRoundRect(15, 12, 130, 104, 5, 0x2104);
    canvas.drawRoundRect(15, 12, 130, 104, 5, TFT_RED);
    canvas.drawRoundRect(16, 13, 128, 102, 4, 0x8000);

    // Başlık
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    canvas.setCursor(20, 20);
    canvas.print("OYUN BITTI");

    // Skor
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 48);
    canvas.print("Skor:  ");
    canvas.setTextColor(TFT_YELLOW);
    canvas.setTextSize(2);
    canvas.print(score);

    // Dalga
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 65);
    canvas.print("Dalga: ");
    canvas.setTextColor(TFT_CYAN);
    canvas.print(wave);

    // Rekor
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 78);
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

    // Sprite tamponu (çift tamponlama)
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // Joystick merkez kalibrasyonu
    joyCenterX = analogRead(JOY_X);

    // Rastgele tohum
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    initStars();
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

    // ---- Buton kenar tespiti ----
    int btnA = digitalRead(BTN_A);
    int btnB = digitalRead(BTN_B);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    bool pressB = (btnB == LOW && prevBtnB == HIGH);
    prevBtnA = btnA;
    prevBtnB = btnB;

    bool anyPress = pressA || pressB;
    bool anyHeld = (btnA == LOW || btnB == LOW);

    // ---- Durum Makinesi ----
    switch (state) {

        // ======================================
        //  ANA MENÜ
        // ======================================
        case MENU:
            drawMenu();
            if (anyPress) {
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

            // === Joystick → Oyuncu Hareketi ===
            int rawJx = analogRead(JOY_X);
            int jx = rawJx - joyCenterX;
            if (abs(jx) > DEADZONE) {
                float factor = (float)(abs(jx) - DEADZONE) / (float)(2048 - DEADZONE);
                if (factor > 1.0f) factor = 1.0f;
                float spd = 1.0f + factor * 2.5f;    // 1.0 ~ 3.5 piksel/kare
                playerX += (jx > 0) ? spd : -spd;
            }
            // Ekran sınırları
            if (playerX < PLAYER_W / 2 + 1) playerX = PLAYER_W / 2 + 1;
            if (playerX > SCR_W - PLAYER_W / 2 - 1) playerX = SCR_W - PLAYER_W / 2 - 1;

            // === Oyuncu Ateşi (basılı tutunca 250ms aralıkla) ===
            if (anyHeld && now - lastFireMs >= FIRE_COOLDOWN) {
                // Aktif mermi sayısını kontrol et
                int activePB = 0;
                for (int i = 0; i < MAX_PBULLETS; i++)
                    if (pBullets[i].active) activePB++;

                if (activePB < MAX_PBULLETS) {
                    for (int i = 0; i < MAX_PBULLETS; i++) {
                        if (!pBullets[i].active) {
                            pBullets[i].x = playerX;
                            pBullets[i].y = PLAYER_Y - 2;
                            pBullets[i].active = true;
                            lastFireMs = now;
                            playSound(800, 25);
                            break;
                        }
                    }
                }
            }

            // === Oyuncu Mermilerini Güncelle ===
            for (int i = 0; i < MAX_PBULLETS; i++) {
                if (!pBullets[i].active) continue;
                pBullets[i].y -= PBULLET_SPD;

                // Ekrandan çıktı mı?
                if (pBullets[i].y < HUD_H) {
                    pBullets[i].active = false;
                    continue;
                }

                // Uzaylı çarpışma kontrolü
                bool hit = false;
                for (int r = 0; r < ALIEN_ROWS && !hit; r++) {
                    for (int c = 0; c < ALIEN_COLS && !hit; c++) {
                        if (!aliens[r][c]) continue;
                        float ax = getAlienX(c);
                        float ay = getAlienY(r);
                        // AABB çarpışma
                        if (pBullets[i].x >= ax - 1 &&
                            pBullets[i].x <= ax + ALIEN_W + 1 &&
                            pBullets[i].y >= ay &&
                            pBullets[i].y <= ay + ALIEN_H) {
                            // İsabet!
                            aliens[r][c] = false;
                            pBullets[i].active = false;
                            alienCount--;
                            score += ALIEN_POINTS[r % 4];
                            playSound(1200, 40);

                            // Kalan uzaylı azaldıkça hız artar
                            float baseSpd = 0.25f + (wave - 1) * 0.08f;
                            int total = ALIEN_ROWS * ALIEN_COLS;
                            float speedMul = 1.0f + (float)(total - alienCount) / (float)total * 2.5f;
                            alienSpeed = baseSpd * speedMul;
                            if (alienSpeed > 3.0f) alienSpeed = 3.0f;

                            hit = true;
                        }
                    }
                }
            }

            // === Tüm Uzaylılar Yok Edildi mi? (Dalga Geçişi) ===
            if (alienCount <= 0) {
                state = WAVE_CLEAR;
                waveClearMs = now;
                // Zafer sesi
                playSound(1047, 80);
                delay(90);
                playSound(1319, 80);
                delay(90);
                playSound(1568, 150);
                break;
            }

            // === Uzaylı Hareketi ===
            alienGridX += alienSpeed * alienDir;

            // Kenar kontrolü — canlı uzaylıların sınırlarına göre
            int leftC, rightC, topR, botR;
            getAlienBounds(leftC, rightC, topR, botR);

            if (rightC >= 0) {  // En az bir uzaylı hayatta
                float rightEdge = getAlienX(rightC) + ALIEN_W;
                float leftEdge  = getAlienX(leftC);

                if (rightEdge >= SCR_W - 1 && alienDir > 0) {
                    alienDir = -1;
                    alienGridY += ALIEN_DROP;
                }
                if (leftEdge <= 1 && alienDir < 0) {
                    alienDir = 1;
                    alienGridY += ALIEN_DROP;
                }

                // Oyun bitti kontrolü: uzaylılar oyuncuya ulaştı mı?
                float botEdge = getAlienY(botR) + ALIEN_H;
                if (botEdge >= PLAYER_Y - 2) {
                    state = GAMEOVER;
                    newRecord = (score > highScore);
                    if (newRecord) highScore = score;
                    gameOverMs = now;
                    // --- V2.1: Yüksek skoru NVS'e kaydet ---
                    { Preferences prefs; prefs.begin("os", false);
                      int32_t hs = prefs.getInt("hs_space", 0);
                      if (score > hs) prefs.putInt("hs_space", score);
                      prefs.end(); }
                    playSound(200, 300);
                    delay(150);
                    playSound(100, 400);
                    break;
                }
            }

            // Animasyon karesi değiştirme (500ms aralıkla)
            if (now - lastAnimMs > 500) {
                alienAnimFrame = !alienAnimFrame;
                lastAnimMs = now;
            }

            // === Düşman Ateşi ===
            // Ateş aralığı dalga arttıkça kısalır
            unsigned long eFireInterval = 2200;
            if (wave > 1) {
                unsigned long reduction = (unsigned long)(wave - 1) * 250UL;
                if (reduction >= 1400) reduction = 1400;
                eFireInterval -= reduction;
            }

            if (now - lastEFireMs > eFireInterval && alienCount > 0) {
                lastEFireMs = now;

                // Aktif düşman mermisi sayısı
                int activeEB = 0;
                for (int i = 0; i < MAX_EBULLETS; i++)
                    if (eBullets[i].active) activeEB++;

                if (activeEB < MAX_EBULLETS) {
                    // Rastgele bir kolon seç (canlı uzaylısı olan)
                    for (int tries = 0; tries < 20; tries++) {
                        int c = random(0, ALIEN_COLS);
                        // Bu kolondaki en alttaki canlı uzaylıyı bul
                        for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
                            if (aliens[r][c]) {
                                // Boş mermi yuvası bul ve ateş et
                                for (int b = 0; b < MAX_EBULLETS; b++) {
                                    if (!eBullets[b].active) {
                                        eBullets[b].x = getAlienX(c) + ALIEN_W / 2.0f;
                                        eBullets[b].y = getAlienY(r) + ALIEN_H + 1;
                                        eBullets[b].active = true;
                                        tries = 99; // Dış döngüden çık
                                        r = -1;     // İç döngüden çık
                                        break;
                                    }
                                }
                                break;  // Bu kolonda en alttakini bulduk
                            }
                        }
                    }
                }
            }

            // === Düşman Mermilerini Güncelle ===
            for (int i = 0; i < MAX_EBULLETS; i++) {
                if (!eBullets[i].active) continue;
                eBullets[i].y += EBULLET_SPD;

                // Ekrandan çıktı mı?
                if (eBullets[i].y > SCR_H) {
                    eBullets[i].active = false;
                    continue;
                }

                // Oyuncu çarpışma kontrolü (dokunulmazlık kontrolü)
                bool invincible = (now - hitTime < INVINCIBLE_MS);
                if (!invincible) {
                    if (eBullets[i].x >= playerX - PLAYER_W / 2 - 1 &&
                        eBullets[i].x <= playerX + PLAYER_W / 2 + 1 &&
                        eBullets[i].y >= PLAYER_Y &&
                        eBullets[i].y <= PLAYER_Y + PLAYER_H) {
                        // Oyuncu vuruldu!
                        eBullets[i].active = false;
                        lives--;
                        hitTime = now;
                        playSound(200, 150);

                        if (lives <= 0) {
                            state = GAMEOVER;
                            newRecord = (score > highScore);
                            if (newRecord) highScore = score;
                            gameOverMs = now;
                            // --- V2.1: Yüksek skoru NVS'e kaydet ---
                            { Preferences prefs; prefs.begin("os", false);
                              int32_t hs = prefs.getInt("hs_space", 0);
                              if (score > hs) prefs.putInt("hs_space", score);
                              prefs.end(); }
                            delay(100);
                            playSound(100, 300);
                            break;
                        }
                    }
                }
            }

            // Oyun durumu değiştiyse çizime geçme
            if (state != PLAYING) break;

            // ============================
            //  SAHNE ÇİZİMİ (Sprite'a)
            // ============================
            canvas.fillSprite(COL_SPACE);
            drawStars();

            // Uzaylılar
            for (int r = 0; r < ALIEN_ROWS; r++)
                for (int c = 0; c < ALIEN_COLS; c++)
                    if (aliens[r][c])
                        drawAlien(r, c);

            // Oyuncu mermileri
            for (int i = 0; i < MAX_PBULLETS; i++) {
                if (pBullets[i].active) {
                    canvas.fillRect((int)pBullets[i].x - 1, (int)pBullets[i].y, 2, 5, COL_PBULLET);
                }
            }

            // Düşman mermileri
            for (int i = 0; i < MAX_EBULLETS; i++) {
                if (eBullets[i].active) {
                    canvas.fillRect((int)eBullets[i].x - 1, (int)eBullets[i].y, 2, 5, COL_EBULLET);
                }
            }

            // Oyuncu
            drawPlayer();

            // HUD (en üstte)
            drawHUD();

            // Sprite'ı ekrana bas
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
            if (pressB && now - gameOverMs > 600) {
                returnToOS();
            }
            break;

        // ======================================
        //  PAUSE MENÜSÜ
        // ======================================
        case PAUSE:
            // Çizime devam et ama mantığı dondur
            canvas.fillSprite(COL_SPACE);
            drawStars();
            
            // Uzaylılar
            for (int r = 0; r < ALIEN_ROWS; r++)
                for (int c = 0; c < ALIEN_COLS; c++)
                    if (aliens[r][c])
                        drawAlien(r, c);
                        
            // Oyuncu mermileri
            for (int i = 0; i < MAX_PBULLETS; i++) {
                if (pBullets[i].active) {
                    canvas.fillRect((int)pBullets[i].x - 1, (int)pBullets[i].y, 2, 5, COL_PBULLET);
                }
            }
            // Düşman mermileri
            for (int i = 0; i < MAX_EBULLETS; i++) {
                if (eBullets[i].active) {
                    canvas.fillRect((int)eBullets[i].x - 1, (int)eBullets[i].y, 2, 5, COL_EBULLET);
                }
            }
            // Oyuncu
            drawPlayer();
            // HUD
            drawHUD();
            
            // Üzerine karartma ve menü
            canvas.fillRect(30, 40, 100, 50, COL_SPACE);
            canvas.drawRect(30, 40, 100, 50, TFT_WHITE);
            
            canvas.setTextSize(1);
            canvas.setTextColor(TFT_WHITE);
            canvas.setCursor(65, 48); canvas.print("PAUSE");
            canvas.setTextColor(TFT_YELLOW);
            canvas.setCursor(42, 62); canvas.print("[A] Devam Et");
            canvas.setCursor(42, 74); canvas.print("[B] OS Menu");
            
            canvas.pushSprite(0, 0);
            
            if (pressA) {
                playSound(800, 50);
                delay(200);
                state = PLAYING;
                lastFrameMs = millis();
            }
            if (pressB) {
                playSound(400, 50);
                delay(200);
                returnToOS();
            }
            break;
    }
}
