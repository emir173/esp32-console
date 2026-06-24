// ============================================================
//  EMİR OS — FLAPPY BIRD (60 FPS Delta-Time Edition)
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
#include <U8g2lib.h>
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
#include <Preferences.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"

// ============ Ekran Boyutları (Landscape) ============
#define SCR_W  160
#define SCR_H  128

// ============ Oyun Sabitleri ============
#define BIRD_X       25        // Kuşun sabit X pozisyonu
#define BIRD_R       5         // Kuş yarıçapı
// FPS60: Yerçekimi piksel/saniye^2 cinsinden (0.4 * 30 * 30 = 360)
#define GRAVITY      360.0f    // Yerçekimi ivmesi (piksel/sn²)
// FPS60: Zıplama hızı piksel/saniye cinsinden (-3.0 * 30 = -90)
#define JUMP_VEL    -90.0f     // Zıplama hızı (piksel/sn, negatif = yukarı)
#define PIPE_W       16        // Boru genişliği
#define PIPE_GAP     36        // Borular arası dikey boşluk
// FPS60: Boru hızı piksel/saniye cinsinden (1.5 * 30 = 45)
#define BASE_SPEED   45.0f     // Başlangıç boru hızı (piksel/sn)
// FPS60: Maksimum boru hızı piksel/saniye cinsinden (3.2 * 30 = 96)
#define MAX_SPEED    96.0f     // Maksimum boru hızı (piksel/sn)
#define NUM_PIPES    3         // Ekrandaki boru sayısı
#define PIPE_DIST    56        // Borular arası yatay mesafe
#define GROUND_H     12        // Zemin yüksekliği
#define GROUND_Y     (SCR_H - GROUND_H)  // Zemin Y koordinatı
// FPS60: TARGET_FPS sadece referans, frame kilidi kaldırıldı (Delta-Time kullanılıyor)
#define TARGET_FPS   60

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
#define COL_HUD_TEXT  0xBDF7  // Açık gri (menü yazıları)
#define COL_FLAPPY_SHADOW 0x4208  // Başlık gölge rengi (flappy özel)
#define COL_RED_DARK       0x8000  // Koyu kırmızı (game over panel iç çerçeve)

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

// ============ FPS Sayacı ============
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;

// Buton durumu (edge detection)
int prevBtnA = HIGH;

// V2.1: Ses Ayarı
bool soundEnabled = true;
// playSound — GameBase.h osPlaySound wrapper (eski API uyumu)
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// FPS60: Bulut offset float tipine çevrildi (delta-time ile kesirli birikim için)
float cloudOffset = 0.0f;

// ==========================================
//  OS'a Dönüş Fonksiyonu — GameBase.h wrapper
// ==========================================
void returnToOS() {
    osReturnToOS(tft);
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
    cloudOffset = 0.0f; // FPS60: float sıfırlama
    initPipes();
    state = PLAYING;
}

