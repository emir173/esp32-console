// ============================================================
//  EMIR OS — MODERN SNAKE (YILAN) v2.0
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite-based double buffering + pre-rendered background
//  Partial snake redraw + conditional HUD for 60 FPS stability
//
//  Controls:
//    JOY_X/Y -> Direction
//    JOY_SW  -> Pause
//    BTN_A   -> Start / Restart
//    BTN_B   -> Return to OS Launcher
//    Buzzer  -> Sound effects
// ============================================================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"
#include "Config.h"
#include "Snake.h"
#include "Food.h"
#include "Particles.h"
#include "Renderer.h"
#include "../SharedJoystick.h"

// ============ Display Objects ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);
TFT_eSprite bgSprite = TFT_eSprite(&tft);

// ============ Game Instances ============
Snake snake;
FoodManager food;
ParticleSystem particles;
JoystickProcessor joystick;

// ============ Game State ============
GameState state = MENU;
int score = 0;
int highScore = 0;
int level = 1;
bool newRecord = false;

// ============ Juice State ============
ScreenShake shake;
unsigned long frozenUntil = 0;
unsigned long squashStartMs = 0;

// ============ Combo State ============
int comboCount = 0;
unsigned long comboExpireMs = 0;
unsigned long comboFlashEndMs = 0;

// ============ Power-Up State ============
float ghostCharge = 0.0f;        // 0-1 sarj
float magnetCharge = 0.0f;
unsigned long ghostActiveUntil = 0;
unsigned long magnetActiveUntil = 0;
int prevBtnC = HIGH;
int prevBtnD = HIGH;

// ============ Arena Shrink State ============
int arenaInset = 0;                       // 0 = tam alan, >0 = daraldi
unsigned long arenaStartMs = 0;           // daralma baslangiç referansi
unsigned long arenaLastShrinkMs = 0;      // son daralma zamani
unsigned long shrinkFlashEndMs = 0;       // daralma parciltma sonu

// ============ Theme State (Hamle 5A) ============
uint16_t g_theme[THEME_COLOR_COUNT] = {};  // setup'ta applyTheme(0) doldurur (NEON tema)
uint16_t* g_themeLUT = nullptr;           // PSRAM'de 5x16 LUT (setup'ta alloc)
int currentThemeIdx = 0;

// ============ OLED State (Hamle 5B) ============
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
uint8_t oledPrev[1024];                   // son gonderilen buffer (page-diff icin)
unsigned long oledLastUpdateMs = 0;
int oledLastState = -1;                   // state degisince tam gönder (sentinel)


// ============ Death Cinematic State (Hamle 6B) ============
bool deathCinematic = false;
int deathSegmentIdx = 0;
unsigned long deathNextSegmentMs = 0;
unsigned long deathCinematicStartMs = 0;
unsigned long deathFlashEndMs = 0;
unsigned long deathSegmentIntervalMs = DEATH_SEGMENT_INTERVAL_MS;

// ============ Score Popup ============
int popupX, popupY;
float popupTimer = 0.0f;
int lastFoodPts = 10;

// ============ Rendering Flags ============
bool needSnakeFullRedraw = true;
bool needBgRender = true;

// ============ Sound ============
bool soundEnabled = true;
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// ============ Async Tone Sweep (non-blocking combo break sound) ============
uint16_t sweepNotes[3] = {0, 0, 0};
int sweepCount = 0;
int sweepIndex = 0;
unsigned long sweepNextMs = 0;

void startSweep(uint16_t n0, uint16_t n1, uint16_t n2) {
    sweepNotes[0] = n0;
    sweepNotes[1] = n1;
    sweepNotes[2] = n2;
    sweepCount = 3;
    sweepIndex = 0;
    sweepNextMs = millis();
}

void updateSweep(unsigned long now) {
    if (sweepIndex < sweepCount && now >= sweepNextMs) {
        playSound(sweepNotes[sweepIndex], 30);
        sweepIndex++;
        sweepNextMs = now + 40;
    }
}

// ============ Timing ============
unsigned long lastFrameMs;
unsigned long lastMoveMs;
unsigned long gameOverMs;
unsigned long levelChangeMs = 0; // Seviye atlama zamani (tema vs)
unsigned long obstacleSpawnMs = 0; // Seviye 2 engel dogus zamani

