// ============================================================
//  E-OS — MODE7 RACING (Modüler Mimari)
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Mode7 pseudo-3D rendering + AI rakipler + OLED radar
//
//  Kontroller:
//    JOY_X  -> Direksiyon
//    BTN_A  -> Gaz
//    BTN_B  -> Fren / OS Menu
//    JOY_SW -> Pause
//    Buzzer -> Motor sesi + efektler
//
//  Dosya Yapisi:
//    Config.h   -> Sabitler, renkler, struct'lar, statik bellek
//    Player.h   -> Oyuncu fizigi, AI, motor sesi, carpisma
//    Renderer.h -> Tum grafik cizim fonksiyonlari
//    mode7.ino  -> Setup, loop, state machine (bu dosya)
// ============================================================

#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"
#include "Config.h"
#include "Input.h"
#include "Race.h"
#include "Player.h"
#include "Renderer.h"

// ============ OLED Nesnesi ============
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// ============ TFT / Sprite / Framebuffer ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);
uint16_t* fb = nullptr;

// ============ Statik Bellek Dizileri (Config.h'de extern, burada tanimli) ============
// MALLOC YASAK! Tum harita, direk ve checkpoint verileri sabit boyutlu statik dizilerdir.
uint8_t    trackMap[MAP_W * MAP_H];
Vec2       posts[NUM_POSTS];
Checkpoint checkpoints[NUM_CHECKPOINTS];

// ============ Ses ============
bool soundEnabled = true;

void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

void returnToOS() {
    osBuzzerOff();
    osReturnToOS(tft, soundEnabled);
}

// ============ Oyun Durumu ============
GameState state = ST_TITLE;
Racer player;
Racer aiCars[NUM_AI];
const uint16_t aiColors[NUM_AI] = { RGB(40, 120, 255), RGB(255, 200, 40) };
int aiCheckpoint[NUM_AI] = {1, 1};
int aiLap[NUM_AI] = {0, 0};
int playerNextCP = 1;
int playerLap = 0;

// ============ Zamanlama ============
uint32_t lastFrameMs = 0;
uint32_t animTick = 0;
uint32_t stateTimer = 0;
uint32_t pauseStartTime = 0;
int countdownVal = 3;
uint32_t lapStartTime = 0;
uint32_t lastLapTime = 0;
uint32_t bestLapTime = 0;       // 0 = kayıt yok (ms cinsinden en iyi tur)
bool newRecord = false;         // Yarışta yeni rekor kırıldı mı (RESULT ekranı vurgusu)

// ============ FPS ============
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;
bool showFps = false;

// ============ OLED Radar Sayaci ============
uint8_t radarTick = 0;

// ============ Joystick Kalibrasyon ============
int joyCenterX, joyCenterY;
Mode7Button buttonA, buttonB, buttonC, buttonJoy;

void showTitle() {
    osBuzzerOff();
    state = ST_TITLE;
    drawTitleScreen(canvas, bestLapTime);
    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}

void startRace(uint32_t now) {
    player = {checkpoints[0].x, checkpoints[0].y, 0.0f, 0.0f};
    playerLap = 0;
    playerNextCP = 1;
    lastLapTime = 0;
    newRecord = false;
    radarTick = 0;
    for (int i = 0; i < NUM_AI; ++i) {
        aiCars[i] = {player.x - 8.0f * (i + 1), player.y + i, 0.0f, 0.0f};
        aiLap[i] = 0;
        aiCheckpoint[i] = 1;
    }
    countdownVal = 3;
    stateTimer = now;
    state = ST_COUNTDOWN;
    playSound(NOTE_E5, 50);
}

void resumeRace(uint32_t now) {
    // Freeze the lap clock while paused, using wrap-safe unsigned durations.
    lapStartTime += now - pauseStartTime;
    lastFrameMs = now;
    state = ST_RACING;
    playSound(NOTE_D5, 40);
}

