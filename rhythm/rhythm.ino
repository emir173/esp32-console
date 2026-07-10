// ============================================================
//  E-OS — RHYTHM BEATS v1.0
//  160x128 TFT LCD (Landscape) | ST7735 | TFT_eSPI
//  Sprite-based double buffering, 60 FPS + SH1106 OLED HUD
//
//  Guitar Hero tarzi 4 seritli ritim oyunu:
//  ustten dusen notalar hit line'a ulastiginda dogru butona bas.
//
//  Controls:
//    BTN_D/A/C/B -> Serit 1/2/3/4 vurus (soldan saga)
//    JOY_SW      -> Pause toggle
//    JOY Y       -> Menude sarki secimi
//    BTN_A       -> Menude baslat / sonucta menuye don
//    BTN_B       -> OS Launcher'a don
// ============================================================
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include "../hardware_config.h"
#include "../dev_tools.h"
#include "../GameBase.h"
#include "Config.h"
#include "Songs.h"
#include "NoteManager.h"
#include "Renderer.h"
#include "../SharedJoystick.h"

// ============ Display Objects ============
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// ============ Game Instances ============
NoteManager noteMgr;
JoystickProcessor joystick;

// ============ Game State ============
GameState state = ST_MENU;
int  selSong = 0;              // menude secili sarki
long best = 0;                 // NVS rekoru (tum sarkilar geneli)
bool newRecord = false;

// ============ Song Time ============
// Sarki zamani SADECE ST_PLAYING'de ilerler -> pause dogal olarak durur
uint32_t songTime = 0;         // ms, sarki basindan
unsigned long readyStartMs = 0;   // geri sayim baslangici
int lastCountStep = -1;           // geri sayim ses tetigi

// ============ Melody Player (dongulu, non-blocking) ============
struct MelodyPlayer {
    uint8_t  idx = 0;
    uint32_t loopStart = 0;    // aktif dongunun songTime baslangici
    uint32_t muteUntil = 0;    // miss sonrasi susma (songTime ms)
    void reset() { idx = 0; loopStart = 0; muteUntil = 0; }
};
MelodyPlayer melody;
unsigned long sfxUntil = 0;    // vurus sesi oncelikli: bu ana kadar melodi calmaz

// ============ Feedback ============
unsigned long flashUntil = 0;  // PERFECT hit-line flash bitisi (millis)

struct ScreenShake {
    float intensity = 0;
    int offsetX = 0, offsetY = 0;
    void reset() { intensity = 0; offsetX = offsetY = 0; }
    void trigger(float mag) { if (mag > intensity) intensity = mag; }
    void update() {
        if (intensity < 0.2f) { reset(); return; }
        offsetX = random(-(int)intensity, (int)intensity + 1);
        offsetY = random(-(int)intensity, (int)intensity + 1);
        intensity *= 0.85f;   // ustel sonumlenme
    }
};
ScreenShake shake;

// ============ Timing ============
unsigned long lastFrameMs = 0;
unsigned long resultMs = 0;    // sonuc/gameover ekrani girdi kilidi

// ============ FPS Counter ============
uint32_t fpsFrameCount = 0;
uint32_t fpsStartTime = 0;
int currentFPS = 0;
bool showFps = false;

// ============ Buttons ============
int prevLane[LANE_COUNT] = { HIGH, HIGH, HIGH, HIGH };   // 4 serit butonu
int prevBtnA = HIGH;
int lastFrameDir = -1;         // joystick menu kenar tespiti

// ============ Sound ============
bool soundEnabled = true;
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// Vurus sesi: melodiye gore oncelikli (buzzer tek ses calar)
void playHitSfx(uint16_t freq, uint32_t dur) {
    playSound(freq, dur);
    sfxUntil = millis() + dur + 10;
}

// ============================================================
//  Standalone korumasi: diger OTA bolumunde GECERLI bir uygulama
//  (OS launcher) var mi? Tek basina yuklemede bos bolume boot
//  bayragi yazmak bootloop'a yol acar.
// ============================================================
bool osPartitionValid() {
    const esp_partition_t *osPart = esp_ota_get_next_update_partition(NULL);
    if (osPart == NULL) return false;
    esp_app_desc_t desc;
    return esp_ota_get_partition_description(osPart, &desc) == ESP_OK;
}

void saveBestIfRecord() {
    if (noteMgr.score > best) {
        best = noteMgr.score;
        osSaveHighScore("hs_rhythm", (int)best);
    }
}

