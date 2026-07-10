// ============================================================
//  E-OS — 2048 v1.0
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite-based double buffering, 60 FPS
//
//  Controls:
//    JOY_X/Y -> Slide tiles
//    JOY_SW  -> Pause
//    BTN_A   -> Start / Restart
//    BTN_B   -> Return to OS Launcher
//    BTN_D   -> Keep going (win ekraninda devam)
// ============================================================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"
#include "Config.h"
#include "Board.h"
#include "Renderer.h"
#include "../SharedJoystick.h"

// ============ Display Objects ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Game Instances ============
Board2048 board;
JoystickProcessor joystick;

// ============ Game State ============
GameState state = ST_MENU;
long score = 0;
long best = 0;
long bestAtRunStart = 0;   // NEW BEST rozeti icin run baslangic rekoru
bool newRecord = false;
bool wonShown = false;     // 2048 win ekrani bir kez gosterilir

// ============ Move Animation State ============
AnimTile anims[16];
uint8_t animCount = 0;
uint8_t pendingGrid[BOARD_N][BOARD_N];   // animasyon bitince uygulanacak tahta
int pendingGain = 0;
unsigned long animStartMs = 0;

// ============ Juice State ============
unsigned long mergeMs[BOARD_N][BOARD_N];  // birlesme pop baslangici
unsigned long spawnMs[BOARD_N][BOARD_N];  // yeni karo buyume baslangici
int gainVal = 0;                          // paneldeki "+N"
unsigned long gainMs = 0;

// ============ Timing ============
unsigned long lastFrameMs = 0;
unsigned long gameOverMs = 0;

// ============ FPS Counter ============
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;
bool showFps = false;

// ============ Buttons ============
int prevBtnA = HIGH;
int prevBtnD = HIGH;
int lastFrameDir = -1;     // joystick kenar tespiti (deadzone -> yon)

// ============ Sound ============
bool soundEnabled = true;
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// ============================================================
//  Standalone korumasi: diger OTA bolumunde GECERLI bir uygulama
//  (OS launcher) var mi? Arduino IDE ile tek basina yuklemede
//  oyun ota_0'a yazilir ve ota_1 BOS kalir; bos bolume boot
//  bayragi yazmak bootloader panic/bootloop'a yol acar.
// ============================================================
bool osPartitionValid() {
    const esp_partition_t *osPart = esp_ota_get_next_update_partition(NULL);
    if (osPart == NULL) return false;
    esp_app_desc_t desc;
    return esp_ota_get_partition_description(osPart, &desc) == ESP_OK;
}

void saveBestIfRecord() {
    if (score > best) {
        best = score;
        osSaveHighScore("hs_2048", (int)best);
    }
}

void returnToOS() {
    saveBestIfRecord();   // yarim kalan run'in rekoru kaybolmasin
    if (osPartitionValid()) {
        osReturnToOS(tft, soundEnabled);
        return;   // (ulasilmaz — osReturnToOS restart eder)
    }
    // Standalone mod: donulecek OS yok — bilgi goster, menude kal
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(14, 60);
    tft.print("Standalone: no OS");
    delay(STANDALONE_MSG_MS);
    tft.fillScreen(TFT_BLACK);
    state = ST_MENU;
}

// ============================================================
//  Reset game
// ============================================================
void resetGame() {
    board.clear();
    memset(mergeMs, 0, sizeof(mergeMs));
    memset(spawnMs, 0, sizeof(spawnMs));
    score = 0;
    bestAtRunStart = best;
    newRecord = false;
    wonShown = false;
    gainVal = 0;
    animCount = 0;

    uint8_t r, c;
    board.spawn(r, c); spawnMs[r][c] = millis();
    board.spawn(r, c); spawnMs[r][c] = millis();

    joystick.reset();
    state = ST_PLAYING;
}

// ============================================================
//  Hamle baslat: kaydirma animasyonuna gec
// ============================================================
void startMove(int dir) {
    if (!board.slide(dir, pendingGrid, anims, animCount, pendingGain)) return;
    playSound(NOTE_F4, 20);
    animStartMs = millis();
    state = ST_ANIM;
}

