// ============================================================
//  dungeon/dungeon.ino — E-OS DUNGEON CRAWLER v2.0 (Roguelike)
//  160x128 TFT LCD | ST7735 | TFT_eSPI | ESP32-S3
//
//  Sorumluluk: Setup boilerplate, girdi işleme, durum makinesi
//  (MENU / PLAYING / GAMEOVER / PAUSE / LEVEL_CLEAR / INVENTORY /
//   SPELL_MENU / MERCHANT / BOSS_INTRO / BOSS_FIGHT)
//  ve millis() tabanlı delta-time ana oyun döngüsü (60 FPS limiter).
//
//  Kontroller:
//    JOY     -> 4 yönlü grid hareketi (düşmana yürümek = saldırı)
//    JOY_SW  -> Pause
//    BTN_A   -> Saldır / Sandık aç / Onay / Eşya-büyü kullan
//    BTN_B   -> Büyü kitabı (oyunda) / OS'a dönüş (menülerde)
//    BTN_C   -> Envanter aç/kapat
//    BTN_D   -> (kullanılmıyor)
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

// Oyun modülleri (bağımlılık zinciri: Config → veri → Spells → Combat → Renderer)
#include "Config.h"
#include "Player.h"
#include "Map.h"
#include "../SharedParticles.h"
#include "Enemies.h"
#include "Items.h"
#include "Spells.h"
#include "SaveManager.h"
#include "Combat.h"
#include "Renderer.h"

// ------------------------------------------------------------
//  Donanım nesneleri ve genel durum
// ------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();               // Ana TFT sürücüsü
TFT_eSprite canvas = TFT_eSprite(&tft);  // Double-buffer sprite

bool soundEnabled = true;    // NVS'ten yüklenir
bool showFps = false;        // NVS'ten yüklenir
int  highScore = 0;          // Ulaşılan en derin kat ("hs_dungeon")
bool newRecord = false;      // Bu oyunda rekor kırıldı mı

GameState state = MENU;
GameState playState = PLAYING;      // Menülere girerken dönülecek state
int floorNum = 1;                   // Mevcut kat
uint32_t stateTimer = 0;            // Ekran bekleme süreleri için

// NPC Diyalog Sistemi (Faz 3)
char dialogSpeaker[16];
char dialogText[64];
GameState dialogNextState;

int joyCenterX = 2048, joyCenterY = 2048;   // Joystick kalibrasyon merkezi

// Delta-time ve zamanlayıcılar
uint32_t lastFrameUs = 0;    // Son frame zamanı µs (dt hesabı + tam-60 FPS limiter)
float    enemyTickAcc = 0;   // Düşman AI için 60 Hz tick biriktirici
float    statusAcc    = 0;   // Durum efektleri için 1 Hz tick biriktirici (v2.0)

// Girdi durumu
uint32_t lastMoveMs = 0;     // Son hareket zamanı (150 ms tekrar)
int      heldDir    = -1;    // Basılı tutulan yön (typewriter pattern)
uint32_t lastNavMs  = 0;     // Envanter imleç tekrar zamanı
int      heldNav    = 0;     // Basılı tutulan menü yönü
int prevBtnA = HIGH, prevBtnB = HIGH, prevBtnC = HIGH, prevBtnD = HIGH, prevJoySW = HIGH;

// FPS sayacı
uint32_t fpsWindowStart = 0;
int fpsFrames = 0, currentFps = 0;

// Menü durumu
bool menuHasSave = false;
int menuCursor = 0;

// ------------------------------------------------------------
//  GameBase sarmalayıcıları
// ------------------------------------------------------------
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

// ------------------------------------------------------------
//  Standalone koruması
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
        return;
    }
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
//  Girdi yardımcıları
// ------------------------------------------------------------
bool btnPressed(uint8_t pin, int &prev) {
    int cur = digitalRead(pin);
    bool fired = (prev == HIGH && cur == LOW);
    prev = cur;
    return fired;
}

int readJoyDir() {
    int jx = analogRead(JOY_X) - joyCenterX;
    int jy = analogRead(JOY_Y) - joyCenterY;
    if (abs(jx) <= JOY_DEADZONE && abs(jy) <= JOY_DEADZONE) return -1;
    if (abs(jx) > abs(jy)) return (jx > 0) ? DIR_RIGHT : DIR_LEFT;
    return (jy > 0) ? DIR_DOWN : DIR_UP;
}

