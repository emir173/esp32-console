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

// ============ FPS Sayaci ============
unsigned long fpsFrameCount = 0;
unsigned long fpsStartTime = 0;
int currentFPS = 0;
bool showFps = false;

// ============ Buton ============
int prevBtnA = HIGH;
int prevBtnB = HIGH;

// ============ Ses ============
bool soundEnabled = true;
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// ============ OS'a Donus ============
void returnToOS() {
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
        highScore = prefs.getInt("hs_spaceinvaders", 0);
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
    lastFrameMs = millis();
    fpsStartTime = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {
        if (state == PLAYING) {
            state = PAUSE;
            playSound(NOTE_G4, 50);
        }
    }
    prevJoySw = currJoySw;

    unsigned long now = millis();
    unsigned long elapsed = now - lastFrameMs;
    if (elapsed < (unsigned long)FRAME_MS) return;
    float dt = (float)elapsed / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    lastFrameMs = now;

    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = (int)fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    joystick.update();

    int btnA = digitalRead(BTN_A);
    int btnB = digitalRead(BTN_B);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    bool pressB = (btnB == LOW && prevBtnB == HIGH);
    prevBtnA = btnA;
    prevBtnB = btnB;

    starfield.update(dt);
    shake.update();
    flash.update();

    switch (state) {

        case MENU:
            drawMenu(canvas, starfield, highScore);
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

        case PLAYING: {
            float rawJoyX = (float)analogRead(JOY_X);
            player.move(rawJoyX, joystick.centerX, dt);

            bool fireHeld = (btnA == LOW);
            if (fireHeld && player.canFire()) {
                if (projectiles.activePB() < MAX_PBULLETS) {
                    if (projectiles.addPlayerBullet(player.gunTipX(), player.gunTipY())) {
                        player.markFired();
                        playSound(NOTE_A4, 22);
                    }
                }
            }

            projectiles.updatePlayerBullets(dt);

            for (int i = 0; i < MAX_PBULLETS; i++) {
                if (!projectiles.pBullets[i].active) continue;
                float bx = projectiles.pBullets[i].x;
                float by = projectiles.pBullets[i].y;

                bool hit = false;
                for (int r = 0; r < ALIEN_ROWS && !hit; r++) {
                    for (int c = 0; c < ALIEN_COLS && !hit; c++) {
                        if (!invaders.aliens[r][c]) continue;
                        float ax = invaders.alienX(c);
                        float ay = invaders.alienY(r);
                        if (bx >= ax - 1.0f && bx <= ax + (float)ALIEN_W + 1.0f &&
                            by >= ay && by <= ay + (float)ALIEN_H) {
                            invaders.killAt(r, c, wave);
                            projectiles.deactivatePB(i);
                            score += ALIEN_POINTS[r & 3];
                            playSound(NOTE_D5, 30);
                            float px = ax + (float)ALIEN_W / 2.0f;
                            float py = ay + (float)ALIEN_H / 2.0f;
                            particles.spawn(px, py, COL_ALIEN_ROW[r & 3], 12);
                            shake.trigger(2);
                            hit = true;
                        }
                    }
                }

                if (!hit && bunkers.checkBulletHit(bx, by)) {
                    projectiles.deactivatePB(i);
                }
            }

            if (invaders.count <= 0) {
                state = WAVE_CLEAR;
                waveClearMs = now;
                playSound(NOTE_C5, 50); delay(60);
                playSound(NOTE_E5, 50); delay(60);
                playSound(NOTE_G5, 40);
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
                state = GAMEOVER;
                newRecord = (score > highScore);
                if (newRecord) highScore = score;
                gameOverMs = now;
                {
                    Preferences prefs; prefs.begin("os", false);
                    int32_t hs = prefs.getInt("hs_spaceinvaders", 0);
                    if (score > hs) prefs.putInt("hs_spaceinvaders", score);
                    prefs.end();
                }
                playSound(NOTE_G3, 100); delay(100);
                playSound(NOTE_E3, 100);
                shake.trigger(5);
                break;
            }

            if (invaders.canFire(wave) && invaders.count > 0 &&
                projectiles.activeEB() < MAX_EBULLETS) {
                invaders.markFired();
                for (int tries = 0; tries < 20; tries++) {
                    int c = random(0, ALIEN_COLS);
                    for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
                        if (invaders.aliens[r][c]) {
                            float ex = invaders.alienCenterX(c);
                            float ey = invaders.alienBottomY(r);
                            projectiles.addEnemyBullet(ex, ey + 1.0f, COL_ALIEN_ROW[r & 3]);
                            tries = 99;
                            break;
                        }
                    }
                }
            }

            projectiles.updateEnemyBullets(dt);

            for (int i = 0; i < MAX_EBULLETS; i++) {
                if (!projectiles.eBullets[i].active) continue;
                float bx = projectiles.eBullets[i].x;
                float by = projectiles.eBullets[i].y;

                if (bunkers.checkBulletHit(bx, by)) {
                    projectiles.deactivateEB(i);
                    continue;
                }

                if (!player.isInvincible()) {
                    if (bx >= player.hitboxLeft() - 1.0f &&
                        bx <= player.hitboxRight() + 1.0f &&
                        by >= player.hitboxTop() &&
                        by <= player.hitboxBottom()) {
                        projectiles.deactivateEB(i);
                        player.takeDamage();
                        playSound(NOTE_B3, 80);
                        shake.trigger(4);
                        flash.trigger();

                        if (player.lives <= 0) {
                            state = GAMEOVER;
                            newRecord = (score > highScore);
                            if (newRecord) highScore = score;
                            gameOverMs = now;
                            {
                                Preferences prefs; prefs.begin("os", false);
                                int32_t hs = prefs.getInt("hs_spaceinvaders", 0);
                                if (score > hs) prefs.putInt("hs_spaceinvaders", score);
                                prefs.end();
                            }
                            delay(100);
                            playSound(NOTE_E3, 100);
                            shake.trigger(5);
                            break;
                        }
                    }
                }
            }

            if (state != PLAYING) break;

            particles.update(dt);

            renderGameScene(canvas, starfield, invaders, player, bunkers,
                            projectiles, particles, flash, score, wave,
                            player.lives, currentFPS, showFps);

            checkScreenshot(canvas);
            canvas.pushSprite((int)shake.shakeX, (int)shake.shakeY);
            break;
        }

        case WAVE_CLEAR:
            drawWaveClear(canvas, starfield, wave, score);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            if (now - waveClearMs > 2000) {
                startNewWave();
                state = PLAYING;
            }
            break;

        case GAMEOVER:
            drawGameOver(canvas, starfield, score, highScore, newRecord);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            if (pressA && now - gameOverMs > 600) {
                playSound(NOTE_D5, 50);
                resetGame();
            }
            if (pressB && now - gameOverMs > 600) {
                returnToOS();
            }
            break;

        case PAUSE:
            renderGameScene(canvas, starfield, invaders, player, bunkers,
                            projectiles, particles, flash, score, wave,
                            player.lives, currentFPS, showFps);
            drawPauseOverlay(canvas);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            if (pressA) {
                playSound(NOTE_D5, 40);
                state = PLAYING;
                lastFrameMs = millis();
            }
            if (pressB) {
                playSound(NOTE_G4, 50);
                returnToOS();
            }
            break;
    }
}
