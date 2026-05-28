// ============================================================
//  EMİR OS — FLAWLESS PACMAN
//  160x128 TFT LCD | ST7735 | TFT_eSPI
//  Double-buffered (TFT_eSprite), Flicker-Free, Grid-Snapped
// ============================================================

#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <esp_ota_ops.h>

#define JOY_X   1
#define JOY_Y   2
#define JOY_SW  18
#define BTN_A   3
#define BTN_B   21
#define BTN_C   4
#define BTN_D   6
#define BUZZER  5

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

Preferences prefs;
bool soundEnabled = true;

#define SCR_W 160
#define SCR_H 128
#define HUD_H 16
#define TILE 8
#define HALF_TILE 4
#define COLS 20
#define ROWS 14
#define MAP_Y HUD_H

#define C_BG TFT_BLACK
#define C_WALL tft.color565(30, 30, 200)
#define C_DOT tft.color565(255, 180, 150)
#define C_POWER tft.color565(255, 255, 100)
#define C_PAC tft.color565(255, 255, 0)

const uint8_t PROGMEM_MAP[ROWS][COLS] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,2,2,2,2,2,2,2,2,1,1,2,2,2,2,2,2,2,2,1},
  {1,3,1,1,2,1,1,1,2,1,1,2,1,1,1,2,1,1,3,1},
  {1,2,1,1,2,1,1,1,2,1,1,2,1,1,1,2,1,1,2,1},
  {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
  {1,2,1,1,2,1,2,1,1,1,1,1,1,2,1,2,1,1,2,1},
  {1,2,2,2,2,1,2,2,2,1,1,2,2,2,1,2,2,2,2,1},
  {1,1,1,1,2,1,1,0,0,0,0,0,0,1,1,2,1,1,1,1}, 
  {0,0,0,1,2,1,0,0,1,0,0,1,0,0,1,2,1,0,0,0}, 
  {1,1,1,1,2,1,0,0,1,0,0,1,0,0,1,2,1,1,1,1}, 
  {0,0,0,0,2,0,0,0,1,1,1,1,0,0,0,2,0,0,0,0}, 
  {1,1,1,1,2,1,1,0,0,0,0,0,0,1,1,2,1,1,1,1}, 
  {1,2,2,2,2,2,2,2,2,1,1,2,2,2,2,2,2,2,2,1}, 
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}  
};

uint8_t gameMap[ROWS][COLS];

enum GameState { TITLE, READY, PLAYING, DYING, GAMEOVER, WIN, PAUSE };
GameState state = TITLE;

int score = 0, highScore = 0;
int lives = 3;
int dotsLeft = 0;
int level = 1;
uint32_t stateTimer = 0;
int joyCenterX = 2048, joyCenterY = 2048;
uint32_t soundEndTime = 0;

void playSound(int freq, int dur) {
    if (soundEnabled) {
        tone(BUZZER, freq); // Süre vermiyoruz, ESP32'nin donanım timer bug'ını aşıyoruz
        soundEndTime = millis() + dur;
    }
}

void returnToOS() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 60);
    tft.print("Ana Menuye Donuluyor...");
    delay(500);
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); }
    ESP.restart();
}

bool isWall(int c, int r) {
    if (c < 0 || c >= COLS) return false; 
    if (r < 0 || r >= ROWS) return true;
    return gameMap[r][c] == 1;
}

struct Actor {
    float x, y;
    int dx, dy;
    int ndx, ndy;
    float speed;
};

Actor pac;

#define NUM_GHOSTS 3
struct Ghost {
    Actor a;
    int type; 
    int mode; 
    uint32_t scaredUntil;
    uint16_t color;
    int lastTileC;
    int lastTileR;
} ghosts[NUM_GHOSTS];

// Arduino IDE'nin prototip uretirken struct'i gorememe hatasini asmak icin manuel prototipler
void moveGhost(Ghost &g, float dt);
void drawGhost(Ghost &g);