int readJoyVert() {
    int jy = analogRead(JOY_Y) - joyCenterY;
    if (jy < -JOY_DEADZONE) return -1;
    if (jy > JOY_DEADZONE) return 1;
    return 0;
}

// ------------------------------------------------------------
//  Non-blocking jingle çalıcı
// ------------------------------------------------------------
enum JingleMode : uint8_t { JI_FANFARE, JI_DEATH, JI_CHEST, JI_BOSSWIN };

struct Jingle {
    JingleMode mode = JI_FANFARE;
    uint8_t idx = 0;
    bool active = false;
    uint32_t nextMs = 0;

    void start(JingleMode m) {
        mode = m;
        idx = 0;
        active = true;
        nextMs = millis();
    }

    void update() {
        if (!active) return;
        uint32_t now = millis();
        if (now < nextMs) return;
        switch (mode) {
            case JI_FANFARE:
                switch (idx) {
                    case 0:  playSound(NOTE_C5, FANFARE_NOTE_MS); nextMs = now + FANFARE_GAP_MS; break;
                    case 1:  playSound(NOTE_E5, FANFARE_NOTE_MS); nextMs = now + FANFARE_GAP_MS; break;
                    default: playSound(NOTE_G5, FANFARE_TOP_MS);  active = false; break;
                }
                break;
            case JI_DEATH:
                switch (idx) {
                    case 0:  playSound(NOTE_E3, SND_DIE1_MS); nextMs = now + SND_DIE_GAP_MS; break;
                    default: playSound(NOTE_G3, SND_DIE2_MS); active = false; break;
                }
                break;
            case JI_CHEST:
                switch (idx) {
                    case 0:  playSound(NOTE_C5, SND_CHEST_MS); nextMs = now + SND_CHEST_GAP_MS; break;
                    default: playSound(NOTE_E5, SND_CHEST_MS); active = false; break;
                }
                break;
            default:
                switch (idx) {
                    case 0:  playSound(NOTE_E3, SND_BOSS_DEATH_MS);
                             nextMs = now + SND_BOSS_DEATH_MS + 10; break;
                    case 1:  playSound(NOTE_C5, FANFARE_NOTE_MS); nextMs = now + FANFARE_GAP_MS; break;
                    case 2:  playSound(NOTE_E5, FANFARE_NOTE_MS); nextMs = now + FANFARE_GAP_MS; break;
                    default: playSound(NOTE_G5, FANFARE_TOP_MS);  active = false; break;
                }
                break;
        }
        idx++;
    }
};

Jingle jingle;

void startFanfare()       { jingle.start(JI_FANFARE); }
void startBossWinJingle() { jingle.start(JI_BOSSWIN); }

// ------------------------------------------------------------
//  Oyun akışı
// ------------------------------------------------------------
void spawnMerchant() {
    for (int t = 0; t < SPAWN_TRIES; t++) {
        int x, y;
        if (roomCount > 1) {
            if (!randomFloorInRoom(rooms[random(1, roomCount)], x, y)) continue;
        } else if (roomCount == 1) {
            if (!randomFloorInRoom(rooms[0], x, y)) continue;
            // Zindanın ortasına çok yakın çıkmasın (Oyuncu üstünde doğmasın)
            if (abs(x - startTileX) + abs(y - startTileY) < 5) continue;
        } else {
            return;
        }
        if (enemyIndexAt(x, y) >= 0) continue;
        merchant.active = true;
        merchant.tileX = x;
        merchant.tileY = y;
        return;
    }
}

void showBiomeMessage() {
    switch (currentBiome) {
        case BIOME_CAVE:   showMessage("ENTERED CAVE!");  break;
        case BIOME_FOREST: showMessage("ENTERED FOREST!");    break;
        case BIOME_HELL:   showMessage("ENTERED HELL!"); break;
        default:           showMessage("ENTERED DUNGEON!");   break;
    }
}