void finishRace(uint32_t now) {
    state = ST_FINISH;
    stateTimer = now;
    osBuzzerOff();
    int pos = racePosition(player, playerLap, playerNextCP, aiCars, aiLap, aiCheckpoint);
    drawFinishScreen(canvas, pos, bestLapTime, newRecord);
    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
    playSound(NOTE_C5, 50);
    delay(60);
    playSound(NOTE_E5, 50);
    delay(60);
    playSound(NOTE_G5, 40);
}

// ============================================================
//  buildTrack — Pist haritasini olustur (oval/virajli halka)
//  trackMap statik dizisine dogrudan yazar. MALLOC YOK.
// ============================================================
void buildTrack() {
    memset(trackMap, 0, MAP_H * MAP_W);

    float cx = MAP_W / 2.0f;
    float cy = MAP_H / 2.0f;
    float ra = MAP_W / 2.0f - 26;
    float rb = MAP_H / 2.0f - 26;

    for (int deg = 0; deg < 360; deg++) {
        float rad = deg * M_PI / 180.0f;
        float wob = TRK_WOB(rad);
        float mx = cx + ra * wob * cosf(rad);
        float my = cy + rb * wob * sinf(rad);

        for (int dy = -ROAD_W; dy <= ROAD_W; dy++) {
            for (int dx = -ROAD_W; dx <= ROAD_W; dx++) {
                int px = (int)(mx + dx);
                int py = (int)(my + dy);
                if (px >= 0 && px < MAP_W && py >= 0 && py < MAP_H) {
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist <= ROAD_W) trackMap[py * MAP_W + px] = 1;
                }
            }
        }
    }
}

// ============================================================
//  buildCheckpoints — Pist cevresine esit aralikli checkpoint yerlestir
// ============================================================
void buildCheckpoints() {
    float cx = MAP_W / 2.0f;
    float cy = MAP_H / 2.0f;
    float ra = MAP_W / 2.0f - 26;
    float rb = MAP_H / 2.0f - 26;

    for (int i = 0; i < NUM_CHECKPOINTS; i++) {
        float rad = -M_PI / 2.0f + i * 2.0f * M_PI / NUM_CHECKPOINTS;
        float wob = TRK_WOB(rad);
        checkpoints[i].x = cx + ra * wob * cosf(rad);
        checkpoints[i].y = cy + rb * wob * sinf(rad);
        checkpoints[i].radius = ROAD_W + 4;
    }
}

// ============================================================
//  buildPosts — Yol kenarina direkleri yerlestir (billboard render icin)
// ============================================================
void buildPosts() {
    float cx = MAP_W / 2.0f, cy = MAP_H / 2.0f;
    float ra = MAP_W / 2.0f - 26, rb = MAP_H / 2.0f - 26;
    float edge = ROAD_W + 3;

    for (int i = 0; i < NUM_POSTS; i++) {
        float rad = i * 2.0f * M_PI / NUM_POSTS;
        float wob = TRK_WOB(rad);
        posts[i].x = cx + (ra * wob + edge) * cosf(rad);
        posts[i].y = cy + (rb * wob + edge) * sinf(rad);
    }
}

