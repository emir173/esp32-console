// ============================================================
//  E-OS — GALACTIC STRIKE
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanli cift tamponlama (Flicker-Free)
//
//  Moduler yapi: Config.h + Player.h + Enemies.h +
//                Projectiles.h + Renderer.h + .ino
//
//  Kontroller:
//    JOY_X/Y -> Gemi hareketi
//    BTN_A   -> Ates
//    BTN_B   -> OS Menu'ye don
//    Buzzer  -> Ses efektleri
// ============================================================

#include "Config.h"
#include "Player.h"
#include "Enemies.h"
#include "Projectiles.h"
#include "Renderer.h"
#include "Input.h"

// ============ Donanim Nesneleri (Tekil Tanim) ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Ses ============
bool soundEnabled = true;
bool showFps = false;

void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

void returnToOS() {
    saveBestIfNeeded();
    osReturnToOS(tft, soundEnabled);
}

void saveBestIfNeeded() {
    if (ship.score > highScore) {
        highScore = ship.score;
        osSaveHighScore(HIGH_SCORE_KEY, highScore);
    }
}

void finishGame() {
    newRecord = ship.score > highScore;
    saveBestIfNeeded();
    state = ST_GAMEOVER;
    stateTimer = millis();
}

void enterGameMenu() {
    saveBestIfNeeded();
    state = ST_TITLE;
}

// ============ Durum ============
State state = ST_TITLE;

// ============ Yildiz Havuzu ============
Star stars[MAX_STARS];

// ============ Mermi Havuzlari ============
Bullet pBullets[MAX_P_BULLETS];
Bullet eBullets[MAX_E_BULLETS];

// ============ Dusman Havuzu ============
Enemy enemies[MAX_ENEMIES];

// ============ Patlama Havuzu ============
Explosion explosions[MAX_EXPLOSIONS];

// ============ Power-Up Havuzu ============
PowerUp powerUps[MAX_POWERUPS];

// ============ Oyuncu Gemisi ============
Ship ship;

// ============ Dalga Sistemi ============
int curWave = 0;
float waveSpawnTimer = 0.0f;
int waveEnemiesSpawned = 0;
int waveEnemiesPerWave = 5;
bool waveActive = false;

// ============ Genel Degiskenler ============
int joyCenterX, joyCenterY;
uint32_t lastFrameMs = 0;
float animTime = 0.0f;
int animFrame = 0;
uint32_t stateTimer = 0;
int highScore = 0;
bool newRecord = false;
bool canvasReady = false;
StrikeButton buttonA, buttonB, buttonC, buttonJoy;

uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;

// ============================================================
//  BASLANGIC AYARLARI
// ============================================================
void initStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(0, SCR_W);
        stars[i].y = random(0, SCR_H);
        stars[i].layer = random(0, 3);
        stars[i].speed = 9.0f + stars[i].layer * 12.0f;
    }
}

