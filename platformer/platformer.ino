// ============================================================
//  E-OS — PLATFORMER
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanlı çift tamponlama (Flicker-Free)
//  Modüler yapı: Config.h + Player.h + Physics.h + Renderer.h
//
//  Kontroller:
//    JOY_X  -> Sağ/Sol hareket
//    BTN_A  -> Zıplama
//    BTN_B  -> OS Menü'ye dön
//    JOY_SW -> Pause toggle
//    Buzzer -> Ses efektleri
// ============================================================
#include "Renderer.h"
#include "Input.h"

// ============ Global Nesneler ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// ============ Ses ============
bool soundEnabled = true;

void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

void returnToOS() {
    saveHighScoreIfNeeded();
    osReturnToOS(tft, soundEnabled);
}

void enterGameMenu() {
    saveHighScoreIfNeeded();
    fanfare.active = false;
    osBuzzerOff();
    state = ST_TITLE;
}

// ============ Oyun Durumu ============
GameState state = ST_TITLE;

// ============ Oyuncu / Düşman ============
Player plr;
Enemy enemies[MAX_ENEMIES];
int numEnemies = 0;

// ============ Harita / Seviye ============
uint8_t mapData[MAP_H][MAP_W];
int curLevel = 0;

// ============ Kamera ============
int camX = 0;

// ============ Joystick ============
int joyCenterX = 0, joyCenterY = 0;

// ============ Zamanlama / Delta-Time ============
float gameDT = 0.0f;
uint32_t lastFrameMs = 0;
uint32_t lastFrameUs = 0;
constexpr uint32_t FRAME_US = 1000000UL / 60;
uint32_t animTick = 0;
uint32_t stateTimer = 0;
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;
bool showFps = false;

// ============ Skor ============
int highScore = 0;
bool newRecord = false;

PlatformerButton buttonA, buttonB, buttonC, buttonJoy;

// ============ Non-Blocking Fanfare ============
FlagFanfare fanfare;

// ============================================================
//  SETUP
// ============================================================
void setup() {
    osInitBuzzer();

    Wire.begin(I2C_SDA, I2C_SCL);
    osOLEDOff();

    // OTA güvenlik
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    osInitButtons();

    // NVS — ses ayarı + FPS göstergesi
    { Preferences prefs; prefs.begin("os", true); soundEnabled = prefs.getBool("sound_en", true); showFps = prefs.getBool("show_fps", false); prefs.end(); }

    // NVS — yüksek skor ("hs_platformer" anahtarı, "os" namespace)
    highScore = osLoadHighScore("hs_platformer", 0);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    initDevTools(tft);

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
    if (!canvas.createSprite(SCR_W, SCR_H)) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(8, 60);
        tft.print("NO FRAMEBUFFER RAM");
        while (true) {
            if (!digitalRead(BTN_B) || !digitalRead(BTN_C)) returnToOS();
            delay(10);
        }
    }

    // Joystick kalibrasyon
    bool warningShown = false;
    while (analogRead(JOY_X) < 1400 || analogRead(JOY_X) > 2600 || analogRead(JOY_Y) < 1400 || analogRead(JOY_Y) > 2600) {
        if (!warningShown) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED); tft.setTextSize(1);
            tft.setCursor(23, 60); tft.print("RELEASE JOYSTICK!");
            warningShown = true;
        }
        delay(50);
    }
    if (warningShown) { tft.fillScreen(TFT_BLACK); delay(300); }
    osCalibrateJoystick(joyCenterX, joyCenterY);

    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    state = ST_TITLE;
    lastFrameMs = millis();
    lastFrameUs = micros();
    fpsStartTime = millis();
    buttonA.reset(digitalRead(BTN_A), lastFrameMs);
    buttonB.reset(digitalRead(BTN_B), lastFrameMs);
    buttonC.reset(digitalRead(BTN_C), lastFrameMs);
    buttonJoy.reset(digitalRead(JOY_SW), lastFrameMs);
}

