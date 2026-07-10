// ============================================================
//  E-OS TETRIS v1.0
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  128x64 OLED (SH1106, I2C) — skor/satir/seviye gostergesi
//  Sprite tabanli cift tamponlama (flicker-free, 60 FPS)
//
//  Mekanikler:
//    - 10x20 grid, 6x6 px hucre (sol tarafta 60x120 px alan)
//    - SRS dondurme + 5 testli wall kick
//    - 7-bag randomizer, hayalet parca, hard/soft drop
//    - Seviyeye gore artan dusme hizi, satir temizleme animasyonu
//
//  Kontroller:
//    Joystick SOL/SAG -> yatay hareket (EMA + deadzone + DAS)
//    Joystick ASAGI   -> soft drop
//    BTN_A  -> saat yonu dondur      BTN_C -> saat tersi dondur
//    BTN_D  -> hard drop             JOY_SW -> pause
//    BTN_B  -> OS Launcher'a don
// ============================================================
#include <TFT_eSPI.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"
#include "Config.h"
#include "Tetromino.h"
#include "Board.h"
#include "Renderer.h"

// ============ Ekran Nesneleri ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// ============ Oyun Nesneleri ============
Board board;
PieceBag bag;

// Aktif parca durumu
uint8_t curType = 0, curRot = 0;
int curX = 3, curY = 0;
uint8_t nextType = 0;
bool pieceActive = false;   // clearing/gameover sirasinda false

// ============ Oyun Durumu ============
GameState state = MENU;
int score = 0;
int highScore = 0;
int level = 1;
int linesTotal = 0;
bool newRecord = false;
int menuSel = 0;            // 0=BASLA, 1=CIKIS

// ============ Satir Temizleme Animasyonu ============
uint8_t fullRows[4];
int numFullRows = 0;
bool clearing = false;
unsigned long lineClearStart = 0;

// ============ Zamanlama ============
unsigned long lastFrameMs = 0;
unsigned long lastFallMs = 0;
unsigned long gameOverMs = 0;

// ============ Joystick (EMA + Deadzone + DAS) ============
int joyCenterX = 2048, joyCenterY = 2048;
float emaX = 0.0f, emaY = 0.0f;
int hDir = 0, vDir = 0;          // -1 / 0 / +1
int prevHDir = 0, prevVDir = 0;
unsigned long dasNextMs = 0;     // yatay oto-tekrar zamani

// ============ Butonlar (kenar tespiti) ============
int prevBtnA = HIGH, prevBtnC = HIGH, prevBtnD = HIGH;
bool prevJoySw = true;

// ============ FPS Sayaci ============
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;
bool showFps = false;

// ============ Ses & OS Donus ============
bool soundEnabled = true;
void playSound(uint16_t f, uint32_t d) { osPlaySound(f, d, soundEnabled); }
void returnToOS() { osReturnToOS(tft, soundEnabled); }

// ============================================================
//  ZAFER FANFARI — Do-Mi-Sol arpej, NON-BLOCKING
//  (delay() yasak oldugu icin millis() tabanli nota kuyrugu)
// ============================================================
struct Fanfare {
    bool active = false;
    uint8_t idx = 0;
    unsigned long nextMs = 0;

    void start() { active = true; idx = 0; nextMs = millis(); }

    void update() {
        if (!active) return;
        unsigned long now = millis();
        if (now < nextMs) return;
        switch (idx) {
            case 0: playSound(NOTE_C5, 50); nextMs = now + 60; break;   // Do5
            case 1: playSound(NOTE_E5, 50); nextMs = now + 60; break;   // Mi5
            case 2: playSound(NOTE_G5, 40); active = false;    break;   // Sol5 (tavan)
        }
        idx++;
    }
};
Fanfare fanfare;

// ============================================================
//  Seviyeye gore dusme araligi: max(50, 800 - (seviye-1)*70) ms
// ============================================================
unsigned long fallInterval() {
    long iv = (long)FALL_BASE_MS - (long)(level - 1) * (long)FALL_DEC_MS;
    if (iv < (long)FALL_MIN_MS) iv = (long)FALL_MIN_MS;
    return (unsigned long)iv;
}