void startFloor() {
    generateMap(floorNum);
    player.setPosition(startTileX, startTileY);
    updateFog(player.tileX, player.tileY);

    boss.active = false;
    if (isBossFloor) initBoss(floorNum);
    spawnEnemies(floorNum);

    merchant.active = false;
    merchant.talkedThisFloor = false;
    if (!isBossFloor && floorNum >= MERCHANT_MIN_FLOOR &&
        random(0, PCT_MAX) < MERCHANT_PCT) {
        spawnMerchant();
    }

    clearEffects();
    floorKills = 0;
    floorDamageTaken = 0;
    enemyTickAcc = 0;
    statusAcc = 0;
    heldDir = -1;

    if (floorNum >= ACH_DEEP_FLOOR) unlockAchievement(ACH_DEEP);

    if (floorNum > highScore) {
        highScore = floorNum;
        osSaveHighScore("hs_dungeon", highScore);
        newRecord = true;
    }
    saveGameState(floorNum, killsTotal);
}

void startNewGame() {
    player.init();
    inventory.init();
    syncPlayerKeys();
    initSpells();
    killsTotal = 0;
    floorNum = 1;
    newRecord = false;
    startFloor();
    state = PLAYING;
    playState = PLAYING;
    lastFrameUs = micros();
}

void goLevelClear() {
    if (floorDamageTaken == 0) unlockAchievement(ACH_UNTOUCHED);
    startFanfare();
    state = LEVEL_CLEAR;
    stateTimer = millis();
}

void goGameOver() {
    jingle.start(JI_DEATH);
    state = GAMEOVER;
    stateTimer = millis();
}

void openChest(int tx, int ty) {
    if (inventory.isFull()) {
        showMessage("INVENTORY FULL!");
        return;
    }
    ItemType it = rollChestItem();
    inventory.addItem(it);
    syncPlayerKeys();
    tiles[ty][tx] = TILE_FLOOR;
    jingle.start(JI_CHEST);
    particles.emit(tx * TILE_PX + TILE_PX / 2, ty * TILE_PX + TILE_PX / 2,
                   COL_CHEST, PART_N_CHEST);

    if (random(0, PCT_MAX) < CHEST_GOLD_PCT) {
        int g = random(CHEST_GOLD_MIN, CHEST_GOLD_MAX + 1);
        addGold(g);
        char buf[MSG_BUF_LEN];
        snprintf(buf, sizeof(buf), "%s +%dg", itemName(it), g);
        showMessage(buf);
    } else {
        showMessage(itemName(it));
    }
}

void tryPlayerMove(int d) {
    if (status.stunned()) return;
    player.dir = d;
    int nx = player.tileX + DIR_DX[d];
    int ny = player.tileY + DIR_DY[d];

    if (bossOccupies(nx, ny)) {
        playerAttackDir(d);
        return;
    }

    if (enemyIndexAt(nx, ny) >= 0) {
        playerAttackDir(d);
        return;
    }

    if (merchant.active && merchant.tileX == nx && merchant.tileY == ny) {
        if (!merchant.talkedThisFloor) {
            merchant.talkedThisFloor = true;
            merchCursor = 0;
            startDialogue("Tuna", "You don't look good,\nlet's see what I have.", MERCHANT);
        } else {
            showMessage("SOLD OUT!");
        }
        return;
    }

    if (tileAt(nx, ny) == TILE_LOCKED) {
        int ki = inventory.firstOf(ITEM_KEY);
        if (ki >= 0) {
            inventory.removeAt(ki);
            syncPlayerKeys();
            tiles[ny][nx] = TILE_DOOR;
            playSound(NOTE_D5, SND_UNLOCK_MS);
            showMessage("DOOR OPENED!");
        } else {
            showMessage("LOCKED! NEED KEY");
        }
        return;
    }

    if (tileAt(nx, ny) == TILE_LAVA) {
        damagePlayer(LAVA_DMG);
        showMessage("LAVA BURNS!");
        return;
    }

    if (!canPlayerWalk(nx, ny)) return;

    player.prevTileX = player.tileX;   // Kayma animasyonu başlangıcı (v3.0)
    player.prevTileY = player.tileY;
    player.tileX = nx;
    player.tileY = ny;
    player.lastMoveMs = millis();
    updateFog(nx, ny);

    uint8_t t = tiles[ny][nx];
    if (t == TILE_SWAMP) {
        if (!status.slowed()) showMessage("SWAMP! SLOWED DOWN");
        status.slowUntil = millis() + SWAMP_SLOW_MS;
    } else if (t == TILE_FLOWER) {
        status.poisonStacks += FLOWER_POISON_STACKS;
        if (status.poisonStacks > POISON_MAX_STACKS)
            status.poisonStacks = POISON_MAX_STACKS;
        particles.emit(nx * TILE_PX + TILE_PX / 2, ny * TILE_PX + TILE_PX / 2,
                       COL_POTION, PART_N_POISON);
        showMessage("POISONOUS FLOWER!");
    }

    if (t == TILE_STAIRS) goLevelClear();
}