void returnToOS() {
    saveBestIfRecord();   // yarim kalan run'in rekoru kaybolmasin
    oled.clearDisplay();
    if (osPartitionValid()) {
        osReturnToOS(tft, soundEnabled);
        return;   // (ulasilmaz — osReturnToOS restart eder)
    }
    // Standalone mod: donulecek OS yok — bilgi goster, menude kal
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(14, 60);
    tft.print("Standalone: no OS");
    delay(STANDALONE_MSG_MS);
    tft.fillScreen(TFT_BLACK);
    state = ST_MENU;
}

// ============================================================
//  OLED HUD — sadece degerler degisince gonderilir (I2C yavas)
// ============================================================
void updateOLED(bool force) {
    static long lastScore = -1;
    static int lastCombo = -1, lastHealth = -1;
    if (!force && noteMgr.score == lastScore &&
        noteMgr.combo == lastCombo && noteMgr.health == lastHealth) return;
    lastScore = noteMgr.score;
    lastCombo = noteMgr.combo;
    lastHealth = noteMgr.health;

    char buf[28];
    oled.clearBuffer();
    oled.setFont(u8g2_font_7x13B_tr);
    oled.drawStr(19, 12, "RHYTHM BEATS");
    oled.setFont(u8g2_font_6x10_tr);
    snprintf(buf, sizeof(buf), "Score: %ld", noteMgr.score);
    oled.drawStr(0, 26, buf);
    snprintf(buf, sizeof(buf), "Combo: %d (x%d)", noteMgr.combo, comboMultiplier(noteMgr.combo));
    oled.drawStr(0, 38, buf);
    snprintf(buf, sizeof(buf), "Best:  %ld", best);
    oled.drawStr(0, 50, buf);
    // Saglik bari + sarki no
    oled.drawFrame(0, 55, 90, 8);
    int w = noteMgr.health * 86 / HEALTH_MAX;
    if (w > 0) oled.drawBox(2, 57, w, 4);
    snprintf(buf, sizeof(buf), "%d/%d", selSong + 1, SONG_COUNT);
    oled.drawStr(102, 63, buf);
    oled.sendBuffer();
}

// ============================================================
//  Melodi: dongulu fraz, songTime'a senkron. Vurus sesi calarken
//  (sfxUntil) atlanir; miss sonrasi MELODY_MUTE_MS susar.
// ============================================================
void updateMelody(unsigned long now) {
    const SongInfo *s = noteMgr.song;
    uint32_t due = melody.loopStart + s->melody[melody.idx].timeMs;
    if (songTime < due) return;

    // Notanin sirasi geldi — cakisma yoksa cal
    if (now >= sfxUntil && songTime >= melody.muteUntil) {
        playSound(s->melody[melody.idx].freq, s->melody[melody.idx].dur);
    }
    melody.idx++;
    if (melody.idx >= s->melCount) {
        melody.idx = 0;
        melody.loopStart += s->melLoopMs;
    }
}

// ============================================================
//  Sarki baslat: geri sayima gec
// ============================================================
void startSong() {
    noteMgr.begin(&SONGS[selSong]);
    melody.reset();
    songTime = 0;
    newRecord = false;
    shake.reset();
    flashUntil = 0;
    readyStartMs = millis();
    lastCountStep = -1;
    state = ST_READY;
    updateOLED(true);
}

// ============================================================
//  Vurus derecesine gore ses + gorsel geri bildirim
// ============================================================
void applyGradeFeedback(HitGrade g, unsigned long now) {
    switch (g) {
        case GRADE_PERFECT:
            playHitSfx(NOTE_G5, 20);     // parlak odul
            flashUntil = now + FLASH_MS;
            break;
        case GRADE_GOOD:
            playHitSfx(NOTE_E5, 25);     // olumlu
            break;
        case GRADE_OK:
            playHitSfx(NOTE_C5, 25);     // notr
            break;
        case GRADE_MISS:
            playHitSfx(NOTE_E3, 60);     // tok ceza
            shake.trigger(2.0f);
            melody.muteUntil = songTime + MELODY_MUTE_MS;   // melodi bozulur
            break;
        default: break;
    }
}