// ==========================================
//  Çizim: Kuş
// ==========================================
void drawBird(int x, int y) {
    // Gövde (sarı daire)
    canvas.fillCircle(x, y, BIRD_R, COL_BIRD);

    // Kanat (hıza göre pozisyon değişir)
    // FPS60: birdVel artık piksel/sn cinsinden, < 0 kontrolü çalışmaya devam eder
    int wingY = y + (birdVel < 0 ? 2 : -1);
    canvas.fillCircle(x - 3, wingY, 3, COL_WING);

    // Göz
    canvas.fillCircle(x + 3, y - 2, 2, TFT_WHITE);
    canvas.fillCircle(x + 4, y - 2, 1, TFT_BLACK);

    // Gaga
    canvas.fillTriangle(
        x + BIRD_R, y,
        x + BIRD_R + 5, y + 2,
        x + BIRD_R, y + 4,
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
    // FPS60: cloudOffset float olduğu için int'e cast edip mod al
    int off = (int)cloudOffset;
    int cx1 = 200 - (off % 220);
    int cx2 = 330 - (off % 350);

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
//  Çizim: Skor (gölgeli beyaz metin) ve FPS
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

    // FPS Sayacı (sağ taraf)
    char fpsStr[10];
    sprintf(fpsStr, "FPS:%d", currentFPS);
    int fpsWidth = strlen(fpsStr) * 6; // Her harf/rakam 6 piksel
    
    canvas.setTextSize(1);
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(SCR_W - fpsWidth - 2, 5);
    canvas.print("FPS:");
    canvas.setTextColor(TFT_GREEN);
    canvas.print(currentFPS);
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
    canvas.setTextColor(COL_FLAPPY_SHADOW);
    canvas.setCursor(15, 11);
    canvas.print("FLAPPY BIRD");
    // Başlık
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(14, 10);
    canvas.print("FLAPPY BIRD");

    // Animasyonlu kuş (yukarı-aşağı salınım) — millis() tabanlı, dt'den bağımsız
    float menuBirdY = 55.0f + sin(millis() / 200.0f) * 8.0f;
    birdVel = -1.0f; // Kanat yukarıda gösterilsin
    drawBird(SCR_W / 2, (int)menuBirdY);

    // Alt Menuler
    canvas.setTextSize(1);
    
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(10, 95);
    canvas.print("[A] Basla");

    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(85, 95);
    canvas.print("[B] OS Menu");

    // En yüksek skor
    char rekorBuf[20];
    sprintf(rekorBuf, "Rekor: %d", highScore);
    int rekorW = strlen(rekorBuf) * 6;
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(SCR_W / 2 - (rekorW / 2), 107);
    canvas.print(rekorBuf);

    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}

// ==========================================
//  Ekran: Oyun Bitti
// ==========================================
void drawGameOver() {
    canvas.fillSprite(TFT_BLACK);

    // Panel çerçevesi (Büyütüldü)
    canvas.fillRoundRect(15, 6, 130, 120, 5, COL_PANEL);
    canvas.drawRoundRect(15, 6, 130, 120, 5, TFT_RED);
    canvas.drawRoundRect(16, 7, 128, 118, 4, COL_RED_DARK);

    // Başlık
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    canvas.setCursor(20, 14);
    canvas.print("OYUN BITTI");

    // Skor bilgisi
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 48);
    canvas.print("Skor:   ");
    canvas.setTextColor(TFT_YELLOW);
    canvas.setTextSize(2);
    canvas.setCursor(75, 42);
    canvas.print(score);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 68);
    canvas.print("Rekor:  ");
    canvas.setTextColor(TFT_GREEN);
    canvas.setTextSize(2);
    canvas.setCursor(75, 62);
    canvas.print(highScore);

    // Yeni rekor bildirimi
    if (newRecord) {
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_MAGENTA);
        canvas.setCursor(47, 88);
        canvas.print("YENI REKOR!");
    }

    // Tekrar oyna
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
    tft.setRotation(1);   // Landscape modu: 160x128
    // TFT donanimini RGB moduna gecir (standart RGB565 sabitler icin)
    tft.startWrite();
    tft.writecommand(0x36);  // MADCTL
    tft.writedata(0xA0);     // MY|MV, BGR bit kapali (RGB modu)
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    // Sprite tamponu oluştur (çift tamponlama / flicker-free)
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // Rastgele tohum (joystick analog gürültüsünden)
    randomSeed(analogRead(JOY_X) ^ (analogRead(JOY_Y) << 8) ^ micros());

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

    // FPS60: Delta-Time hesaplama (frame kilidi kaldırıldı)
    unsigned long now = millis();
    float dt = (now - lastFrameMs) / 1000.0f;
    // FPS60: Lag spike koruması — dt'yi 50ms ile sınırla (en kötü 20 FPS)
    if (dt > 0.05f) dt = 0.05f;
    lastFrameMs = now;

    // ---- BTN_A: Kenar tespiti (rising edge) ----
    int btnA = digitalRead(BTN_A);
    bool btnA_press = (btnA == LOW && prevBtnA == HIGH);
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
            // FPS60: Bulut kayması delta-time ile (orijinalde frame başına 1 birim = 30 birim/sn)
            cloudOffset += 30.0f * dt;
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
            // --- Fizik güncelle (FPS60: dt ile çarpıldı) ---
            birdVel += GRAVITY * dt;
            birdY += birdVel * dt;

            // Zıplama
            if (btnA_press) {
                birdVel = JUMP_VEL; // FPS60: JUMP_VEL artık piksel/sn (-90)
                playSound(660, 30);
            }

            // Boru hızı (zorluk artışı)
            // FPS60: Hız artışı piksel/sn cinsinden (0.04 * 30 = 1.2)
            float spd = BASE_SPEED + score * 1.2f;
            if (spd > MAX_SPEED) spd = MAX_SPEED;

            // --- Boruları güncelle (FPS60: dt ile çarpıldı) ---
            for (int i = 0; i < NUM_PIPES; i++) {
                pipes[i].x -= spd * dt;

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

            // FPS60: Bulut animasyonu delta-time ile
            cloudOffset += 30.0f * dt;

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
            drawBird(BIRD_X, (int)birdY);
            drawScore();

            // Sprite'ı ekrana bas
            checkScreenshot(canvas);
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
            drawBird(BIRD_X, (int)birdY);
            drawScore();
            
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
                delay(200);
                state = PLAYING;
                // FPS60: Pause'dan çıkarken dt sıfırlaması için timestamp güncelle
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
