// ============================================================
//  towerdefense/towerdefense.ino — E-OS TOWER DEFENSE
//  160x128 TFT LCD | ST7735 | TFT_eSPI | ESP32-S3
//
//  Sorumluluk: Setup boilerplate, girdi işleme, durum makinesi
//  (MENU / PLAYING / PAUSE / GAMEOVER / WAVE_CLEAR / TOWER_MENU)
//  ve millis() tabanlı delta-time + 60 Hz sabit tick oyun döngüsü.
//
//  Kontroller:
//    JOY     -> İmleç hareketi (grid tile'ları arasında)
//    JOY_SW  -> Pause aç/kapa (oyun sırasında TEK duraklatma yolu)
//    BTN_A   -> Boş çimende kule kur menüsü / kule üzerinde bilgi-yönetim
//               paneli (yükselt/sat) / menü onay
//    BTN_B   -> OS'a dönüş (menü/pause) — oyun sırasında YILDIRIM yeteneği (v4.1)
//    BTN_C   -> Menüde bölüm seçimi ekranını açar (oyun içinde kullanılmıyor)
//    BTN_D   -> Hazırlıkta: dalgayı erken başlat / Dalga sırasında: sonraki
//               dalgayı erken çağır (canlı düşman ölmeden, bölüm içinde)
// ============================================================

// TFT ekran için grafik kütüphanesi (ST7735 sürücüsü)
#include <TFT_eSPI.h>
// SPI haberleşmesi (TFT veri transferi)
#include <SPI.h>
// I2C haberleşmesi (OLED kapatma komutu için)
#include <Wire.h>
// NVS erişimi (rekor + ses ayarı)
#include <Preferences.h>
// OTA partition işlemleri (OS'a dönüş)
#include <esp_ota_ops.h>
// Donanım pinleri
#include "../hardware_config.h"
// Geliştirici araçları (screenshot + FPS dump)
#include "../dev_tools.h"
// Ortak oyun API'si (osPlaySound, osReturnToOS, NOTE_* ...)
#include "../GameBase.h"

// Oyun modülleri (bağımlılık zinciri: Config → veri → Combat → Renderer)
#include "Config.h"
#include "Map.h"
#include "Tower.h"
#include "Enemies.h"
#include "Wave.h"
#include "Combat.h"
#include "Renderer.h"

// ------------------------------------------------------------
//  Donanım nesneleri ve genel durum
// ------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();               // Ana TFT sürücüsü
TFT_eSprite canvas = TFT_eSprite(&tft);  // Double-buffer sprite

bool soundEnabled = true;    // NVS'ten yüklenir
bool showFps = false;        // NVS'ten yüklenir
int  highScore = 0;          // Ulaşılan en yüksek dalga ("hs_towerdefense")
bool newRecord = false;      // Bu oyunda rekor kırıldı mı
bool startedFromL1 = true;   // Rekor yalnız bölüm 1'den başlayınca sayılır
int  selLevel = 1;           // Bölüm seçimi ekranındaki seçili bölüm (1..9)

GameState state = MENU;

int joyCenterX = 2048, joyCenterY = 2048;   // Joystick kalibrasyon merkezi

// İmleç ve kule menüsü
int curX = 8, curY = 8;      // Grid imleci (tile)
int tmCursor = 0;            // Kule seçim paneli imleci
int tiCursor = 0;            // Kule bilgi/yönetim paneli imleci

// Dalga temiz ekranı verileri
int clearedWave = 0;         // Az önce temizlenen dalga no
int lastBonus = 0;           // Verilen bonus (ekranda gösterilir)

// Delta-time ve zamanlayıcılar
uint32_t lastFrameMs = 0;    // Son frame zamanı (dt hesabı)
uint32_t stateTimer  = 0;    // State geçiş zamanlayıcısı
float    gameTickAcc = 0;    // Düşman/kule için 60 Hz tick biriktirici

// Girdi durumu
uint32_t lastMoveMs = 0;     // Son imleç hareketi (tekrar koruması)
int      heldDir    = -1;    // Basılı tutulan yön (typewriter pattern)
uint32_t lastNavMs  = 0;     // Panel imleç tekrar zamanı
int      heldNav    = 0;     // Basılı tutulan menü yönü
int prevBtnA = HIGH, prevBtnB = HIGH, prevBtnC = HIGH,
    prevBtnD = HIGH, prevJoySW = HIGH;

