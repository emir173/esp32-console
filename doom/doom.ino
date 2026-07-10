// ============================================================
//  EMİR DOOM V8.0 — MODÜLER REFACTOR
//  Dosyalar: Config.h (sabitler), Renderer.h (çizim),
//            Player.h (oyuncu), Enemies.h (düşman AI)
// ============================================================
#pragma GCC optimize ("O3")
#pragma GCC optimize ("unroll-loops")

#include "Config.h"
#include "Renderer.h"
#include "Player.h"
#include "Enemies.h"

// ════════════════════════════════════════════════════════════
//  GLOBAL NESNE TANIMLARI
// ════════════════════════════════════════════════════════════
TFT_eSPI tft = TFT_eSPI();
U8G2_SH1106_128X64_NONAME_F_SW_I2C oled(U8G2_R0, 9, 8, U8X8_PIN_NONE);

// ════════════════════════════════════════════════════════════
//  TRIPLE BUFFER & RAYCASTING
// ════════════════════════════════════════════════════════════
uint16_t* fb[3];
volatile int8_t fb_render = 0;
volatile int8_t fb_ready = -1;
volatile int8_t fb_display = 1;
SemaphoreHandle_t fb_swap_mutex;

float zBuffer[SW];
float camXTable[SW];

// ════════════════════════════════════════════════════════════
//  TEXTURE ATLAS
// ════════════════════════════════════════════════════════════
uint16_t *tex[MAX_TEX];
bool sdReady = false;

// ════════════════════════════════════════════════════════════
//  MUTEX & HARİTA
// ════════════════════════════════════════════════════════════
SemaphoreHandle_t gameMutex;
uint8_t MAP[MH][MW];

// ════════════════════════════════════════════════════════════
//  SPRITE DİZİSİ
// ════════════════════════════════════════════════════════════
Sprite sprites[NUM_SPRITES];

// ════════════════════════════════════════════════════════════
//  OYUNCU & KAMERA
// ════════════════════════════════════════════════════════════
float px = 1.5, py = 1.5;
float dirX = 1, dirY = 0;
float planeX = 0, planeY = 0.66;
int joyCenterX = 2048, joyCenterY = 2048;
volatile int joyRawX = 0, joyRawY = 0;

// ════════════════════════════════════════════════════════════
//  ZAMANLAMA & İSTATİSTİK
// ════════════════════════════════════════════════════════════
uint32_t lastFrame = 0, fpsTimer = 0;
int fps = 0, frameCount = 0;
int hp = 100, ammo = 75, armor = 0;
int currentLevel = 1;
bool hasKey = false;

uint32_t fireT = 0, lastDamageTime = 0, sonKullanma = 0;
uint32_t shieldSawTime = 0, lastEnemyFire = 0;
uint32_t shieldStartTime = 0;
bool lastShieldState = false;
uint32_t meleeTimer = 0;

int lastAmmo = -1, lastHp = -1, lastArmor = -1, lastFps = -1;
bool lastInfState = false;

bool soundEnabled = true;
bool showFps = false;   // ayarlardan ("os"/"show_fps") yuklenir

// ════════════════════════════════════════════════════════════
//  OYUN DURUMU & MENÜ
// ════════════════════════════════════════════════════════════
volatile GameState gameState = STATE_BOOT;
bool titleDrawn = false;
uint32_t bootStartTime = 0;
uint16_t *titlePicBuf = nullptr;

int masterMenuSel = 0;
bool masterMenuDrawn = false;

int menuSelection = 0;
int levelSelectIdx = 0;
bool inLevelSelect = false;
bool menuDrawn = false;

// ════════════════════════════════════════════════════════════
//  SNAKE MİNİ OYUN
// ════════════════════════════════════════════════════════════
SnakeCell snakeBody[SNAKE_MAX];
int snakeLen = 3;
int8_t snakeDirX = 1, snakeDirY = 0;
int8_t snakeFoodX = 15, snakeFoodY = 13;
uint32_t snakeLastMove = 0;
int snakeScore = 0;
bool snakeDead = false;
bool snakeDrawn = false;

int weaponType = 0;

// ════════════════════════════════════════════════════════════
//  SEVİYE İSTATİSTİKLERİ (orijinal Doom tarzı intermission)
// ════════════════════════════════════════════════════════════
volatile bool levelDone = false;
int levelItemTotal = 0;

// ════════════════════════════════════════════════════════════
//  KAPI ANİMASYONU & ÇIKIŞ SWITCH'İ
// ════════════════════════════════════════════════════════════
DoorAnim doorAnims[DOOR_ANIM_MAX];
int doorAnimCount = 0;
bool exitPressed = false;
bool elevatorPending = false;
int8_t elevSrcX = 0, elevSrcY = 0;

// startDoorAnim — kapıyı animasyon listesine ekler. Liste doluysa
// eski davranışa düşer (anında açılır) — oyun asla kilitlenmez.
void startDoorAnim(int x, int y) {
    if (doorAnimCount >= DOOR_ANIM_MAX) { MAP[y][x] = 0; return; }
    doorAnims[doorAnimCount].x = (int8_t)x;
    doorAnims[doorAnimCount].y = (int8_t)y;
    doorAnims[doorAnimCount].t = 0.0f;
    doorAnimCount++;
}

// updateDoorAnims — açılan kapıları ilerletir; t>=1'de hücre boşalır.
// TaskEngine'de gameMutex altında çağrılır.
static void updateDoorAnims(float dt) {
    for (int i = 0; i < doorAnimCount; ) {
        doorAnims[i].t += DOOR_OPEN_SPEED * dt;
        if (doorAnims[i].t >= 1.0f) {
            MAP[doorAnims[i].y][doorAnims[i].x] = 0;
            doorAnims[i] = doorAnims[doorAnimCount - 1];
            doorAnimCount--;
        } else i++;
    }
}
uint32_t levelPlayMs = 0;
int totalKills = 0, totalKillTotal = 0;
uint32_t totalTimeMs = 0;

// isMonsterType / isItemType artık Config.h'de (ortak yardımcılar).

// Cesetler haritada kaldığı için (state=1, ANIM_DEAD) kill sayısı sprite
// dizisinden türetilir — oyun mantığındaki 6 ölüm noktasına sayaç gerekmez.
int countLevelMonsters() {
    int n = 0;
    for (int i = 0; i < NUM_SPRITES; i++)
        if (sprites[i].state != 0 && isMonsterType(sprites[i].type)) n++;
    return n;
}

int countLevelKills() {
    int n = 0;
    for (int i = 0; i < NUM_SPRITES; i++)
        if (sprites[i].state != 0 && isMonsterType(sprites[i].type) &&
            (sprites[i].animState == ANIM_DYING || sprites[i].animState == ANIM_DEAD)) n++;
    return n;
}

int countLevelItemsLeft() {
    int n = 0;
    for (int i = 0; i < NUM_SPRITES; i++)
        if (sprites[i].state != 0 && isItemType(sprites[i].type)) n++;
    return n;
}

// ════════════════════════════════════════════════════════════
//  WRAPPER FONKSİYONLARI (Platform API)
// ════════════════════════════════════════════════════════════
void playSound(uint16_t freq, uint32_t dur) {
    osPlaySound(freq, dur, soundEnabled);
}

void returnToOS() {
    osReturnToOS(tft, false);
}

