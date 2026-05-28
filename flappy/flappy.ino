// ============================================================
//  EMİR OS — FLAPPY BIRD
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanlı çift tamponlama (Flicker-Free)
//
//  Kontroller:
//    BTN_A  -> Zıplama
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
#define BTN_C    4     // OS'a dönüş (her zaman rezerve)
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
#define BIRD_X       25        // Kuşun sabit X pozisyonu
#define BIRD_R       5         // Kuş yarıçapı
#define GRAVITY      0.4f      // Yerçekimi ivmesi
#define JUMP_VEL    -3.0f      // Zıplama hızı (negatif = yukarı)
#define PIPE_W       16        // Boru genişliği
#define PIPE_GAP     36        // Borular arası dikey boşluk
#define BASE_SPEED   1.5f      // Başlangıç boru hızı
#define MAX_SPEED    3.2f      // Maksimum boru hızı
#define NUM_PIPES    3         // Ekrandaki boru sayısı
#define PIPE_DIST    56        // Borular arası yatay mesafe
#define GROUND_H     12        // Zemin yüksekliği
#define GROUND_Y     (SCR_H - GROUND_H)  // Zemin Y koordinatı
#define TARGET_FPS   30
#define FRAME_MS     (1000 / TARGET_FPS)

// ============ Özel Renkler (RGB565) ============
// Gökyüzü: Açık mavi (100, 180, 255) -> 0x65BF
#define COL_SKY       0x65BF
// Kuş: Sarı (255, 220, 0) -> 0xFEE0
#define COL_BIRD      0xFEE0
// Kanat: Turuncu (255, 165, 0) -> 0xFD20
#define COL_WING      0xFD20
// Gaga: Kırmızı-turuncu
#define COL_BEAK      0xFB20
// Boru: Koyu yeşil (34, 139, 34) -> 0x2444
#define COL_PIPE      0x2444
// Boru dudağı: Açık yeşil (50, 205, 50) -> 0x3666
#define COL_PIPE_LIP  0x3666
// Zemin: Kahverengi (101, 67, 33) -> 0x6204
#define COL_GROUND    0x6204
// Çimen: Parlak yeşil
#define COL_GRASS     0x07E0
// Panel arka plan
#define COL_PANEL     0x2104

// ============ Nesneler ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Oyun Durumları ============
enum GameState { MENU, PLAYING, GAMEOVER, PAUSE };
GameState state = MENU;

// Kuş değişkenleri
float birdY, birdVel;

// Boru yapısı
struct Pipe {
    float x;
    int gapCenter;   // Boşluğun merkez Y koordinatı
    bool scored;
};
Pipe pipes[NUM_PIPES];

// Skor sistemi
int score = 0;
int highScore = 0;
bool newRecord = false;

// Zamanlama
unsigned long lastFrameMs = 0;
unsigned long gameOverMs  = 0;

// Buton durumu (edge detection)
int prevBtnA = HIGH;

// V2.1: Ses Ayarı
bool soundEnabled = true;
void playSound(uint16_t freq, uint32_t dur) {
    if (soundEnabled) { tone(BUZZER, freq, dur); }
}

// Bulut animasyonu
int cloudOffset = 0;

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
//  Boruları Başlat
// ==========================================
void initPipes() {
    for (int i = 0; i < NUM_PIPES; i++) {
        pipes[i].x = SCR_W + 30 + i * PIPE_DIST;
        pipes[i].gapCenter = random(PIPE_GAP / 2 + 10, GROUND_Y - PIPE_GAP / 2 - 10);
        pipes[i].scored = false;
    }
}

// ==========================================
//  Oyunu Sıfırla
// ==========================================
void resetGame() {
    birdY   = SCR_H / 2.0f;
    birdVel = 0;
    score   = 0;
    newRecord = false;
    cloudOffset = 0;
    initPipes();
    state = PLAYING;
}