// ============================================================
//  Animasyon bitti: tahtayi uygula, karo dogur, win/lose kontrol
// ============================================================
void finalizeMove() {
    unsigned long now = millis();
    memcpy(board.grid, pendingGrid, sizeof(board.grid));

    if (pendingGain > 0) {
        score += pendingGain;
        gainVal = pendingGain;
        gainMs = now;

        uint8_t biggest = 0;
        for (uint8_t i = 0; i < animCount; i++) {
            if (anims[i].merge) {
                mergeMs[anims[i].tr][anims[i].tc] = now;
                if (anims[i].v + 1 > biggest) biggest = anims[i].v + 1;
            }
        }
        playSound(biggest >= 7 ? NOTE_E5 : NOTE_C5, 30);   // 128+ daha parlak
    }

    uint8_t r, c;
    if (board.spawn(r, c)) spawnMs[r][c] = now;
    animCount = 0;
    state = ST_PLAYING;

    // 2048'e ilk ulasma — win ekrani (D ile devam edilebilir)
    if (!wonShown && board.maxExp() >= 11) {
        wonShown = true;
        saveBestIfRecord();
        playSound(NOTE_C5, 50); delay(60);
        playSound(NOTE_E5, 50); delay(60);
        playSound(NOTE_G5, 40);
        state = ST_WIN;
        gameOverMs = now;
        return;
    }

    if (!board.canMove()) {
        newRecord = (score > bestAtRunStart && score > 0);
        saveBestIfRecord();
        playSound(NOTE_E3, 90); delay(100);
        playSound(NOTE_E3, 120);
        state = ST_GAMEOVER;
        gameOverMs = now;
    }
}

// ============================================================
//  Oyun sahnesi (tahta + panel) — birden cok state kullanir
// ============================================================
void drawScene(unsigned long now) {
    drawBoardBg(canvas);
    drawStaticTiles(canvas, board.grid, mergeMs, spawnMs, now);
    drawPanel(canvas, score, best, board.maxExp(), gainVal, gainMs, now);
    if (showFps) {
        canvas.setTextSize(1);
        canvas.setTextColor(COL_GRAY_TXT);
        canvas.setCursor(PANEL_X, 118);
        canvas.print(currentFPS);
    }
}