// FPS sayacı
uint32_t fpsWindowStart = 0;
int fpsFrames = 0, currentFps = 0;

// ------------------------------------------------------------
//  GameBase sarmalayıcıları
// ------------------------------------------------------------
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// ------------------------------------------------------------
//  Standalone koruması: diğer OTA bölümünde GEÇERLİ bir uygulama
//  (OS launcher) var mı? Arduino IDE ile tek başına yüklemede
//  oyun ota_0'a yazılır ve ota_1 BOŞ kalır; boş bölüme boot
//  bayrağı yazmak bootloader panic/bootloop'a yol açar.
// ------------------------------------------------------------
bool osPartitionValid() {
    const esp_partition_t *osPart = esp_ota_get_next_update_partition(NULL);
    if (osPart == NULL) return false;
    esp_app_desc_t desc;   // Bölümde geçerli app imajı yoksa ESP_OK dönmez
    return esp_ota_get_partition_description(osPart, &desc) == ESP_OK;
}

void returnToOS() {
    if (osPartitionValid()) {
        osReturnToOS(tft, soundEnabled);
        return;   // (buraya ulaşılmaz — osReturnToOS restart eder)
    }
    // Standalone mod: dönülecek OS yok — kısa bilgi göster, menüde kal
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(14, 60);
    tft.print("Standalone: no OS");
    delay(STANDALONE_MSG_MS);
    tft.fillScreen(TFT_BLACK);
    state = MENU;
}

// ------------------------------------------------------------
//  Zafer fanfarı — Do-Mi-Sol arpej, NON-BLOCKING
//  (Tetris'teki millis() tabanlı nota kuyruğu pattern'i)
// ------------------------------------------------------------
struct Fanfare {
    bool active = false;
    uint8_t idx = 0;
    unsigned long nextMs = 0;

    void start() { active = true; idx = 0; nextMs = millis(); }
    void update() {
        if (!active) return;
        if (millis() < nextMs) return;
        switch (idx) {
            case 0: playSound(NOTE_C5, FANFARE_NOTE_MS); nextMs = millis() + FANFARE_GAP_MS; break;
            case 1: playSound(NOTE_E5, FANFARE_NOTE_MS); nextMs = millis() + FANFARE_GAP_MS; break;
            case 2: playSound(NOTE_G5, FANFARE_TOP_MS);  active = false; break;
        }
        idx++;
    }
};
Fanfare fanfare;

// ------------------------------------------------------------
//  Girdi yardımcıları
// ------------------------------------------------------------
// Düşen kenar algılama (INPUT_PULLUP: basılı = LOW)
bool btnPressed(uint8_t pin, int &prev) {
    int cur = digitalRead(pin);
    bool fired = (prev == HIGH && cur == LOW);
    prev = cur;
    return fired;
}

// Joystick'ten baskın 4 yön (yok ise -1)
int readJoyDir() {
    int jx = analogRead(JOY_X) - joyCenterX;
    int jy = analogRead(JOY_Y) - joyCenterY;
    if (abs(jx) <= JOY_DEADZONE && abs(jy) <= JOY_DEADZONE) return -1;
    if (abs(jx) > abs(jy)) return (jx > 0) ? DIR_RIGHT : DIR_LEFT;
    return (jy > 0) ? DIR_DOWN : DIR_UP;
}

// Panel için dikey yön (-1 yukarı, +1 aşağı, 0 yok)
int readJoyVert() {
    int jy = analogRead(JOY_Y) - joyCenterY;
    if (jy < -JOY_DEADZONE) return -1;
    if (jy > JOY_DEADZONE) return 1;
    return 0;
}

