// ============================================================
//  E-OS PACMAN — Moduler mimari
//  Setup, loop, state machine ve global degiskenler
// ============================================================

#include "Config.h"
#include "Player.h"
#include "Ghosts.h"
#include "Renderer.h"

// ============================================================
//  Global degisken tanimlari
// ============================================================
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);
bool soundEnabled = true;
bool showFps = false;

Actor pac;
Ghost ghosts[NUM_GHOSTS];
uint8_t gameMap[ROWS][COLS];

int score = 0;
int highScore = 0;
int lives = 3;
int dotsLeft = 0;
int level = 1;
int joyCenterX = ADC_CENTER;
int joyCenterY = ADC_CENTER;
uint32_t stateTimer = 0;
uint32_t soundEndTime = 0;
int prevBtnA = HIGH;
GameState state = TITLE;

// ============================================================
//  OS Wrapper fonksiyonlari
// ============================================================
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySoundManual(freq, dur, soundEnabled, soundEndTime);
}

void returnToOS() {
    osReturnToOS(tft, false);
}

// ============================================================
//  resetActors — Pacman ve hayaletleri baslangica koy
// ============================================================
void resetActors() {
    pac.x = 9 * TILE + HALF_TILE;
    pac.y = 11 * TILE + HALF_TILE;
    pac.dx = -1; pac.dy = 0;
    pac.ndx = -1; pac.ndy = 0;
    pac.speed = PAC_BASE_SPEED + (level * PAC_SPEED_PER_LV);

    uint16_t ghostColors[] = {COL_GHOST_RED, COL_GHOST_PINK, COL_GHOST_CYAN};
    for (int i = 0; i < NUM_GHOSTS; i++) {
        ghosts[i].a.x = (8 + i) * TILE + HALF_TILE;
        ghosts[i].a.y = 7 * TILE + HALF_TILE;
        ghosts[i].a.dx = (i == 1) ? 1 : -1;
        ghosts[i].a.dy = 0;
        ghosts[i].a.speed = GHOST_BASE_SPEED + (level * GHOST_SPEED_PER_LV);
        ghosts[i].type = i;
        ghosts[i].mode = GHOST_CHASE;
        ghosts[i].color = ghostColors[i];
        ghosts[i].lastTileC = -1;
        ghosts[i].lastTileR = -1;
    }
}

// ============================================================
//  resetLevel — Bolumu veya tum oyunu sifirla
//  fullReset: true ise skor/can/level sifirlanir (yeni oyun)
// ============================================================
void resetLevel(bool fullReset) {
    if (fullReset) {
        score = 0;
        lives = 3;
        level = 1;
    }

    if (fullReset || dotsLeft == 0) {
        dotsLeft = 0;
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                gameMap[r][c] = MAP_TEMPLATE[r][c];
                if (gameMap[r][c] == CELL_DOT || gameMap[r][c] == CELL_POWER) dotsLeft++;
            }
        }
    }

    resetActors();
    state = READY;
    stateTimer = millis();
}

// ============================================================
//  setup — E-OS baslatma sablonu
// ============================================================
void setup() {
    // 1) Buzzer sustur
    osInitBuzzer();

    // 2) OLED kapat (acilis flicker onleme)
    Wire.begin(I2C_SDA, I2C_SCL);
    osOLEDOff();

    // 3) Boot partition OS'e yonlendir
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) esp_ota_set_boot_partition(os_part);

    // 4) Buton pinleri
    osInitButtons();

    // 5) NVS ayarlari yukle
    soundEnabled = osLoadSoundSetting(true);
    showFps = false;
    highScore = osLoadHighScore("hs_pacman", 0);

    // 6) SPI baslat
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    // 7) Dev Tools
    initDevTools(tft);

    // 8) TFT baslat
    tft.init();
    tft.setRotation(1);
    tft.startWrite();
    tft.writecommand(0x36);
    tft.writedata(0xA0);
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);

    // 9) Canvas sprite (double buffer)
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // 10) Joystick kalibrasyon
    bool warn = false;
    while (analogRead(JOY_X) < 1400 || analogRead(JOY_X) > 2600 ||
           analogRead(JOY_Y) < 1400 || analogRead(JOY_Y) > 2600) {
        if (!warn) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED); tft.setTextSize(1);
            tft.setCursor(23, 60); tft.print("RELEASE JOYSTICK!");
            warn = true;
        }
        delay(50);
    }
    if (warn) { tft.fillScreen(TFT_BLACK); delay(300); }
    osCalibrateJoystick(joyCenterX, joyCenterY);

    // 11) Rastgele tohum
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());
}