// ============================================================
//  SETUP — E-OS zorunlu boilerplate (adim sirasi degistirilmez)
// ============================================================
void setup() {
    // 1) Buzzer sustur (reset sonrasi cizirti onler)
    osInitBuzzer();

    // 2) I2C baslat + OLED kapat (acilis flicker onleme)
    Wire.begin(I2C_SDA, I2C_SCL);
    osOLEDOff();

    // 3) Guvenlik: elektrik kesintisinde OS'tan basla.
    //    SADECE diger bolumde gecerli bir OS imaji varsa boot bayragi
    //    degistirilir (standalone yuklemede bootloop engellenir).
    if (osPartitionValid()) {
        const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
        esp_ota_set_boot_partition(os_part);
    }

    // 4) Buton pinleri
    osInitButtons();

    // 5) NVS'ten ayarlar + rekor
    {
        Preferences prefs;
        prefs.begin("os", true);
        soundEnabled = prefs.getBool("sound_en", true);
        showFps = prefs.getBool("show_fps", false);
        prefs.end();
    }
    best = osLoadHighScore("hs_2048", 0);

    // 6) SPI + Dev Tools (USB screenshot)
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
    initDevTools(tft);

    // 7) TFT baslat (E-OS standart yonlendirme)
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    tft.startWrite();
    tft.writecommand(0x36);
    tft.writedata(0xA0);
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // 8) Joystick kalibrasyonu
    bool warningShown = false;
    while (analogRead(JOY_X) < 1400 || analogRead(JOY_X) > 2600 ||
           analogRead(JOY_Y) < 1400 || analogRead(JOY_Y) > 2600) {
        if (!warningShown) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED); tft.setTextSize(1);
            tft.setCursor(23, 60); tft.print("RELEASE JOYSTICK!");
            warningShown = true;
        }
        delay(50);
    }
    if (warningShown) { tft.fillScreen(TFT_BLACK); delay(300); }

    int joyCenterX, joyCenterY;
    osCalibrateJoystick(joyCenterX, joyCenterY);
    joystick.init(joyCenterX, joyCenterY);

    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    state = ST_MENU;
    lastFrameMs = millis();
    fpsStartTime = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    // ---- JOY_SW: Pause ----
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw && state == ST_PLAYING) {
        state = ST_PAUSE;
        playSound(NOTE_G4, 50);
    }
    prevJoySw = currJoySw;

    // ---- Frame rate control ----
    unsigned long now = millis();
    if (now - lastFrameMs < FRAME_MS) return;
    lastFrameMs = now;

    // ---- FPS ----
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // ---- Joystick: kenar tespiti (deadzone'dan yon'e veya yon degisimi) ----
    joystick.update();
    int dir = joystick.currentDir;
    bool dirEdge = (dir != -1 && dir != lastFrameDir);
    lastFrameDir = dir;

    // ---- Butonlar: kenar tespiti ----
    int btnA = digitalRead(BTN_A);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    prevBtnA = btnA;
    int btnD = digitalRead(BTN_D);
    bool pressD = (btnD == LOW && prevBtnD == HIGH);
    prevBtnD = btnD;

    switch (state) {

        // ======================================
        //  MAIN MENU
        // ======================================
        case ST_MENU: {
            if (pressA) {
                playSound(NOTE_E5, 50);
                resetGame();
                break;
            }
            if (!digitalRead(BTN_B)) {
                returnToOS();
                break;
            }
            drawMenu(canvas, best);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  PLAYING — girdi bekle
        // ======================================
        case ST_PLAYING: {
            if (dirEdge) {
                startMove(dir);
                if (state == ST_ANIM) break;   // animasyon bir sonraki karede cizilir
            }
            drawScene(now);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  ANIM — karolar kayiyor
        // ======================================
        case ST_ANIM: {
            float t = (float)(now - animStartMs) / (float)ANIM_MS;
            if (t >= 1.0f) {
                finalizeMove();
                drawScene(now);
                if (state == ST_WIN || state == ST_GAMEOVER) break; // overlay asagida cizilir
            } else {
                drawBoardBg(canvas);
                drawAnimTiles(canvas, anims, animCount, t);
                drawPanel(canvas, score, best, board.maxExp(), gainVal, gainMs, now);
            }
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  WIN — 2048'e ulasildi (D = devam)
        // ======================================
        case ST_WIN: {
            drawScene(now);
            char sbuf[12], bbuf[12];
            snprintf(sbuf, sizeof(sbuf), "%ld", score);
            snprintf(bbuf, sizeof(bbuf), "%ld", best);
            OsStat rows[2] = {
                { "Score", sbuf, TFT_WHITE, TFT_YELLOW },
                { "Best",  bbuf, TFT_WHITE, TFT_GREEN  },
            };
            osDrawGameOver(canvas, true, rows, 2, "[D] Keep Going", TFT_CYAN);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (now - gameOverMs > 600) {
                if (pressD) {
                    playSound(NOTE_D5, 40);
                    state = ST_PLAYING;
                } else if (pressA) {
                    playSound(NOTE_D5, 50);
                    resetGame();
                } else if (!digitalRead(BTN_B)) {
                    returnToOS();
                }
            }
            break;
        }

        // ======================================
        //  GAME OVER
        // ======================================
        case ST_GAMEOVER: {
            drawScene(now);
            char sbuf[12], tbuf[8], bbuf[12];
            snprintf(sbuf, sizeof(sbuf), "%ld", score);
            tileValueStr(board.maxExp(), tbuf, sizeof(tbuf));
            snprintf(bbuf, sizeof(bbuf), "%ld", best);
            OsStat rows[3] = {
                { "Score", sbuf, TFT_WHITE, TFT_YELLOW },
                { "Top",   tbuf, TFT_WHITE, COL_GOLD   },
                { "Best",  bbuf, TFT_WHITE, TFT_GREEN  },
            };
            osDrawGameOver(canvas, false, rows, 3, newRecord ? "NEW BEST!" : nullptr);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (pressA && now - gameOverMs > 600) {
                playSound(NOTE_D5, 50);
                resetGame();
            }
            if (!digitalRead(BTN_B) && now - gameOverMs > 600) {
                returnToOS();
            }
            break;
        }

        // ======================================
        //  PAUSE
        // ======================================
        case ST_PAUSE: {
            drawScene(now);
            osDrawPause(canvas, COL_GOLD);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (!digitalRead(BTN_A)) {
                playSound(NOTE_D5, 40);
                state = ST_PLAYING;
                lastFrameMs = millis();
            }
            if (!digitalRead(BTN_B)) {
                playSound(NOTE_G4, 50);
                returnToOS();
            }
            break;
        }
    }
}