// ════════════════════════════════════════════════════════════
//  loadLevel — Seviye yükleme: harita + sprite başlatma
// ════════════════════════════════════════════════════════════
void loadLevel(int level) {
    lastAmmo = -1; lastHp = -1; lastArmor = -1; lastFps = -1; lastInfState = false;
    hasKey = false;
    doorAnimCount = 0;      // yarım kalmış kapı animasyonları temizlenir
    exitPressed = false;    // çıkış switch'i normal haline döner
    elevatorPending = false;
    for (int i = 0; i < NUM_SPRITES; i++) initSprite(i, 0, 0, 0, 0);
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) {
        if (level == 1)      MAP[y][x] = LEVEL1[y][x];
        else if (level == 2) MAP[y][x] = LEVEL2[y][x];
        else if (level == 3) MAP[y][x] = LEVEL3[y][x];
    }

    if (level == 1) {
        px = 4.5; py = 28.5; dirX = 0; dirY = -1; planeX = 0.66; planeY = 0;
        // E1 HANGAR: baslangic -> zigzag depo (ANAHTAR) -> ana salon -> kuzey
        // kanat -> kilitli cikis. Ambar ile dongu; T(25,20) gizli odayi acar (pusu).
        initSprite(0, 6.5, 25.5, 5, 1); initSprite(1, 3.5, 29.5, 5, 1); initSprite(2, 8.5, 29.5, 15, 1);
        initSprite(3, 2.5, 24.5, ST_LAMP, 1); initSprite(4, 7.5, 27.5, ST_CORPSE, 1); initSprite(5, 2.5, 30.5, 9, 1);
        initSprite(6, 9.5, 30.5, 10, 1); initSprite(7, 7.5, 13.5, 17, 1); initSprite(8, 11.5, 16.5, 17, 1);
        initSprite(9, 3.5, 14.5, 17, 1); initSprite(10, 10.5, 12.5, 5, 1); initSprite(11, 6.5, 16.5, 5, 1);
        initSprite(12, 2.5, 16.5, 11, 1); initSprite(13, 2.5, 13.5, 10, 1); initSprite(14, 12.5, 17.5, 9, 1);
        initSprite(15, 4.5, 17.5, 15, 1); initSprite(16, 8.5, 14.5, ST_CORPSE, 1); initSprite(17, 12.5, 12.5, ST_LAMP, 1);
        initSprite(18, 17.5, 13.5, ST_PILLAR, 1); initSprite(19, 24.5, 13.5, ST_PILLAR, 1); initSprite(20, 17.5, 17.5, ST_PILLAR, 1);
        initSprite(21, 24.5, 17.5, ST_PILLAR, 1); initSprite(22, 16.5, 12.5, 5, 1); initSprite(23, 22.5, 16.5, 5, 1);
        initSprite(24, 26.5, 12.5, 5, 1); initSprite(25, 19.5, 18.5, 5, 1); initSprite(26, 21.5, 13.5, 17, 1);
        initSprite(27, 15.5, 18.5, 17, 1); initSprite(28, 21.5, 15.5, 14, 1); initSprite(29, 14.5, 11.5, 15, 1);
        initSprite(30, 27.5, 11.5, 15, 1); initSprite(31, 14.5, 19.5, 15, 1); initSprite(32, 27.5, 19.5, 15, 1);
        initSprite(33, 16.5, 19.5, 9, 1); initSprite(34, 26.5, 18.5, 9, 1); initSprite(35, 20.5, 11.5, 9, 1);
        initSprite(36, 22.5, 11.5, 10, 1); initSprite(37, 18.5, 15.5, ST_CORPSE, 1); initSprite(38, 23.5, 18.5, ST_CORPSE, 1);
        initSprite(39, 27.5, 12.5, ST_SKULLS, 1); initSprite(40, 14.5, 15.5, ST_LAMP, 1); initSprite(41, 27.5, 15.5, ST_LAMP, 1);
        initSprite(42, 21.5, 9.5, ST_LAMP, 1); initSprite(43, 15.5, 4.5, 5, 1); initSprite(44, 20.5, 6.5, 5, 1);
        initSprite(45, 25.5, 4.5, 5, 1); initSprite(46, 23.5, 5.5, 14, 1); initSprite(47, 14.5, 7.5, 15, 1);
        initSprite(48, 26.5, 3.5, 15, 1); initSprite(49, 14.5, 3.5, 9, 1); initSprite(50, 20.5, 3.5, 10, 1);
        initSprite(51, 26.5, 4.5, ST_CBRA, 1); initSprite(52, 26.5, 6.5, ST_CBRA, 1); initSprite(53, 17.5, 6.5, ST_CORPSE, 1);
        initSprite(54, 15.5, 7.5, ST_SKULLS, 1); initSprite(55, 28.5, 6.5, 10, 1); initSprite(56, 30.5, 6.5, 9, 1);
        initSprite(57, 28.5, 3.5, ST_CBRA, 1); initSprite(58, 14.5, 24.5, 5, 1); initSprite(59, 21.5, 27.5, 5, 1);
        initSprite(60, 23.5, 23.5, 5, 1); initSprite(61, 17.5, 26.5, 17, 1); initSprite(62, 13.5, 28.5, 15, 1);
        initSprite(63, 15.5, 28.5, 15, 1); initSprite(64, 22.5, 28.5, 15, 1); initSprite(65, 24.5, 26.5, 15, 1);
        initSprite(66, 13.5, 23.5, 15, 1); initSprite(67, 24.5, 29.5, 9, 1); initSprite(68, 13.5, 29.5, 9, 1);
        initSprite(69, 18.5, 29.5, 10, 1); initSprite(70, 19.5, 24.5, ST_CORPSE, 1); initSprite(71, 16.5, 23.5, ST_LAMP, 1);
        initSprite(72, 29.5, 14.5, 43, 1); initSprite(73, 30.5, 13.5, 9, 1); initSprite(74, 29.5, 13.5, ST_SKULLS, 1);
        initSprite(75, 29.5, 15.5, 5, -1); initSprite(76, 30.5, 15.5, 5, -1);
    } else if (level == 2) {
        px = 15.5; py = 27.5; dirX = 0; dirY = -1; planeX = 0.66; planeY = 0;
        // E2 DEIMOS: merkez hub + 4 kanat. Anahtar bati kislada, kilit kuzeyde.
        // Yemekhane dongusu; T(30,16) dogu ambarindaki gizli odayi acar (pusu).
        initSprite(0, 13.5, 28.5, ST_CORPSE, 1); initSprite(1, 12.5, 25.5, ST_LAMP, 1); initSprite(2, 19.5, 25.5, ST_LAMP, 1);
        initSprite(3, 18.5, 26.5, 5, 1); initSprite(4, 12.5, 29.5, 9, 1); initSprite(5, 3.5, 24.5, 5, 1);
        initSprite(6, 8.5, 27.5, 5, 1); initSprite(7, 5.5, 29.5, 5, 1); initSprite(8, 9.5, 24.5, 17, 1);
        initSprite(9, 2.5, 23.5, 15, 1); initSprite(10, 10.5, 29.5, 15, 1); initSprite(11, 2.5, 29.5, 10, 1);
        initSprite(12, 6.5, 23.5, 9, 1); initSprite(13, 6.5, 26.5, ST_CORPSE, 1); initSprite(14, 2.5, 26.5, ST_CBRA, 1);
        initSprite(15, 3.5, 12.5, 17, 1); initSprite(16, 6.5, 18.5, 17, 1); initSprite(17, 6.5, 12.5, 5, 1);
        initSprite(18, 2.5, 18.5, 5, 1); initSprite(19, 3.5, 15.5, 14, 1); initSprite(20, 2.5, 15.5, 11, 1);
        initSprite(21, 7.5, 11.5, 10, 1); initSprite(22, 2.5, 11.5, 9, 1); initSprite(23, 5.5, 15.5, ST_CORPSE, 1);
        initSprite(24, 2.5, 19.5, ST_SKULLS, 1); initSprite(25, 13.5, 13.5, 5, 1); initSprite(26, 18.5, 18.5, 5, 1);
        initSprite(27, 18.5, 13.5, 17, 1); initSprite(28, 12.5, 19.5, 15, 1); initSprite(29, 19.5, 12.5, 15, 1);
        initSprite(30, 14.5, 17.5, ST_PILLAR, 1); initSprite(31, 17.5, 14.5, ST_PILLAR, 1); initSprite(32, 14.5, 14.5, ST_LAMP, 1);
        initSprite(33, 17.5, 17.5, ST_LAMP, 1); initSprite(34, 12.5, 12.5, 10, 1); initSprite(35, 16.5, 22.5, ST_LAMP, 1);
        initSprite(36, 24.5, 13.5, 15, 1); initSprite(37, 25.5, 13.5, 15, 1); initSprite(38, 24.5, 14.5, 15, 1);
        initSprite(39, 28.5, 17.5, 15, 1); initSprite(40, 29.5, 18.5, 15, 1); initSprite(41, 26.5, 18.5, 15, 1);
        initSprite(42, 27.5, 12.5, 5, 1); initSprite(43, 23.5, 17.5, 5, 1); initSprite(44, 28.5, 14.5, 5, 1);
        initSprite(45, 25.5, 16.5, 17, 1); initSprite(46, 23.5, 12.5, 9, 1); initSprite(47, 29.5, 13.5, 9, 1);
        initSprite(48, 23.5, 19.5, 9, 1); initSprite(49, 29.5, 19.5, 43, 1); initSprite(50, 26.5, 12.5, ST_CORPSE, 1);
        initSprite(51, 24.5, 19.5, ST_SKULLS, 1); initSprite(52, 23.5, 16.5, ST_LAMP, 1); initSprite(53, 13.5, 5.5, 14, 1);
        initSprite(54, 18.5, 5.5, 14, 1); initSprite(55, 12.5, 6.5, 5, 1); initSprite(56, 19.5, 6.5, 5, 1);
        initSprite(57, 12.5, 7.5, 10, 1); initSprite(58, 19.5, 7.5, 9, 1); initSprite(59, 12.5, 3.5, ST_CBRA, 1);
        initSprite(60, 19.5, 3.5, ST_CBRA, 1); initSprite(61, 15.5, 6.5, ST_CORPSE, 1); initSprite(62, 16.5, 6.5, ST_SKULLS, 1);
        initSprite(63, 15.5, 9.5, ST_LAMP, 1); initSprite(64, 25.5, 9.5, 43, 1); initSprite(65, 26.5, 9.5, 10, 1);
        initSprite(66, 24.5, 9.5, 9, 1); initSprite(67, 24.5, 10.5, 5, -1); initSprite(68, 27.5, 10.5, 5, -1);
    } else if (level == 3) {
        px = 15.5; py = 28.5; dirX = 0; dirY = -1; planeX = 0.66; planeY = 0;
        // E3 INFERNO: narteks -> koridor -> tapinak halkasi + ic mabet. Anahtar
        // bati sapelinde, final apsiste. T(23,7) gizli hucreyi acar (PUSU BARONU).
        initSprite(0, 13.5, 28.5, ST_CORPSE, 1); initSprite(1, 12.5, 26.5, ST_LAMP, 1); initSprite(2, 19.5, 26.5, ST_LAMP, 1);
        initSprite(3, 12.5, 29.5, 9, 1); initSprite(4, 19.5, 29.5, 10, 1); initSprite(5, 17.5, 26.5, 5, 1);
        initSprite(6, 15.5, 23.5, ST_CBRA, 1); initSprite(7, 16.5, 21.5, ST_CBRA, 1); initSprite(8, 7.5, 9.5, 5, 1);
        initSprite(9, 24.5, 9.5, 5, 1); initSprite(10, 7.5, 18.5, 5, 1); initSprite(11, 24.5, 18.5, 5, 1);
        initSprite(12, 15.5, 9.5, 5, 1); initSprite(13, 9.5, 14.5, 17, 1); initSprite(14, 22.5, 12.5, 17, 1);
        initSprite(15, 15.5, 18.5, 17, 1); initSprite(16, 21.5, 9.5, 14, 1); initSprite(17, 6.5, 8.5, 15, 1);
        initSprite(18, 25.5, 8.5, 15, 1); initSprite(19, 6.5, 19.5, 15, 1); initSprite(20, 25.5, 19.5, 15, 1);
        initSprite(21, 10.5, 10.5, ST_LAMP, 1); initSprite(22, 21.5, 10.5, ST_LAMP, 1); initSprite(23, 10.5, 17.5, ST_LAMP, 1);
        initSprite(24, 21.5, 17.5, ST_LAMP, 1); initSprite(25, 6.5, 13.5, 9, 1); initSprite(26, 25.5, 13.5, 9, 1);
        initSprite(27, 8.5, 11.5, ST_CORPSE, 1); initSprite(28, 23.5, 16.5, ST_CORPSE, 1); initSprite(29, 12.5, 18.5, ST_CORPSE, 1);
        initSprite(30, 6.5, 9.5, ST_SKULLS, 1); initSprite(31, 25.5, 18.5, ST_SKULLS, 1); initSprite(32, 14.5, 13.5, 14, 1);
        initSprite(33, 17.5, 13.5, 14, 1); initSprite(34, 15.5, 14.5, 43, 1); initSprite(35, 12.5, 12.5, ST_CBRA, 1);
        initSprite(36, 19.5, 12.5, ST_CBRA, 1); initSprite(37, 16.5, 14.5, ST_SKULLS, 1); initSprite(38, 12.5, 15.5, 10, 1);
        initSprite(39, 19.5, 15.5, 9, 1); initSprite(40, 2.5, 13.5, 11, 1); initSprite(41, 3.5, 11.5, 17, 1);
        initSprite(42, 3.5, 15.5, 17, 1); initSprite(43, 2.5, 10.5, ST_CBRA, 1); initSprite(44, 2.5, 16.5, 10, 1);
        initSprite(45, 3.5, 13.5, ST_CORPSE, 1); initSprite(46, 28.5, 11.5, 5, 1); initSprite(47, 28.5, 16.5, 5, 1);
        initSprite(48, 29.5, 14.5, 17, 1); initSprite(49, 27.5, 10.5, 9, 1); initSprite(50, 30.5, 17.5, 10, 1);
        initSprite(51, 27.5, 17.5, 15, 1); initSprite(52, 30.5, 10.5, ST_LAMP, 1); initSprite(53, 29.5, 10.5, ST_SKULLS, 1);
        initSprite(54, 29.5, 7.5, 43, 1); initSprite(55, 28.5, 7.5, 9, 1); initSprite(56, 30.5, 7.5, 10, 1);
        initSprite(57, 29.5, 6.5, 14, -1); initSprite(58, 28.5, 6.5, ST_SKULLS, 1); initSprite(59, 12.5, 4.5, 14, 1);
        initSprite(60, 19.5, 4.5, 14, 1); initSprite(61, 11.5, 5.5, 5, 1); initSprite(62, 20.5, 5.5, 5, 1);
        initSprite(63, 15.5, 5.5, 5, 1); initSprite(64, 16.5, 3.5, 5, 1); initSprite(65, 10.5, 3.5, 15, 1);
        initSprite(66, 21.5, 3.5, 15, 1); initSprite(67, 10.5, 6.5, 10, 1); initSprite(68, 21.5, 6.5, 9, 1);
        initSprite(69, 13.5, 3.5, ST_CBRA, 1); initSprite(70, 18.5, 3.5, ST_CBRA, 1);
    }

    levelItemTotal = countLevelItemsLeft();   // intermission ITEMS x/y payı
    levelPlayMs = 0;
}