void interactA() {
    if (status.stunned()) return;
    int tx = player.tileX + DIR_DX[player.dir];
    int ty = player.tileY + DIR_DY[player.dir];
    if (tileAt(tx, ty) == TILE_CHEST) {
        openChest(tx, ty);
        return;
    }
    playerAttackDir(player.dir);
}

void trySelectedSpell() {
    Spell &sp = spellbook[selectedSpell];
    char buf[MSG_BUF_LEN];
    if (player.lvl < sp.unlockLvl) {
        snprintf(buf, sizeof(buf), "LEVEL %d NEEDED!", sp.unlockLvl);
        showMessage(buf);
        playSound(NOTE_E3, SND_NO_MONEY_MS);
        return;
    }
    if (sp.cooldownLeft > 0) {
        snprintf(buf, sizeof(buf), "COOLDOWN: %d S", spellCooldownSec(sp));
        showMessage(buf);
        return;
    }
    if (player.mana < sp.manaCost) {
        showMessage("NOT ENOUGH MANA!");
        playSound(NOTE_E3, SND_NO_MONEY_MS);
        return;
    }
    if (spellNeedsTarget(sp.type)) {
        spellTargeting = true;
        targetX = player.tileX;
        targetY = player.tileY;
        return;
    }
    if (castSpellAt(selectedSpell, player.tileX, player.tileY)) {
        state = playState;
        lastFrameUs = micros();
    }
}

void doMerchantAction() {
    bool isBuy = (merchCursor < 4);
    ItemType t = TRADE_ITEMS[isBuy ? merchCursor : merchCursor - 4];
    if (isBuy) {
        int price = buyPriceOf(t);
        if (player.gold < price) {
            playSound(NOTE_E3, SND_NO_MONEY_MS);
            showMessage("NOT ENOUGH GOLD!");
            return;
        }
        if (inventory.isFull()) {
            showMessage("INVENTORY FULL!");
            return;
        }
        player.gold -= price;
        inventory.addItem(t);
        syncPlayerKeys();
        playSound(NOTE_E5, SND_BUY_MS);
        showMessage("BOUGHT!");
    } else {
        int idx = inventory.firstOf(t);
        if (idx < 0) {
            showMessage("NOTHING TO SELL!");
            return;
        }
        inventory.removeAt(idx);
        syncPlayerKeys();
        addGold(sellPriceOf(t));
        playSound(NOTE_C5, SND_SELL_MS);
        showMessage("SOLD!");
    }
}

void drawScene() {
    // v3.0: piksel-bazlı kamera — lerp'li oyuncuyu takip eder,
    // harita alt-tile ofsetiyle akıcı kayar.
    int camPx, camPy;
    computeCameraPx(camPx, camPy);
    int shX = shake.offsetX, shY = shake.offsetY;
    int offX = -camPx + shX;                  // dünya px → ekran px
    int offY = HUD_H - camPy + shY;
    int camX = camPx / TILE_PX, camY = camPy / TILE_PX;
    int subX = camPx - camX * TILE_PX;        // Tile içi kayma (0..7)
    int subY = camPy - camY * TILE_PX;

    canvas.fillSprite(COL_BG);
    drawMap(canvas, camX, camY, shX - subX, shY - subY);
    drawTelegraphs(canvas, offX, offY);
    drawEntities(canvas, offX, offY);
    drawBolts(canvas, offX, offY);         // Lich mermileri varlıkların üstünde (v3.2)
    drawSlashes(canvas, offX, offY);
    particles.draw(canvas, offX, offY);
    drawPopups(canvas, offX, offY);
    drawHUD(canvas, floorNum, currentFps, showFps);
    drawBossBar(canvas);
    drawMinimap(canvas);
    drawMessage(canvas);
    drawAchievementBanner(canvas);
}

