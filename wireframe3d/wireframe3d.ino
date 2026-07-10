// ============================================================
//  E-OS — WIREFRAME 3D
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite tabanli cift tamponlama (Flicker-Free)
//  3D wireframe render + uzay savasi
//
//  Kontroller:
//    JOY_X/Y -> Kamera / Nisan alma
//    BTN_A   -> Ates
//    BTN_B   -> OS Menu
//    Buzzer  -> Ses efektleri
// ============================================================

#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <esp_ota_ops.h>
#include <Preferences.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"
#include "Config.h"
#include "Math3D.h"
#include "Renderer.h"

// ============ Ekran Nesneleri ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// ============ Ses & OS Donus ============
bool soundEnabled = true;
bool showFps = false;

void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

void returnToOS() {
    osReturnToOS(tft, soundEnabled);
}

// ============ Oyun Durumu ============
GameState state = ST_TITLE;
int score = 0;
int hp = INITIAL_HP;
int kills = 0;
int wave = 0;
int highScore = 0;
float spawnTimer = INITIAL_SPAWN_T;
float shootCD = 0;

// ============ Kamera ============
float camYaw = 0;
float camPitch = 0;

// ============ Nesne Havuzlari ============
Star stars[MAX_STARS];
Object3D objects[MAX_OBJECTS];
Bullet3D bullets[MAX_BULLETS];
Boom booms[MAX_EXPLOSIONS];
SpaceDust dustArr[MAX_DUST];
bool dustInit = false;

// ============ Zamanlama ============
uint32_t lastFrameMs = 0;
uint32_t stateTimer = 0;
int joyCenterX = 2048, joyCenterY = 2048;

// ============ Dalga Yükselme Fanfari (Non-Blocking) ============
struct WaveFanfare {
    bool active = false;
    uint8_t idx = 0;
    unsigned long nextMs = 0;

    void start() { active = true; idx = 0; nextMs = millis(); }

    void update() {
        if (!active) return;
        unsigned long now = millis();
        if (now < nextMs) return;
        switch (idx) {
            case 0: playSound(NOTE_C5, 50); nextMs = now + 60; break;
            case 1: playSound(NOTE_E5, 50); nextMs = now + 60; break;
            case 2: playSound(NOTE_G5, 40); active = false; break;
        }
        idx++;
    }
};
WaveFanfare waveFanfare;

// ============================================================
//  BASLANGIC FONKSIYONLARI
// ============================================================

void initStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(0, SCR_W);
        stars[i].y = random(0, SCR_H - HUD_H);
        stars[i].bright = random(40, 160);
    }
}

void clearObjects() {
    for (int i = 0; i < MAX_OBJECTS; i++) objects[i].active = false;
}

void clearBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
}

void clearBooms() {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) booms[i].active = false;
}

void initDust() {
    for (int i = 0; i < MAX_DUST; i++) {
        dustArr[i].x = random(-40, 40);
        dustArr[i].y = random(-20, 20);
        dustArr[i].z = random(5, 50);
    }
    dustInit = true;
}

// ============================================================
//  SPAWN / PATLAMA
// ============================================================

void spawnObject() {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (objects[i].active) continue;
        Object3D &o = objects[i];
        o.active = true;
        o.type = (ObjType)random(0, 3);
        o.scale = 1.0f + random(0, 100) / 100.0f;
        o.rotAngle = random(0, 628) / 100.0f;
        o.rotSpeed = 0.5f + random(0, 125) / 100.0f;
        o.hitTimer = 0;

        float dist = SPAWN_DIST_MIN + random(0, (int)(SPAWN_DIST_RANGE * 10)) / 10.0f;
        float hAngle = random(0, 628) / 100.0f;
        float vAngle = (random(-30, 30)) * M_PI / 180.0f;
        o.pos.x = sinf(hAngle) * cosf(vAngle) * dist;
        o.pos.y = sinf(vAngle) * dist * 0.5f;
        o.pos.z = cosf(hAngle) * cosf(vAngle) * dist;

        Vec3 target = {0, 0, 0};
        target.x += random(-8, 8);
        target.y += random(-4, 4);
        target.z += random(-8, 8);
        Vec3 dir = v3norm(v3sub(target, o.pos));
        float spd = OBJ_BASE_SPEED + random(0, 100) / 20.0f + (wave * 0.5f);
        o.vel = v3mul(dir, spd);

        switch (o.type) {
            case OBJ_CUBE:    o.hp = 2; break;
            case OBJ_PYRAMID: o.hp = 1; break;
            case OBJ_DIAMOND: o.hp = 3; break;
        }
        return;
    }
}

void spawnBoom(Vec3 pos) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!booms[i].active) {
            booms[i] = {pos, 0, true};
            return;
        }
    }
}

// ============================================================
//  OYUN SIFIRLAMA
// ============================================================

void resetGame() {
    score = 0;
    hp = INITIAL_HP;
    kills = 0;
    wave = 0;
    spawnTimer = INITIAL_SPAWN_T;
    shootCD = 0;
    camYaw = 0;
    camPitch = 0;
    clearObjects();
    clearBullets();
    clearBooms();
}