void resetActors() {
    pac.x = 9 * TILE + HALF_TILE;
    pac.y = 11 * TILE + HALF_TILE; 
    pac.dx = -1; pac.dy = 0;
    pac.ndx = -1; pac.ndy = 0;
    pac.speed = 40.0f + (level * 1.5f); // Daha dengeli Pacman hizi

    uint16_t cols[] = {tft.color565(255, 0, 0), tft.color565(255, 180, 255), tft.color565(0, 255, 255)};
    for(int i=0; i<NUM_GHOSTS; i++) {
        // Tüm hayaletleri evin dışında (Row 7) başlatıyoruz ki içeride takılmasınlar
        ghosts[i].a.x = (8 + i) * TILE + HALF_TILE; 
        ghosts[i].a.y = 7 * TILE + HALF_TILE; 
        
        ghosts[i].a.dx = (i==1) ? 1 : -1; ghosts[i].a.dy = 0;
        ghosts[i].a.speed = 22.0f + (level * 1.0f); // Hayalet hizi dusuruldu (takilmadiklari icin cok hizlanmislardi)
        ghosts[i].type = i;
        ghosts[i].mode = 0;
        ghosts[i].color = cols[i];
        ghosts[i].lastTileC = -1;
        ghosts[i].lastTileR = -1;
    }
}

void resetLevel(bool fullReset) {
    if (fullReset) {
        score = 0;
        lives = 3;
        level = 1;
    }
    
    // Haritayı her yeni bölümde (veya oyunda) sıfırlamamız lazım
    if (fullReset || dotsLeft == 0) {
        dotsLeft = 0;
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                gameMap[r][c] = PROGMEM_MAP[r][c];
                if (gameMap[r][c] == 2 || gameMap[r][c] == 3) dotsLeft++;
            }
        }
    }
    
    resetActors();
    state = READY;
    stateTimer = millis();
}

void applyPacmanInput() {
    int jx = analogRead(JOY_X) - joyCenterX; 
    int jy = analogRead(JOY_Y) - joyCenterY;
    int dead = 400;
    if (abs(jx) > dead || abs(jy) > dead) {
        if (abs(jx) > abs(jy)) {
            pac.ndx = (jx > 0) ? 1 : -1; pac.ndy = 0;
        } else {
            pac.ndy = (jy > 0) ? 1 : -1; pac.ndx = 0;
        }
    }
}

void movePacman(float dt) {
    int cx = ((int)pac.x / TILE) * TILE + HALF_TILE;
    int cy = ((int)pac.y / TILE) * TILE + HALF_TILE;
    
    if (abs(pac.x - cx) <= 1.5f && abs(pac.y - cy) <= 1.5f) {
        if (pac.ndx != pac.dx || pac.ndy != pac.dy) {
            int nc = ((int)cx / TILE) + pac.ndx;
            int nr = ((int)cy / TILE) + pac.ndy;
            if (!isWall(nc, nr)) {
                pac.dx = pac.ndx; pac.dy = pac.ndy;
                pac.x = cx; pac.y = cy; 
            }
        }
    }
    
    int nc = ((int)pac.x / TILE) + pac.dx;
    int nr = ((int)pac.y / TILE) + pac.dy;
    
    if (abs(pac.x - cx) <= 1.5f && abs(pac.y - cy) <= 1.5f && isWall(nc, nr)) {
        pac.x = cx; pac.y = cy;
    } else {
        pac.x += pac.dx * pac.speed * dt;
        pac.y += pac.dy * pac.speed * dt;
    }
    
    if (pac.x < 0) pac.x += SCR_W;
    if (pac.x >= SCR_W) pac.x -= SCR_W;
}