// ------------------------------------------------------------
//  Oyun akışı
// ------------------------------------------------------------
// Seçili bölümden yeni oyun (level 1..9). Ortadan başlarken dalga numarası
// sürekli kalır ve başlangıç parası bölüme göre ölçeklenir.
void startNewGameAt(int level) {
    if (level < 1) level = 1;
    if (level > LEVEL_COUNT) level = LEVEL_COUNT;
    initMap(level);
    clearTowers();
    clearEnemies();
    clearEffects();
    resetWaves();                       // waveNum=0, currentLevel=1, ilk dalga geri sayımı
    currentLevel = level;               // Seçilen bölüme geç
    waveNum      = (level - 1) * WAVES_PER_LEVEL;   // Dalga sayacı sürekli
    prepLeft     = FIRST_WAVE_PREP_S;
    money = START_MONEY + (level - 1) * START_MONEY_PER_LEVEL;
    baseHp = BASE_HP_MAX;
    towersBuilt = 0;
    killsTotal = 0;
    baseDestroyed = false;
    lightningCd = 0.0f;
    newRecord = false;
    startedFromL1 = (level == 1);       // Rekor dürüstlüğü
    curX = 8; curY = 8;
    heldDir = -1;
    gameTickAcc = 0;
    state = PLAYING;
    lastFrameMs = millis();
}

// Sıfırdan yeni oyun (bölüm 1)
void startNewGame() { startNewGameAt(1); }

// Bölüm geçişi: yeni harita, kuleler sıfırlanır, para + bonus korunur
void startNextLevel() {
    currentLevel++;
    initMap(currentLevel);
    clearTowers();
    clearEnemies();
    clearEffects();        // Sadece görsel efektler (istatistikler korunur)
    curX = 8; curY = 8;
    prepLeft = WAVE_PREP_S;
    state = PLAYING;
    lastFrameMs = millis();
}

// Yeni dalgayı başlat + rekor kontrolü + ses
void startWaveGo() {
    startWave();                       // waveNum burada artar
    // Rekor yalnız bölüm 1'den başlayan oyunlarda sayılır (ortadan başlama şişirmesin)
    if (startedFromL1 && waveNum > highScore) {
        highScore = waveNum;
        osSaveHighScore("hs_towerdefense", highScore);
        newRecord = true;
    }
    playSound(NOTE_G4, SND_WAVE_MS);   // Dalga başlangıcı
}

// Kale düştü → game over (iki notalı ciddi ses)
void goGameOver() {
    playSound(NOTE_E3, SND_DIE1_MS);   // 165 Hz: en ciddi olay
    delay(SND_DIE_GAP_MS);
    playSound(NOTE_G3, SND_DIE2_MS);
    state = GAMEOVER;
    stateTimer = millis();
}

// BTN_A: imleç boş çimende ise kule kurma menüsü, kule üzerinde ise
// bilgi/yönetim paneli (yükselt/sat) açar.
void tryOpenTowerMenu() {
    if (canBuildTower(curX, curY)) {
        playSound(NOTE_G4, SND_NAV_MS);   // Panel açma onayı
        tmCursor = 0;
        state = TOWER_MENU;
        return;
    }
    if (towerIndexAt(curX, curY) >= 0) {
        playSound(NOTE_G4, SND_NAV_MS);   // Kule bilgi paneli
        tiCursor = 0;
        state = TOWER_INFO;
        return;
    }
    playSound(NOTE_F4, SND_ERROR_MS);     // Geçersiz tile (kaya/yol)
}

// BTN_B (dalga sırasında): YILDIRIM yeteneği — tüm düşmanlara hasar (v4.1)
void tryLightning() {
    if (lightningCd <= 0.0f && money >= LIGHTNING_COST && aliveEnemyCount() > 0) {
        money -= LIGHTNING_COST;
        lightningCd = LIGHTNING_CD_S;
        castLightning();
    } else {
        playSound(NOTE_F4, SND_ERROR_MS);   // Beklemede, para yok veya düşman yok
    }
}

// BTN_D (dalga sırasında): sonraki dalgayı erken çağır — canlı düşmanlar
// ölmeden yeni dalga üstlerine biner (agresif oyuna rush bonusu). (v4.1)
void tryCallNextWave() {
    if (canCallNextWave()) {
        money += RUSH_BONUS;
        startWaveGo();   // waveNum artar, yeni dalga mevcut düşmanların üstüne spawn olur
    } else {
        playSound(NOTE_F4, SND_ERROR_MS);   // Dalga hâlâ spawn oluyor / bölümün son dalgası
    }
}