// ════════════════════════════════════════════════════════════
//  snakeReset — Yılan oyununu başlangıç durumuna getir
// ════════════════════════════════════════════════════════════
void snakeReset() {
    snakeLen = 3;
    snakeDirX = 1; snakeDirY = 0;
    snakeScore = 0; snakeDead = false; snakeDrawn = false;
    for (int i = 0; i < snakeLen; i++) {
        snakeBody[i].x = 5 - i;
        snakeBody[i].y = 5;
    }
    snakeFoodX = (millis() % (SNAKE_W - 2)) + 1;
    snakeFoodY = (millis() % (SNAKE_H - 2)) + 1;
}

// ════════════════════════════════════════════════════════════
//  snakeUpdate — Yılan mantığı (hareket, çarpışma, yeme)
// ════════════════════════════════════════════════════════════
void snakeUpdate(uint32_t now) {
    if (snakeDead) return;

    uint32_t speed = max((uint32_t)80, (uint32_t)(200 - snakeLen * 2));
    if (now - snakeLastMove < speed) return;
    snakeLastMove = now;

    const int CS = 5;
    tft.fillRect(snakeBody[snakeLen - 1].x * CS, snakeBody[snakeLen - 1].y * CS, CS - 1, CS - 1, COL_SNAKE_BG);

    for (int i = snakeLen - 1; i > 0; i--) snakeBody[i] = snakeBody[i - 1];

    snakeBody[0].x += snakeDirX;
    snakeBody[0].y += snakeDirY;

    if (snakeBody[0].x < 0 || snakeBody[0].x >= SNAKE_W ||
        snakeBody[0].y < 0 || snakeBody[0].y >= SNAKE_H) {
        snakeDead = true; playSound(NOTE_E3, 150); return;
    }

    for (int i = 1; i < snakeLen; i++) {
        if (snakeBody[0].x == snakeBody[i].x && snakeBody[0].y == snakeBody[i].y) {
            snakeDead = true; playSound(NOTE_E3, 150); return;
        }
    }

    if (snakeBody[0].x == snakeFoodX && snakeBody[0].y == snakeFoodY) {
        if (snakeLen < SNAKE_MAX) snakeLen++;
        snakeScore += 10;
        playSound(NOTE_E5, 40);
        do {
            snakeFoodX = (millis() % (SNAKE_W - 2)) + 1;
            snakeFoodY = (millis() % (SNAKE_H - 2)) + 1;
        } while ([&](){
            for (int i = 0; i < snakeLen; i++)
                if (snakeBody[i].x == snakeFoodX && snakeBody[i].y == snakeFoodY) return true;
            return false;
        }());
        tft.fillRect(snakeFoodX * CS, snakeFoodY * CS, CS - 1, CS - 1, COL_SNAKE_FOOD);
    }
}