// ============================================================
//  OLED guncelleme — SADECE degerler degistiginde I2C trafigi
//  yaratir (sendBuffer ~milisaniyeler surer, her frame cagirma!)
// ============================================================
void updateOLED(bool force) {
    static int lastScore = -1, lastLines = -1, lastLevel = -1, lastHs = -1;
    if (!force && score == lastScore && linesTotal == lastLines &&
        level == lastLevel && highScore == lastHs) return;
    lastScore = score;
    lastLines = linesTotal;
    lastLevel = level;
    lastHs = highScore;

    char buf[24];
    oled.clearBuffer();
    oled.setFont(u8g2_font_7x13B_tr);
    oled.drawStr(43, 12, "TETRIS");
    oled.setFont(u8g2_font_6x10_tr);
    snprintf(buf, sizeof(buf), "SCORE: %d", score);
    oled.drawStr(0, 26, buf);
    snprintf(buf, sizeof(buf), "LINES: %d", linesTotal);
    oled.drawStr(0, 38, buf);
    snprintf(buf, sizeof(buf), "LEVEL: %d", level);
    oled.drawStr(0, 50, buf);
    snprintf(buf, sizeof(buf), "HS: %d", highScore);
    oled.drawStr(0, 62, buf);
    oled.sendBuffer();
}

// ============================================================
//  Yeni parca cikar (torbadan). Spawn aninda carpisma = oyun bitti.
// ============================================================
void doGameOver();   // ileri bildirim

void spawnPiece() {
    curType = nextType;
    curRot = 0;
    curX = 3;
    curY = -1;               // ust satir kismen tavan ustunde baslar
    nextType = bag.next();
    pieceActive = true;
    if (board.collides(curType, curRot, curX, curY)) {
        doGameOver();
    }
}

// ============================================================
//  Oyun bitti: rekor kaydet, ses cal
// ============================================================
void doGameOver() {
    state = GAMEOVER;
    pieceActive = false;
    clearing = false;
    newRecord = (score > highScore);
    if (newRecord) {
        highScore = score;
        osSaveHighScore("hs_tetris", highScore);
    }
    gameOverMs = millis();
    playSound(NOTE_E3, 120);   // olum sesi (en alcak ton)
}

// ============================================================
//  Parcayi kilitle: satir kontrolu -> animasyon veya yeni parca
// ============================================================
void lockPiece() {
    if (!board.lockPiece(curType, curRot, curX, curY)) {
        doGameOver();          // tavan ustunde kilitlendi (top-out)
        return;
    }
    pieceActive = false;

    numFullRows = board.findFullRows(fullRows);
    if (numFullRows > 0) {
        clearing = true;
        lineClearStart = millis();
        if (numFullRows == 4) fanfare.start();      // TETRIS! zafer arpeji
        else playSound(NOTE_E5, 40);                // ding!
    } else {
        spawnPiece();
        lastFallMs = millis();
    }
}

// ============================================================
//  Animasyon bitti: satirlari sil, skor/seviye guncelle
// ============================================================
void finishLineClear() {
    board.collapseRows(fullRows, numFullRows);
    score += SCORE_TABLE[numFullRows] * level;
    linesTotal += numFullRows;
    int newLevel = 1 + linesTotal / LINES_PER_LEVEL;
    if (newLevel > MAX_LEVEL) newLevel = MAX_LEVEL;
    level = newLevel;
    clearing = false;
    numFullRows = 0;
    spawnPiece();
    lastFallMs = millis();
}

// ============================================================
//  Yatay hareket dene (basarirsa ses)
// ============================================================
bool tryMove(int dx) {
    if (board.collides(curType, curRot, curX + dx, curY)) return false;
    curX += dx;
    playSound(NOTE_F4, 20);
    return true;
}

// ============================================================
//  SRS dondurme + 5 wall kick testi
// ============================================================
void tryRotate(bool cw) {
    if (curType == PIECE_O) return;   // O parcasi donmez
    uint8_t newRot = (uint8_t)((curRot + (cw ? 1 : 3)) & 3);
    const int8_t (*kicks)[2] = (curType == PIECE_I)
                                   ? KICK_I[curRot][cw ? 0 : 1]
                                   : KICK_JLSTZ[curRot][cw ? 0 : 1];
    for (int t = 0; t < 5; t++) {
        int nx = curX + kicks[t][0];
        int ny = curY + kicks[t][1];
        if (!board.collides(curType, newRot, nx, ny)) {
            curRot = newRot;
            curX = nx;
            curY = ny;
            playSound(NOTE_G4, 25);
            return;
        }
    }
    // 5 test de basarisiz -> donme iptal (sessiz)
}

// ============================================================
//  Hard drop: aninda yere indir (+2 puan/hucre) ve kilitle
// ============================================================
void hardDrop() {
    int dist = 0;
    while (!board.collides(curType, curRot, curX, curY + 1)) {
        curY++;
        dist++;
    }
    score += dist * HARD_DROP_PTS;
    playSound(NOTE_A3, 30);   // tok darbe
    lockPiece();
}