// ============================================================
//  GUNCELLEME FONKSIYONLARI (Delta-Time ile)
// ============================================================

void updateObjects(float dt) {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) continue;
        Object3D &o = objects[i];
        o.rotAngle += o.rotSpeed * dt;
        if (o.hitTimer > 0) { o.hitTimer -= dt; if (o.hitTimer < 0) o.hitTimer = 0; }
        o.pos = v3add(o.pos, v3mul(o.vel, dt));

        float dist = v3len(o.pos);
        if (dist < COLLIDE_DIST) {
            o.active = false;
            hp--;
            spawnBoom(o.pos);
            playSound(NOTE_G3, 100);
            if (hp <= 0) {
                stateTimer = millis();
                if (score > highScore) {
                    highScore = score;
                    osSaveHighScore("hs_wireframe3d", highScore);
                }
                state = ST_GAMEOVER;
            }
            continue;
        }

        if (v3len(o.pos) > DESPAWN_DIST) {
            o.active = false;
        }
    }
}

void updateBullets(float dt) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        Bullet3D &b = bullets[i];
        b.pos = v3add(b.pos, v3mul(b.vel, dt));
        b.life -= dt;
        if (b.life <= 0) { b.active = false; continue; }

        for (int j = 0; j < MAX_OBJECTS; j++) {
            if (!objects[j].active) continue;
            Vec3 d = v3sub(b.pos, objects[j].pos);
            float dist2 = v3dot(d, d);
            float hitR = objects[j].scale * 2.0f;

            if (dist2 < hitR * hitR) {
                b.active = false;
                objects[j].hp--;
                objects[j].hitTimer = HIT_FLASH_DUR;

                if (objects[j].hp <= 0) {
                    objects[j].active = false;
                    spawnBoom(objects[j].pos);
                    int pts = objects[j].type == OBJ_DIAMOND ? 150 :
                              objects[j].type == OBJ_CUBE ? 100 : 75;
                    score += pts;
                    kills++;
                    playSound(NOTE_D5, 40);
                } else {
                    playSound(NOTE_A4, 25);
                }
                break;
            }
        }
    }
}

void updateSpawning(float dt) {
    spawnTimer -= dt;
    if (spawnTimer <= 0) {
        int activeCount = 0;
        for (int i = 0; i < MAX_OBJECTS; i++)
            if (objects[i].active) activeCount++;

        int maxActive = min(3 + wave, MAX_OBJECTS);
        if (activeCount < maxActive) {
            spawnObject();
        }

        spawnTimer = max(SPAWN_BASE_INT - wave * SPAWN_WAVE_DEC, SPAWN_MIN_INT);

        if (kills > 0 && kills % 10 == 0 && kills / 10 > wave) {
            wave = kills / 10;
            waveFanfare.start();
        }
    }
}

void updateBooms(float dt) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!booms[i].active) continue;
        booms[i].frame += dt;
        if (booms[i].frame > BOOM_DUR) booms[i].active = false;
    }
}

void updateDust(float dt) {
    if (!dustInit) initDust();
    for (int i = 0; i < MAX_DUST; i++) {
        dustArr[i].z -= DUST_SPEED * dt;
        if (dustArr[i].z < 1.0f) {
            dustArr[i].x = random(-40, 40);
            dustArr[i].y = random(-20, 20);
            dustArr[i].z = DUST_Z_RESET + random(0, 10);
        }
    }
}

// ============================================================
//  SETUP — E-OS Standart Baslatma Sirasi
// ============================================================

void setup() {
    Serial.begin(115200);

    // 1) Buzzer sustur (reset sonrasi cizirti onler)
    osInitBuzzer();

    // 2) OLED kapat (acilis flicker onleme)
    Wire.begin(I2C_SDA, I2C_SCL);
    osOLEDOff();

    // 3) Guvenlik: Elektrik kesintisinde OS'tan basla
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    // 4) Buton pinleri ayarla
    osInitButtons();

    // 5) NVS'ten ayarlari yukle
    {
        Preferences prefs;
        prefs.begin("os", true);
        soundEnabled = prefs.getBool("sound_en", true);
        showFps = prefs.getBool("show_fps", false);
        prefs.end();
    }
    highScore = osLoadHighScore("hs_wireframe3d", 0);

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

    // 9) OLED baslat (I2C hizini artir)
    oled.setBusClock(400000);
    oled.begin();

    // 10) Sprite tamponu (double buffering)
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // 11) Joystick kalibrasyon
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

    // 12) Rastgele tohum
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    // Oyun ilk durumu
    initStars();
    state = ST_TITLE;
    lastFrameMs = millis();
}

// ============================================================
//  LOOP — Delta-Time + State Machine (60 FPS)
// ============================================================