// ==========================================
//  Çizim: Kuş
// ==========================================
void drawBird(int y) {
    // Gövde (sarı daire)
    canvas.fillCircle(BIRD_X, y, BIRD_R, COL_BIRD);

    // Kanat (hıza göre pozisyon değişir)
    int wingY = y + (birdVel < 0 ? 2 : -1);
    canvas.fillCircle(BIRD_X - 3, wingY, 3, COL_WING);

    // Göz
    canvas.fillCircle(BIRD_X + 3, y - 2, 2, TFT_WHITE);
    canvas.fillCircle(BIRD_X + 4, y - 2, 1, TFT_BLACK);

    // Gaga
    canvas.fillTriangle(
        BIRD_X + BIRD_R, y,
        BIRD_X + BIRD_R + 5, y + 2,
        BIRD_X + BIRD_R, y + 4,
        COL_BEAK
    );
}

// ==========================================
//  Çizim: Boru
// ==========================================
void drawPipe(Pipe &p) {
    int px = (int)p.x;
    if (px > SCR_W + 5 || px + PIPE_W < -5) return;

    int gapTop = p.gapCenter - PIPE_GAP / 2;
    int gapBot = p.gapCenter + PIPE_GAP / 2;

    // === Üst Boru ===
    if (gapTop > 0) {
        // Gövde
        canvas.fillRect(px, 0, PIPE_W, gapTop, COL_PIPE);
        // Dudak (biraz daha geniş)
        int lipY = gapTop - 5;
        if (lipY < 0) lipY = 0;
        canvas.fillRect(px - 2, lipY, PIPE_W + 4, gapTop - lipY, COL_PIPE_LIP);
        canvas.drawRect(px - 2, lipY, PIPE_W + 4, gapTop - lipY, COL_PIPE);
        // Parlaklık çizgisi
        int hlH = lipY;
        if (hlH > 0) canvas.drawFastVLine(px + 2, 0, hlH, COL_PIPE_LIP);
    }

    // === Alt Boru ===
    if (gapBot < GROUND_Y) {
        // Gövde
        canvas.fillRect(px, gapBot, PIPE_W, GROUND_Y - gapBot, COL_PIPE);
        // Dudak
        int lipH = 5;
        if (gapBot + lipH > GROUND_Y) lipH = GROUND_Y - gapBot;
        canvas.fillRect(px - 2, gapBot, PIPE_W + 4, lipH, COL_PIPE_LIP);
        canvas.drawRect(px - 2, gapBot, PIPE_W + 4, lipH, COL_PIPE);
        // Parlaklık çizgisi
        int hlStart = gapBot + lipH;
        int hlH = GROUND_Y - hlStart;
        if (hlH > 0) canvas.drawFastVLine(px + 2, hlStart, hlH, COL_PIPE_LIP);
    }
}

// ==========================================
//  Çizim: Zemin
// ==========================================
void drawGround() {
    canvas.fillRect(0, GROUND_Y, SCR_W, GROUND_H, COL_GROUND);
    // Çimen dokusu
    for (int x = 0; x < SCR_W; x += 3) {
        int h = 2 + ((x % 7 == 0) ? 2 : 0);
        canvas.drawFastVLine(x, GROUND_Y, h, COL_GRASS);
    }
}

// ==========================================
//  Çizim: Bulutlar (yavaş kayar)
// ==========================================
void drawClouds() {
    int cx1 = 200 - (cloudOffset % 220);
    int cx2 = 330 - (cloudOffset % 350);

    // Bulut 1
    canvas.fillCircle(cx1,      18, 7, TFT_WHITE);
    canvas.fillCircle(cx1 + 10, 15, 9, TFT_WHITE);
    canvas.fillCircle(cx1 + 20, 19, 6, TFT_WHITE);

    // Bulut 2
    canvas.fillCircle(cx2,      34, 5, TFT_WHITE);
    canvas.fillCircle(cx2 + 8,  31, 7, TFT_WHITE);
    canvas.fillCircle(cx2 + 15, 35, 5, TFT_WHITE);
}

