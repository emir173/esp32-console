// ============================================================
//  EMIR OS — SPACE INVADERS v3.0 (Modern Premium Arcade)
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanli cift tamponlama + Parallax + Particul + Shake
//
//  Kontroller:
//    JOY_X  -> Saga / Sola hareket
//    BTN_A  -> Ates et
//    BTN_B  -> OS Launcher'a Don
//    JOY_SW -> Pause
//    Buzzer -> Ses efektleri
// ============================================================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"
#include "Config.h"
#include "Player.h"
#include "Invaders.h"
#include "Bunker.h"
#include "Projectiles.h"
#include "Renderer.h"
#include "../SharedJoystick.h"
#include "Input.h"

// ============ Goruntu Nesneleri ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Oyun Nesneleri ============
Player player;
InvaderGrid invaders;
BunkerManager bunkers;
ProjectileManager projectiles;
ParticleSystem particles;
Starfield starfield;
ScreenShake shake;
ScreenFlash flash;
JoystickProcessor joystick;

// ============ Oyun Durumu ============
GameState state = MENU;
int score = 0;
int highScore = 0;
int wave = 1;
bool newRecord = false;

// ============ Zamanlama ============
unsigned long lastFrameMs;
unsigned long gameOverMs;
unsigned long waveClearMs;
unsigned long pauseMs;

// ============ FPS Sayaci ============
unsigned long fpsFrameCount = 0;
unsigned long fpsStartTime = 0;
int currentFPS = 0;
bool showFps = false;

// ============ Buton ============
InvadersButton buttonA, buttonB, buttonC, buttonJoy;

// Transition tones run between frames without blocking input/rendering.
int soundSequence = 0; // 0 = none, 1 = wave clear, 2 = game over
int soundStage = 0;
unsigned long soundSequenceMs = 0;

// ============ Ses ============
bool soundEnabled = true;
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

void updateTransitionSound(unsigned long now) {
    unsigned long elapsed = now - soundSequenceMs;
    if (soundSequence == 1) {
        if (soundStage == 0 && elapsed >= 60) {
            playSound(NOTE_E5, 50);
            soundStage = 1;
            soundSequenceMs = now;
        } else if (soundStage == 1 && elapsed >= 60) {
            playSound(NOTE_G5, 40);
            soundSequence = 0;
        }
    } else if (soundSequence == 2 && elapsed >= 100) {
        playSound(NOTE_E3, 100);
        soundSequence = 0;
    }
}

void saveHighScoreIfNeeded() {
    if (score > highScore) highScore = score;
    Preferences prefs;
    if (!prefs.begin("os", false)) return;
    int saved = prefs.getInt(HIGH_SCORE_KEY, 0);
    if (highScore > saved && prefs.putInt(HIGH_SCORE_KEY, highScore) != sizeof(int32_t)) {
        Serial.println("Space Invaders: high score write failed");
    }
    prefs.end();
}

// ============ OS'a Donus ============
void returnToOS() {
    saveHighScoreIfNeeded();
    osReturnToOS(tft, soundEnabled);
}

// ============================================================
//  Oyunu Sifirla
// ============================================================
void resetGame() {
    player.init();
    wave = 1;
    score = 0;
    newRecord = false;
    invaders.init(wave);
    bunkers.init();
    projectiles.clear();
    particles.clear();
    starfield.init();
    shake.reset();
    flash.reset();
    joystick.reset();
    state = PLAYING;
    soundSequence = 0;
    lastFrameMs = millis();
}

void enterGameMenu() {
    saveHighScoreIfNeeded();
    soundSequence = 0;
    shake.reset();
    flash.reset();
    state = MENU;
}

void enterPause() {
    pauseMs = millis();
    state = PAUSE;
    playSound(NOTE_G4, 50);
}

void resumeGame() {
    unsigned long pausedFor = millis() - pauseMs;
    player.hitTime += pausedFor;
    player.lastFireMs += pausedFor;
    invaders.lastEFireMs += pausedFor;
    invaders.lastAnimMs += pausedFor;
    state = PLAYING;
    lastFrameMs = millis();
    playSound(NOTE_D5, 40);
}

void finishGame() {
    newRecord = score > highScore;
    saveHighScoreIfNeeded();
    state = GAMEOVER;
    gameOverMs = millis();
    soundSequenceMs = gameOverMs;
    soundSequence = 2;
    shake.trigger(5);
}