// ------------------------------------------------------------
//  Oyun sahnesi çizimi (PLAYING/PAUSE/TOWER_MENU/... ortak)
// ------------------------------------------------------------
void drawScene() {
    canvas.fillSprite(COL_BG);
    drawMapAll(canvas);
    drawTowers(canvas);
    drawEnemies(canvas);
    drawShots(canvas);
    drawRays(canvas);
    drawRings(canvas);
    particles.draw(canvas);
    drawPopups(canvas);
    drawHUD(canvas, currentFps, showFps);
}

// ------------------------------------------------------------
//  SETUP — E-OS zorunlu boilerplate (adım sırası değiştirilmez)
// ------------------------------------------------------------
void setup() {
    // 1. Buzzer sustur (reset sonrası cızırtı önler)
    osInitBuzzer();

    // 2. OLED kapat (açılış flicker önleme)
    Wire.begin(I2C_SDA, I2C_SCL);
    osOLEDOff();

    // 3. Güvenlik: elektrik kesintisinde OS'tan başla.
    //    SADECE diğer bölümde geçerli bir OS imajı varsa boot bayrağı
    //    değiştirilir (standalone yüklemede bootloop engellenir).
    if (osPartitionValid()) {
        const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
        esp_ota_set_boot_partition(os_part);
    }

    // 4. Buton pinleri
    osInitButtons();

    // 5. NVS'ten ayarları yükle
    {
        Preferences prefs;
        prefs.begin("os", true);   // true = read-only
        soundEnabled = prefs.getBool("sound_en", true);
        showFps = prefs.getBool("show_fps", false);
        prefs.end();
    }
    // Yüksek skor ("hs_towerdefense" anahtarı, "os" namespace)
    highScore = osLoadHighScore("hs_towerdefense", 0);

    // 6. SPI başlat
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

    // 7. Dev Tools (USB screenshot/video + FPS)
    initDevTools(tft);

    // 8. TFT ekran başlat
    tft.init();
    tft.setRotation(1);      // Landscape: 160x128
    tft.setSwapBytes(true);
    tft.startWrite();
    tft.writecommand(0x36);  // MADCTL (panel yönü)
    tft.writedata(0xA0);
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    // 9. OLED kullanılmıyor (bu oyun sadece TFT)

    // 10. Sprite tamponu (double buffering)
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

    // 11. Joystick kalibrasyonu (merkez değer ölçümü)
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

    // 12. Rastgele tohum
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    initMap(1);              // Menü arkası için bölüm 1 haritası hazır olsun
    state = MENU;
    lastFrameMs = millis();
    fpsWindowStart = millis();
}