// ==========================================
//  Çizim: Skor (gölgeli beyaz metin)
// ==========================================
void drawScore() {
    char buf[8];
    sprintf(buf, "%d", score);
    // Ortala: her karakter setTextSize(2)'de 12px genişliğinde
    int sx = SCR_W / 2 - (strlen(buf) * 6);

    canvas.setTextSize(2);
    // Gölge
    canvas.setTextColor(TFT_BLACK);
    canvas.setCursor(sx + 1, 6);
    canvas.print(buf);
    // Metin
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(sx, 5);
    canvas.print(buf);
}

// ==========================================
//  Çarpışma Kontrolü
// ==========================================
bool checkCollision() {
    // Zemin veya tavan
    if (birdY + BIRD_R >= GROUND_Y || birdY - BIRD_R <= 0) {
        return true;
    }

    // Boru çarpışması
    for (int i = 0; i < NUM_PIPES; i++) {
        int px = (int)pipes[i].x;
        // Yatay örtüşme: kuşun sağ kenarı > borunun sol kenarı VE kuşun sol kenarı < borunun sağ kenarı
        if (BIRD_X + BIRD_R > px && BIRD_X - BIRD_R < px + PIPE_W) {
            int gapTop = pipes[i].gapCenter - PIPE_GAP / 2;
            int gapBot = pipes[i].gapCenter + PIPE_GAP / 2;
            // Kuş boşluğun dışındaysa çarpışma var
            if ((int)birdY - BIRD_R < gapTop || (int)birdY + BIRD_R > gapBot) {
                return true;
            }
        }
    }
    return false;
}

// ==========================================
//  Ekran: Ana Menü
// ==========================================
void drawMenu() {
    canvas.fillSprite(COL_SKY);
    drawClouds();
    drawGround();

    // Başlık gölgesi
    canvas.setTextSize(2);
    canvas.setTextColor(0x4208);
    canvas.setCursor(10, 11);
    canvas.print("FLAPPY BIRD");
    // Başlık
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(9, 10);
    canvas.print("FLAPPY BIRD");

    // Animasyonlu kuş (yukarı-aşağı salınım)
    float menuBirdY = 55.0f + sin(millis() / 200.0f) * 8.0f;
    birdVel = -1.0f; // Kanat yukarıda gösterilsin
    drawBird((int)menuBirdY);

    // Talimatlar
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(35, 80);
    canvas.print("[A] Basla");
    canvas.setTextColor(0xBDF7);
    canvas.setCursor(35, 95);
    canvas.print("[B] OS Menu");

    // En yüksek skor
    if (highScore > 0) {
        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(35, 70);
        canvas.print("Rekor: ");
        canvas.print(highScore);
    }

    canvas.pushSprite(0, 0);
}