// Sample the whole travelled segment at <= 1px spacing. A slow frame cannot
// skip a 3px bunker block; the first encountered object consumes the bullet.
void checkPlayerBulletCollisions() {
    for (int i = 0; i < MAX_PBULLETS; i++) {
        if (!projectiles.pBullets[i].active) continue;
        float bx = projectiles.pBullets[i].x;
        float fromY = projectiles.pBullets[i].previousY;
        float travel = projectiles.pBullets[i].y - fromY;
        int steps = (int)ceilf(fabsf(travel));
        if (steps < 1) steps = 1;
        for (int s = 0; s <= steps && projectiles.pBullets[i].active; s++) {
            float by = fromY + travel * (float)s / (float)steps;
            if (bunkers.checkBulletHit(bx, by)) {
                projectiles.deactivatePB(i);
                break;
            }
            bool hit = false;
            for (int r = ALIEN_ROWS - 1; r >= 0 && !hit; r--) {
                for (int c = 0; c < ALIEN_COLS && !hit; c++) {
                    if (!invaders.aliens[r][c]) continue;
                    float ax = invaders.alienX(c);
                    float ay = invaders.alienY(r);
                    if (bx + 1.0f > ax && bx - 1.0f < ax + (float)ALIEN_W &&
                        by + 2.0f > ay && by - 2.0f < ay + (float)ALIEN_H) {
                        invaders.killAt(r, c, wave);
                        projectiles.deactivatePB(i);
                        score += ALIEN_POINTS[r & 3];
                        playSound(NOTE_D5, 30);
                        particles.spawn(ax + (float)ALIEN_W / 2.0f,
                                        ay + (float)ALIEN_H / 2.0f, COL_ALIEN_ROW[r & 3], 12);
                        shake.trigger(2);
                        hit = true;
                    }
                }
            }
        }
    }
}

void checkEnemyBulletCollisions() {
    for (int i = 0; i < MAX_EBULLETS; i++) {
        if (!projectiles.eBullets[i].active) continue;
        float bx = projectiles.eBullets[i].x;
        float fromY = projectiles.eBullets[i].previousY;
        float travel = projectiles.eBullets[i].y - fromY;
        int steps = (int)ceilf(fabsf(travel));
        if (steps < 1) steps = 1;
        for (int s = 0; s <= steps && projectiles.eBullets[i].active; s++) {
            float by = fromY + travel * (float)s / (float)steps;
            if (bunkers.checkBulletHit(bx, by)) {
                projectiles.deactivateEB(i);
                break;
            }
            if (!player.isInvincible() && bx + 1.0f > player.hitboxLeft() &&
                bx - 1.0f < player.hitboxRight() && by + 2.0f > player.hitboxTop() &&
                by - 2.0f < player.hitboxBottom()) {
                projectiles.deactivateEB(i);
                player.takeDamage();
                playSound(NOTE_B3, 80);
                shake.trigger(4);
                flash.trigger();
                if (player.lives <= 0) { finishGame(); return; }
            }
        }
    }
}

void fireEnemyBullet() {
    int columns[ALIEN_COLS];
    int columnCount = 0;
    for (int c = 0; c < ALIEN_COLS; c++) {
        for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
            if (invaders.aliens[r][c]) { columns[columnCount++] = c; break; }
        }
    }
    if (columnCount == 0) return;
    int c = columns[random(0, columnCount)];
    for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
        if (invaders.aliens[r][c]) {
            if (projectiles.addEnemyBullet(invaders.alienCenterX(c),
                    invaders.alienBottomY(r) + 1.0f, COL_ALIEN_ROW[r & 3])) invaders.markFired();
            break;
        }
    }
}

void presentFrame(int x = 0, int y = 0) {
    // A shifted sprite leaves TFT strips uncovered. Clear those stale pixels.
    if (x > 0) tft.fillRect(0, 0, x, SCR_H, COL_SPACE);
    else if (x < 0) tft.fillRect(SCR_W + x, 0, -x, SCR_H, COL_SPACE);
    if (y > 0) tft.fillRect(0, 0, SCR_W, y, COL_SPACE);
    else if (y < 0) tft.fillRect(0, SCR_H + y, SCR_W, -y, COL_SPACE);
    checkScreenshot(canvas);
    canvas.pushSprite(x, y);
}