// ============================================================
//  SETUP — Donanim baslatma, kalibrasyon, pist olusturma
//  E-OS zorunlu adim sirasina uygun (osInitBuzzer ONCE gelir)
// ============================================================
void setup() {
    // 1) Buzzer sustur (HEMEN — reset sonrasi cizirti onler)
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
    soundEnabled = osLoadSoundSetting(true);
    // En iyi tur suresi — "hs_mode7" anahtari, "os" namespace (0 = kayit yok)
    int savedBestLap = osLoadHighScore("hs_mode7", 0);
    bestLapTime = savedBestLap > 0 ? (uint32_t)savedBestLap : 0;
    {
        Preferences prefs;
        prefs.begin("os", true);
        showFps = prefs.getBool("show_fps", false);
        prefs.end();
    }

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
    tft.setTextWrap(false);

    // 9) OLED baslat
    oled.setBusClock(400000);
    oled.begin();
    oled.clearBuffer();
    oled.sendBuffer();

    // 10) Sprite tamponu (double buffering)
    canvas.setColorDepth(16);
    canvas.createSprite(SW, SH);
    canvas.setTextWrap(false);
    fb = (uint16_t*)canvas.getPointer();
    if (!fb) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(1);
        tft.setCursor(10, 50); tft.print("FRAMEBUFFER ERROR");
        tft.setCursor(10, 65); tft.print("B/C: OS Menu");
        return;
    }

    // 11) Joystick kalibrasyon
    bool warn = false;
    uint32_t calibrationStart = millis();
    while (analogRead(JOY_X) < 1400 || analogRead(JOY_X) > 2600 ||
           analogRead(JOY_Y) < 1400 || analogRead(JOY_Y) > 2600) {
        if (!warn) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED); tft.setTextSize(1);
            tft.setCursor(23, 60); tft.print("RELEASE JOYSTICK!");
            warn = true;
        }
        delay(50);
        if (millis() - calibrationStart >= 5000) break;
    }
    if (warn) { tft.fillScreen(TFT_BLACK); delay(300); }
    if (analogRead(JOY_X) >= 1400 && analogRead(JOY_X) <= 2600 &&
        analogRead(JOY_Y) >= 1400 && analogRead(JOY_Y) <= 2600) {
        osCalibrateJoystick(joyCenterX, joyCenterY);
    } else {
        joyCenterX = joyCenterY = 2048;
    }

    // 12) Rastgele tohum
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    // Pist olustur — statik dizilere yazar, MALLOC YOK
    buildTrack();
    buildCheckpoints();
    buildPosts();

    // Baslangic pozisyonu (oval pistin ust noktasi)
    player.x = checkpoints[0].x;
    player.y = checkpoints[0].y;
    player.angle = 0.0f;
    player.speed = 0;

    // AI baslangic grid — oyuncunun arkasinda
    aiCars[0] = {player.x - 8, player.y, 0.0f, 0};
    aiCars[1] = {player.x - 16, player.y + 1, 0.0f, 0};
    aiLap[0] = aiLap[1] = 0;
    aiCheckpoint[0] = aiCheckpoint[1] = 1;

    // Ilk durum: baslik ekrani
    state = ST_TITLE;
    uint32_t inputNow = millis();
    buttonA.reset(digitalRead(BTN_A), inputNow);
    buttonB.reset(digitalRead(BTN_B), inputNow);
    buttonC.reset(digitalRead(BTN_C), inputNow);
    buttonJoy.reset(digitalRead(JOY_SW), inputNow);
    showTitle();

    lastFrameMs = millis();
    fpsStartTime = millis();
}