void moveGhost(Ghost &g, float dt) {
    int cx = ((int)g.a.x / TILE) * TILE + HALF_TILE;
    int cy = ((int)g.a.y / TILE) * TILE + HALF_TILE;
    int curC = cx / TILE;
    int curR = cy / TILE;
    
    if (abs(g.a.x - cx) <= 1.5f && abs(g.a.y - cy) <= 1.5f) {
        if (g.lastTileC != curC || g.lastTileR != curR) {
            g.lastTileC = curC;
            g.lastTileR = curR;
            g.a.x = cx; g.a.y = cy;
            
            int bestDx = g.a.dx, bestDy = g.a.dy;
            float minDist = 99999.0f;
            int tc = 0, tr = 0;
            
            if (g.mode == 2) { 
                tc = 9; tr = 9; 
                if (((int)g.a.x/TILE) == 9 && ((int)g.a.y/TILE) == 9) g.mode = 0; 
            } else if (g.mode == 1) { 
                tc = random(0, COLS); tr = random(0, ROWS);
                if (millis() > g.scaredUntil) g.mode = 0;
            } else { 
                if (g.type == 0) { tc = (int)pac.x/TILE; tr = (int)pac.y/TILE; }
                else if (g.type == 1) { tc = ((int)pac.x/TILE) + pac.dx*3; tr = ((int)pac.y/TILE) + pac.dy*3; }
                else { tc = (millis()/3000)%2==0 ? (int)pac.x/TILE : 1; tr = 1; }
            }
            
            int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
            bool foundMove = false;
            for (int i=0; i<4; i++) {
                int dx = dirs[i][0]; int dy = dirs[i][1];
                if (dx == -g.a.dx && dy == -g.a.dy && (g.a.dx!=0 || g.a.dy!=0)) continue;
                int nc = ((int)g.a.x/TILE) + dx;
                int nr = ((int)g.a.y/TILE) + dy;
                if (isWall(nc, nr)) continue;
                
                float dist = sq(nc - tc) + sq(nr - tr);
                if (dist < minDist) {
                    minDist = dist;
                    bestDx = dx; bestDy = dy;
                    foundMove = true;
                }
            }
            if (!foundMove) {
                bestDx = -g.a.dx; bestDy = -g.a.dy;
            }
            g.a.dx = bestDx; g.a.dy = bestDy;
        }
    }
    
    float spd = g.a.speed;
    if (g.mode == 1) spd *= 0.6f;
    if (g.mode == 2) spd *= 2.0f;
    
    g.a.x += g.a.dx * spd * dt;
    g.a.y += g.a.dy * spd * dt;
    if (g.a.x < 0) g.a.x += SCR_W;
    if (g.a.x >= SCR_W) g.a.x -= SCR_W;
}

void drawMap() {
    canvas.fillSprite(C_BG);
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int x = c * TILE;
            int y = MAP_Y + r * TILE;
            if (gameMap[r][c] == 1) {
                canvas.fillRect(x, y, TILE, TILE, C_WALL);
                canvas.drawRect(x+1, y+1, TILE-2, TILE-2, tft.color565(60,60,250));
            } else if (gameMap[r][c] == 2) {
                canvas.fillRect(x + HALF_TILE - 1, y + HALF_TILE - 1, 2, 2, C_DOT);
            } else if (gameMap[r][c] == 3) {
                canvas.fillCircle(x + HALF_TILE, y + HALF_TILE, 3, C_POWER);
            }
        }
    }
}

void drawHUD() {
    canvas.fillRect(0, 0, SCR_W, HUD_H, tft.color565(10,10,40));
    canvas.drawFastHLine(0, HUD_H-1, SCR_W, C_WALL);
    canvas.setTextSize(1);
    canvas.setTextColor(C_PAC);
    canvas.setCursor(2, 4);
    canvas.printf("SC:%05d", score);
    canvas.setTextColor(tft.color565(180,180,180));
    canvas.setCursor(70, 4);
    canvas.printf("HI:%05d", highScore);
    for (int i=0; i<lives; i++) {
        canvas.fillCircle(150 - i*8, 7, 3, C_PAC);
    }
}

void drawPacman(float x, float y, int dx, int dy) {
    bool mouthOpen = (millis() / 150) % 2 == 0;
    canvas.fillCircle(x, y + MAP_Y, 3, C_PAC);
    if (mouthOpen) {
        if (dx == 1) canvas.fillTriangle(x, y+MAP_Y, x+4, y+MAP_Y-3, x+4, y+MAP_Y+3, C_BG);
        else if (dx == -1) canvas.fillTriangle(x, y+MAP_Y, x-4, y+MAP_Y-3, x-4, y+MAP_Y+3, C_BG);
        else if (dy == 1) canvas.fillTriangle(x, y+MAP_Y, x-3, y+MAP_Y+4, x+3, y+MAP_Y+4, C_BG);
        else if (dy == -1) canvas.fillTriangle(x, y+MAP_Y, x-3, y+MAP_Y-4, x+3, y+MAP_Y-4, C_BG);
    }
}

