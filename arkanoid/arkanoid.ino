// ============================================================
//  E-OS ARKANOID (TUGLA KIRMA) — v2.0 Premium
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanli cift tamponlama (Flicker-Free)
//  OOP + Moduler Mimari: Config / Paddle / Ball / Bricks / Renderer
//
//  Premium Efektler:
//    - Parcacik Patlama Fizigi (Particle System)
//    - Ekran Titremesi (Screen Shake)
//    - Kuyruklu Top (Ball Trail)
//    - Dinamik Raket Fizigi (Aciya bagli sekme)
//
//  Kontroller:
//    JOY_X  -> Cubugu saga / sola hareket ettir
//    BTN_A  -> Topu firlat / Menu onay
//    BTN_B  -> Oyun menusu (ana menudeyken OS Launcher)
//    BTN_C  -> Pause / Game Over ekranindan OS Launcher'a Don
//    JOY_SW -> Pause
// ============================================================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"
#include "Config.h"
#include "Paddle.h"
#include "Ball.h"
#include "Bricks.h"
#include "Renderer.h"
#include "Input.h"

// ============ Oyun Nesneleri ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

GameState state = MENU;
Paddle paddle;
Ball ball;
Bricks bricks;
ParticleSystem particles;
ScreenShake shake;

// ============ Skor & Oyun ============
int score = 0;
int highScore = 0;
int wave = 1;
int lives = START_LIVES;
bool newRecord = false;

// ============ Zamanlama ============
unsigned long lastFrameMs = 0;
unsigned long gameOverMs = 0;
unsigned long levelClearMs = 0;
unsigned long ballLostMs = 0;     // Top kaybı dondurma timer'ı (BALL_LOST state)

// ============ FPS Sayaci ============
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;

// ============ Buton & Ayarlar ============
ArkanoidButton buttonA, buttonB, buttonC, buttonJoy;
int joyCenterX = 0;
bool soundEnabled = true;
bool showFps = false;

// ============ Ses & OS Donus Yardimcilari ============
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

void saveBestScore() {
    if (score > highScore) {
        highScore = score;
        osSaveHighScore("hs_arkanoid", highScore);
    }
}

void returnToOS() {
    saveBestScore();
    osReturnToOS(tft, soundEnabled);
}

void enterGameMenu() {
    saveBestScore();
    resetGame();
    state = MENU;
}

// A shifted sprite leaves strips of the previous frame exposed at the edges.
void presentShakenFrame() {
    if (shake.offsetX > 0) tft.fillRect(0, 0, shake.offsetX, SCR_H, COL_BG);
    if (shake.offsetX < 0) tft.fillRect(SCR_W + shake.offsetX, 0, -shake.offsetX, SCR_H, COL_BG);
    if (shake.offsetY > 0) tft.fillRect(0, 0, SCR_W, shake.offsetY, COL_BG);
    if (shake.offsetY < 0) tft.fillRect(0, SCR_H + shake.offsetY, SCR_W, -shake.offsetY, COL_BG);
    canvas.pushSprite(shake.offsetX, shake.offsetY);
}

// ============ Topu Sifirla (Cubuga Yapistir) ============
void resetBall() {
    float spd = BASE_BALL_SPD + (float)(wave - 1) * 5.0f;
    ball.init(paddle.getX(), (float)(PADDLE_Y - BALL_R - 1), spd);
    ball.clearTrail();
}

// ============ Oyunu Bastan Baslat ============
void resetGame() {
    paddle.reset();
    bricks.init();
    particles.clear();
    shake.reset();
    score = 0;
    wave = 1;
    lives = START_LIVES;
    newRecord = false;
    resetBall();
    state = PLAYING;
}