// ============================================================
//  SETUP — E-OS zorunlu boilerplate (adim sirasi degistirilmez)
// ============================================================
void setup() {
    // 1) Buzzer sustur (reset sonrasi cizirti onler)
    osInitBuzzer();

    // 2) I2C baslat + OLED kapat (acilis flicker onleme)
    Wire.begin(I2C_SDA, I2C_SCL);
    osOLEDOff();

    // 3) Guvenlik: elektrik kesintisinde OS'tan basla.
    //    SADECE diger bolumde gecerli OS imaji varsa bayrak degistirilir.
    if (osPartitionValid()) {
        const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
        esp_ota_set_boot_partition(os_part);
    }

    // 4) Buton pinleri
    osInitButtons();

    // 5) NVS'ten ayarlar + rekor
    {
        Preferences prefs;
        prefs.begin("os", true);
        soundEnabled = prefs.getBool("sound_en", true);
        showFps = prefs.getBool("show_fps", false);
        prefs.end();
    }
    best = osLoadHighScore("hs_rhythm", 0);

    // 6) SPI + Dev Tools (USB screenshot)
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
    initDevTools(tft);

    // 7) TFT baslat (E-OS standart yonlendirme)
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    tft.startWrite();
    tft.writecommand(0x36);
    tft.writedata(0xA0);
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    // 8) OLED baslat (HUD olarak kullanilir)
    oled.setBusClock(400000);
    oled.begin();

    // 9) Sprite tamponu (double buffering)
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // 10) Joystick kalibrasyonu
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

    int joyCenterX, joyCenterY;
    osCalibrateJoystick(joyCenterX, joyCenterY);
    joystick.init(joyCenterX, joyCenterY);

    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    state = ST_MENU;
    lastFrameMs = millis();
    fpsStartTime = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    // ---- JOY_SW: Pause toggle (kenar tespiti) ----
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {
        if (state == ST_PLAYING) {
            state = ST_PAUSE;
            osBuzzerOff();            // calan melodi/sfx pause'da kalmasin
            playSound(NOTE_G4, 50);
        } else if (state == ST_PAUSE) {
            state = ST_PLAYING;
            playSound(NOTE_D5, 40);
        }
    }
    prevJoySw = currJoySw;

    // ---- Kare hizi siniri (60 FPS) ----
    unsigned long now = millis();
    if (now - lastFrameMs < FRAME_MS) return;
    unsigned long elapsed = now - lastFrameMs;
    if (elapsed > 50) elapsed = 50;   // lag spike korumasi
    lastFrameMs = now;

    // ---- FPS sayaci ----
    fpsFrameCount++;
    if (now - fpsStartTime >= 1000) {
        currentFPS = fpsFrameCount;
        fpsFrameCount = 0;
        fpsStartTime = now;
    }

    // ---- BTN_A kenar tespiti (menu/sonuc ekranlari) ----
    int btnA = digitalRead(BTN_A);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    prevBtnA = btnA;

    // ---- 4 serit butonu: kenar tespiti + basili maske ----
    bool lanePress[LANE_COUNT];
    uint8_t pressedMask = 0;
    for (int i = 0; i < LANE_COUNT; i++) {
        int v = digitalRead(LANE_PINS[i]);
        lanePress[i] = (v == LOW && prevLane[i] == HIGH);
        if (v == LOW) pressedMask |= (1 << i);
        prevLane[i] = v;
    }

    switch (state) {

        // ======================================
        //  MENU — sarki secimi
        // ======================================
        case ST_MENU: {
            joystick.update();
            int dir = joystick.currentDir;
            bool dirEdge = (dir != -1 && dir != lastFrameDir);
            lastFrameDir = dir;
            if (dirEdge && dir == DIR_UP) {
                selSong = (selSong + SONG_COUNT - 1) % SONG_COUNT;
                playSound(NOTE_F4, 25);
            } else if (dirEdge && dir == DIR_DOWN) {
                selSong = (selSong + 1) % SONG_COUNT;
                playSound(NOTE_F4, 25);
            }

            if (pressA) {
                playSound(NOTE_E5, 50);
                startSong();
                break;
            }
            if (!digitalRead(BTN_B)) {
                returnToOS();
                break;
            }

            drawMenu(canvas, selSong, best, now);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  READY — "3-2-1-GO!" geri sayimi
        // ======================================
        case ST_READY: {
            int step = (int)((now - readyStartMs) / COUNTDOWN_STEP_MS);
            if (step != lastCountStep && step <= 3) {
                playSound(step < 3 ? NOTE_C4 : NOTE_E5, step < 3 ? 60 : 50);
                lastCountStep = step;
            }
            if (step > 3) {
                state = ST_PLAYING;
                songTime = 0;
                break;
            }

            // Seritler arka planda gorunur, notalar henuz dusmez
            drawPlayScene(canvas, noteMgr, 0, pressedMask, false, now);
            drawCountdown(canvas, (uint8_t)step);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        // ======================================
        //  PLAYING — ana oyun dongusu
        // ======================================
        case ST_PLAYING: {
            songTime += elapsed;   // sarki zamani sadece burada ilerler

            // Nota spawn + otomatik miss
            noteMgr.update(songTime, now);
            if (noteMgr.autoMissThisFrame > 0) {
                applyGradeFeedback(GRADE_MISS, now);
            }

            // Serit vuruslari
            for (int i = 0; i < LANE_COUNT; i++) {
                if (!lanePress[i]) continue;
                HitGrade g = noteMgr.press((uint8_t)i, songTime, now);
                if (g != GRADE_NONE) applyGradeFeedback(g, now);
            }

            updateMelody(now);
            shake.update();

            // Saglik bitti -> game over (sarki yarida)
            if (noteMgr.dead()) {
                osBuzzerOff();
                saveBestIfRecord();
                playSound(NOTE_E3, 120);
                shake.trigger(4.0f);
                state = ST_GAMEOVER;
                resultMs = now;
            } else if (noteMgr.songFinished(songTime)) {
                // Sarki tamamlandi -> sonuclar
                osBuzzerOff();
                newRecord = (noteMgr.score > best && noteMgr.score > 0);
                saveBestIfRecord();
                playSound(NOTE_C5, 50); delay(60);   // zafer triadi
                playSound(NOTE_E5, 50); delay(60);
                playSound(NOTE_G5, 40);
                state = ST_RESULTS;
                resultMs = now;
            }

            drawPlayScene(canvas, noteMgr, songTime, pressedMask,
                          now < flashUntil, now);
            if (showFps) {
                canvas.setTextSize(1);
                canvas.setTextColor(COL_GRAY_TXT);
                canvas.setCursor(146, 2);
                canvas.print(currentFPS);
            }
            checkScreenshot(canvas);
            canvas.pushSprite(shake.offsetX, shake.offsetY);

            updateOLED(false);
            break;
        }

        // ======================================
        //  RESULTS — sarki tamamlandi
        // ======================================
        case ST_RESULTS: {
            drawPlayScene(canvas, noteMgr, songTime, 0, false, now);

            char sbuf[12], pbuf[16], cbuf[12], bbuf[12];
            snprintf(sbuf, sizeof(sbuf), "%ld", noteMgr.score);
            snprintf(pbuf, sizeof(pbuf), "%d/%d", noteMgr.cntPerfect, noteMgr.song->noteCount);
            snprintf(cbuf, sizeof(cbuf), "%d", noteMgr.maxCombo);
            snprintf(bbuf, sizeof(bbuf), "%ld", best);
            OsStat rows[4] = {
                { "Score",   sbuf, TFT_WHITE, TFT_YELLOW  },
                { "Perfect", pbuf, TFT_WHITE, COL_PERFECT },
                { "Combo",   cbuf, TFT_WHITE, TFT_CYAN    },
                { "Best",    bbuf, TFT_WHITE, TFT_GREEN   },
            };
            osDrawGameOver(canvas, true, rows, 4, newRecord ? "NEW BEST!" : nullptr);
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (now - resultMs > RESULT_LOCK_MS) {
                if (pressA) {
                    playSound(NOTE_D5, 50);
                    state = ST_MENU;
                } else if (!digitalRead(BTN_B)) {
                    returnToOS();
                }
            }
            break;
        }

        // ======================================
        //  GAMEOVER — saglik bitti
        // ======================================
        case ST_GAMEOVER: {
            shake.update();
            drawPlayScene(canvas, noteMgr, songTime, 0, false, now);

            char sbuf[12], cbuf[12], bbuf[12];
            snprintf(sbuf, sizeof(sbuf), "%ld", noteMgr.score);
            snprintf(cbuf, sizeof(cbuf), "%d", noteMgr.maxCombo);
            snprintf(bbuf, sizeof(bbuf), "%ld", best);
            OsStat rows[3] = {
                { "Score", sbuf, TFT_WHITE, TFT_YELLOW },
                { "Combo", cbuf, TFT_WHITE, TFT_CYAN   },
                { "Best",  bbuf, TFT_WHITE, TFT_GREEN  },
            };
            osDrawGameOver(canvas, false, rows, 3);
            checkScreenshot(canvas);
            canvas.pushSprite(shake.offsetX, shake.offsetY);

            if (now - resultMs > RESULT_LOCK_MS) {
                if (pressA) {
                    playSound(NOTE_D5, 50);
                    state = ST_MENU;
                } else if (!digitalRead(BTN_B)) {
                    returnToOS();
                }
            }
            break;
        }

        // ======================================
        //  PAUSE
        // ======================================
        case ST_PAUSE: {
            drawPlayScene(canvas, noteMgr, songTime, 0, false, now);
            osDrawPause(canvas, COL_LANES[1]);   // cyan tema
            checkScreenshot(canvas);
            canvas.pushSprite(0, 0);

            if (pressA) {
                playSound(NOTE_D5, 40);
                state = ST_PLAYING;
                lastFrameMs = millis();
            }
            if (!digitalRead(BTN_B)) {
                playSound(NOTE_G4, 50);
                returnToOS();
            }
            break;
        }
    }
}
