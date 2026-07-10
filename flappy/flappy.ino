// ============================================================
//  E-OS — FLAPPY BIRD v3.0 (Modular OOP Edition)
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite-based double buffering + Delta-Time @ 60 FPS
//  Premium: Bird Rotation, Scrolling Ground, 3D Pipes,
//           Screen Shake, Flash Impact, DYING Animation
//
//  Controls:  BTN_A -> Jump  |  BTN_B -> OS Menu
// ============================================================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"
#include "Config.h"
#include "Bird.h"
#include "Pipes.h"
#include "Renderer.h"

// ============ Display Objects ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Game Instances ============
Bird bird;
Pipes pipes;

// ============ Game State ============
GameState state = MENU;
int score = 0;
int highScore = 0;
bool newRecord = false;

// ============ Score Popup Animation ============
float popupTimer = 0;

// ============ Scrolling Ground ============
float groundOffset = 0;

// ============ Screen Shake ============
int shakeFramesLeft = 0;
int flashFramesLeft = 0;

// ============ DYING State ============
bool birdLanded = false;
unsigned long dyingStartMs = 0;

// ============ Timing ============
unsigned long lastFrameMs = 0;
unsigned long gameOverMs = 0;

// ============ FPS Counter ============
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;

// ============ Sound ============
bool soundEnabled = true;
bool showFps = false;

void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// ============ Cloud Animation ============
float cloudOffset = 0;

// ============ Button Edge Detection ============
int prevBtnA = HIGH;

// ============================================================
//  Return to OS Launcher
// ============================================================
void returnToOS() {
    osReturnToOS(tft, soundEnabled);
}

// ============================================================
//  Reset Game
// ============================================================
void resetGame() {
    bird.init();
    pipes.init();
    score = 0;
    newRecord = false;
    cloudOffset = 0;
    groundOffset = 0;
    popupTimer = 0;
    shakeFramesLeft = 0;
    flashFramesLeft = 0;
    birdLanded = false;
    state = PLAYING;
}