void drawGhost(Ghost &g) {
    uint16_t c = g.color;
    if (g.mode == 1) {
        if (g.scaredUntil - millis() < 2000 && (millis()/200)%2 == 0) c = TFT_WHITE;
        else c = tft.color565(50,50,255);
    } else if (g.mode == 2) {
        c = tft.color565(100,200,255); 
    }
    
    int ix = (int)g.a.x;
    int iy = (int)g.a.y + MAP_Y;
    
    canvas.fillRect(ix-3, iy-1, 7, 5, c);
    canvas.fillCircle(ix, iy-2, 3, c);
    canvas.fillRect(ix-3, iy+4, 2, 2, c);
    canvas.fillRect(ix, iy+4, 2, 2, c);
    canvas.fillRect(ix+2, iy+4, 2, 2, c);
    
    if (g.mode != 2) {
        canvas.fillRect(ix-2, iy-3, 2, 2, TFT_WHITE);
        canvas.fillRect(ix+1, iy-3, 2, 2, TFT_WHITE);
        canvas.drawPixel(ix-1+g.a.dx, iy-2+g.a.dy, TFT_BLUE);
        canvas.drawPixel(ix+2+g.a.dx, iy-2+g.a.dy, TFT_BLUE);
    }
}

void setup() {
    // OLED Kapatma ve Buzzer Susturma (Hızlı Başlatma için)
    pinMode(5, OUTPUT);
    digitalWrite(5, LOW);
    Wire.begin(8, 9);
    Wire.beginTransmission(0x3C);
    Wire.write(0x00);
    Wire.write(0xAE); // Display OFF
    Wire.endTransmission();

    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) esp_ota_set_boot_partition(os_part);
    
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP);
    pinMode(JOY_SW, INPUT_PULLUP);
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
    
    SPI.begin(12, 42, 11, -1);
    tft.init(); tft.setRotation(1); tft.setSwapBytes(true);
    
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);
    
    prefs.begin("os", true);
    soundEnabled = prefs.getBool("sound_en", true);
    prefs.end();
    
    prefs.begin("os", true);
    highScore = prefs.getInt("hs_pacman", 0);
    prefs.end();
    
    // Joystick başlangıçta dokunulma ihtimaline karşı sabit merkez kullanıyoruz
    joyCenterX = 2048; 
    joyCenterY = 2048;
}