void startDialogue(const char *speaker, const char *text, GameState nextState) {
    strncpy(dialogSpeaker, speaker, sizeof(dialogSpeaker) - 1);
    dialogSpeaker[sizeof(dialogSpeaker) - 1] = '\0';
    strncpy(dialogText, text, sizeof(dialogText) - 1);
    dialogText[sizeof(dialogText) - 1] = '\0';
    dialogNextState = nextState;
    playState = state;
    state = DIALOGUE;
    playSound(NOTE_C5, 100);
}

void setup() {
    osInitBuzzer();
    Wire.begin(I2C_SDA, I2C_SCL);
    osOLEDOff();

    if (osPartitionValid()) {
        const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
        esp_ota_set_boot_partition(os_part);
    }

    osInitButtons();

    {
        Preferences prefs;
        prefs.begin("os", true);
        soundEnabled = prefs.getBool("sound_en", true);
        showFps = prefs.getBool("show_fps", false);
        highScore = prefs.getInt("hs_dungeon", 0);
        prefs.end();
    }
    loadAchievements();

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
    initDevTools(tft);

    tft.init();
    tft.setRotation(1);
    tft.startWrite();
    tft.writecommand(0x36);
    tft.writedata(0xA0);
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP);
    tft.fillScreen(TFT_BLACK);

    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);

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
    randomSeed(analogRead(JOY_Y) ^ (analogRead(JOY_X) << 8) ^ micros());

    menuHasSave = hasSaveGame();
    menuCursor = menuHasSave ? 0 : 1;
    state = MENU;
    lastFrameUs = micros();
    fpsWindowStart = millis();
}