// ============================================================
//  LOOP — Ana oyun dongusu (delta-time + state machine)
// ============================================================
void loop() {
    if (!fb) {
        if (!digitalRead(BTN_B) || !digitalRead(BTN_C)) returnToOS();
        delay(10);
        return;
    }
    uint32_t now = millis();
    float dt = (now - lastFrameMs) / 1000.0f;
    dt = constrain(dt, 0.001f, 0.05f);
    lastFrameMs = now;
    animTick++;

    // --- Buton okuma + kenar algilama ---
    bool pressA = buttonA.update(digitalRead(BTN_A), now);
    bool pressB = buttonB.update(digitalRead(BTN_B), now);
    bool pressC = buttonC.update(digitalRead(BTN_C), now);
    bool pressJoy = buttonJoy.update(digitalRead(JOY_SW), now);
    bool btnA = buttonA.down();
    bool btnB = buttonB.down();

    // --- FPS hesaplama ---
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // --- JOY_SW: Pause toggle (kenar tespiti) ---
    if (pressJoy) {
        if (state == ST_RACING) {
            state = ST_PAUSE;
            pauseStartTime = now;
            osBuzzerOff();
            playSound(NOTE_G4, 40);
            drawPauseScreen(canvas);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            return;
        } else if (state == ST_PAUSE) {
            resumeRace(now);
            dt = 0.0f;
        }
    }

    // --- Direksiyon (joystick X) ---
    float jx = constrain((analogRead(JOY_X) - joyCenterX) / 2048.0f, -1.0f, 1.0f);
    if (fabsf(jx) < 0.15f) jx = 0;

    // --- State Machine ---
    switch (state) {

    case ST_TITLE:
        checkScreenshot(canvas);
        if (pressB || pressC) { returnToOS(); break; }
        if (pressA) {
            startRace(now);
        }
        break;

    case ST_COUNTDOWN:
        if (pressB) { showTitle(); break; }
        if (pressC) { returnToOS(); break; }
        if (now - stateTimer >= 1000) {
            stateTimer = now;
            countdownVal--;
            if (countdownVal <= 0) {
                state = ST_RACING;
                lapStartTime = now;
                playSound(NOTE_E5, 40);
            } else {
                playSound(NOTE_A4, 50);
            }
        }

        renderMode7(fb, player, animTick);
        renderPosts(fb, player);
        for (int i = 0; i < NUM_AI; i++) renderAICar(fb, aiCars[i], player, aiColors[i]);
        renderPlayerCar(fb, 0, false);

        canvas.setTextSize(2);
        canvas.setTextColor(countdownVal == 1 ? TFT_RED : TFT_YELLOW, TFT_BLACK);
        canvas.setCursor(SW / 2 - (countdownVal > 0 ? 6 : 12), ROAD_H / 2 - 8);
        if (countdownVal > 0) canvas.print(countdownVal);
        else canvas.print("GO");

        drawHUD(canvas, player, playerLap, playerNextCP, aiCars, aiLap, aiCheckpoint,
                lastLapTime, bestLapTime, currentFPS, showFps);
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);
        break;

    case ST_RACING:
        {
            bool gas = btnA;
            bool brake = btnB;
            float jy = (analogRead(JOY_Y) - joyCenterY) / 2048.0f;
            if (jy < -0.3f) gas = true;

            updatePlayerPhysics(player, dt, gas, brake, jx);
            updateMotorSound(player, gas, soundEnabled, animTick);

            // Checkpoint kontrolu
            if (advanceCheckpoint(player, playerNextCP, playerLap)) {
                    lastLapTime = now - lapStartTime;
                    lapStartTime = now;
                    // 0 = kayıt yok → ilk tur her zaman rekor sayılır
                    if (bestLapTime == 0 || lastLapTime < bestLapTime) {
                        bestLapTime = lastLapTime;
                        osSaveHighScore("hs_mode7", (int)bestLapTime);
                        newRecord = true;
                    }
                    playSound(NOTE_D5, 40);

                    if (playerLap >= TOTAL_LAPS) {
                        finishRace(now);
                        break;
                    }
            }

            // AI guncelle
            for (int i = 0; i < NUM_AI; i++) updateAI(aiCars[i], aiCheckpoint[i], aiLap[i], dt, player);

            // Carpisma
            for (int i = 0; i < NUM_AI; i++) {
                bool collisionSfx = false;
                handlePlayerAICollision(player, aiCars[i], collisionSfx);
                if (collisionSfx && radarTick % 4 == 0) playSound(NOTE_G3, 50);
            }
            player.x = constrain(player.x, 1.0f, (float)MAP_W - 2);
            player.y = constrain(player.y, 1.0f, (float)MAP_H - 2);
        }

        renderMode7(fb, player, animTick);
        renderPosts(fb, player);
        for (int i = 0; i < NUM_AI; i++) renderAICar(fb, aiCars[i], player, aiColors[i]);
        renderPlayerCar(fb, jx, btnB);

        drawHUD(canvas, player, playerLap, playerNextCP, aiCars, aiLap, aiCheckpoint,
                lastLapTime, bestLapTime, currentFPS, showFps);
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);

        if (++radarTick >= 10) { radarTick = 0; drawRadar(oled, player, aiCars, playerLap, aiLap); }
        break;

    case ST_FINISH:
        if (pressC) { returnToOS(); break; }
        if (pressB) { showTitle(); break; }
        if (pressA) { startRace(now); break; }
        checkScreenshot(canvas);
        break;

    case ST_PAUSE:
        drawPauseScreen(canvas);
        checkScreenshot(canvas);
        canvas.pushSprite(0, 0);

        if (pressA) {
            resumeRace(now);
        }
        if (pressB) {
            showTitle();
        }
        if (pressC) {
            returnToOS();
        }
        break;
    }
}