// ------------------------------------------------------------
//  ANA DÖNGÜ — millis() tabanlı delta-time durum makinesi
// ------------------------------------------------------------
void loop() {
    // ---- Kare hızı sınırı (60 FPS) ----
    uint32_t now = millis();
    if (now - lastFrameMs < FRAME_MS) return;
    float dt = (now - lastFrameMs) / 1000.0f;
    if (dt > DT_CAP) dt = DT_CAP;       // Lag spike koruması
    lastFrameMs = now;

    // ---- FPS ölçümü ----
    fpsFrames++;
    if (now - fpsWindowStart >= FPS_WINDOW_MS) {
        currentFps = fpsFrames;
        fpsFrames = 0;
        fpsWindowStart = now;
    }

    // ---- Zafer fanfarı (non-blocking nota kuyruğu) ----
    fanfare.update();

    // ---- Buton kenarları (frame'de bir kez okunur) ----
    bool aP  = btnPressed(BTN_A, prevBtnA);
    bool bP  = btnPressed(BTN_B, prevBtnB);
    bool cP  = btnPressed(BTN_C, prevBtnC);
    bool dP  = btnPressed(BTN_D, prevBtnD);
    bool swP = btnPressed(JOY_SW, prevJoySW);

    switch (state) {

        // ---------------- MENÜ ----------------
        case MENU: {
            if (aP) {
                playSound(NOTE_E5, SND_START_MS);   // Standart menü başlat
                startNewGame();
                break;
            }
            if (cP) {                               // Bölüm seçimi ekranı
                playSound(NOTE_G4, SND_NAV_MS);
                selLevel = 1;
                heldDir = -1;
                state = LEVEL_SELECT;
                break;
            }
            if (bP) returnToOS();
            drawMenu(canvas, highScore);
            break;
        }

        // ---------------- BÖLÜM SEÇİMİ ----------------
        case LEVEL_SELECT: {
            // Joystick sol/sağ ile 1..9 arası seçim (ilk basış anında, tekrar korumalı)
            int d = readJoyDir();
            if (d != DIR_LEFT && d != DIR_RIGHT) {
                heldDir = -1;
            } else if (d != heldDir || now - lastMoveMs >= MENU_REPEAT_MS) {
                selLevel += (d == DIR_RIGHT) ? 1 : -1;
                if (selLevel < 1) selLevel = LEVEL_COUNT;
                if (selLevel > LEVEL_COUNT) selLevel = 1;
                playSound(NOTE_F4, SND_NAV_MS);
                heldDir = d;
                lastMoveMs = now;
            }

            if (aP) {                               // Seçilen bölümden başla
                playSound(NOTE_E5, SND_START_MS);
                startNewGameAt(selLevel);
                break;
            }
            if (bP) {                               // Menüye geri dön
                playSound(NOTE_F4, SND_NAV_MS);
                state = MENU;
                break;
            }
            drawLevelSelect(canvas, selLevel);
            break;
        }

        // ---------------- OYUN ----------------
        case PLAYING: {
            // Pause geçişi: SADECE joystick tuşu (JOY_SW). B oyun sırasında
            // hiçbir şey yapmaz (yanlışlıkla duraklatma/OS'a düşme koruması).
            if (swP) {
                playSound(NOTE_G4, SND_PAUSE_MS);
                state = PAUSE;
                break;
            }

            // İmleç hareketi: ilk basışta anında, tutuşta tekrar korumalı
            int d = readJoyDir();
            if (d < 0) {
                heldDir = -1;
            } else if (d != heldDir || now - lastMoveMs >= MOVE_REPEAT_MS) {
                int nx = curX + DIR_DX[d];
                int ny = curY + DIR_DY[d];
                if (inBounds(nx, ny)) { curX = nx; curY = ny; }
                heldDir = d;
                lastMoveMs = now;
            }

            if (aP) tryOpenTowerMenu();     // Boş çimen: kur menüsü / kule: bilgi paneli
            if (state != PLAYING) { drawScene(); drawCursor(canvas, curX, curY); break; }

            // Dalga öncesi geri sayım + erken başlatma bonusu (BTN_D).
            // Dalga sırasında: BTN_B = yıldırım, BTN_D = sonraki dalgayı erken çağır.
            if (!waveRunning && prepLeft > 0.0f) {
                prepLeft -= dt;
                if (dP) {
                    money += (int)(prepLeft * EARLY_BONUS_PER_S);   // Kalan sn × bonus
                    startWaveGo();
                } else if (prepLeft <= 0.0f) {
                    startWaveGo();
                }
            } else if (waveRunning) {
                if (bP) tryLightning();       // BTN_B: yıldırım yeteneği
                if (dP) tryCallNextWave();     // BTN_D: sonraki dalgayı erken çağır
            }

            // Yıldırım bekleme süresi
            if (lightningCd > 0.0f) lightningCd -= dt;

            // Oyun mantığı: 60 Hz sabit tick (dt biriktirme, FPS bağımsız)
            gameTickAcc += dt;
            while (gameTickAcc >= FRAME_SEC) {
                gameTickAcc -= FRAME_SEC;
                tickWaveSpawns();
                tickEnemies();
                tickTowers();
                if (baseDestroyed) break;
            }

            // Efekt güncellemeleri
            particles.update(dt);
            updatePopups(dt);
            updateRays(dt);
            updateShots(dt);
            updateRings(dt);
            shake.update();

            if (baseDestroyed) {
                drawScene();               // Son kare (sarsıntılı) görünsün
                goGameOver();
                break;
            }

            // Dalga temizlendi mi?
            if (waveCleared()) {
                waveRunning = false;

                // Son dalga da savuşturuldu → ZAFER
                if (waveNum >= TOTAL_WAVES) {
                    fanfare.start();       // Non-blocking Do-Mi-Sol
                    particles.emit(baseTileX * TILE_PX + TILE_PX / 2,
                                   baseTileY * TILE_PX + TILE_PX / 2,
                                   COL_MONEY, PART_N_KILL);   // Kalede kutlama
                    state = VICTORY;
                    stateTimer = now;
                    drawScene();
                    drawVictory(canvas);
                    break;
                }

                // Bölümün son dalgası → yeni harita duyurusu
                if (waveNum % WAVES_PER_LEVEL == 0) {
                    lastBonus = waveBonus() + LEVEL_BONUS;
                    money += lastBonus;
                    fanfare.start();
                    state = LEVEL_CLEAR;
                    stateTimer = now;
                    drawScene();
                    drawLevelClear(canvas, currentLevel, lastBonus);
                    break;
                }

                clearedWave = waveNum;
                lastBonus = waveBonus();
                money += lastBonus;
                fanfare.start();           // Non-blocking Do-Mi-Sol
                state = WAVE_CLEAR;
                stateTimer = now;
                drawScene();
                drawWaveClear(canvas, clearedWave, lastBonus);
                break;
            }

            drawScene();
            drawPrepBanner(canvas);
            drawWaveHints(canvas);
            drawCursor(canvas, curX, curY);
            break;
        }

        // ---------------- KULE SEÇİM MENÜSÜ ----------------
        case TOWER_MENU: {
            // Oyun donuk: sadece panel gezinme ve seçim
            int nav = readJoyVert();
            if (nav == 0) {
                heldNav = 0;
            } else if (nav != heldNav || now - lastNavMs >= MENU_REPEAT_MS) {
                tmCursor = (tmCursor + nav + TM_OPTIONS) % TM_OPTIONS;
                playSound(NOTE_F4, SND_NAV_MS);
                heldNav = nav;
                lastNavMs = now;
            }

            if (aP) {
                if (tmCursor >= TOWER_TYPE_COUNT) {
                    // İptal seçeneği
                    playSound(NOTE_F4, SND_NAV_MS);
                    state = PLAYING;
                    lastFrameMs = millis();   // dt sıçramasını önle
                } else if (placeTower((TowerType)tmCursor, curX, curY)) {
                    playSound(NOTE_C5, SND_BUILD_MS);   // Yerleştirme (ödül hissi)
                    int ti = towerIndexAt(curX, curY);
                    particles.emit(towerCenterX(towers[ti]),
                                   towerCenterY(towers[ti]),
                                   COL_TOWER_BASE, PART_N_BUILD);
                    state = PLAYING;
                    lastFrameMs = millis();
                } else {
                    playSound(NOTE_F4, SND_ERROR_MS);   // Para yetersiz
                }
            }

            // B: menüyü kapat
            if (bP) {
                playSound(NOTE_F4, SND_NAV_MS);
                state = PLAYING;
                lastFrameMs = millis();
            }

            drawScene();
            drawCursor(canvas, curX, curY);
            drawTowerMenu(canvas, tmCursor);
            break;
        }

        // ---------------- KULE BİLGİ / YÖNETİM ----------------
        case TOWER_INFO: {
            int ti = towerIndexAt(curX, curY);
            if (ti < 0) {                       // Kule satıldıysa/yoksa çık
                state = PLAYING;
                lastFrameMs = millis();
                break;
            }
            Tower &t = towers[ti];

            // Panel gezinme (dikey joystick)
            int nav = readJoyVert();
            if (nav == 0) {
                heldNav = 0;
            } else if (nav != heldNav || now - lastNavMs >= MENU_REPEAT_MS) {
                tiCursor = (tiCursor + nav + TI_OPTIONS) % TI_OPTIONS;
                playSound(NOTE_F4, SND_NAV_MS);
                heldNav = nav;
                lastNavMs = now;
            }

            if (aP) {
                if (tiCursor == 0) {            // Yukselt
                    if (applyUpgrade(t)) {
                        playSound(NOTE_E5, SND_UPGRADE_MS);
                        particles.emit(towerCenterX(t), towerCenterY(t),
                                       COL_MONEY, PART_N_UPGRADE);
                    } else {
                        playSound(NOTE_F4, SND_ERROR_MS);   // Para yok / maks seviye
                    }
                } else if (tiCursor == 1) {     // Sat
                    particles.emit(towerCenterX(t), towerCenterY(t),
                                   COL_MONEY, PART_N_BUILD);
                    sellTower(t);
                    playSound(NOTE_C5, SND_BUILD_MS);
                    state = PLAYING;
                    lastFrameMs = millis();
                    break;
                } else {                        // Kapat
                    playSound(NOTE_F4, SND_NAV_MS);
                    state = PLAYING;
                    lastFrameMs = millis();
                    break;
                }
            }

            if (bP) {                           // B: paneli kapat
                playSound(NOTE_F4, SND_NAV_MS);
                state = PLAYING;
                lastFrameMs = millis();
                break;
            }

            drawScene();
            drawCursor(canvas, curX, curY);
            drawTowerInfo(canvas, t, tiCursor);
            break;
        }

        // ---------------- DALGA TEMİZ ----------------
        case WAVE_CLEAR: {
            shake.update();                // Kalan sarsıntı sönümlensin
            particles.update(dt);
            updatePopups(dt);
            updateRays(dt);
            updateShots(dt);
            updateRings(dt);
            drawScene();
            drawWaveClear(canvas, clearedWave, lastBonus);
            if (now - stateTimer >= WAVE_CLEAR_MS) {   // 2 sn otomatik geçiş
                prepLeft = WAVE_PREP_S;    // Sonraki dalga hazırlık süresi
                state = PLAYING;
                lastFrameMs = millis();
            }
            break;
        }

        // ---------------- BÖLÜM GEÇİŞİ ----------------
        case LEVEL_CLEAR: {
            shake.update();
            particles.update(dt);
            updatePopups(dt);
            updateRays(dt);
            updateShots(dt);
            updateRings(dt);
            drawScene();
            drawLevelClear(canvas, currentLevel, lastBonus);
            if (now - stateTimer >= LEVEL_CLEAR_MS) {  // 2.5 sn sonra yeni harita
                startNextLevel();
            }
            break;
        }

        // ---------------- PAUSE ----------------
        case PAUSE: {
            drawPauseOverlay(canvas);      // v4.1: temiz düz arka plan (sahne çizmeye gerek yok)
            if (aP || swP) {
                playSound(NOTE_D5, SND_RESUME_MS);     // Standart devam
                state = PLAYING;
                lastFrameMs = millis();    // dt sıçramasını önle
            } else if (bP) {
                returnToOS();
            }
            break;
        }

        // ---------------- GAME OVER ----------------
        case GAMEOVER: {
            shake.update();                // Sarsıntı sönümlensin
            particles.update(dt);
            updatePopups(dt);
            updateRays(dt);
            updateShots(dt);
            updateRings(dt);
            drawGameOver(canvas, highScore, newRecord);   // v4.1: temiz düz arka plan

            if (now - stateTimer >= GAMEOVER_GUARD_MS) {   // 600 ms girdi koruması
                if (aP) {
                    playSound(NOTE_D5, SND_RESTART_MS);    // Standart restart
                    startNewGame();
                } else if (bP) {
                    returnToOS();
                }
            }
            break;
        }

        // ---------------- ZAFER ----------------
        case VICTORY: {
            shake.update();                // Kalan efektler sönümlensin
            particles.update(dt);
            updatePopups(dt);
            updateRays(dt);
            updateShots(dt);
            updateRings(dt);
            drawVictory(canvas);   // v4.1: temiz düz arka plan

            if (now - stateTimer >= GAMEOVER_GUARD_MS) {   // 600 ms girdi koruması
                if (aP) {
                    playSound(NOTE_D5, SND_RESTART_MS);    // Standart restart
                    startNewGame();
                } else if (bP) {
                    returnToOS();
                }
            }
            break;
        }
    }

    // Her state'te: screenshot kontrolü + double buffer push (sarsıntılı)
    checkScreenshot(canvas);
    canvas.pushSprite(shake.offsetX, shake.offsetY);
}