// ============ Yeni Dalga Baslat ============
void startNewWave() {
    wave++;
    bricks.init();
    particles.clear();
    paddle.reset();
    resetBall();
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    delay(50);  // Hardware stabilizasyon

    // 1) Buzzer sustur (reset sonrasi cizirti onler)
    osInitBuzzer();

    // 2) OLED kapat (acilis flicker onleme)
    Wire.begin(I2C_SDA, I2C_SCL);
    osOLEDOff();

    // 3) Guvenlik: elektrik kesintisinde OS'tan basla (OTA partition)
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    // 4) Buton pinleri ayarla
    osInitButtons();

    // NVS'ten ses ve ekran ayarlarini yukle
    {
        Preferences prefs;
        prefs.begin("os", true);
        soundEnabled = prefs.getBool("sound_en", true);
        showFps = prefs.getBool("show_fps", false);
        highScore = prefs.getInt("hs_arkanoid", 0);
        prefs.end();
    }

    // SPI baslat
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    initDevTools(tft);

    // Ekran baslatma
    tft.init();
    tft.setRotation(1);   // Landscape: 160x128
    tft.startWrite();
    tft.writecommand(0x36);  // MADCTL
    tft.writedata(0xA0);     // MY|MV, BGR kapali (RGB modu)
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    // Sprite tamponu (cift tamponlama / flicker-free)
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // Joystick merkez kalibrasyonu (guvenlik kontrolu)
    bool warn = false;
    while (analogRead(JOY_X) < 1400 || analogRead(JOY_X) > 2600) {
        if (!warn) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED); tft.setTextSize(1);
            tft.setCursor(23, 60); tft.print("RELEASE JOYSTICK!");
            warn = true;
        }
        delay(50);
    }
    if (warn) { tft.fillScreen(TFT_BLACK); delay(300); }

    long sumX = 0;
    for (int j = 0; j < 10; j++) { sumX += analogRead(JOY_X); delay(2); }
    joyCenterX = sumX / 10;

    // Rastgele tohum
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    // Nesneleri ilklendir
    paddle.init(joyCenterX);
    particles.clear();
    shake.reset();
    bricks.init();

    state = MENU;
    lastFrameMs = millis();
    fpsStartTime = millis();
    buttonA.reset(digitalRead(BTN_A), lastFrameMs);
    buttonB.reset(digitalRead(BTN_B), lastFrameMs);
    buttonC.reset(digitalRead(BTN_C), lastFrameMs);
    buttonJoy.reset(digitalRead(JOY_SW), lastFrameMs);
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    // ---- Delta-Time (FPS60) ----
    unsigned long now = millis();
    buttonA.update(digitalRead(BTN_A), now);
    buttonB.update(digitalRead(BTN_B), now);
    buttonC.update(digitalRead(BTN_C), now);
    buttonJoy.update(digitalRead(JOY_SW), now);
    unsigned long delta = now - lastFrameMs;
    if (delta < FRAME_MS) {
        yield();   // FreeRTOS nefesi (delay() yok)
        return;
    }
    float dt = (float)delta / 1000.0f;
    if (dt > MAX_DT) dt = MAX_DT;   // Lag spike korumasi
    lastFrameMs = now;

    bool pressA = buttonA.takePress();
    bool pressB = buttonB.takePress();
    bool pressC = buttonC.takePress();
    bool pressJoy = buttonJoy.takePress();
    if (pressJoy && state == PLAYING) {
        state = PAUSE;
        pressA = false;  // A simultaneous press must not immediately resume.
        playSound(NOTE_G4, 50);
    }

    // ---- FPS Hesaplama ----
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = (int)fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // ======================================
    //  DURUM MAKINESI
    // ======================================
    switch (state) {

        // --------------------------------------------------
        //  ANA MENU
        // --------------------------------------------------
        case MENU:
            drawMenu(canvas, highScore);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            if (pressB) {
                returnToOS();
            } else if (pressA) {
                playSound(NOTE_E5, 50);
                resetGame();
            }
            break;

        // --------------------------------------------------
        //  OYUN
        // --------------------------------------------------
        case PLAYING: {
            if (ball.isStuck()) {
                paddle.update(dt);
                // Top cubuga yapisik
                ball.stickTo(paddle.getX());
                if (pressA) {
                    ball.launch();
                    playSound(NOTE_C5, 40);
                }
            } else {
                // At most one pixel of travel per step, including during lag spikes.
                int steps = (int)ceilf(ball.getSpeed() * dt);
                if (steps < 1) steps = 1;
                float stepDt = dt / (float)steps;
                for (int step = 0; step < steps; step++) {
                    paddle.update(stepDt);
                    // === Top Guncelle (hareket + duvar carpismasi) ===
                    int wallFlags = ball.update(stepDt);

                    if (wallFlags < 0) {
                        // --- Top altta dustu, can kaybi ---
                        lives--;
                        playSound(NOTE_G3, 100);
                        delay(80);
                        playSound(NOTE_E3, 120);
                        shake.trigger(4.0f);

                        if (lives <= 0) {
                            state = GAMEOVER;
                            newRecord = (score > highScore);
                            if (newRecord) highScore = score;
                            gameOverMs = now;
                            osSaveHighScore("hs_arkanoid", highScore);
                            break;
                        } else {
                            ballLostMs = now;
                            state = BALL_LOST;
                            break;
                        }
                    }

                    // Duvar carpma sesi
                    if (wallFlags & 3) playSound(NOTE_G4, 20);

                    // === Raket Carpisma ===
                    if (ball.handlePaddleCollision(paddle.getX())) {
                        playSound(NOTE_A4, 25);
                        shake.trigger(0.5f);
                    }

                    // === Tugla Carpisma ===
                    int hitRow, hitCol;
                    bool bounceX, bounceY;
                    if (bricks.checkHit(ball.getX(), ball.getY(),
                                        ball.getPrevX(), ball.getPrevY(),
                                        (float)BALL_R,
                                        hitRow, hitCol, bounceX, bounceY)) {

                        // Sekme yonu
                        ball.restorePreviousPosition();
                        if (bounceX) ball.bounceX();
                        if (bounceY) ball.bounceY();

                        // Tuglayi kir
                        int pts = bricks.breakAt(hitRow, hitCol);
                        score += pts;

                        // Parcacik patlamasi (tuglanin kendi renginde)
                        int bx = bricks.getBrickX(hitCol) + BRICK_W / 2;
                        int by = bricks.getBrickY(hitRow) + BRICK_H / 2;
                        particles.emit((float)bx, (float)by, BRICK_COLORS[hitRow], 8);

                        // Ses efekti (ust satirlar daha parlak)
                        playSound(BRICK_HIT_FREQ[hitRow], 35);

                        // Ekran titremesi
                        shake.trigger(1.5f);

                        // Tum tuglalar kirildi mi?
                        if (bricks.isEmpty()) {
                            state = LEVEL_CLEAR;
                            levelClearMs = now;
                            playSound(NOTE_C5, 50);
                            delay(60);
                            playSound(NOTE_E5, 50);
                            delay(60);
                            playSound(NOTE_G5, 40);
                            break;
                        }
                    }
                }
            }

            if (state != PLAYING) break;
            // Keep the trail sampled once per frame, independent of physics steps.
            if (!ball.isStuck()) ball.pushTrail();

            // === Parcacik ve Titreme Guncelle ===
            particles.update(dt);
            shake.update();

            // ============================
            //  SAHNE CIzIMI
            // ============================
            canvas.fillSprite(COL_BG);
            drawWalls(canvas);
            bricks.draw(canvas);
            particles.draw(canvas);
            paddle.draw(canvas);
            ball.draw(canvas);
            drawHUD(canvas, score, wave, lives, currentFPS, showFps);

            // Top yapisiksa bilgi metni
            if (ball.isStuck()) {
                canvas.setTextSize(1);
                canvas.setTextColor(COL_HUD_TEXT);
                canvas.setCursor(47, 85);
                canvas.print("[A] Launch!");
            }

            checkScreenshot(canvas);
            presentShakenFrame();
            break;
        }

        // --------------------------------------------------
        //  TOP KAYBI — 250ms dondurulmus sahne (delay() yerine millis timer)
        // --------------------------------------------------
        case BALL_LOST:
            shake.update();
            canvas.fillSprite(COL_BG);
            drawWalls(canvas);
            bricks.draw(canvas);
            particles.draw(canvas);
            paddle.draw(canvas);
            ball.draw(canvas);
            drawHUD(canvas, score, wave, lives, currentFPS, showFps);
            checkScreenshot(canvas);
            presentShakenFrame();
            if (now - ballLostMs > 250) {
                resetBall();
                state = PLAYING;
                lastFrameMs = millis();
            }
            break;

        // --------------------------------------------------
        //  DALGA TEMIZLENDI
        // --------------------------------------------------
        case LEVEL_CLEAR:
            drawLevelClear(canvas, wave, score);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            if (now - levelClearMs > 2000) {
                startNewWave();
                state = PLAYING;
            }
            break;

        // --------------------------------------------------
        //  OYUN BITTI
        // --------------------------------------------------
        case GAMEOVER:
            drawGameOver(canvas, score, wave, highScore, newRecord);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            if (pressA && now - gameOverMs > 600) {
                playSound(NOTE_D5, 50);
                resetGame();
            } else if (pressB && now - gameOverMs > 600) {
                playSound(NOTE_G4, 50);
                enterGameMenu();
            } else if (pressC && now - gameOverMs > 600) {
                returnToOS();
            }
            break;

        // --------------------------------------------------
        //  PAUSE
        // --------------------------------------------------
        case PAUSE:
            // Oyun sahnesini dondurulmus halde ciz
            canvas.fillSprite(COL_BG);
            drawWalls(canvas);
            bricks.draw(canvas);
            particles.draw(canvas);
            paddle.draw(canvas);
            ball.draw(canvas);
            drawHUD(canvas, score, wave, lives, currentFPS, showFps);
            drawPauseOverlay(canvas);

            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (pressA) {
                playSound(NOTE_D5, 40);
                state = PLAYING;
                lastFrameMs = millis();
            } else if (pressB) {
                playSound(NOTE_G4, 50);
                enterGameMenu();
            } else if (pressC) {
                returnToOS();
            }
            break;
    }
}