// ============ FPS Counter ============
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;
bool showFps = false;

// ============ Button ============
int prevBtnA = HIGH;

// ============================================================
//  Return to OS Launcher
// ============================================================
void returnToOS() {
    osReturnToOS(tft, soundEnabled);
}

// ============================================================
//  Handle death (DRY — single point for all collision types)
// ============================================================
void handleDeath(int hitX, int hitY, uint16_t particleColor, int particleCount) {
    newRecord = (score > highScore);
    if (newRecord) {
        highScore = score;
        osSaveHighScore("hs_snake", score);
    }
    shake.trigger(SHAKE_DEATH);
    frozenUntil = millis() + HITSTOP_DEATH_MS;

    particles.spawn(
        hitX * GRID + (float)GRID / 2,
        OFFSET_Y + hitY * GRID + (float)GRID / 2,
        particleColor, particleCount);

    int segMs = DEATH_CINEMATIC_MS / snake.len;
    if (segMs < 15) segMs = 15;
    deathSegmentIntervalMs = segMs;

    playSound(NOTE_E3, 90);
    delay(100);
    playSound(NOTE_E3, 120);

    deathCinematic = true;
    deathSegmentIdx = 0;
    deathCinematicStartMs = millis();
    deathNextSegmentMs = millis() + deathSegmentIntervalMs;
    deathFlashEndMs = millis() + DEATH_FLASH_MS;
    state = PLAYING;
}

// ============================================================
//  OLED (Hamle 5B) — SH1106 128x64 ikinci ekran
//  U8g2 ile metin ciz (font hazir), sonra page-diff ile sadece
//  degisen sayfalari raw Wire gonder (60 FPS guvenli).
//  cap: state degisince 8 (tam), steady'de 2 sayfa/kare.
// ============================================================
void oledSendPage(uint8_t page, const uint8_t* data) {
    // SH1106: column offset=2, page=P, 128 byte
    Wire.beginTransmission(OLED_I2C_ADDR);
    Wire.write(0x00);                       // komut stream (Co=0, D=0)
    Wire.write(0x10);                       // column high (col=2 -> 0x10)
    Wire.write(0x02);                       // column low  (col=2 -> 0x02)
    Wire.write(0xB0 | page);                // page address
    Wire.endTransmission();
    // Data 64 byte'lik parcalar halinde (Wire TX buffer guvenli)
    for (int off = 0; off < 128; off += 64) {
        Wire.beginTransmission(OLED_I2C_ADDR);
        Wire.write(0x40);                   // data stream (Co=0, D=1)
        Wire.write(data + off, 64);
        Wire.endTransmission();
    }
}

void drawOLED() {
    // Kullanici istegi uzerine OLED gostergeleri iptal edildi.
    // Tum barlar TFT uzerine (HUD) toplandi.
}