// ════════════════════════════════════════════════════════════
//  TaskRadar — OLED mini harita (Core 1, düşük öncelik)
//  Dirty flag: sadece harita/dusman/oyuncu degistiginde sendBuffer
// ════════════════════════════════════════════════════════════
void TaskRadar(void * pvParameters) {
    oled.begin();
    Wire.setClock(400000);
    uint8_t r_lastMap[MH][MW] = {{0}};
    float r_lastPx = -1, r_lastPy = -1, r_lastDX = 0, r_lastDY = 0;
    Sprite r_lastSprites[NUM_SPRITES] = {};

    for (;;) {
        if (gameState != STATE_PLAYING) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        oled.setPowerSave(0);

        xSemaphoreTake(gameMutex, portMAX_DELAY);
        float r_px = px, r_py = py, r_dirX = dirX, r_dirY = dirY;
        uint8_t r_map[MH][MW];
        memcpy(r_map, MAP, sizeof(MAP));
        Sprite r_sprites[NUM_SPRITES];
        memcpy(r_sprites, sprites, sizeof(sprites));

        // Dirty kontrol: harita/oyuncu/sprites degisti mi?
        bool dirty = (memcmp(r_lastMap, r_map, sizeof(r_map)) != 0);
        if (!dirty) {
            dirty = (fabsf(r_lastPx - r_px) > 0.3f || fabsf(r_lastPy - r_py) > 0.3f ||
                     fabsf(r_lastDX - r_dirX) > 0.1f || fabsf(r_lastDY - r_dirY) > 0.1f);
        }
        if (!dirty) {
            dirty = (memcmp(r_lastSprites, r_sprites, sizeof(r_sprites)) != 0);
        }
        if (!dirty) {
            xSemaphoreGive(gameMutex);
            vTaskDelay(pdMS_TO_TICKS(80));
            continue;
        }
        memcpy(r_lastMap, r_map, sizeof(r_map));
        r_lastPx = r_px; r_lastPy = r_py;
        r_lastDX = r_dirX; r_lastDY = r_dirY;
        memcpy(r_lastSprites, r_sprites, sizeof(r_sprites));
        xSemaphoreGive(gameMutex);

        oled.clearBuffer();

        int bs = 2, ox = 32;

        for (int y = 0; y < MH; y++) {
            for (int x = 0; x < MW; x++) {
                if (r_map[y][x] > 0 && r_map[y][x] <= 5)
                    oled.drawBox(ox + x * bs, y * bs, bs - 1, bs - 1);
                else if (r_map[y][x] >= 6 && r_map[y][x] <= 9)
                    oled.drawFrame(ox + x * bs, y * bs, bs - 1, bs - 1);
                else if (r_map[y][x] == 31)
                    oled.drawFrame(ox + x * bs, y * bs, bs - 1, bs - 1);
            }
        }
        for (int i = 0; i < NUM_SPRITES; i++) {
            if (r_sprites[i].state >= 1 && r_sprites[i].animState != ANIM_DEAD) {
                if (isMonsterType(r_sprites[i].type))
                    oled.drawBox(ox + (int)r_sprites[i].x * bs, (int)r_sprites[i].y * bs, 2, 2);
                else if (!isDecorType(r_sprites[i].type))   // dekor radarı kirletmesin
                    oled.drawPixel(ox + (int)r_sprites[i].x * bs + 1, (int)r_sprites[i].y * bs + 1);
            }
        }
        int rx = ox + r_px * bs, ry = r_py * bs;
        oled.drawDisc(rx, ry, 1);
        oled.drawLine(rx, ry, rx + r_dirX * 3, ry + r_dirY * 3);
        oled.sendBuffer();
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}

// ════════════════════════════════════════════════════════════
//  TaskEngine — Ana oyun motoru (Core 0, IRAM_ATTR)
//  Menü, duraklatma ve oyun döngüsünü yürütür.
// ════════════════════════════════════════════════════════════
void IRAM_ATTR TaskEngine(void * pvParameters) {
    bool joySw_prev = true;
    for (;;) {
        uint32_t current_ms = millis();

        // ==========================================
        // DOOM İÇ MENÜSÜ (STATE_MENU)
        // ==========================================
        if (gameState == STATE_MENU) {
            int jy_menu = joyRawY - joyCenterY;
            static uint32_t lastMenuMove = 0;
            uint32_t mnow = millis();

            if (!inLevelSelect) {
                if (abs(jy_menu) > 500 && (mnow - lastMenuMove > 250)) {
                    menuSelection = (jy_menu < 0) ? 0 : 1;
                    lastMenuMove = mnow;
                    playSound(NOTE_F4, 25);
                }

                if (!menuDrawn) {
                    if (titlePicBuf) tft.pushImage(0, 0, 160, 128, titlePicBuf);
                    else { tft.fillScreen(TFT_BLACK); }

                    for (int y = 0; y < 28; y++) {
                        if (y > 10) {
                            uint8_t a8 = min((uint32_t)255, (uint32_t)(y - 10) * 20);
                            uint16_t darkRow = tft.color565(
                                (uint8_t)(10 * (255 - a8) / 255), 0, 0);
                            tft.drawFastHLine(0, 100 + y, 160, darkRow);
                        }
                    }
                    tft.fillRect(0, 110, 160, 18, tft.color565(0, 0, 0));
                    tft.drawFastHLine(0, 100, 160, COL_RGB(180, 0, 0));
                    tft.drawFastHLine(0, 101, 160, COL_RGB(80, 0, 0));
                    menuDrawn = true;
                }

                tft.fillRect(0, 102, 160, 26, tft.color565(0, 0, 0));

                {
                    bool sel = (menuSelection == 0);
                    tft.setTextSize(1);
                    tft.setCursor(47, 105);
                    if (sel) {
                        uint8_t pulse = (uint8_t)(128 + 127 * sin(mnow * 0.008f));
                        tft.setTextColor(tft.color565(255, pulse / 2, 0));
                        tft.print("> ");
                        tft.setTextColor(COL_RGB(255, 200, 0));
                        tft.print("NEW GAME");
                    } else {
                        tft.setTextColor(TFT_WHITE);
                        tft.print("  NEW GAME");
                    }
                }
                {
                    bool sel = (menuSelection == 1);
                    tft.setTextSize(1);
                    tft.setCursor(44, 118);
                    if (sel) {
                        uint8_t pulse = (uint8_t)(128 + 127 * sin(mnow * 0.008f));
                        tft.setTextColor(tft.color565(255, pulse / 2, 0));
                        tft.print("> ");
                        tft.setTextColor(COL_RGB(255, 200, 0));
                        tft.print("SELECT LVL");
                    } else {
                        tft.setTextColor(TFT_WHITE);
                        tft.print("  SELECT LVL");
                    }
                }

                if (!digitalRead(BTN_A)) {
                    playSound(NOTE_E5, 50);
                    delay(200);
                    if (menuSelection == 0) {
                        currentLevel = 1;
                        hp = 100; ammo = 75; armor = 0; hasKey = false; weaponType = 0;
                        totalKills = 0; totalKillTotal = 0; totalTimeMs = 0;
                        loadLevel(1);
                        tft.fillScreen(TFT_BLACK);
                        drawStaticHUD();
                        lastFrame = millis(); fpsTimer = millis();
                        gameState = STATE_PLAYING;
                        menuDrawn = false;
                    } else {
                        inLevelSelect = true;
                        levelSelectIdx = 0;
                        menuDrawn = false;
                    }
                }

                if (!digitalRead(BTN_B)) {
                    playSound(NOTE_G4, 50);
                    delay(200);
                    returnToOS();
                }
            } else {
                if (abs(jy_menu) > 500 && (mnow - lastMenuMove > 250)) {
                    if (jy_menu < 0) { levelSelectIdx--; if (levelSelectIdx < 0) levelSelectIdx = 2; }
                    else             { levelSelectIdx++; if (levelSelectIdx > 2) levelSelectIdx = 0; }
                    lastMenuMove = mnow;
                    playSound(NOTE_F4, 25);
                }

                if (!menuDrawn) {
                    if (titlePicBuf) {
                        uint16_t* darkBuf = (uint16_t*)heap_caps_malloc(160 * 128 * 2, MALLOC_CAP_SPIRAM);
                        if (darkBuf) {
                            for (int py = 0; py < 128; py++) {
                                for (int px2 = 0; px2 < 160; px2++) {
                                    uint16_t c = titlePicBuf[py * 160 + px2];
                                    uint8_t r = ((c >> 11) & 0x1F) * 35 / 100;
                                    uint8_t g = ((c >> 5)  & 0x3F) * 35 / 100;
                                    uint8_t b = ( c        & 0x1F) * 35 / 100;
                                    darkBuf[py * 160 + px2] = (r << 11) | (g << 5) | b;
                                }
                            }
                            tft.pushImage(0, 0, 160, 128, darkBuf);
                            free(darkBuf);
                        } else {
                            tft.fillScreen(tft.color565(8, 0, 0));
                        }
                    } else {
                        tft.fillScreen(tft.color565(8, 0, 0));
                    }
                    menuDrawn = true;
                }

                const struct { const char* ep; const char* name; } levels[3] = {
                    { "EP.1", "PHOBOS" },
                    { "EP.2", "DEIMOS" },
                    { "EP.3", "INFERNO" },
                };

                for (int i = 0; i < 3; i++) {
                    int yRow = 48 + i * 17;
                    bool sel = (i == levelSelectIdx);

                    if (sel) {
                        tft.fillRect(14, yRow - 1, 132, 15, tft.color565(30, 5, 0));
                        tft.drawFastHLine(14, yRow - 1,  132, COL_RGB(120, 30, 0));
                        tft.drawFastHLine(14, yRow + 13, 132, COL_RGB(80, 15, 0));
                    } else {
                        tft.fillRect(14, yRow - 1, 132, 15, tft.color565(5, 0, 0));
                    }

                    tft.setTextSize(1);
                    tft.setCursor(17, yRow + 2);
                    if (sel) {
                        uint8_t blink = ((mnow / 300) % 2) ? 255 : 180;
                        tft.setTextColor(tft.color565(blink, blink / 3, 0));
                        tft.print("> ");
                    } else {
                        tft.setTextColor(tft.color565(40, 8, 0));
                        tft.print("  ");
                    }

                    if (sel) tft.setTextColor(COL_MENU_TITLE);
                    else    tft.setTextColor(tft.color565(50, 10, 0));
                    tft.print(levels[i].ep);
                    tft.print(" ");

                    if (sel) tft.setTextColor(COL_RGB(255, 210, 0));
                    else    tft.setTextColor(tft.color565(80, 20, 0));
                    tft.print(levels[i].name);
                }

                if (!digitalRead(BTN_A)) {
                    playSound(NOTE_E5, 50);
                    delay(200);
                    currentLevel = levelSelectIdx + 1;
                    hp = 100; ammo = 75; armor = 0; hasKey = false; weaponType = 0;
                    totalKills = 0; totalKillTotal = 0; totalTimeMs = 0;
                    loadLevel(currentLevel);
                    tft.fillScreen(TFT_BLACK);
                    drawStaticHUD();
                    lastFrame = millis(); fpsTimer = millis();
                    gameState = STATE_PLAYING;
                    menuDrawn = false;
                    inLevelSelect = false;
                }
                if (!digitalRead(BTN_B)) {
                    playSound(NOTE_G4, 50);
                    delay(200);
                    inLevelSelect = false;
                    menuDrawn = false;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(30));
            continue;
        }

        // ==========================================
        // DOOM PAUSE MENÜSÜ (STATE_PAUSE)
        // ==========================================
        if (gameState == STATE_PAUSE) {
            if (!menuDrawn) {
                vTaskDelay(pdMS_TO_TICKS(100));
                // Ortak OS pause kutusu (doom kirmizi temasi). menuDrawn ile
                // sadece bir kez cizilir → pause sirasinda render durur, FPS'e etki yok.
                osDrawPause(tft, COL_RGB(255, 50, 50));
                menuDrawn = true;
            }

            if (!digitalRead(BTN_A)) {
                playSound(NOTE_D5, 40);
                delay(200);
                gameState = STATE_PLAYING;
                lastFrame = millis();
            }
            if (!digitalRead(BTN_B)) {
                playSound(NOTE_G4, 50);
                delay(200);
                gameState = STATE_MENU;
                menuDrawn = false;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // ==========================================
        // OYUN DURUMU (STATE_PLAYING)
        // ==========================================
        uint32_t now = millis();
        float dt = (now - lastFrame) / 1000.0f;
        lastFrame = now;
        if (dt > 0.1f) dt = 0.1f;
        levelPlayMs += (uint32_t)(dt * 1000.0f);   // intermission TIME (pause hariç)

        xSemaphoreTake(gameMutex, portMAX_DELAY);

        bool joySw_curr = digitalRead(JOY_SW);
        if (joySw_prev && !joySw_curr) {
            gameState = STATE_PAUSE;
            menuDrawn = false;
            joySw_prev = joySw_curr;
            xSemaphoreGive(gameMutex);
            playSound(NOTE_G4, 50);
            continue;
        }
        joySw_prev = joySw_curr;

        updateAnimations(now);

        if (hp <= 0) {
            xSemaphoreGive(gameMutex);

            gameState = STATE_BOOT;   // oyun mantigini dondur
            vTaskDelay(pdMS_TO_TICKS(100));

            // Doom'a özel game-over ekranı (mavi renk ve boşluk hatalarını çözer)
            tft.fillScreen(COL_NEARBLACK);
            tft.drawRect(15, 15, 130, 98, COL_DARKRED);

            tft.setTextSize(2);
            tft.setTextColor(COL_GAMEOVER);
            tft.setCursor(80 - tft.textWidth("GAME OVER") / 2, 35);
            tft.print("GAME OVER");

            tft.setTextSize(1);
            tft.setTextColor(COL_WHITE);
            {
                char gbuf[20];
                snprintf(gbuf, sizeof(gbuf), "KILLS %d/%d", countLevelKills(), countLevelMonsters());
                tft.setCursor(80 - tft.textWidth(gbuf) / 2, 53);
                tft.print(gbuf);
            }
            int16_t menuX = 80 - tft.textWidth("[A] Play Again") / 2;
            tft.setCursor(menuX, 65);
            tft.print("[A] Play Again");
            tft.setCursor(menuX, 80);
            tft.print("[B] OS Menu");
            playSound(NOTE_E3, 150);

            // Olum anindaki A basili kalintisini yut: once birakilmasini bekle.
            vTaskDelay(pdMS_TO_TICKS(1500));
            while (!digitalRead(BTN_A)) { vTaskDelay(pdMS_TO_TICKS(50)); }

            while (1) {
                if (!digitalRead(BTN_A)) {          // [A] Play Again → ayni seviyeyi bastan
                    playSound(NOTE_D5, 50);
                    delay(300);
                    hp = 100; ammo = 75; armor = 0; hasKey = false; weaponType = 0;
                    loadLevel(currentLevel);
                    tft.fillScreen(TFT_BLACK);      // game-over kutusunu tamamen sil
                    drawStaticHUD();                // HUD cercevesini yeniden ciz
                    lastFrame = millis(); fpsTimer = millis();
                    gameState = STATE_PLAYING;
                    menuDrawn = false;
                    break;
                }
                if (!digitalRead(BTN_B)) {          // [B] OS Menu → launcher'a don
                    returnToOS();
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }

        updateShieldState(now);
        dusukCanKalpAtisi(now);
        updateMovement(dt);
        updateDoorAnims(dt);
        handleUseAction(now, dt);

        // Çıkış kapısı kullanıldıysa frame'in kalanı atlanır: çıkışa ulaşan
        // oyuncu aynı karede artık hasar alamaz.
        if (levelDone) {
            levelDone = false;
            xSemaphoreGive(gameMutex);

            gameState = STATE_BOOT;   // oyun mantigini dondur (game-over kalibi)
            vTaskDelay(pdMS_TO_TICKS(100));

            // Switch'in basılı hali kısa süre gösterilir (orijinal Doom'daki
            // switch flip). Oyun donduğu için state güvenli; TaskDisplay
            // STATE_BOOT'ta uyuduğundan pushImage'ı burada yapıyoruz.
            {
                uint16_t* flipFB = fb[fb_render];
                renderWalls(flipFB, 0);
                renderSprites(flipFB, 0);
                renderWeapon(flipFB, 0, millis());
                tft.pushImage(0, 0, SW, SH, flipFB);
                playSound(NOTE_B3, 60);   // switch klik
                vTaskDelay(pdMS_TO_TICKS(450));

                // Asansör bitişi: kanatlar kapanır + motor gürültüsü,
                // seviye asansörle terk edilir, sonra intermission gelir.
                int prevW = 0;
                for (int s = 1; s <= 8; s++) {
                    int curW = (SW / 2) * s / 8;
                    for (int y = 0; y < SH; y++)
                        for (int x = prevW; x < curW; x++) {
                            flipFB[y * SW + x] = COL_NEARBLACK;
                            flipFB[y * SW + (SW - 1 - x)] = COL_NEARBLACK;
                        }
                    prevW = curW;
                    tft.pushImage(0, 0, SW, SH, flipFB);
                    if (s & 1) playSound(NOTE_E3, 40);
                    vTaskDelay(pdMS_TO_TICKS(45));
                }
                playSound(NOTE_G3, 160); vTaskDelay(pdMS_TO_TICKS(200));
                playSound(NOTE_E3, 160); vTaskDelay(pdMS_TO_TICKS(300));
            }

            // İstatistikler sprite dizisinden türetilir (loadLevel henüz çağrılmadı)
            int kills = countLevelKills();
            int killTotal = countLevelMonsters();
            int items = levelItemTotal - countLevelItemsLeft();
            uint32_t tsec = levelPlayMs / 1000;
            totalKills += kills; totalKillTotal += killTotal;
            totalTimeMs += levelPlayMs;
            bool victory = (currentLevel == 3);

            char buf[24];
            tft.fillScreen(COL_NEARBLACK);
            tft.drawRect(15, 15, 130, 98, COL_DARKRED);

            if (victory) {
                tft.setTextSize(2);
                tft.setTextColor(COL_RGB(255, 210, 0));
                tft.setCursor(80 - tft.textWidth("YOU WIN!") / 2, 26);
                tft.print("YOU WIN!");

                tft.setTextSize(1);
                tft.setTextColor(COL_WHITE);
                uint32_t vsec = totalTimeMs / 1000;
                snprintf(buf, sizeof(buf), "KILLS %d/%d", totalKills, totalKillTotal);
                tft.setCursor(80 - tft.textWidth(buf) / 2, 52);
                tft.print(buf);
                snprintf(buf, sizeof(buf), "TIME %u:%02u", (unsigned)(vsec / 60), (unsigned)(vsec % 60));
                tft.setCursor(80 - tft.textWidth(buf) / 2, 64);
                tft.print(buf);

                int16_t menuX = 80 - tft.textWidth("[A] Play Again") / 2;
                tft.setCursor(menuX, 84); tft.print("[A] Play Again");
                tft.setCursor(menuX, 97); tft.print("[B] OS Menu");

                // Zafer fanfarı (platform ses tabanı >= NOTE_E3/165Hz)
                playSound(NOTE_E4, 90);  vTaskDelay(pdMS_TO_TICKS(110));
                playSound(NOTE_A4, 90);  vTaskDelay(pdMS_TO_TICKS(110));
                playSound(NOTE_C5, 90);  vTaskDelay(pdMS_TO_TICKS(110));
                playSound(NOTE_E5, 200);
            } else {
                tft.setTextSize(1);
                tft.setTextColor(COL_AMMO_VAL);
                const char* epname = (currentLevel == 1) ? "EP.1 PHOBOS" : "EP.2 DEIMOS";
                tft.setCursor(80 - tft.textWidth(epname) / 2, 24);
                tft.print(epname);

                tft.setTextSize(2);
                tft.setTextColor(COL_GAMEOVER);
                tft.setCursor(80 - tft.textWidth("FINISHED") / 2, 36);
                tft.print("FINISHED");

                tft.setTextSize(1);
                tft.setTextColor(COL_WHITE);
                tft.setCursor(38, 62); tft.print("KILLS");
                snprintf(buf, sizeof(buf), "%d/%d", kills, killTotal);
                tft.setCursor(122 - tft.textWidth(buf), 62); tft.print(buf);

                tft.setCursor(38, 74); tft.print("ITEMS");
                snprintf(buf, sizeof(buf), "%d/%d", items, levelItemTotal);
                tft.setCursor(122 - tft.textWidth(buf), 74); tft.print(buf);

                tft.setCursor(38, 86); tft.print("TIME");
                snprintf(buf, sizeof(buf), "%u:%02u", (unsigned)(tsec / 60), (unsigned)(tsec % 60));
                tft.setCursor(122 - tft.textWidth(buf), 86); tft.print(buf);

                tft.setTextColor(COL_AMMO_VAL);
                tft.setCursor(80 - tft.textWidth("[A] CONTINUE") / 2, 101);
                tft.print("[A] CONTINUE");

                playSound(NOTE_C5, 70);
                vTaskDelay(pdMS_TO_TICKS(90));
                playSound(NOTE_E5, 120);
            }

            // Kapıyı açarken basılı kalan B'yi (ve olası A'yı) yut.
            vTaskDelay(pdMS_TO_TICKS(victory ? 800 : 400));
            while (!digitalRead(BTN_A) || !digitalRead(BTN_B)) { vTaskDelay(pdMS_TO_TICKS(50)); }

            while (1) {
                if (!digitalRead(BTN_A)) {
                    playSound(NOTE_D5, 50);
                    delay(200);
                    if (victory) {      // [A] Play Again → NEW GAME gibi baştan
                        totalKills = 0; totalKillTotal = 0; totalTimeMs = 0;
                        currentLevel = 1;
                        hp = 100; ammo = 75; armor = 0; hasKey = false; weaponType = 0;
                    } else {            // [A] CONTINUE → hp/ammo sonraki seviyeye taşınır
                        currentLevel++;
                    }
                    loadLevel(currentLevel);
                    tft.fillScreen(TFT_BLACK);
                    drawStaticHUD();
                    lastFrame = millis(); fpsTimer = millis();
                    gameState = STATE_PLAYING;
                    menuDrawn = false;
                    break;
                }
                if (victory && !digitalRead(BTN_B)) {   // [B] OS Menu → launcher
                    returnToOS();
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }

        // Asansör kullanıldıysa kabin sekansı: kanatlar kapanır, motor
        // gürültüsü, haritadaki diğer asansörün önüne ışınlanma, kanatlar
        // açılır. Oyun exit-flip kalıbıyla dondurulur (STATE_BOOT →
        // TaskDisplay/TaskRadar uyur, pushImage burada yapılır).
        if (elevatorPending) {
            elevatorPending = false;
            int destX = -1, destY = -1;
            for (int y = 0; y < MH && destX < 0; y++)
                for (int x = 0; x < MW; x++)
                    if (MAP[y][x] == 9 && !(x == elevSrcX && y == elevSrcY)) { destX = x; destY = y; break; }
            int ax = -1, ay = -1;
            if (destX >= 0) {
                const int8_t NBX[4] = {0, 0, -1, 1}, NBY[4] = {-1, 1, 0, 0};
                for (int k = 0; k < 4; k++) {
                    int cx = destX + NBX[k], cy = destY + NBY[k];
                    if (cx >= 0 && cx < MW && cy >= 0 && cy < MH && MAP[cy][cx] == 0) { ax = cx; ay = cy; break; }
                }
            }
            if (ax >= 0) {
                xSemaphoreGive(gameMutex);
                gameState = STATE_BOOT;
                vTaskDelay(pdMS_TO_TICKS(100));

                // Kapanış: mevcut sahnenin üstüne iki kanat ortada birleşir
                uint16_t* efb = fb[fb_render];
                renderWalls(efb, 0);
                renderSprites(efb, 0);
                renderWeapon(efb, 0, millis());
                int prevW = 0;
                for (int s = 1; s <= 8; s++) {
                    int curW = (SW / 2) * s / 8;
                    for (int y = 0; y < SH; y++)
                        for (int x = prevW; x < curW; x++) {
                            efb[y * SW + x] = COL_NEARBLACK;
                            efb[y * SW + (SW - 1 - x)] = COL_NEARBLACK;
                        }
                    prevW = curW;
                    tft.pushImage(0, 0, SW, SH, efb);
                    if (s & 1) playSound(NOTE_E3, 40);   // kanat takırtısı
                    vTaskDelay(pdMS_TO_TICKS(45));
                }

                // Kabin hareket halinde: karanlıkta motor gürültüsü
                playSound(NOTE_G3, 160); vTaskDelay(pdMS_TO_TICKS(200));
                playSound(NOTE_E3, 160); vTaskDelay(pdMS_TO_TICKS(200));
                playSound(NOTE_G3, 160); vTaskDelay(pdMS_TO_TICKS(240));

                // Işınlanma: hedef asansörün önündeki boş hücre, kabinden dışarı bakar
                px = ax + 0.5f; py = ay + 0.5f;
                float ndx = (float)(ax - destX), ndy = (float)(ay - destY);
                dirX = ndx; dirY = ndy;
                planeX = -ndy * 0.66f; planeY = ndx * 0.66f;

                playSound(NOTE_C5, 50);                  // varış zili
                for (int s = 8; s >= 0; s--) {           // açılış (sahne her adım tazelenir)
                    int curW = (SW / 2) * s / 8;
                    renderWalls(efb, 0);
                    renderSprites(efb, 0);
                    renderWeapon(efb, 0, millis());
                    for (int y = 0; y < SH; y++)
                        for (int x = 0; x < curW; x++) {
                            efb[y * SW + x] = COL_NEARBLACK;
                            efb[y * SW + (SW - 1 - x)] = COL_NEARBLACK;
                        }
                    tft.pushImage(0, 0, SW, SH, efb);
                    vTaskDelay(pdMS_TO_TICKS(45));
                }

                // B kalıntısı varış asansörünü tetiklemesin
                while (!digitalRead(BTN_B)) { vTaskDelay(pdMS_TO_TICKS(30)); }
                sonKullanma = millis();
                lastFrame = millis();
                gameState = STATE_PLAYING;
                continue;
            }
            // Eşi olmayan/önü kapalı asansör: sessizce yok sayılır
        }

        bool isParrying = lastShieldState && (now - shieldStartTime < 300);
        updateAllEnemies(dt, now, isParrying);
        handleWeaponSwitch(now);
        handleShooting(now);

        xSemaphoreGive(gameMutex);

        // ===== RENDERING =====
        uint16_t* activeFB = fb[fb_render];
        static uint32_t lcg = 12345;
        lcg = lcg * 1103515245 + 12345;
        int pitch = (now - lastDamageTime < 200 && !lastShieldState) ? (int)(lcg % 13) - 6 : 0;

        renderWalls(activeFB, pitch);
        renderSprites(activeFB, pitch);
        renderWeapon(activeFB, pitch, now);
        renderDamageEffect(activeFB, now);

        if (xSemaphoreTake(fb_swap_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            int8_t old_ready = fb_ready;
            fb_ready = fb_render;
            if (old_ready >= 0) {
                fb_render = old_ready;
            } else {
                fb_render = 3 - fb_render - fb_display;
                if (fb_render == fb_display) fb_render = (fb_display + 1) % 3;
            }
            xSemaphoreGive(fb_swap_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ════════════════════════════════════════════════════════════
//  TaskDisplay — Framebuffer'ı TFT'ye basar (Core 1)
//  checkScreenshotFB her frame'de pushImage ÖNCESİ çağrılır.
// ════════════════════════════════════════════════════════════
void IRAM_ATTR TaskDisplay(void * pvParameters) {
    for (;;) {
        if (gameState != STATE_PLAYING) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (fb_ready >= 0) {
            int8_t toDisplay = -1;
            if (xSemaphoreTake(fb_swap_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
                if (fb_ready >= 0) {
                    fb_display = fb_ready;
                    fb_ready = -1;
                    toDisplay = fb_display;
                }
                xSemaphoreGive(fb_swap_mutex);
            }
            if (toDisplay >= 0) {
                checkScreenshotFB(fb[toDisplay], SW, SH);

                tft.pushImage(0, 0, SW, SH, fb[toDisplay]);

                uint32_t now = millis();
                int hy = SH;
                bool currentInfState = (lastShieldState || (now - meleeTimer < 300));
                if (currentInfState != lastInfState || ammo != lastAmmo) {
                    tft.setTextColor(COL_AMMO_VAL, COL_HUDBG);
                    if (currentInfState) {
                        tft.setCursor(15, hy + 14); tft.print("INF");
                    } else {
                        tft.setCursor(15, hy + 14); tft.printf("%03d", ammo);
                    }
                    lastInfState = currentInfState; lastAmmo = ammo;
                }

                if (hp != lastHp) {
                    tft.setTextColor(hp > 25 ? COL_WHITE : COL_HP_LOW, COL_HUDBG);
                    tft.setCursor(68, hy + 14); tft.printf("%03d%%", hp);
                    lastHp = hp;
                }

                if (armor != lastArmor) {
                    tft.setTextColor(COL_WHITE, COL_HUDBG);
                    tft.setCursor(120, hy + 14); tft.printf("%03d%%", armor);
                    lastArmor = armor;
                }

                if (hasKey) tft.fillRect(145, hy + 13, 8, 8, COL_RGB(255, 255, 0));
                else        tft.fillRect(145, hy + 13, 8, 8, COL_HUDBG);

                frameCount++;
                if (now - fpsTimer >= 1000) {
                    fps = frameCount; frameCount = 0; fpsTimer = now;
                    if (showFps && fps != lastFps) {   // yalnizca ayar acikken ciz
                        tft.setTextColor(COL_FPS_VAL, COL_HUDBG);
                        tft.setCursor(142, hy + 3); tft.printf("%03d", fps);
                        lastFps = fps;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ════════════════════════════════════════════════════════════
//  TaskJoy — Joystick analog okuma (Core 1, 10ms periyot)
// ════════════════════════════════════════════════════════════
void TaskJoy(void * pvParameters) {
    for (;;) {
        joyRawX = analogRead(JOY_X);
        joyRawY = analogRead(JOY_Y);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ════════════════════════════════════════════════════════════
//  setup() — Donanım başlatma, PSRAM tahsisi, texture yükleme,
//            FreeRTOS task ataması
// ════════════════════════════════════════════════════════════
void setup() {
    // 1) Buzzer sustur (reset sonrasi cizirti onler)
    osInitBuzzer();

    Serial.begin(115200);

    pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS, OUTPUT);  digitalWrite(SD_CS, HIGH);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
    delay(50);

    sdReady = SD.begin(SD_CS, SPI, 40000000);
    initDevTools(tft);

    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }

    for (int i = 0; i < 3; i++) {
        fb[i] = (uint16_t*)heap_caps_malloc(160 * 128 * 2, MALLOC_CAP_SPIRAM);
        if (!fb[i]) {
            Serial.println("KRITIK HATA: FB PSRAM'a tahsis edilemedi!");
            fb[i] = (uint16_t*)malloc(160 * 128 * 2);
        }
        if (fb[i]) memset(fb[i], 0, 160 * 128 * 2);
    }
    fb_swap_mutex = xSemaphoreCreateMutex();

    gameMutex = xSemaphoreCreateMutex();

    // Buton pinleri ayarla
    osInitButtons();

    titlePicBuf = (uint16_t*)heap_caps_malloc(160 * 128 * 2, MALLOC_CAP_SPIRAM);
    if (!titlePicBuf) {
        titlePicBuf = (uint16_t*)malloc(160 * 128 * 2);
    }

    for (int i = 1; i < MAX_TEX; i++) {
        tex[i] = (uint16_t*)heap_caps_malloc(TEX_W * TEX_H * 2, MALLOC_CAP_SPIRAM);
        if (!tex[i]) tex[i] = (uint16_t*)malloc(TEX_W * TEX_H * 2);
        if (!tex[i]) {
            Serial.println("KRITIK HATA: Bellek yetersiz!");
            tft.fillScreen(TFT_RED);
            tft.setTextColor(TFT_WHITE);
            tft.setCursor(10, 50);
            tft.print("MEMORY ERROR! NO PSRAM");
            while (1) { vTaskDelay(pdMS_TO_TICKS(100)); }
        }
    }
    for (int i = 8; i <= 13; i++) makeTex(i);
    makeFlatTexes();   // zemin/tavan flat fallback (SD BMP'leri varsa üzerine yazılır)
    // Yeni slot fallback'leri: basılı switch yoksa çıkışın normal hali
    // kullanılır (flip görünmez ama oyun bozulmaz), dekor BMP'si yoksa
    // sprite tamamen chroma = görünmez.
    memcpy(tex[TEX_EXIT_ON], tex[8], TEX_W * TEX_H * 2);
    memcpy(tex[TEX_ELEV], tex[8], TEX_W * TEX_H * 2);
    for (int i = 0; i < TEX_W * TEX_H; i++) {
        tex[ST_LAMP][i]   = COL_CHROMA;
        tex[ST_PILLAR][i] = COL_CHROMA;
        tex[ST_CBRA][i]   = COL_CHROMA;
        tex[ST_SKULLS][i] = COL_CHROMA;
    }

    if (sdReady) {
        oled.begin();
        oled.clearBuffer();
        oled.setFont(u8g2_font_6x10_tr);
        oled.drawStr(4, 20, "Loading graphics...");
        oled.drawStr(10, 40, "Please wait...");
        oled.sendBuffer();

        uint8_t* fileBuf = (uint8_t*)heap_caps_malloc(80000, MALLOC_CAP_SPIRAM);
        if (!fileBuf) fileBuf = (uint8_t*)malloc(80000);

        if (fileBuf) {
            loadBMP("/doom/duvar1.bmp", 1, fileBuf);   loadBMP("/doom/duvar2.bmp", 2, fileBuf);
            loadBMP("/doom/duvar3.bmp", 3, fileBuf);   loadBMP("/doom/s_off.bmp", 4, fileBuf);
            loadBMP("/doom/s_on.bmp", 5, fileBuf);     loadBMP("/doom/kapi.bmp", 6, fileBuf);
            loadBMP("/doom/kilitli.bmp", 7, fileBuf);
            // Gizli duvar (31) normal taş duvar gibi görünür — kilitli kapı
            // dokusu gizliliği ele veriyordu; haritalarda 31'ler taş bölgelerde.
            loadBMP("/doom/duvar2.bmp", 31, fileBuf);
            loadBMP("/doom/zemin.bmp", TEX_FLOOR, fileBuf);
            loadBMP("/doom/tavan.bmp", TEX_CEIL, fileBuf);

            // Çıkış switch'i (yeşil kutu yerine Doom tarzı SW1/SW2 panel)
            if (loadBMP("/doom/cikis.bmp", 8, fileBuf)) {
                if (!loadBMP("/doom/cikis2.bmp", TEX_EXIT_ON, fileBuf))
                    memcpy(tex[TEX_EXIT_ON], tex[8], TEX_W * TEX_H * 2);
            }
            // Asansör kapı dokusu; yoksa normal kapı dokusu kullanılır
            if (!loadBMP("/doom/asansor.bmp", TEX_ELEV, fileBuf))
                memcpy(tex[TEX_ELEV], tex[6], TEX_W * TEX_H * 2);
            // Dekorasyon sprite'ları (ceset tex 33'ü yeniden kullanır)
            loadBMP("/doom/lamba.bmp", ST_LAMP, fileBuf);
            loadBMP("/doom/sutun.bmp", ST_PILLAR, fileBuf);
            loadBMP("/doom/samdan.bmp", ST_CBRA, fileBuf);
            loadBMP("/doom/kafatasi.bmp", ST_SKULLS, fileBuf);

            loadBMP("/doom/mermi.bmp", 9, fileBuf);    loadBMP("/doom/can.bmp", 10, fileBuf);
            loadBMP("/doom/anahtar.bmp", 11, fileBuf); loadBMP("/doom/armor.bmp", 43, fileBuf);

            loadBMP("/doom/t_bekle.bmp", 14, fileBuf); loadBMP("/doom/t_ates.bmp", 15, fileBuf);
            loadBMP("/doom/p_bekle.bmp", 16, fileBuf); loadBMP("/doom/p_ates.bmp", 17, fileBuf);
            loadBMP("/doom/p_cek1.bmp", 47, fileBuf);  loadBMP("/doom/p_cek2.bmp", 48, fileBuf);

            loadBMP("/doom/k_dur.bmp", 18, fileBuf);   loadBMP("/doom/k_vur.bmp", 19, fileBuf);
            loadBMP("/doom/k_sektir.bmp", 20, fileBuf);loadBMP("/doom/y_vur.bmp", 21, fileBuf);

            loadBMP("/doom/z_dur.bmp", 22, fileBuf);   loadBMP("/doom/z_yuru.bmp", 23, fileBuf);
            loadBMP("/doom/z_ates.bmp", 24, fileBuf);  loadBMP("/doom/z_dus.bmp", 25, fileBuf);
            loadBMP("/doom/z_ceset.bmp", 26, fileBuf);

            loadBMP("/doom/p_dur.bmp", 27, fileBuf);   loadBMP("/doom/p_yuru1.bmp", 28, fileBuf);
            loadBMP("/doom/p_yuru2.bmp", 29, fileBuf); loadBMP("/doom/p_isir.bmp", 30, fileBuf);
            loadBMP("/doom/p_isir2.bmp", 44, fileBuf); loadBMP("/doom/p_dus.bmp", 32, fileBuf);
            loadBMP("/doom/p_ceset.bmp", 33, fileBuf);

            loadBMP("/doom/b_dur.bmp", 34, fileBuf);   loadBMP("/doom/b_yuru.bmp", 35, fileBuf);
            loadBMP("/doom/b_ates.bmp", 36, fileBuf);  loadBMP("/doom/b_irkil.bmp", 37, fileBuf);
            loadBMP("/doom/b_dus.bmp", 38, fileBuf);   loadBMP("/doom/b_ceset.bmp", 39, fileBuf);

            loadBMP("/doom/v_dur.bmp", 40, fileBuf);   loadBMP("/doom/v_patla.bmp", 41, fileBuf);
            loadBMP("/doom/v_duman.bmp", 42, fileBuf);

            loadTitlePic(fileBuf);
            free(fileBuf);
        }

        SD.end();
    } else {
        oled.begin();
        oled.clearBuffer();
        oled.setFont(u8g2_font_6x10_tr);
        oled.drawStr(7, 30, "SD CARD NOT FOUND!");
        oled.sendBuffer();
        delay(2000);
    }

    digitalWrite(SD_CS, HIGH); SPI.end(); delay(50);
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    for (int i = 0; i < SW; i++) camXTable[i] = 2.0f * i / SW - 1.0f;
    setScreenshotMode(SCR_BGR_NOSWAP);
    tft.fillScreen(TFT_BLACK);

    analogReadResolution(12);
    tft.setTextColor(COL_RGB(255, 255, 0)); tft.setCursor(38, 60);
    tft.print("CALIBRATION...");

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

    if (warningShown) {
        tft.fillScreen(TFT_BLACK);
        delay(300);
        tft.setTextColor(COL_RGB(255, 255, 0)); tft.setCursor(38, 60);
        tft.print("CALIBRATION...");
    }

    long sumX = 0, sumY = 0;
    for (int i = 0; i < 10; i++) {
        sumX += analogRead(JOY_X); sumY += analogRead(JOY_Y); delay(2);
    }
    joyCenterX = sumX / 10; joyCenterY = sumY / 10;

    gameState = STATE_MENU;
    menuDrawn = false;
    titleDrawn = false;
    inLevelSelect = false;

    soundEnabled = osLoadSoundSetting(true);
    {
        Preferences prefs;
        prefs.begin("os", true);
        showFps = prefs.getBool("show_fps", false);
        prefs.end();
    }

    xTaskCreatePinnedToCore(TaskEngine, "TaskEngine", 30000, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(TaskDisplay, "TaskDisplay", 10000, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(TaskRadar, "TaskRadar", 20000, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(TaskJoy, "TaskJoy", 2048, NULL, 1, NULL, 1);

    lastFrame = millis(); fpsTimer = millis();
}

// ════════════════════════════════════════════════════════════
//  loop() — FreeRTOS task'ları kullanıldığı için boş
// ════════════════════════════════════════════════════════════
void loop() {
    vTaskDelete(NULL);
}