// ============================================================
//  loop — Ana oyun dongusu (60 FPS delta-time)
// ============================================================
void loop() {
    // PAUSE toggle (joystick butonu)
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {
        if (state == PLAYING) {
            state = PAUSE;
            playSound(NOTE_G4, 50);
        }
    }
    prevJoySw = currJoySw;

    // Ton sure kontrolu
    uint32_t now = millis();
    osUpdateSound(soundEndTime);

    // Delta-time + 60 FPS frame kilidi
    static uint32_t lastFrame = 0;
    float dt = (now - lastFrame) / 1000.0f;
    if (dt > DT_CAP) dt = DT_CAP;
    if (dt < FRAME_SEC) return;
    lastFrame = now;

    // BTN_A kenar algilama
    int btnA = digitalRead(BTN_A);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    prevBtnA = btnA;

    // ============================================================
    //  STATE: TITLE — Baslik/acilis ekrani
    // ============================================================
    if (state == TITLE) {
        canvas.fillSprite(COL_BG);

        canvas.setTextSize(2);
        canvas.setTextColor(COL_TITLE_SHADOW);
        canvas.setCursor(45, 11);
        canvas.print("PACMAN");
        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(44, 10);
        canvas.print("PACMAN");

        bool mouthOpen = (millis() / 200) % 2 == 0;
        canvas.fillCircle(80, 50, 15, COL_PAC);
        if (mouthOpen) canvas.fillTriangle(80, 50, 100, 35, 100, 65, TFT_BLACK);

        canvas.setTextSize(1);
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(10, 95);
        canvas.print("[A] Start");

        canvas.setTextColor(COL_LIGHTGREY);
        canvas.setCursor(90, 95);
        canvas.print("[B] OS Menu");

        canvas.setTextColor(TFT_GREEN);
        canvas.setCursor(10, 110);
        canvas.print("[JOY] Move");

        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(92, 110);
        canvas.printf("Best: %d", highScore);

        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        if (pressA) {
            playSound(NOTE_E5, 50);
            resetLevel(true);
        }
        if (!digitalRead(BTN_B)) {
            delay(DEBOUNCE_DELAY);
            returnToOS();
        }
        return;
    }

    // ============================================================
    //  STATE: GAMEOVER / WIN — Sonuc ekrani
    // ============================================================
    if (state == GAMEOVER || state == WIN) {
        canvas.fillSprite(TFT_BLACK);

        // Ortak OS game-over/win ekrani (EN metin + otomatik skor/rekor/NEW BEST)
        osDrawGameOver(canvas, state == WIN, score, highScore);

        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        if (pressA) {
            playSound(NOTE_D5, 50);
            delay(DEBOUNCE_DELAY);
            resetLevel(true);
        }
        if (!digitalRead(BTN_B)) {
            delay(DEBOUNCE_DELAY);
            returnToOS();
        }
        return;
    }

    // ============================================================
    //  STATE: READY — "HAZIR!" geri sayim sonra PLAYING
    // ============================================================
    if (state == READY) {
        drawMap(); drawHUD();
        drawPacman(pac.x, pac.y, pac.dx, pac.dy);
        for (int i = 0; i < NUM_GHOSTS; i++) drawGhost(ghosts[i]);
        canvas.setTextColor(TFT_YELLOW); canvas.setTextSize(1);
        canvas.setCursor(65, 80); canvas.print("READY!");
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        if (now - stateTimer > READY_DURATION) state = PLAYING;
        return;
    }

    // ============================================================
    //  STATE: DYING — Pacman olum animasyonu
    // ============================================================
    if (state == DYING) {
        drawMap(); drawHUD();
        int r = 3 - (now - stateTimer) / DYING_STEP;
        if (r > 0) canvas.fillCircle(pac.x, pac.y + MAP_Y, r, COL_PAC);
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        if (now - stateTimer > DYING_DURATION) {
            lives--;
            if (lives <= 0) {
                if (score > highScore) {
                    highScore = score;
                    osSaveHighScore("hs_pacman", highScore);
                }
                state = GAMEOVER;
                playSound(NOTE_E3, 120);
            } else {
                resetActors();
                state = READY;
                stateTimer = now;
            }
        }
        return;
    }

    // ============================================================
    //  STATE: PLAYING — Aktif oynanis
    // ============================================================
    if (state == PLAYING) {
        applyPacmanInput();
        movePacman(dt);

        int pc = (int)pac.x / TILE;
        int pr = (int)pac.y / TILE;
        if (pr >= 0 && pr < ROWS && pc >= 0 && pc < COLS) {
            if (gameMap[pr][pc] == CELL_DOT) {
                gameMap[pr][pc] = CELL_EMPTY; score += 10; dotsLeft--;
                playSound(NOTE_E5, 20);
            } else if (gameMap[pr][pc] == CELL_POWER) {
                gameMap[pr][pc] = CELL_EMPTY; score += 50; dotsLeft--;
                playSound(NOTE_G5, 40);
                for (int i = 0; i < NUM_GHOSTS; i++) {
                    if (ghosts[i].mode != GHOST_EATEN) {
                        ghosts[i].mode = GHOST_SCARED;
                        ghosts[i].scaredUntil = now + SCARED_DURATION;
                    }
                }
            }
        }

        if (dotsLeft == 0) {
            level++;
            resetLevel(false);
            return;
        }

        for (int i = 0; i < NUM_GHOSTS; i++) {
            moveGhost(ghosts[i], dt);

            if (abs(pac.x - ghosts[i].a.x) < 5.0f && abs(pac.y - ghosts[i].a.y) < 5.0f) {
                if (ghosts[i].mode == GHOST_SCARED) {
                    ghosts[i].mode = GHOST_EATEN;
                    score += 200;
                    playSound(NOTE_D5, 40);
                } else if (ghosts[i].mode == GHOST_CHASE) {
                    state = DYING;
                    stateTimer = now;
                    playSound(NOTE_G3, 150);
                    break;
                }
            }
        }

        drawMap(); drawHUD();
        drawPacman(pac.x, pac.y, pac.dx, pac.dy);
        for (int i = 0; i < NUM_GHOSTS; i++) drawGhost(ghosts[i]);
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        return;
    }

    // ============================================================
    //  STATE: PAUSE — Duraklatilmis, karartma + menu
    // ============================================================
    if (state == PAUSE) {
        drawMap(); drawHUD();
        drawPacman(pac.x, pac.y, pac.dx, pac.dy);
        for (int i = 0; i < NUM_GHOSTS; i++) drawGhost(ghosts[i]);

        // Ortak OS pause kutusu (EN metin, pac-man sarisi vurgu)
        osDrawPause(canvas, TFT_YELLOW);

        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);

        if (!digitalRead(BTN_A)) {
            playSound(NOTE_D5, 40);
            delay(DEBOUNCE_DELAY);
            state = PLAYING;
        }
        if (!digitalRead(BTN_B)) {
            delay(DEBOUNCE_DELAY);
            returnToOS();
        }
        return;
    }
}