void loop() {
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) {
        if (state == PLAYING) {
            state = PAUSE;
            playSound(400, 50);
        }
    }
    prevJoySw = currJoySw;
    
    uint32_t now = millis();
    if (soundEndTime > 0 && now > soundEndTime) {
        noTone(BUZZER);
        digitalWrite(BUZZER, LOW);
        soundEndTime = 0;
    }
    
    static uint32_t lastFrame = 0;
    float dt = (now - lastFrame) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    if (dt < 0.01f) return;
    lastFrame = now;
    
    if (state == TITLE) {
        canvas.fillSprite(C_BG);
        canvas.setTextColor(C_PAC); canvas.setTextSize(2);
        canvas.setCursor(40, 20); canvas.print("PACMAN");
        canvas.setTextSize(1); canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(30, 60); canvas.print("[A] OYUNA BASLA");
        canvas.setCursor(30, 80); canvas.print("[B] ANA MENU");
        canvas.pushSprite(0,0);
        if (!digitalRead(BTN_A)) resetLevel(true);
        if (!digitalRead(BTN_B)) { delay(200); returnToOS(); }
        return;
    }
    
    if (state == GAMEOVER || state == WIN) {
        canvas.fillSprite(C_BG);
        canvas.setTextColor(state==WIN ? TFT_GREEN : TFT_RED); canvas.setTextSize(2);
        canvas.setCursor(15, 20); canvas.print(state==WIN ? "KAZANDIN!" : "OYUN BITTI");
        canvas.setTextSize(1); canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(30, 60); canvas.print("SKOR: "); canvas.print(score);
        canvas.setCursor(30, 80); canvas.print("[A] TEKRAR OYNA");
        canvas.setCursor(30, 90); canvas.print("[B] ANA MENU");
        canvas.pushSprite(0,0);
        if (!digitalRead(BTN_A)) { delay(200); resetLevel(true); }
        if (!digitalRead(BTN_B)) { delay(200); returnToOS(); }
        return;
    }
    
    if (state == READY) {
        drawMap(); drawHUD();
        drawPacman(pac.x, pac.y, pac.dx, pac.dy);
        for(int i=0; i<NUM_GHOSTS; i++) drawGhost(ghosts[i]);
        canvas.setTextColor(TFT_YELLOW); canvas.setTextSize(1);
        canvas.setCursor(65, 80); canvas.print("HAZIR!");
        canvas.pushSprite(0,0);
        if (now - stateTimer > 2000) state = PLAYING;
        return;
    }
    
    if (state == DYING) {
        drawMap(); drawHUD();
        int r = 3 - (now - stateTimer) / 300;
        if (r > 0) canvas.fillCircle(pac.x, pac.y+MAP_Y, r, C_PAC);
        canvas.pushSprite(0,0);
        if (now - stateTimer > 1500) {
            lives--;
            if (lives <= 0) {
                if (score > highScore) { 
                    highScore = score; 
                    Preferences prefsLocal;
                    prefsLocal.begin("os", false); 
                    prefsLocal.putInt("hs_pacman", highScore); 
                    prefsLocal.end(); 
                }
                state = GAMEOVER;
                playSound(150, 600); // Oyun bitiş sesi (Tek bir net bip)
            } else {
                resetActors();
                state = READY;
                stateTimer = now;
            }
        }
        return;
    }
    
    if (state == PLAYING) {
        applyPacmanInput();
        movePacman(dt);
        
        int pc = (int)pac.x / TILE;
        int pr = (int)pac.y / TILE;
        if (pr >= 0 && pr < ROWS && pc >= 0 && pc < COLS) {
            if (gameMap[pr][pc] == 2) {
                gameMap[pr][pc] = 0; score += 10; dotsLeft--;
                playSound(1000, 20);
            } else if (gameMap[pr][pc] == 3) {
                gameMap[pr][pc] = 0; score += 50; dotsLeft--;
                playSound(1500, 100);
                for(int i=0; i<NUM_GHOSTS; i++) {
                    if (ghosts[i].mode != 2) {
                        ghosts[i].mode = 1;
                        ghosts[i].scaredUntil = now + 7000;
                    }
                }
            }
        }
        
        if (dotsLeft == 0) {
            level++;
            // Kullanici istegi uzerine bolum gecis sesi tamamen kaldirildi
            resetLevel(false);
            return;
        }
        
        for(int i=0; i<NUM_GHOSTS; i++) {
            moveGhost(ghosts[i], dt);
            
            if (abs(pac.x - ghosts[i].a.x) < 5.0f && abs(pac.y - ghosts[i].a.y) < 5.0f) {
                if (ghosts[i].mode == 1) {
                    ghosts[i].mode = 2; 
                    score += 200;
                    playSound(2000, 150);
                } else if (ghosts[i].mode == 0) {
                    state = DYING;
                    stateTimer = now;
                    playSound(200, 300); // Daha kisa ve net olum sesi
                    break; // Diger hayaletleri kontrol etmeyi birak (Bip bip onleme)
                }
            }
        }
        
        drawMap(); drawHUD();
        drawPacman(pac.x, pac.y, pac.dx, pac.dy);
        for(int i=0; i<NUM_GHOSTS; i++) drawGhost(ghosts[i]);
        canvas.pushSprite(0,0);
        return;
    }
    
    if (state == PAUSE) {
        drawMap(); drawHUD();
        drawPacman(pac.x, pac.y, pac.dx, pac.dy);
        for(int i=0; i<NUM_GHOSTS; i++) drawGhost(ghosts[i]);
        
        // Üzerine karartma ve menü
        canvas.fillRect(30, 40, 100, 50, TFT_BLACK);
        canvas.drawRect(30, 40, 100, 50, TFT_WHITE);
        
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(65, 48); canvas.print("PAUSE");
        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(42, 62); canvas.print("[A] Devam Et");
        canvas.setCursor(42, 74); canvas.print("[B] OS Menu");
        
        canvas.pushSprite(0,0);
        
        if (!digitalRead(BTN_A)) {
            playSound(800, 50);
            delay(200);
            state = PLAYING;
        }
        if (!digitalRead(BTN_B)) {
            playSound(400, 50);
            delay(200);
            returnToOS();
        }
        return;
    }
}