// ============================================================
//  Yeni Dalga Baslat
// ============================================================
void startNewWave() {
    wave++;
    invaders.init(wave);
    bunkers.init();
    projectiles.clear();
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
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
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
        highScore = prefs.getInt(HIGH_SCORE_KEY, 0);
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
    if (!canvas.createSprite(SCR_W, SCR_H)) {
        tft.setTextColor(TFT_RED); tft.setTextSize(1);
        tft.setCursor(5, 48); tft.print("Display buffer error");
        tft.setCursor(20, 70); tft.print("[B] OS Menu");
        while (true) {
            if (digitalRead(BTN_B) == LOW) returnToOS();
            delay(20);
        }
    }

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
    int joyCX = sumX / 10;
    int joyCY = sumY / 10;
    joystick.init(joyCX, joyCY);

    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    starfield.init();
    particles.clear();
    state = MENU;
    unsigned long inputNow = millis();
    buttonA.reset(digitalRead(BTN_A), inputNow);
    buttonB.reset(digitalRead(BTN_B), inputNow);
    buttonC.reset(digitalRead(BTN_C), inputNow);
    buttonJoy.reset(digitalRead(JOY_SW), inputNow);
    lastFrameMs = millis();
    fpsStartTime = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    unsigned long now = millis();
    buttonA.update(digitalRead(BTN_A), now);
    buttonB.update(digitalRead(BTN_B), now);
    buttonC.update(digitalRead(BTN_C), now);
    buttonJoy.update(digitalRead(JOY_SW), now);
    updateTransitionSound(now);
    unsigned long elapsed = now - lastFrameMs;
    if (elapsed < (unsigned long)FRAME_MS) return;
    float dt = (float)elapsed / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    lastFrameMs = now;

    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = (int)(fpsFrameCount * 1000UL / (now - fpsStartTime));
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    joystick.update();

    bool pressA = buttonA.takePress();
    bool pressB = buttonB.takePress();
    bool pressC = buttonC.takePress();
    bool pressJoy = buttonJoy.takePress();

    if (state != PAUSE) {
        starfield.update(dt);
        shake.update();
        flash.update(elapsed);
    }

    switch (state) {

        case MENU:
            drawMenu(canvas, starfield, highScore);
            presentFrame();
            if (pressA) {
                playSound(NOTE_E5, 50);
                resetGame();
            }
            else if (pressB) {
                returnToOS();
            }
            break;

        case PLAYING: {
            if (pressJoy) { enterPause(); break; }
            float rawJoyX = (float)analogRead(JOY_X);
            player.move(rawJoyX, joystick.centerX, dt);

            bool fireHeld = (buttonA.stable == LOW);
            if (fireHeld && player.canFire()) {
                if (projectiles.activePB() < MAX_PBULLETS) {
                    if (projectiles.addPlayerBullet(player.gunTipX(), player.gunTipY())) {
                        player.markFired();
                        playSound(NOTE_A4, 22);
                    }
                }
            }

            projectiles.updatePlayerBullets(dt);

            checkPlayerBulletCollisions();

            if (invaders.count <= 0) {
                state = WAVE_CLEAR;
                waveClearMs = now;
                soundSequence = 1;
                soundStage = 0;
                soundSequenceMs = now;
                playSound(NOTE_C5, 50);
                break;
            }

            invaders.updateMovement(dt);
            invaders.updateAnimation(now);

            // --- Uzaylilarin Kalkanlari Ezmesi ---
            for (int r = 0; r < ALIEN_ROWS; r++) {
                for (int c = 0; c < ALIEN_COLS; c++) {
                    if (!invaders.aliens[r][c]) continue;
                    float ax = invaders.alienX(c);
                    float ay = invaders.alienY(r);
                    for (int bi = 0; bi < BUNKER_COUNT; bi++) {
                        Bunker &bk = bunkers.bunkers[bi];
                        // Hizli AABB on-kontrol
                        if (ax + (float)ALIEN_W <= (float)bk.posX) continue;
                        if (ax >= (float)(bk.posX + bk.width())) continue;
                        if (ay + (float)ALIEN_H <= (float)bk.posY) continue;
                        if (ay >= (float)(bk.posY + bk.height())) continue;
                        // Blok bazli kesisim
                        bk.hitTest(ax + (float)ALIEN_W / 2.0f, ay + (float)ALIEN_H / 2.0f,
                                   (float)ALIEN_W / 2.0f, (float)ALIEN_H / 2.0f);
                    }
                }
            }

            if (invaders.reachedPlayer()) {
                playSound(NOTE_G3, 100);
                finishGame();
                break;
            }

            if (invaders.canFire(wave) && invaders.count > 0 &&
                projectiles.activeEB() < MAX_EBULLETS) {
                fireEnemyBullet();
            }

            projectiles.updateEnemyBullets(dt);

            checkEnemyBulletCollisions();

            if (state != PLAYING) break;

            particles.update(dt);

            renderGameScene(canvas, starfield, invaders, player, bunkers,
                            projectiles, particles, flash, score, wave,
                            player.lives, currentFPS, showFps);

            presentFrame((int)shake.shakeX, (int)shake.shakeY);
            break;
        }

        case WAVE_CLEAR:
            drawWaveClear(canvas, starfield, wave, score);
            presentFrame();
            if (now - waveClearMs > 2000) {
                startNewWave();
                state = PLAYING;
            }
            break;

        case GAMEOVER:
            drawGameOver(canvas, starfield, score, highScore, newRecord);
            presentFrame();
            if (pressA && now - gameOverMs > 600) {
                playSound(NOTE_D5, 50);
                resetGame();
            }
            else if (pressB && now - gameOverMs > 600) {
                playSound(NOTE_G4, 50);
                enterGameMenu();
            }
            else if (pressC && now - gameOverMs > 600) {
                returnToOS();
            }
            break;

        case PAUSE:
            renderGameScene(canvas, starfield, invaders, player, bunkers,
                            projectiles, particles, flash, score, wave,
                            player.lives, currentFPS, showFps);
            drawPauseOverlay(canvas);
            presentFrame();
            if (pressA || pressJoy) {
                resumeGame();
            }
            else if (pressB) {
                playSound(NOTE_G4, 50);
                enterGameMenu();
            }
            else if (pressC) {
                returnToOS();
            }
            break;
    }
}