void loop() {
    uint32_t nowUs = micros();
    if (nowUs - lastFrameUs < FRAME_US) return;
    float dt = (nowUs - lastFrameUs) / 1000000.0f;
    if (dt > DT_CAP) dt = DT_CAP;
    lastFrameUs = nowUs;
    uint32_t now = millis();

    fpsFrames++;
    if (now - fpsWindowStart >= FPS_WINDOW_MS) {
        currentFps = fpsFrames;
        fpsFrames = 0;
        fpsWindowStart = now;
    }

    jingle.update();
    bool aP  = btnPressed(BTN_A, prevBtnA);
    bool bP  = btnPressed(BTN_B, prevBtnB);
    bool cP  = btnPressed(BTN_C, prevBtnC);
    bool dP  = btnPressed(BTN_D, prevBtnD);
    bool swP = btnPressed(JOY_SW, prevJoySW);

    switch (state) {
        case MENU: {
            if (menuHasSave) {
                int nav = readJoyVert();
                if (nav != 0 && now - lastNavMs > 250) {
                    menuCursor = (menuCursor == 0) ? 1 : 0;
                    lastNavMs = now;
                    playSound(NOTE_C5, 20);
                }
            }
            if (aP) {
                playSound(NOTE_E5, SND_START_MS);
                if (menuHasSave && menuCursor == 0) {
                    loadGameState(floorNum, killsTotal);
                    newRecord = false;
                    startFloor();
                    state = PLAYING;
                    playState = PLAYING;
                    lastFrameUs = micros();
                } else {
                    startNewGame();
                }
                break;
            }
            if (bP) returnToOS();
            drawMenu(canvas, highScore, menuHasSave, menuCursor);
            break;
        }

        case PLAYING:
        case BOSS_FIGHT: {
            if (swP) {
                playSound(NOTE_G4, SND_PAUSE_MS);
                playState = state;
                state = PAUSE;
                break;
            }
            if (cP) {
                playSound(NOTE_F4, SND_INV_MS);
                inventory.cursor = 0;
                playState = state;
                state = INVENTORY;
                break;
            }
            if (bP) {
                if (player.lvl >= SPELLBOOK_LVL) {
                    playSound(NOTE_F4, SND_INV_MS);
                    spellTargeting = false;
                    playState = state;
                    state = SPELL_MENU;
                    break;
                }
                showMessage("LEVEL 2 NEEDED FOR SPELL");
            }

            uint32_t repeatMs = status.slowed() ? MOVE_REPEAT_MS * 2 : MOVE_REPEAT_MS;
            int d = readJoyDir();
            if (d < 0) {
                heldDir = -1;
            } else if (d != heldDir || now - lastMoveMs >= repeatMs) {
                tryPlayerMove(d);
                heldDir = d;
                lastMoveMs = now;
            }
            if (state != PLAYING && state != BOSS_FIGHT) break;

            if (aP) interactA();

            enemyTickAcc += dt;
            while (enemyTickAcc >= FRAME_SEC) {
                enemyTickAcc -= FRAME_SEC;
                tickEnemies();
                if (state == BOSS_FIGHT) tickBoss();
                tickSpellCooldowns();
                if (playerDied) break;
            }

            statusAcc += dt;
            while (statusAcc >= STATUS_TICK_MS / 1000.0f) {
                statusAcc -= STATUS_TICK_MS / 1000.0f;
                tickStatus1Hz();
            }

            updateTelegraphs();
            updateBolts();                 // Lich büyü mermileri (v3.2)
            particles.update(dt);
            updatePopups(dt);
            updateSlashes(dt);
            shake.update();

            if (bossDefeated) {
                bossDefeated = false;
                state = PLAYING;
                playState = PLAYING;
            }

            if (playerDied) {
                drawScene();
                goGameOver();
                break;
            }

            drawScene();
            break;
        }

        case GAMEOVER: {
            shake.update();
            drawScene();
            drawGameOver(canvas, floorNum, killsTotal, highScore, newRecord);
            if (now - stateTimer >= GAMEOVER_GUARD_MS) {
                if (aP) {
                    playSound(NOTE_D5, SND_RESTART_MS);
                    clearSaveGame();
                    menuHasSave = false;
                    menuCursor = 1;
                    startNewGame();
                } else if (bP) {
                    clearSaveGame();
                    returnToOS();
                }
            }
            break;
        }

        case PAUSE: {
            drawScene();
            drawPauseOverlay(canvas);
            if (aP) {
                playSound(NOTE_D5, SND_RESUME_MS);
                state = playState;
                lastFrameUs = micros();
            } else if (bP) {
                returnToOS();
            }
            break;
        }

        case LEVEL_CLEAR: {
            drawLevelClear(canvas, floorNum, floorKills);
            if (now - stateTimer >= LEVEL_CLEAR_MS) {
                floorNum++;
                BiomeType prevBiome = currentBiome;
                startFloor();
                if (currentBiome != prevBiome) showBiomeMessage();
                if (isBossFloor) {
                    playSound(NOTE_G3, SND_BOSS_INTRO_MS);
                    state = BOSS_INTRO;
                    playState = BOSS_FIGHT;
                    stateTimer = millis();
                } else {
                    state = PLAYING;
                    playState = PLAYING;
                }
                lastFrameUs = micros();
            }
            break;
        }

        // ---------------- ENVANTER ----------------
        case INVENTORY: {
            drawScene();                   // Oyun donmuş arka plan
            drawInventoryScreen(canvas);

            // İmleç: yukarı/aşağı (tekrar korumalı)
            int nav = readJoyVert();
            if (nav == 0) {
                heldNav = 0;
            } else if (nav != heldNav || now - lastNavMs >= MENU_REPEAT_MS) {
                inventory.cursor = (inventory.cursor + nav + INV_SLOTS) % INV_SLOTS;
                playSound(NOTE_F4, SND_NAV_MS);
                heldNav = nav;
                lastNavMs = now;
            }

            // Seçili eşyayı kullan
            if (aP) {
                ItemType t = inventory.slots[inventory.cursor];
                if (t != ITEM_NONE && applyItemEffect(t)) {
                    particles.emit(player.tileX * TILE_PX + TILE_PX / 2,
                                   player.tileY * TILE_PX + TILE_PX / 2,
                                   itemColor(t), PART_N_ITEM);
                    inventory.removeAt(inventory.cursor);
                    syncPlayerKeys();
                }
            }

            // Kapat
            if (cP) {
                playSound(NOTE_F4, SND_INV_MS);
                state = playState;         // PLAYING veya BOSS_FIGHT'a dön
                lastFrameUs = micros();    // dt sıçramasını önle
            }
            break;
        }

        // ---------------- BÜYÜ KİTABI (v2.0) ----------------
        case SPELL_MENU: {
            if (spellTargeting) {
                // İmleç modu: FIREBOLT/TELEPORT hedef seçimi
                drawScene();
                drawTargetCursor(canvas, shake.offsetX, shake.offsetY);

                int td = readJoyDir();
                if (td < 0) {
                    heldDir = -1;
                } else if (td != heldDir || now - lastNavMs >= MENU_REPEAT_MS) {
                    int nx = targetX + DIR_DX[td], ny = targetY + DIR_DY[td];
                    if (inMap(nx, ny) &&
                        abs(nx - player.tileX) <= SPELL_RANGE &&
                        abs(ny - player.tileY) <= SPELL_RANGE) {
                        targetX = nx;
                        targetY = ny;
                        playSound(NOTE_F4, SND_NAV_MS);
                    }
                    heldDir = td;
                    lastNavMs = now;
                }

                if (aP && castSpellAt(selectedSpell, targetX, targetY)) {
                    spellTargeting = false;
                    state = playState;
                    lastFrameUs = micros();
                }
                if (bP || cP) {                // İptal: menüye dön
                    spellTargeting = false;
                    playSound(NOTE_F4, SND_NAV_MS);
                }
            } else {
                drawScene();                   // Oyun donmuş arka plan
                drawSpellMenu(canvas);

                // İmleç: yukarı/aşağı (tekrar korumalı)
                int nav = readJoyVert();
                if (nav == 0) {
                    heldNav = 0;
                } else if (nav != heldNav || now - lastNavMs >= MENU_REPEAT_MS) {
                    selectedSpell = (selectedSpell + nav + MAX_SPELLS) % MAX_SPELLS;
                    playSound(NOTE_F4, SND_NAV_MS);
                    heldNav = nav;
                    lastNavMs = now;
                }

                if (aP) trySelectedSpell();

                if (bP || cP) {                // Kapat (B toggle, C ortak kapat)
                    playSound(NOTE_F4, SND_INV_MS);
                    state = playState;
                    lastFrameUs = micros();
                }
            }
            break;
        }

        // ---------------- DIYALOG EKRANI (Faz 3) ----------------
        case DIALOGUE: {
            drawScene();
            drawDialogueScreen(canvas, dialogSpeaker, dialogText);
            if (aP) {
                playSound(NOTE_F4, SND_NAV_MS);
                state = dialogNextState;
                lastFrameUs = micros();
            }
            break;
        }

        // ---------------- TÜCCAR (v2.0) ----------------
        case MERCHANT: {
            drawScene();                       // Oyun donmuş arka plan
            drawMerchantScreen(canvas);

            // İmleç: yukarı/aşağı (tekrar korumalı)
            int nav = readJoyVert();
            if (nav == 0) {
                heldNav = 0;
            } else if (nav != heldNav || now - lastNavMs >= MENU_REPEAT_MS) {
                merchCursor = (merchCursor + nav + TRADE_ROWS) % TRADE_ROWS;
                playSound(NOTE_F4, SND_NAV_MS);
                heldNav = nav;
                lastNavMs = now;
            }

            if (aP) doMerchantAction();

            if (cP || bP) {                    // Kapat
                playSound(NOTE_F4, SND_INV_MS);
                state = playState;
                lastFrameUs = micros();
            }
            break;
        }

        // ---------------- BOSS GİRİŞ (v2.0) ----------------
        case BOSS_INTRO: {
            drawScene();
            drawBossIntro(canvas);
            if (now - stateTimer >= BOSS_INTRO_MS) {   // 500 ms otomatik geçiş
                state = BOSS_FIGHT;
                lastFrameUs = micros();
            }
            break;
        }
    }

    // Her state'te: screenshot kontrolü + double buffer push
    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}