// ============================================================
//  Reset game to initial state
// ============================================================
void resetGame() {
    level = 1;
    score = 0;
    newRecord = false;
    popupTimer = 0.0f;
    shake.reset();
    frozenUntil = 0;
    squashStartMs = 0;
    comboCount = 0;
    comboExpireMs = 0;
    comboFlashEndMs = 0;
    sweepCount = 0;
    sweepIndex = 0;
    ghostCharge = 0.0f;
    magnetCharge = 0.0f;
    ghostActiveUntil = 0;
    magnetActiveUntil = 0;
    arenaInset = 0;
    arenaStartMs = millis();
    arenaLastShrinkMs = arenaStartMs;
    shrinkFlashEndMs = 0;
    currentThemeIdx = 0;
    applyTheme(0);
    deathCinematic = false;
    deathSegmentIdx = 0;
    deathNextSegmentMs = 0;
    deathCinematicStartMs = 0;
    deathFlashEndMs = 0;

    snake.init(level);
    food.init();
    food.spawnFood(snake, level, 0, arenaInset);
    lastMoveMs = millis();
    levelChangeMs = millis();
    obstacleSpawnMs = 0;
    needBgRender = true;
    needSnakeFullRedraw = true;

    particles.clear();
    joystick.reset();

    state = PLAYING;
    lastMoveMs = millis();
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    // 1) Buzzer sustur (reset sonrasi cizirti onler)
    osInitBuzzer();

    // 2) I2C baslat + OLED kapat (acilis flicker onleme)
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);          // OLED I2C hizi (page-diff icin yeterli)
    osOLEDOff();

    // 3) Guvenlik: elektrik kesintisinde OS'tan basla (OTA partition)
    const esp_partition_t* os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    // 4) Buton pinleri ayarla
    osInitButtons();

    {
        Preferences prefs; prefs.begin("os", true);
        soundEnabled = prefs.getBool("sound_en", true);
        prefs.end();
    }
    {
        Preferences prefs; prefs.begin("os", true);
        highScore = prefs.getInt("hs_snake", 0);
        prefs.end();
    }

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    initDevTools(tft);

    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);

    {
        Preferences prefs;
        prefs.begin("os", true);
        showFps = prefs.getBool("show_fps", false);
        prefs.end();
    }

    tft.startWrite();
    tft.writecommand(0x36);
    tft.writedata(0xA0);
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    bgSprite.setColorDepth(16);
    bgSprite.createSprite(SCR_W, SCR_H);

    // Theme LUT -> PSRAM (5x16 = 160 byte). Alloc fail ederse THEME_DATA'ya fallback.
    g_themeLUT = (uint16_t*)heap_caps_malloc(THEME_COUNT * THEME_COLOR_COUNT * 2,
                                              MALLOC_CAP_SPIRAM);
    if (g_themeLUT) {
        memcpy(g_themeLUT, THEME_DATA, THEME_COUNT * THEME_COLOR_COUNT * 2);
    }
    applyTheme(0);

    // Joystick calibration
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

    long sumX = 0, sumY = 0;
    for (int j = 0; j < 10; j++) {
        sumX += analogRead(JOY_X);
        sumY += analogRead(JOY_Y);
        delay(2);
    }
    int joyCenterX = sumX / 10;
    int joyCenterY = sumY / 10;
    joystick.init(joyCenterX, joyCenterY);

    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    particles.clear();
    food.init();
    state = MENU;
    lastFrameMs = millis();
    fpsStartTime = millis();
    oledLastUpdateMs = 0;
    drawOLED();   // OLED'e ilk icerik hemen (boot flicker yok)
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    // ---- JOY_SW: Pause toggle ----
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {
        if (state == PLAYING && !deathCinematic) {
            state = PAUSE;
            playSound(NOTE_G4, 50);
        }
    }
    prevJoySw = currJoySw;

    // ---- Frame rate control ----
    unsigned long now = millis();
    if (now - lastFrameMs < FRAME_MS) return;
    float dt = (now - lastFrameMs) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    lastFrameMs = now;

    // ---- FPS Calculation ----
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // ---- Joystick update (all states — auto-calibration runs here) ----
    joystick.update();

    // ---- BTN_A: Edge detection ----
    int btnA = digitalRead(BTN_A);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    prevBtnA = btnA;

    // ---- BTN_C / BTN_D: Edge detection (power-up) ----
    int btnC = digitalRead(BTN_C);
    bool pressC = (btnC == LOW && prevBtnC == HIGH);
    prevBtnC = btnC;
    int btnD = digitalRead(BTN_D);
    bool pressD = (btnD == LOW && prevBtnD == HIGH);
    prevBtnD = btnD;

    // ==========================================
    //  STATE MACHINE
    // ==========================================
    switch (state) {

        // ======================================
        //  MAIN MENU
        // ======================================
        case MENU: {
            if (pressA) {
                playSound(NOTE_E5, 50);
                resetGame();
                break;
            }
            if (!digitalRead(BTN_B)) {
                returnToOS();
                break;
            }

            drawMenu(canvas, highScore);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  PLAYING
        // ======================================
        case PLAYING: {
            // ---- Ölüm sinematiği (Hamle 6B) — async, delay yok ----
            if (deathCinematic) {
                while (deathSegmentIdx < snake.len && now >= deathNextSegmentMs) {
                    int i = deathSegmentIdx;
                    particles.spawn(
                        snake.x[i] * GRID + (float)GRID / 2,
                        OFFSET_Y + snake.y[i] * GRID + (float)GRID / 2,
                        snake.bodyColor(i), DEATH_SEGMENT_PARTICLES);
                    deathSegmentIdx++;
                    deathNextSegmentMs += deathSegmentIntervalMs;
                }
                if (deathSegmentIdx >= snake.len) {
                    deathCinematic = false;
                    state = GAMEOVER;
                    gameOverMs = now;
                }
                shake.update();
                particles.update(dt);
                if (now < deathFlashEndMs) {
                    canvas.fillSprite(TFT_WHITE);
                } else {
                    drawBackground(canvas, bgSprite);
                    drawArenaWalls(canvas, arenaInset, shrinkFlashEndMs, false);
                    drawFood(canvas, food);
                    for (int i = deathSegmentIdx; i < snake.len; i++) {
                        drawCell(canvas, snake.x[i], snake.y[i],
                                 snake.bodyColor(i), COL_BODY_BRD);
                    }
                    particles.draw(canvas);
                    canvas.drawRect(0, 0, SCR_W, SCR_H, TFT_RED);
                    canvas.drawRect(1, 1, SCR_W - 2, SCR_H - 2, TFT_RED);
                }
                checkScreenshot(canvas);
                canvas.pushSprite(shake.offsetX, shake.offsetY);
                break;
            }

            // ---- Power-Up: aktiflik + sarj dolumu ----
            bool ghostActive = (now < ghostActiveUntil);
            bool magnetActive = (now < magnetActiveUntil);
            if (!ghostActive) {
                ghostCharge += dt / (GHOST_RECHARGE_MS / 1000.0f);
                if (ghostCharge > 1.0f) ghostCharge = 1.0f;
            }
            if (!magnetActive) {
                magnetCharge += dt / (MAGNET_RECHARGE_MS / 1000.0f);
                if (magnetCharge > 1.0f) magnetCharge = 1.0f;
            }
            // C/D basınca kullan (sarj dolu + aktif degil)
            if (pressC && ghostCharge >= 1.0f && !ghostActive) {
                ghostActiveUntil = now + GHOST_DURATION_MS;
                ghostCharge = 0.0f;
                playSound(NOTE_C5, 50);
            }
            if (pressD && magnetCharge >= 1.0f && !magnetActive) {
                magnetActiveUntil = now + MAGNET_DURATION_MS;
                magnetCharge = 0.0f;
                playSound(NOTE_C5, 50);
            }

            // ---- Level change detection ----
            int newLevel = 1 + (score / 100);
            if (newLevel != level) {
                if (newLevel == 2 && level == 1) obstacleSpawnMs = now;
                level = newLevel;
                levelChangeMs = now;
                needBgRender = true;
                // Tema her seviyede degisir (Hamle 5A)
                int tIdx = (level - 1) % THEME_COUNT;
                if (tIdx != currentThemeIdx) {
                    currentThemeIdx = tIdx;
                    applyTheme(tIdx);
                }
            }

            // ---- Joystick -> Direction (Advanced Processor) ----
            int joyDir = joystick.currentDir;
            if (joyDir != -1 && joyDir != joystick.prevDir) {
                snake.queueDirection(joyDir);
            }
            joystick.prevDir = joyDir;

            // ---- Arena shrink (zamanla daralan alan) ----
            if (arenaInset < ARENA_SHRINK_MAX &&
                now - arenaStartMs >= ARENA_SHRINK_START_MS &&
                now - arenaLastShrinkMs >= ARENA_SHRINK_INTERVAL_MS) {
                int x0 = arenaInset, x1 = COLS - 1 - arenaInset;
                int y0 = arenaInset, y1 = ROWS - 1 - arenaInset;
                bool blocked = false;
                for (int i = 0; i < snake.len; i++) {
                    if (snake.x[i] == x0 || snake.x[i] == x1 ||
                        snake.y[i] == y0 || snake.y[i] == y1) {
                        blocked = true; break;
                    }
                }
                if (!blocked) {
                    arenaInset++;
                    arenaLastShrinkMs = now;
                    needBgRender = true;
                    shrinkFlashEndMs = now + 400;
                    shake.trigger(1.5f);
                    playSound(NOTE_B3, 40);
                }
            }

            // ---- Movement timing ----
            unsigned long moveInterval = (unsigned long)BASE_SPEED -
                                         (unsigned long)(score / 50) * SPEED_DEC;
            if (moveInterval < MIN_SPEED) moveInterval = MIN_SPEED;

            if (now - lastMoveMs >= moveInterval && now > frozenUntil) {
                lastMoveMs = now;

                // Compute new head position
                snake.popDirection();
                int newX = snake.x[0];
                int newY = snake.y[0];
                switch (snake.dir) {
                    case DIR_UP:    newY--; break;
                    case DIR_RIGHT: newX++; break;
                    case DIR_DOWN:  newY++; break;
                    case DIR_LEFT:  newX--; break;
                }

                // ---- Wall collision (arena sinirlari) ----
                if (ghostActive) {
                    if (newX < arenaInset) newX = COLS - arenaInset - 1;
                    else if (newX >= COLS - arenaInset) newX = arenaInset;
                    if (newY < arenaInset) newY = ROWS - arenaInset - 1;
                    else if (newY >= ROWS - arenaInset) newY = arenaInset;
                } else {
                    if (newX < arenaInset || newX >= COLS - arenaInset ||
                        newY < arenaInset || newY >= ROWS - arenaInset) {
                        handleDeath(snake.x[0], snake.y[0], COL_HEAD, 6);
                        break;
                    }

                    // ---- Obstacle collision ----
                    if (FoodManager::isObstacle(newX, newY, level, arenaInset)) {
                        if (obstacleSpawnMs == 0 || now - obstacleSpawnMs >= 2000) {
                            handleDeath(newX, newY, TFT_DARKGREY, 8);
                            break;
                        }
                    }
                }

                // Check food/poison before moving
                bool ate = (newX == food.foodX && newY == food.foodY);
                if (magnetActive && (millis() - food.spawnMs > 500)) {
                    float fx = food.foodPixelX;
                    float fy = food.foodPixelY;
                    
                    float hx1 = newX * GRID + GRID / 2.0f;
                    float hy1 = OFFSET_Y + newY * GRID + GRID / 2.0f;
                    float distSq1 = (fx - hx1) * (fx - hx1) + (fy - hy1) * (fy - hy1);
                    
                    float hx2 = snake.x[0] * GRID + GRID / 2.0f;
                    float hy2 = OFFSET_Y + snake.y[0] * GRID + GRID / 2.0f;
                    float distSq2 = (fx - hx2) * (fx - hx2) + (fy - hy2) * (fy - hy2);
                    
                    if (distSq1 < GRID * GRID * 1.5f || distSq2 < GRID * GRID * 1.5f) ate = true;
                }

                bool atePoison = (food.poisonActive && newX == food.poisonX &&
                                  newY == food.poisonY);

                // ---- Move snake (includes self-hit check) ----
                if (!snake.move(newX, newY, ate, atePoison, ghostActive)) {
                    handleDeath(newX, newY, COL_HEAD, 6);
                    break;
                }
                needSnakeFullRedraw = false;

                // ---- Handle food/poison eaten ----
                if (ate || atePoison) {
                    int pts = 10;
                    uint16_t pCol = COL_FOOD;

                    if (atePoison) {
                        pts = -10;
                        pCol = COL_POISON;
                        food.poisonActive = false;
                    } else if (food.foodType == FOOD_GOLD) {
                        pts = 50;
                        pCol = COL_GOLD;
                    } else if (food.foodType == FOOD_GHOST) {
                        pts = 0;
                        pCol = COL_GHOST;
                        ghostCharge = 1.0f;
                    } else if (food.foodType == FOOD_MAGNET) {
                        pts = 0;
                        pCol = COL_MAGNET;
                        magnetCharge = 1.0f;
                    }

                    // ---- Combo engine ----
                    if (atePoison) {
                        if (comboCount > 1) {
                            shake.trigger(2.0f);
                            comboFlashEndMs = now + 150;
                            startSweep(NOTE_G5, NOTE_E5, NOTE_D5);
                        }
                        comboCount = 0;
                        comboExpireMs = 0;
                    } else {
                        if (comboCount > 0 && now <= comboExpireMs) comboCount++;
                        else comboCount = 1;
                        comboExpireMs = now + COMBO_WINDOW_MS;
                        if (food.foodType == FOOD_GOLD)
                            comboExpireMs += COMBO_GOLD_EXTEND_MS;
                        pts *= comboMultiplier(comboCount);
                    }

                    lastFoodPts = pts;
                    score += pts;
                    if (score < 0) score = 0;

                    int fx = atePoison ? food.poisonX : food.foodX;
                    int fy = atePoison ? food.poisonY : food.foodY;

                    popupX = fx * GRID + GRID / 2;
                    popupY = OFFSET_Y + fy * GRID;
                    popupTimer = 15.0f;

                    int pCount = (ate && food.foodType == FOOD_GOLD) ? 10 : 5;
                    particles.spawn(
                        fx * GRID + (float)GRID / 2,
                        OFFSET_Y + fy * GRID + (float)GRID / 2,
                        pCol, pCount);

                    squashStartMs = now;
                    if (atePoison) {
                        shake.trigger(SHAKE_POISON);
                    } else if (food.foodType == FOOD_GOLD) {
                        shake.trigger(SHAKE_GOLD);
                        frozenUntil = now + HITSTOP_GOLD_MS;
                    } else {
                        shake.trigger(SHAKE_FOOD);
                    }

                    if (atePoison) {
                        playSound(NOTE_E3, 60);
                    } else if (food.foodType == FOOD_GOLD) {
                        playSound(NOTE_E5, 30); delay(30); playSound(NOTE_G5, 40);
                    } else {
                        uint16_t eatFreq = (comboCount >= 2) ? NOTE_G5 : NOTE_E5;
                        playSound(eatFreq, 30);
                    }

                    if (ate) {
                        if (arenaInset > 0) {
                            arenaInset -= ARENA_EXPAND_ON_EAT;
                            if (arenaInset < 0) arenaInset = 0;
                            needBgRender = true;
                        }
                        int effectiveCombo = magnetActive ? 0 : comboCount;
                        food.spawnFood(snake, level, effectiveCombo, arenaInset);
                    }
                }
            }

            if (comboCount > 0 && now > comboExpireMs) {
                if (comboCount > 1) {
                    startSweep(NOTE_G5, NOTE_E5, NOTE_D5);
                }
                comboCount = 0;
                comboExpireMs = 0;
            }

            updateSweep(now);
            particles.update(dt);
            shake.update();

            // RENDER
            if (needBgRender) {
                renderBackground(bgSprite, level, arenaInset);
                needBgRender = false;
            }
            drawBackground(canvas, bgSprite);

            // Yeni engel (Seviye 2'de ortada cikan turuncu kutu)
            // Ilk 2 saniye yanip soner, sonra kalici turuncu olur.
            if (level >= 2 && obstacleSpawnMs > 0) {
                if ((now - obstacleSpawnMs >= 2000) || ((now / 200) % 2 == 0)) {
                    int ox = COLS / 2 - 1, oy = ROWS / 2 - 1;
                    if (ox >= arenaInset && ox + 1 < COLS - arenaInset &&
                        oy >= arenaInset && oy + 1 < ROWS - arenaInset) {
                        canvas.fillRect(ox * GRID, OFFSET_Y + oy * GRID, 2 * GRID, 2 * GRID, TFT_ORANGE);
                    }
                }
            }

            bool aboutToShrink = false;
            if (arenaInset < ARENA_SHRINK_MAX &&
                now - arenaStartMs >= ARENA_SHRINK_START_MS &&
                now - arenaLastShrinkMs >= (ARENA_SHRINK_INTERVAL_MS - 2000)) {
                aboutToShrink = true;
            }
            drawArenaWalls(canvas, arenaInset, shrinkFlashEndMs, aboutToShrink);
            food.updatePoison();
            food.updateMagnetPixels(snake.x[0], snake.y[0], magnetActive, dt);
            drawFood(canvas, food);

            int trailLen = (comboCount >= COMBO_TRAIL_HIGH_THRESH)
                         ? COMBO_TRAIL_HIGH : COMBO_TRAIL_LOW;
            drawTrail(canvas, snake, trailLen);

            float squashT = (squashStartMs == 0) ? 2.0f
                                                : (float)(now - squashStartMs) / (float)SQUASH_MS;
            drawSnakeFull(canvas, snake, squashT, ghostActive);

            particles.draw(canvas);
            float popupDt = (popupTimer > 0.0f) ? 30.0f * dt : 0.0f;
            drawPopup(canvas, popupX, popupY, popupTimer, lastFoodPts);
            popupTimer -= popupDt;

            drawHUD(canvas, score, currentFPS, showFps);

            float gActiveFrac = ghostActive ? (float)(ghostActiveUntil - now) / GHOST_DURATION_MS : 0.0f;
            float mActiveFrac = magnetActive ? (float)(magnetActiveUntil - now) / MAGNET_DURATION_MS : 0.0f;
            drawPowerUpStatus(canvas, ghostCharge, magnetCharge, ghostActive, magnetActive, gActiveFrac, mActiveFrac);

            float comboFrac = 0.0f;
            if (comboCount > 1 && comboExpireMs > now) {
                comboFrac = (float)(comboExpireMs - now) / (float)COMBO_WINDOW_MS;
                if (comboFrac > 1.0f) comboFrac = 1.0f;
            }
            drawComboBar(canvas, comboCount, comboFrac);

            checkScreenshot(canvas);
            canvas.pushSprite(shake.offsetX, shake.offsetY);
            break;
        }

        case GAMEOVER:
            drawGameOver(canvas, score, highScore, newRecord);
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

        case PAUSE: {
            if (needBgRender) {
                renderBackground(bgSprite, level, arenaInset);
                needBgRender = false;
            }
            drawBackground(canvas, bgSprite);

            // Yanip sonen yeni engel (sadece Seviye 2'ye ilk gecildiginde 2 saniye)
            if (level >= 2 && obstacleSpawnMs > 0 && (now - obstacleSpawnMs < 2000)) {
                if ((now / 200) % 2 == 0) {
                    int ox = COLS / 2 - 1, oy = ROWS / 2 - 1;
                    if (ox >= arenaInset && ox + 1 < COLS - arenaInset &&
                        oy >= arenaInset && oy + 1 < ROWS - arenaInset) {
                        canvas.fillRect(ox * GRID, OFFSET_Y + oy * GRID, 2 * GRID, 2 * GRID, TFT_ORANGE);
                    }
                }
            }

            drawArenaWalls(canvas, arenaInset, shrinkFlashEndMs, false);
            drawFood(canvas, food);
            int trailLen = (comboCount >= COMBO_TRAIL_HIGH_THRESH)
                         ? COMBO_TRAIL_HIGH : COMBO_TRAIL_LOW;
            drawTrail(canvas, snake, trailLen);
            bool ghostActiveP = (now < ghostActiveUntil);
            drawSnakeFull(canvas, snake, 2.0f, ghostActiveP);
            particles.draw(canvas);
            drawHUD(canvas, score, currentFPS, showFps);

            float gActiveFracP = ghostActiveP ? (float)(ghostActiveUntil - now) / GHOST_DURATION_MS : 0.0f;
            float mActiveFracP = (now < magnetActiveUntil) ? (float)(magnetActiveUntil - now) / MAGNET_DURATION_MS : 0.0f;
            drawPowerUpStatus(canvas, ghostCharge, magnetCharge, ghostActiveP, (now < magnetActiveUntil), gActiveFracP, mActiveFracP);

            float comboFracP = 0.0f;
            if (comboCount > 1 && comboExpireMs > now) {
                comboFracP = (float)(comboExpireMs - now) / (float)COMBO_WINDOW_MS;
                if (comboFracP > 1.0f) comboFracP = 1.0f;
            }
            drawComboBar(canvas, comboCount, comboFracP);

            // Pause overlay
            drawPauseOverlay(canvas);

            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (!digitalRead(BTN_A)) {
                playSound(NOTE_D5, 40);
                state = PLAYING;
                lastMoveMs = millis();
                lastFrameMs = millis();
                arenaLastShrinkMs = millis();
            }
            if (!digitalRead(BTN_B)) {
                playSound(NOTE_G4, 50);
                returnToOS();
            }
            break;
        }
    }

    // ---- OLED update (Hamle 5B) — 500ms, page-diff, cap=2 (60 FPS guvenli) ----
    if (now - oledLastUpdateMs >= 500) {
        oledLastUpdateMs = now;
        drawOLED();
    }
}