// ============================================================
//  LOOP — Delta-Time (60fps-ready) + State Machine
// ============================================================
void loop() {
    uint32_t now = millis();
    buttonA.update(digitalRead(BTN_A), now);
    buttonB.update(digitalRead(BTN_B), now);
    buttonC.update(digitalRead(BTN_C), now);
    buttonJoy.update(digitalRead(JOY_SW), now);

    uint32_t frameNowUs = micros();
    uint32_t elapsedUs = frameNowUs - lastFrameUs;
    if (elapsedUs < FRAME_US) { delay(1); return; }
    lastFrameUs = frameNowUs;

    // --- Delta-Time ---
    float rawDT = elapsedUs / 1000000.0f;
    gameDT = rawDT * DT_SCALE;
    if (gameDT > 1.5f) gameDT = 1.5f;
    lastFrameMs = now;
    animTick++;

    // --- Non-Blocking Fanfare (bayrak sesi) ---
    fanfare.update();

    bool btnA = buttonA.stable == LOW;
    bool btnA_pressed = buttonA.takePress();
    bool btnB_pressed = buttonB.takePress();
    bool btnC_pressed = buttonC.takePress();
    bool joyPressed = buttonJoy.takePress();

    // --- BTN_B: OS'a dön (menü ekranında) ---
    if (btnB_pressed && state == ST_TITLE) {
        playSound(NOTE_G4, 40);
        returnToOS();
        return;
    }

    // --- FPS Hesaplama ---
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = fpsFrameCount * 1000UL / (now - fpsStartTime);
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // --- JOY_SW: Pause toggle ---
    if (joyPressed && state == ST_PLAY) {
        state = ST_PAUSE;
        // The pause press belongs only to this transition.
        joyPressed = false;
        btnA_pressed = btnB_pressed = btnC_pressed = false;
        osBuzzerOff();
        playSound(NOTE_G4, 40);
    }

    // --- Joystick: yatay eksen ---
    float jx = (analogRead(JOY_X) - joyCenterX) / 2048.0f;
    if (fabsf(jx) < 0.15f) jx = 0;

    // === DURUM MAKİNESİ ===
    switch (state) {

    case ST_TITLE:
        drawTitleScreen();
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        if (btnA_pressed) {
            resetGame();
            state = ST_PLAY;
            playSound(NOTE_E5, 50);
        }
        break;

    case ST_PLAY:
        // --- Girdi ---
        if (jx < 0) { plr.vx = -MOVE_SPD; plr.facingRight = false; }
        else if (jx > 0) { plr.vx = MOVE_SPD; plr.facingRight = true; }
        else { plr.vx *= (1.0f - 0.4f * gameDT); if (fabsf(plr.vx) < 0.3f) plr.vx = 0; }

        // Zıplama buffer
        if (btnA_pressed) plr.jumpBuf = JUMP_BUFFER;
        if (plr.jumpBuf > 0.0f) plr.jumpBuf -= gameDT;

        // Zıplama (coyote time + jump buffer)
        if (plr.jumpBuf > 0.0f && (plr.grounded || plr.coyoteT > 0.0f)) {
            plr.vy = JUMP_VEL;
            plr.grounded = false;
            plr.coyoteT = 0.0f;
            plr.jumpBuf = 0.0f;
            playSound(NOTE_D5, 22);
        }

        // Variable jump: A bırakılınca yükselişi kıs
        if (!btnA && plr.vy < -2.0f) plr.vy = -2.0f;

        // Invincibility timer
        if (plr.invincTimer > 0.0f) plr.invincTimer -= gameDT;

        // --- Fizik ---
        updatePhysics();
        if (state == ST_PLAY) checkTilePickups();
        if (state == ST_PLAY) updateEnemies();

        // --- Kamera ---
        camX = (int)plr.x - SCR_W / 2 + PW / 2;
        if (camX < 0) camX = 0;
        if (camX > MAP_W * TILE - SCR_W) camX = MAP_W * TILE - SCR_W;

        // --- Çizim ---
        canvas.fillSprite(COL_SKY);
        drawTiles();
        drawEnemies();
        drawPlayer();
        drawHUD();
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        break;

    case ST_DEAD:
        if (now - stateTimer > 1000) {
            loadLevel(curLevel);
            state = ST_PLAY;
            break;
        }
        canvas.fillSprite(COL_SKY);
        drawTiles();
        drawHUD();
        canvas.setTextSize(1);
        canvas.setTextColor(COL_DEAD_TEXT);
        canvas.setCursor(50, 55);
        canvas.printf("CAN: %d", plr.lives);
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        break;

    case ST_LEVELCLEAR: {
        if (now - stateTimer > 2000) {
            curLevel++;
            if (curLevel >= NUM_LEVELS) {
                state = ST_WIN;
                stateTimer = now;
                saveHighScoreIfNeeded();
            } else {
                loadLevel(curLevel);
                state = ST_PLAY;
            }
            break;
        }
        canvas.fillSprite(COL_SKY);
        drawTiles();
        drawHUD();

        canvas.fillRoundRect(20, 35, 120, 50, 5, RGB(0, 0, 50));
        canvas.drawRoundRect(20, 35, 120, 50, 5, RGB(100, 150, 255));

        canvas.setTextSize(1);
        canvas.setTextColor(RGB(150, 255, 150));
        char lvlBuf[32];
        snprintf(lvlBuf, sizeof(lvlBuf), "LEVEL %d CLEARED!", curLevel + 1);
        int txtW = strlen(lvlBuf) * 6;
        canvas.setCursor((160 - txtW) / 2, 45);
        canvas.print(lvlBuf);

        char scBuf[32];
        snprintf(scBuf, sizeof(scBuf), "Score: %d", plr.score);
        int scW = strlen(scBuf) * 6;
        canvas.setCursor((160 - scW) / 2, 65);
        canvas.setTextColor(COL_WHITE);
        canvas.print("Score: ");
        canvas.setTextColor(COL_HUD_COIN);
        canvas.print(plr.score);

        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        break;
    }

    case ST_GAMEOVER:
        drawGameOverScreen();
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        if (btnC_pressed) {
            returnToOS();
        } else if (btnA_pressed) {
            resetGame();
            state = ST_PLAY;
            playSound(NOTE_D5, 50);
        } else if (btnB_pressed) {
            enterGameMenu();
            playSound(NOTE_G4, 50);
        }
        break;

    case ST_WIN:
        drawWinScreen();
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        if (btnC_pressed) {
            returnToOS();
        } else if (btnA_pressed) {
            resetGame();
            state = ST_PLAY;
            playSound(NOTE_E5, 50);
        } else if (btnB_pressed) {
            enterGameMenu();
            playSound(NOTE_G4, 50);
        }
        break;

    case ST_PAUSE:
        drawPauseOverlay();
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);

        if (btnC_pressed) {
            returnToOS();
        } else if (btnB_pressed) {
            enterGameMenu();
            playSound(NOTE_G4, 50);
        } else if (btnA_pressed || joyPressed) {
            playSound(NOTE_D5, 40);
            state = ST_PLAY;
            lastFrameMs = millis();
            lastFrameUs = micros();
        }
        break;
    }
}