void resetGame() {
    ship.x = SCR_W / 2;
    ship.y = SCR_H - 20;
    ship.hp = ship.maxHp = 5;
    ship.shootCD = 0.0f;
    ship.tripleTimer = 0.0f;
    ship.shieldTimer = 0.0f;
    ship.invincTimer = 3.0f;
    ship.score = 0;
    curWave = 0;
    waveSpawnTimer = 3.0f;
    waveEnemiesSpawned = 0;
    waveEnemiesPerWave = 4;
    waveActive = true;
    animTime = 0.0f;
    animFrame = 0;
    newRecord = false;
    clearBullets();
    clearEnemies();
    clearExplosions();
    clearPowerUps();
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

    // 3) Guvenlik: Elektrik kesintisinde OS'tan basla (OTA partition)
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    // 4) Buton pinleri ayarla
    osInitButtons();

    // 5) NVS'ten ayarlari yukle (ses, rekor, FPS gostergesi)
    {
        Preferences prefs;
        prefs.begin("os", true);
        soundEnabled = prefs.getBool("sound_en", true);
        showFps = prefs.getBool("show_fps", false);
        prefs.end();
    }
    // NVS — maximum 15-character key, shared with the launcher.
    highScore = osLoadHighScore(HIGH_SCORE_KEY, 0);

    // 6) SPI baslat
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    // 7) Dev Tools (USB screenshot/video + FPS)
    initDevTools(tft);

    // 8) TFT ekran baslat
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    tft.startWrite();
    tft.writecommand(0x36);
    tft.writedata(0xA0);
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    // 9) Sprite tamponu (double buffering)
    canvas.setColorDepth(16);
    canvasReady = canvas.createSprite(SCR_W, SCR_H) != nullptr;
    if (!canvasReady) {
        tft.setTextColor(TFT_RED); tft.setTextSize(1);
        tft.setCursor(20, 50); tft.print("FRAMEBUFFER FAILED");
        tft.setCursor(23, 70); tft.print("[B/C] OS Menu");
        return;
    }

    // 10) Joystick kalibrasyon
    bool warn = false;
    bool calibrationTimedOut = false;
    uint32_t calibrationStart = millis();
    while (analogRead(JOY_X) < 1400 || analogRead(JOY_X) > 2600 ||
           analogRead(JOY_Y) < 1400 || analogRead(JOY_Y) > 2600) {
        if (!warn) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED); tft.setTextSize(1);
            tft.setCursor(23, 60); tft.print("RELEASE JOYSTICK!");
            warn = true;
        }
        if (!digitalRead(BTN_B) || !digitalRead(BTN_C)) returnToOS();
        if (millis() - calibrationStart >= 5000) {
            calibrationTimedOut = true;
            break;
        }
        delay(50);
    }
    if (warn) { tft.fillScreen(TFT_BLACK); delay(300); }
    if (calibrationTimedOut) {
        joyCenterX = joyCenterY = 2048;
    } else {
        osCalibrateJoystick(joyCenterX, joyCenterY);
    }

    // 11) Rastgele tohum
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    // 12) Oyun baslangic durumu
    initStars();
    state = ST_TITLE;
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
    uint32_t now = millis();
    if (!canvasReady) {
        if (!digitalRead(BTN_B) || !digitalRead(BTN_C)) returnToOS();
        delay(1);
        return;
    }

    buttonA.update(digitalRead(BTN_A), now);
    buttonB.update(digitalRead(BTN_B), now);
    buttonC.update(digitalRead(BTN_C), now);
    buttonJoy.update(digitalRead(JOY_SW), now);

    // Kare hizi siniri (60 FPS)
    if (now - lastFrameMs < FRAME_MS) return;

    float dt = (now - lastFrameMs) / 1000.0f;
    lastFrameMs = now;
    if (dt <= 0.0f) return;
    if (dt > 0.05f) dt = 0.05f;

    animTime += dt;
    animFrame = (int)(animTime * 30.0f);

    // FPS hesaplama
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    bool btnA = buttonA.stable == LOW;
    bool btnA_pressed = buttonA.takePress();
    bool btnB_pressed = buttonB.takePress();
    bool btnC_pressed = buttonC.takePress();
    bool joy_pressed = buttonJoy.takePress();

    // Joystick okuma + olu bolge
    float jx = (analogRead(JOY_X) - joyCenterX) / 2048.0f;
    float jy = (analogRead(JOY_Y) - joyCenterY) / 2048.0f;
    if (fabsf(jx) < 0.15f) jx = 0;
    if (fabsf(jy) < 0.15f) jy = 0;

    if (state != ST_PAUSE) updateStars(dt);

    // ---- Durum Makinesi ----
    switch (state) {

    case ST_TITLE:
        drawTitle();
        if (btnA_pressed) {
            resetGame();
            state = ST_PLAY;
            playSound(NOTE_E5, 50);
        } else if (btnB_pressed) {
            returnToOS();
        }
        break;

    case ST_PLAY:
        if (joy_pressed) {
            state = ST_PAUSE;
            playSound(NOTE_G4, 50);
            break;
        }
        {
            float mvx = jx, mvy = jy;
            float m = mvx * mvx + mvy * mvy;
            if (m > 1.0f) { float inv = 1.0f / sqrtf(m); mvx *= inv; mvy *= inv; }
            ship.x += mvx * 75.0f * dt;
            ship.y += mvy * 75.0f * dt;
        }
        ship.x = constrain(ship.x, 6.0f, (float)SCR_W - 6);
        ship.y = constrain(ship.y, 10.0f, (float)SCR_H - HUD_H - 8);

        if (ship.shootCD > 0.0f) ship.shootCD -= dt;

        if (ship.invincTimer > 0.0f) ship.invincTimer -= dt;
        if (ship.tripleTimer > 0.0f) ship.tripleTimer -= dt;
        if (ship.shieldTimer > 0.0f) ship.shieldTimer -= dt;
        if (btnA) firePlayer();

        updateBullets(dt);
        updateEnemies(dt);
        updateWaveSpawning(dt);
        updateCollisions(dt);
        if (state == ST_GAMEOVER) {
            drawGameOver();
            break;
        }
        updateExplosions(dt);

        canvas.fillSprite(COL_BG);
        drawStars();
        drawBullets();
        drawEnemies();
        drawShip();
        drawExplosions();
        drawPowerUps();
        drawHUD();
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        break;

    case ST_GAMEOVER:
        drawGameOver();
        if (now - stateTimer < 600) break;
        if (btnA_pressed) {
            resetGame();
            state = ST_PLAY;
            playSound(NOTE_D5, 50);
        } else if (btnB_pressed) {
            enterGameMenu();
            playSound(NOTE_G4, 50);
        } else if (btnC_pressed) {
            returnToOS();
        }
        break;

    case ST_PAUSE:
        canvas.fillSprite(COL_BG);
        drawStars();
        drawBullets();
        drawEnemies();
        drawShip();
        drawExplosions();
        drawPowerUps();
        drawHUD();

        osDrawPause(canvas, TFT_GREEN);   // ortak OS pause kutusu (EN, yesil tema)

        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);

        if (btnA_pressed || joy_pressed) {
            playSound(NOTE_D5, 40);
            state = ST_PLAY;
            lastFrameMs = millis();
        } else if (btnB_pressed) {
            playSound(NOTE_G4, 50);
            enterGameMenu();
        } else if (btnC_pressed) {
            playSound(NOTE_G4, 50);
            returnToOS();
        }
        break;
    }
}