void loop() {
    // ---- Delta-time hesabi ----
    uint32_t now = millis();
    float dt = (now - lastFrameMs) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    lastFrameMs = now;

    // Screenshot sonrasi TFT recovery
    devToolsTick();

    // ---- Non-blocking wave fanfare ----
    waveFanfare.update();

    // ---- JOY_SW: Pause toggle (kenar tespiti) ----
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {
        if (state == ST_PLAY) {
            state = ST_PAUSE;
            playSound(NOTE_G4, 50);
        } else if (state == ST_PAUSE) {
            state = ST_PLAY;
            playSound(NOTE_D5, 40);
        }
    }
    prevJoySw = currJoySw;

    // ---- BTN_B: OS menuye don (tum state'lerde) ----
    if (!digitalRead(BTN_B)) {
        if (state != ST_PLAY) {
            returnToOS();
        }
    }

    // ---- BTN_A kenar tespiti ----
    bool btnA = !digitalRead(BTN_A);
    static bool prevA = false;
    bool pressA = (btnA && !prevA);
    prevA = btnA;

    // ---- Joystick okumasi (merkez cikart + deadzone) ----
    float jx = (analogRead(JOY_X) - joyCenterX) / 2048.0f;
    float jy = (analogRead(JOY_Y) - joyCenterY) / 2048.0f;
    if (fabsf(jx) < 0.15f) jx = 0;
    if (fabsf(jy) < 0.15f) jy = 0;

    // ==========================================
    //  DURUM MAKINESI
    // ==========================================
    switch (state) {

        // ---- Baslik Ekrani ----
        case ST_TITLE:
            drawTitle(canvas, stars, MAX_STARS, camYaw, camPitch, highScore, now);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            if (pressA) {
                resetGame();
                state = ST_PLAY;
                playSound(NOTE_E5, 50);
            }
            break;

        // ---- Oyun ----
        case ST_PLAY:
        {
            // Kamera kontrolu
            camYaw += jx * CAM_YAW_SPEED * dt;
            camPitch -= jy * CAM_PITCH_SPEED * dt;
            camPitch = constrain(camPitch, -CAM_PITCH_MAX, CAM_PITCH_MAX);

            // Ates
            if (shootCD > 0) { shootCD -= dt; if (shootCD < 0) shootCD = 0; }
            if (btnA && shootCD <= 0) {
                shootCD = SHOOT_CD;
                Vec3 dir = {0, 0, 1};
                dir = rotateX(dir, camPitch);
                dir = rotateY(dir, camYaw);
                for (int i = 0; i < MAX_BULLETS; i++) {
                    if (!bullets[i].active) {
                        bullets[i].pos = {0, 0, 0};
                        bullets[i].vel = v3mul(dir, BULLET_SPEED);
                        bullets[i].life = BULLET_LIFETIME;
                        bullets[i].active = true;
                        break;
                    }
                }
                if (bullets[0].active) playSound(NOTE_A4 + random(-15, 15), 25);
            }

            // Guncellemeler
            updateObjects(dt);
            updateBullets(dt);
            updateSpawning(dt);
            updateBooms(dt);
            updateDust(dt);

            // Radar (OLED) — ~6Hz
            {
                static uint32_t lastRadarMs = 0;
                if (now - lastRadarMs > 160) {
                    drawRadar(oled, objects, MAX_OBJECTS, camYaw, now);
                    lastRadarMs = now;
                }
            }

            // Cizim
            canvas.fillSprite(COL_BG);
            drawStars(canvas, stars, MAX_STARS, camYaw, camPitch);
            drawSpaceDust(canvas, dustArr, MAX_DUST, camYaw, camPitch);
            drawObjects(canvas, objects, MAX_OBJECTS, camYaw, camPitch);
            drawBullets(canvas, bullets, MAX_BULLETS, camYaw, camPitch);
            drawBooms(canvas, booms, MAX_EXPLOSIONS, camYaw, camPitch);
            drawCrosshair(canvas);
            drawHUD(canvas, score, hp, wave, kills);
            if (showFps) drawDevHUD(canvas);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        // ---- Oyun Bitti ----
        case ST_GAMEOVER:
            drawGameOver(canvas, stars, MAX_STARS, camYaw, camPitch,
                         score, kills, wave, highScore);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            if (pressA && now - stateTimer > 600) {
                state = ST_TITLE;
                playSound(NOTE_D5, 50);
            }
            break;

        // ---- Pause ----
        case ST_PAUSE:
            updateDust(dt);  // Pause'ta da toz hareket etmeye devam eder
            canvas.fillSprite(COL_BG);
            drawStars(canvas, stars, MAX_STARS, camYaw, camPitch);
            drawSpaceDust(canvas, dustArr, MAX_DUST, camYaw, camPitch);
            drawObjects(canvas, objects, MAX_OBJECTS, camYaw, camPitch);
            drawBullets(canvas, bullets, MAX_BULLETS, camYaw, camPitch);
            drawBooms(canvas, booms, MAX_EXPLOSIONS, camYaw, camPitch);
            drawCrosshair(canvas);
            drawHUD(canvas, score, hp, wave, kills);

            // Ortak OS pause kutusu (EN, cyan tema)
            osDrawPause(canvas, COL_WIRE_CYAN);

            if (showFps) drawDevHUD(canvas);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (pressA) {
                state = ST_PLAY;
                playSound(NOTE_D5, 40);
            }
            break;
    }
}