// ==========================================
//  Ekran: Oyun Bitti
// ==========================================
void drawGameOver() {
    canvas.fillSprite(0x0000);

    // Panel çerçevesi
    canvas.fillRoundRect(15, 12, 130, 104, 5, COL_PANEL);
    canvas.drawRoundRect(15, 12, 130, 104, 5, TFT_RED);
    canvas.drawRoundRect(16, 13, 128, 102, 4, 0x8000);

    // Başlık
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    canvas.setCursor(20, 20);
    canvas.print("OYUN BITTI");

    // Skor bilgisi
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 48);
    canvas.print("Skor:   ");
    canvas.setTextColor(TFT_YELLOW);
    canvas.setTextSize(2);
    canvas.print(score);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 68);
    canvas.print("Rekor:  ");
    canvas.setTextColor(TFT_GREEN);
    canvas.setTextSize(2);
    canvas.print(highScore);

    // Yeni rekor bildirimi
    if (newRecord) {
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_MAGENTA);
        canvas.setCursor(30, 88);
        canvas.print("** YENI REKOR! **");
    }

    // Tekrar oyna
    canvas.setTextSize(1);
    canvas.setTextColor(0xBDF7);
    canvas.setCursor(30, 100);
    canvas.print("[A] Tekrar Oyna");
    
    canvas.setCursor(30, 112);
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
    tft.setRotation(1);   // Landscape modu: 160x128
    tft.fillScreen(TFT_BLACK);

    // Sprite tamponu oluştur (çift tamponlama / flicker-free)
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // Rastgele tohum (joystick analog gürültüsünden)
    randomSeed(analogRead(JOY_X) ^ (analogRead(JOY_Y) << 8) ^ micros());

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

    // ---- BTN_A: Kenar tespiti (rising edge) ----
    int btnA = digitalRead(BTN_A);
    bool btnA_press = (btnA == LOW && prevBtnA == HIGH);
    prevBtnA = btnA;

    // ---- Durum Makinesi ----
    switch (state) {

        // ======================================
        //  ANA MENÜ
        // ======================================
        case MENU:
            cloudOffset++;
            drawMenu();
            if (btnA_press) {
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
            // --- Fizik güncelle ---
            birdVel += GRAVITY;
            birdY += birdVel;

            // Zıplama
            if (btnA_press) {
                birdVel = JUMP_VEL;
                playSound(660, 30);
            }

            // Boru hızı (zorluk artışı)
            float spd = BASE_SPEED + score * 0.04f;
            if (spd > MAX_SPEED) spd = MAX_SPEED;

            // --- Boruları güncelle ---
            for (int i = 0; i < NUM_PIPES; i++) {
                pipes[i].x -= spd;

                // Skor sayma (kuş boruyu geçti mi?)
                if (!pipes[i].scored && pipes[i].x + PIPE_W < BIRD_X) {
                    pipes[i].scored = true;
                    score++;
                    playSound(1047, 30);  // Skor sesi
                }

                // Boru ekrandan çıktıysa geri dönüştür
                if (pipes[i].x + PIPE_W < -5) {
                    // En sağdaki boruyu bul
                    float maxX = 0;
                    for (int j = 0; j < NUM_PIPES; j++) {
                        if (pipes[j].x > maxX) maxX = pipes[j].x;
                    }
                    pipes[i].x = maxX + PIPE_DIST;
                    pipes[i].gapCenter = random(PIPE_GAP / 2 + 10, GROUND_Y - PIPE_GAP / 2 - 10);
                    pipes[i].scored = false;
                }
            }

            // Bulut animasyonu
            cloudOffset++;

            // --- Çarpışma kontrolü ---
            if (checkCollision()) {
                state = GAMEOVER;
                newRecord = (score > highScore);
                if (newRecord) highScore = score;
                gameOverMs = millis();
                // --- V2.1: Yüksek skoru NVS'e kaydet ---
                { Preferences prefs; prefs.begin("os", false);
                  int32_t hs = prefs.getInt("hs_flappy", 0);
                  if (score > hs) prefs.putInt("hs_flappy", score);
                  prefs.end(); }
                // Ölüm sesi (iki tonlu düşüş)
                playSound(300, 120);
                delay(130);
                playSound(150, 200);
                break;
            }

            // --- Sahne çizimi (sprite'a) ---
            canvas.fillSprite(COL_SKY);
            drawClouds();

            for (int i = 0; i < NUM_PIPES; i++) {
                drawPipe(pipes[i]);
            }

            drawGround();
            drawBird((int)birdY);
            drawScore();

            // Sprite'ı ekrana bas
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  OYUN BİTTİ
        // ======================================
        case GAMEOVER:
            drawGameOver();
            // 600ms bekle, sonra yeniden başlamaya izin ver
            if (btnA_press && (millis() - gameOverMs > 600)) {
                playSound(880, 50);
                resetGame();
            }
            if (!digitalRead(BTN_B) && (millis() - gameOverMs > 600)) {
                returnToOS();
            }
            break;

        // ======================================
        //  PAUSE MENÜSÜ
        // ======================================
        case PAUSE:
            // Arka planı olduğu gibi tutuyoruz
            canvas.fillSprite(COL_SKY);
            drawClouds();
            for (int i = 0; i < NUM_PIPES; i++) {
                drawPipe(pipes[i]);
            }
            drawGround();
            drawBird((int)birdY);
            drawScore();
            
            // Üzerine karartma ve menü
            canvas.fillRect(30, 40, 100, 50, COL_PANEL);
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
                lastFrameMs = millis();
            }
            if (!digitalRead(BTN_B)) {
                playSound(400, 50);
                delay(200);
                returnToOS();
            }
            break;
    }
}