// ============================================================
//  Oyunu bastan baslat
// ============================================================
void resetGame() {
    board.clear();
    bag.reset();
    score = 0;
    linesTotal = 0;
    level = 1;
    newRecord = false;
    clearing = false;
    numFullRows = 0;
    emaX = 0.0f;
    emaY = 0.0f;
    dasNextMs = 0;
    nextType = bag.next();
    spawnPiece();
    state = PLAYING;
    lastFallMs = millis();
    updateOLED(true);
}

// ============================================================
//  Oyun sahnesi cizimi (PLAYING / PAUSE / GAMEOVER ortak)
// ============================================================
void drawGame(bool flashOn) {
    canvas.fillSprite(TFT_BLACK);
    drawBoardFrame(canvas);
    drawStack(canvas, board, fullRows, numFullRows, flashOn);

    if (pieceActive) {
        // Hayalet parca: dusecegi yeri hesapla
        int ghostY = curY;
        while (!board.collides(curType, curRot, curX, ghostY + 1)) ghostY++;
        if (ghostY > curY) drawGhost(canvas, curType, curRot, curX, ghostY);
        drawPiece(canvas, curType, curRot, curX, curY);
    }

    drawPanel(canvas, nextType, score, level, linesTotal, highScore,
              currentFPS, showFps);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    // 1) Buzzer sustur
    osInitBuzzer();

    // 2) OLED'i hemen kapat (acilis flicker onleme)
    Wire.begin(I2C_SDA, I2C_SCL);
    osOLEDOff();

    // 3) Guvenlik: elektrik kesintisinde OS'tan basla
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    // 4) Butonlar INPUT_PULLUP
    osInitButtons();

    // 5) NVS'ten ses ayari + rekor + FPS gostergesi
    {
        Preferences prefs;
        prefs.begin("os", true);
        soundEnabled = prefs.getBool("sound_en", true);
        highScore = prefs.getInt("hs_tetris", 0);
        showFps = prefs.getBool("show_fps", false);
        prefs.end();
    }

    // 6) SPI baslat
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    // 7) Gelistirici araclari (screenshot / USB video)
    initDevTools(tft);

    // 8) TFT baslat
    tft.init();
    tft.setRotation(1);   // Landscape 160x128
    tft.setSwapBytes(true);
    tft.startWrite();
    tft.writecommand(0x36);   // MADCTL (diger oyunlarla ayni panel yonu)
    tft.writedata(0xA0);
    tft.endWrite();
    tft.fillScreen(TFT_BLACK);

    // 9) OLED baslat (I2C hizini artir: sendBuffer suresi kisalir)
    oled.setBusClock(400000);
    oled.begin();

    // 10) Sprite tamponu + screenshot renk modu
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);
    setScreenshotMode(SCR_RGB_SWAP);

    // Joystick kalibrasyonu (once birakildigindan emin ol)
    bool warningShown = false;
    while (analogRead(JOY_X) < 1400 || analogRead(JOY_X) > 2600 ||
           analogRead(JOY_Y) < 1400 || analogRead(JOY_Y) > 2600) {
        if (!warningShown) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED);
            tft.setTextSize(1);
            tft.setCursor(23, 60);
            tft.print("RELEASE JOYSTICK!");
            warningShown = true;
        }
        delay(50);
    }
    if (warningShown) { tft.fillScreen(TFT_BLACK); delay(300); }
    osCalibrateJoystick(joyCenterX, joyCenterY);

    // Rastgele tohum (7-bag icin)
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    state = MENU;
    lastFrameMs = millis();
    fpsStartTime = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    // ---- JOY_SW: Pause'a gir (kenar tespiti). Devam artik [A] ile. ----
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw && state == PLAYING) {
        state = PAUSE;
        playSound(NOTE_G4, 40);
    }
    prevJoySw = currJoySw;

    // ---- Kare hizi siniri (60 FPS) ----
    unsigned long now = millis();
    if (now - lastFrameMs < FRAME_MS) return;
    lastFrameMs = now;

    // ---- FPS hesabi ----
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = (int)fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // ---- Zafer fanfari (non-blocking nota kuyrugu) ----
    fanfare.update();

    // ---- Joystick: EMA yumusatma + deadzone ----
    int rawX = analogRead(JOY_X) - joyCenterX;
    int rawY = analogRead(JOY_Y) - joyCenterY;
    emaX = EMA_ALPHA * (float)rawX + (1.0f - EMA_ALPHA) * emaX;
    emaY = EMA_ALPHA * (float)rawY + (1.0f - EMA_ALPHA) * emaY;
    hDir = (emaX > JOY_DEADZONE) ? 1 : (emaX < -JOY_DEADZONE) ? -1 : 0;
    vDir = (emaY > JOY_DEADZONE) ? 1 : (emaY < -JOY_DEADZONE) ? -1 : 0;

    // ---- Butonlar: kenar tespiti ----
    int bA = digitalRead(BTN_A);
    bool pressA = (bA == LOW && prevBtnA == HIGH);
    prevBtnA = bA;
    int bC = digitalRead(BTN_C);
    bool pressC = (bC == LOW && prevBtnC == HIGH);
    prevBtnC = bC;
    int bD = digitalRead(BTN_D);
    bool pressD = (bD == LOW && prevBtnD == HIGH);
    prevBtnD = bD;

    // ==========================================
    //  DURUM MAKINESI
    // ==========================================
    switch (state) {

        // ======================================
        //  ANA MENU
        // ======================================
        case MENU: {
            drawMenu(canvas, highScore);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (pressA) {
                playSound(NOTE_E5, 50);
                resetGame();
            }
            if (!digitalRead(BTN_B)) {
                returnToOS();
            }
            break;
        }

        // ======================================
        //  OYUN
        // ======================================
        case PLAYING: {
            if (clearing) {
                // Satir temizleme animasyonu: giris kilitli, sadece sure bekle
                if (now - lineClearStart >= LINE_CLEAR_MS) {
                    finishLineClear();
                }
            } else if (pieceActive) {
                // ---- Dondurme / hard drop ----
                if (pressA) tryRotate(true);    // saat yonu
                if (pressC) tryRotate(false);   // saat tersi
                if (pressD) hardDrop();
            }

            // hardDrop/lockPiece durumu degistirmis olabilir
            if (state == PLAYING && !clearing && pieceActive) {
                // ---- Yatay hareket: DAS (ilk basis + oto-tekrar) ----
                if (hDir != 0) {
                    if (prevHDir != hDir) {
                        tryMove(hDir);                       // ilk adim aninda
                        dasNextMs = now + DAS_DELAY_MS;
                    } else if (now >= dasNextMs) {
                        tryMove(hDir);                       // oto-tekrar
                        dasNextMs = now + DAS_REPEAT_MS;
                    }
                }

                // ---- Yercekimi (+ soft drop hizlandirmasi) ----
                bool softDrop = (vDir > 0);
                unsigned long iv = fallInterval();
                if (softDrop && SOFT_DROP_MS < iv) iv = SOFT_DROP_MS;

                if (now - lastFallMs >= iv) {
                    lastFallMs = now;
                    if (!board.collides(curType, curRot, curX, curY + 1)) {
                        curY++;
                        if (softDrop) playSound(NOTE_C4, 15);
                    } else {
                        lockPiece();
                    }
                }
            }

            // ---- Cizim ----
            bool flashOn = clearing &&
                (((now - lineClearStart) / LINE_CLEAR_FLASH_MS) % 2 == 0);
            drawGame(flashOn);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            // ---- OLED (sadece PLAYING'de ve deger degistiyse) ----
            if (state == PLAYING) updateOLED(false);

            break;
        }

        // ======================================
        //  OYUN BITTI
        // ======================================
        case GAMEOVER: {
            drawGame(false);
            drawGameOverPanel(canvas, score, linesTotal, highScore, newRecord);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (pressA && now - gameOverMs > GAMEOVER_LOCK_MS) {
                playSound(NOTE_D5, 50);   // restart
                resetGame();
            }
            if (!digitalRead(BTN_B) && now - gameOverMs > GAMEOVER_LOCK_MS) {
                returnToOS();
            }
            break;
        }

        // ======================================
        //  PAUSE
        // ======================================
        case PAUSE: {
            drawGame(false);
            drawPauseOverlay(canvas);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (pressA) {              // [A] Continue (diger oyunlarla tutarli)
                state = PLAYING;
                lastFallMs = millis();
                lastFrameMs = millis();
                playSound(NOTE_G4, 40);
            }
            if (!digitalRead(BTN_B)) {
                returnToOS();
            }
            break;
        }
    }

    // Kenar tespiti icin onceki joystick yonlerini sakla
    prevHDir = hDir;
    prevVDir = vDir;
}