// ============================================================
//  Handle Collision -> DYING State
// ============================================================
void enterDying() {
    state = DYING;
    dyingStartMs = millis();
    flashFramesLeft = FLASH_FRAMES;
    shakeFramesLeft = SHAKE_DURATION_MS / (1000 / TARGET_FPS);
    birdLanded = false;
    newRecord = (score > highScore);
    if (newRecord) {
        highScore = score;
        osSaveHighScore("hs_flappy", score);
    }
    playSound(NOTE_E3, 90);
    delay(100);
    playSound(NOTE_E3, 150);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    // 1) Buzzer sustur (reset sonrasi cizirti onler)
    osInitBuzzer();

    // 2) OLED kapat (acilis flicker onleme)
    Wire.begin(I2C_SDA, I2C_SCL);
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
        highScore = prefs.getInt("hs_flappy", 0);
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

    randomSeed(analogRead(JOY_X) ^ (analogRead(JOY_Y) << 8) ^ micros());

    bird.init();
    pipes.init();
    state = MENU;
    lastFrameMs = millis();
    fpsStartTime = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    // ---- JOY_SW: Pause toggle ----
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {
        if (state == PLAYING) {
            state = PAUSE;
            playSound(NOTE_G4, 50);
        }
    }
    prevJoySw = currJoySw;

    // ---- Delta-Time ----
    unsigned long now = millis();
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

    // ---- BTN_A: Edge detection ----
    int btnA = digitalRead(BTN_A);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    prevBtnA = btnA;

    // ==========================================
    //  STATE MACHINE
    // ==========================================
    switch (state) {

        // ======================================
        //  MAIN MENU
        // ======================================
        case MENU:
            cloudOffset += 30.0f * dt;
            drawMenu(canvas, highScore, cloudOffset);

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

        // ======================================
        //  PLAYING
        // ======================================
        case PLAYING: {
            // --- Physics ---
            bird.update(dt);

            // --- Jump ---
            if (pressA) {
                bird.jump();
                playSound(NOTE_E5, 30);
            }

            // --- Pipe speed (difficulty scaling) ---
            float spd = BASE_SPEED + score * 1.2f;
            if (spd > MAX_SPEED) spd = MAX_SPEED;

            // --- Update pipes ---
            int scored = pipes.update(dt, spd);
            if (scored > 0) {
                score += scored;
                popupTimer = SCORE_POPUP_DUR;
                playSound(NOTE_G5, 30);
            }

            // --- Update scrolling ground ---
            groundOffset += spd * dt;

            // --- Update clouds ---
            cloudOffset += 30.0f * dt;

            // --- Popup timer decrement ---
            if (popupTimer > 0) {
                popupTimer -= dt;
                if (popupTimer < 0) popupTimer = 0;
            }

            // --- Collision check ---
            if (pipes.collidesWith(bird.y, BIRD_R) ||
                bird.y - BIRD_R <= 0 ||
                bird.y + BIRD_R >= GROUND_Y) {
                enterDying();
                break;
            }

            // --- Render ---
            drawSky(canvas);
            drawClouds(canvas, cloudOffset);
            drawAllPipes(canvas, pipes);
            drawGround(canvas, groundOffset);
            drawBird(canvas, BIRD_X, (int)bird.y, bird.angle);
            drawScore(canvas, score, popupTimer, currentFPS, showFps, (int)bird.y);

            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  DYING (Impact -> Bird Falls -> Game Over)
        // ======================================
        case DYING: {
            // --- Bird continues falling ---
            if (!birdLanded) {
                bird.dieFall(dt);
                if (bird.hitsGround()) {
                    birdLanded = true;
                }
            }

            // --- Popup timer decrement ---
            if (popupTimer > 0) {
                popupTimer -= dt;
                if (popupTimer < 0) popupTimer = 0;
            }

            // --- Render scene ---
            drawSky(canvas);
            drawClouds(canvas, cloudOffset);
            drawAllPipes(canvas, pipes);
            drawGround(canvas, groundOffset);

            // --- Flash overlay (BEFORE bird/score so they draw on top) ---
            if (flashFramesLeft > 0) {
                canvas.fillRect(0, 0, SCR_W, SCR_H, TFT_WHITE);
                flashFramesLeft--;
            }

            drawBird(canvas, BIRD_X, (int)bird.y, bird.angle, true);
            drawScore(canvas, score, popupTimer, currentFPS, showFps, (int)bird.y);

            // --- Screen shake offset ---
            int shakeX = 0, shakeY = 0;
            if (shakeFramesLeft > 0) {
                shakeFramesLeft--;
                shakeX = random(-SHAKE_INTENSITY, SHAKE_INTENSITY + 1);
                shakeY = random(-SHAKE_INTENSITY, SHAKE_INTENSITY + 1);
            }

            checkScreenshot(canvas);
            canvas.pushSprite(shakeX, shakeY);

            // --- Transition to GAMEOVER when bird has landed ---
            if (birdLanded) {
                state = GAMEOVER;
                gameOverMs = millis();
            }
            break;
        }

        // ======================================
        //  GAME OVER
        // ======================================
        case GAMEOVER:
            drawGameOver(canvas, score, highScore, newRecord);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (pressA && (millis() - gameOverMs > 600)) {
                playSound(NOTE_D5, 50);
                resetGame();
            }
            if (!digitalRead(BTN_B) && (millis() - gameOverMs > 600)) {
                returnToOS();
            }
            break;

        // ======================================
        //  PAUSE
        // ======================================
        case PAUSE:
            drawSky(canvas);
            drawClouds(canvas, cloudOffset);
            drawAllPipes(canvas, pipes);
            drawGround(canvas, groundOffset);
            drawBird(canvas, BIRD_X, (int)bird.y, bird.angle);
            drawScore(canvas, score, popupTimer, currentFPS, showFps, (int)bird.y);

            drawPauseOverlay(canvas);

            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (!digitalRead(BTN_A)) {
                playSound(NOTE_D5, 40);
                state = PLAYING;
                lastFrameMs = millis();
            }
            if (!digitalRead(BTN_B)) {
                playSound(NOTE_G4, 50);
                returnToOS();
            }
            break;
    }
}
