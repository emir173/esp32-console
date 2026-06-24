// ============================================================
//  EMİR OS — FLAWLESS PACMAN
//  160x128 TFT LCD | ST7735 | TFT_eSPI
//  Double-buffered (TFT_eSprite), Flicker-Free, Grid-Snapped
// ============================================================

// TFT ekrani icin grafik kutuphanesi (ST7735 sürücüsü)
#include <TFT_eSPI.h>
// SPI haberlesmesi (TFT ile veri transferi icin)
#include <SPI.h>
// I2C haberlesmesi (OLED ekran kontrolu icin)
#include <Wire.h>
// OLED (SH1106) icin grafik kutuphanesi
#include <U8g2lib.h>
// OLED nesnesi - 128x64 SH1106, donanimsal I2C, donus yok (R0), reset pini yok
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
// NVS (Non-Volatile Storage) erisimi - skor/ses ayari kaydetmek icin
#include <Preferences.h>
// ESP32 OTA (Over-The-Air) partition islemleri - OS'e donus icin
#include <esp_ota_ops.h>
// Donanim pinleri ve sabitleri (BUZZER, BTN_*, JOY_*, SPI pinleri vb.)
#include "../hardware_config.h"
// Gelistirici araclar: ekran görüntüsü (screenshot) ve dev tools baslatma
#include "../dev_tools.h"

// TFT ekran nesnesi (ana donanim yüzeyi)
TFT_eSPI tft = TFT_eSPI();
// Off-screen buffer (double-buffer) - titremesiz cizim icin sprite
TFT_eSprite canvas = TFT_eSprite(&tft);

// Kalici ayar oku/yaz nesnesi (NVS)
Preferences prefs;
// Ses ayari (true = acik, baslangicta true)
bool soundEnabled = true;

#define SCR_W 160          // Ekran genisligi (piksel)
#define SCR_H 128          // Ekran yuksekligi (piksel)
#define HUD_H 16           // Ust HUD (skor/can) seridinin yuksekligi
#define TILE 8             // Bir kare (tile) boyutu - piksel
#define HALF_TILE 4        // Kare yari boyutu - hizalama icin
#define COLS 20            // Harita sutun sayisi
#define ROWS 14            // Harita satir sayisi
#define MAP_Y HUD_H        // Haritanin baslangic Y'si (HUD altinda)

// Oyun rengi sabitleri (RGB565 formatinda)
#define C_BG TFT_BLACK                               // Arka plan (siyah)
#define C_WALL tft.color565(30, 30, 200)             // Duvar rengi (koyu mavi)
#define C_DOT tft.color565(255, 180, 150)            // Dot (yem) rengi (acik turuncu)
#define C_POWER tft.color565(255, 255, 100)          // Power pellet rengi (sari)
#define C_PAC tft.color565(255, 255, 0)              // Pacman rengi (sari)

// Delta-time (dt) sistemi: kare hizi hedefi 60 FPS
// dt = son kareden gecen sure (saniye). Hareketler dt ile carpilir,
// boylece FPS degisse bile oyun hizi sabit kalir.
#define TARGET_FPS 60                 // FPS60: Hedef frame hizi 60 FPS
#define FRAME_SEC (1.0f / TARGET_FPS)  // FPS60: Bir frame'in saniye cinsinden ideal suresi (~0.0167s)
#define DT_CAP 0.05f                   // FPS60: Lag spike korumasi - dt ust siniri (50ms)

// Sabit harita (PROGMEM'de degil ama salt okunur baslangic sablonu)
// 0 = bos alan, 1 = duvar, 2 = dot (yem), 3 = power pellet
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

// Oynanis sirasinda degistirilen harita kopyasi (dot'lar yenirken 0'lanir)
uint8_t gameMap[ROWS][COLS];

// Oyun durum makinesi (state machine) - her state ayri isler
// TITLE:    acilis/baslik ekrani
// READY:    "HAZIR!" geri sayim gosterimi (oyun oncesi)
// PLAYING:  aktif oynanis
// DYING:    pacman olum animasyonu
// GAMEOVER: tum canlar bitti, sonuc ekrani
// WIN:      tum dot'lar yendi, bolum gecildi (simdi kullanilmiyor, reset var)
// PAUSE:    duraklatilmis, menu ile devam/cikis
enum GameState { TITLE, READY, PLAYING, DYING, GAMEOVER, WIN, PAUSE };
GameState state = TITLE;

// Oyun degiskenleri
int score = 0, highScore = 0;   // Guncel skor ve kalici rekor
int lives = 3;                  // Kalan can sayisi
int dotsLeft = 0;               // Yenmesi gereken kalan dot/power sayisi
int level = 1;                  // Mevcut bolum (hiz artisi icin)
uint32_t stateTimer = 0;        // State gecisleri icin zamanlayici (millis)
int joyCenterX = 2048, joyCenterY = 2048; // Joystick kalibrasyon merkezi (ADC orta degeri)
uint32_t soundEndTime = 0;      // Su anki tonun bitis zamani (millis) - manuel sure kontrolu
int prevBtnA = HIGH;            // BTN_A'nin onceki durumu (kenar algilama icin)

// ============================================================
//  playSound — Buzzer uzerinden ton uretir
//  ESP32'nin tone() fonksiyonu sure parametresini desteklemedigi
//  (donanım timer bug'i) icin manuel millis() tabanli sure kontrolu
//  yapar: tone() baslatir, soundEndTime'e bitis zamani yazilir,
//  loop() icinde sure dolunca noTone() cagrilir.
//  freq: Frekans (Hz), dur: Sure (ms)
// ============================================================
void playSound(int freq, int dur) {
    if (soundEnabled) {
        tone(BUZZER, freq); // Süre vermiyoruz, ESP32'nin donanım timer bug'ını aşıyoruz
        soundEndTime = millis() + dur;
    }
}

// ============================================================
//  returnToOS — Mevcut oyunu birakip EMİR OS ana menusune doner
//  TFT'yi temizler, kisa bir bekleme gosterir, sonra OTA boot
//  partition'ini OS partition'ina cevirip ESP'yi yeniden baslatir.
// ============================================================
void returnToOS() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 60);
    tft.print("Ana Menuye Donuluyor...");
    delay(500);
    // Sonraki OTA partition'ini al (OS'in yuklu oldugu partition)
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) { esp_ota_set_boot_partition(os_part); } // Boot'u OS'e yonlendir
    ESP.restart(); // ESP32'yi yeniden baslat
}

// ============================================================
//  isWall — Verilen (c,r) kare konumunun duvar olup olmadigini doner
//  c: sutun, r: satir. Harita disinda sutun false (gecis var, tunnel),
//  harita disinda satir true (duvar) kabul edilir.
//  return: true ise duvar
// ============================================================
bool isWall(int c, int r) {
    if (c < 0 || c >= COLS) return false;  // Yatay tunnel (ekran disi gecis)
    if (r < 0 || r >= ROWS) return true;   // Dikey sinir = duvar
    return gameMap[r][c] == 1;
}

// Actor (oyun karakteri) yapisi - Pacman ve hayaletler ortak kullanir
struct Actor {
    float x, y;     // Piksel cinsinden konum (yumuşak hareket icin float)
    int dx, dy;     // Su anki yon vektoru (-1/0/+1)
    int ndx, ndy;   // Istek (next) yon vektoru - oyuncu girisi ile belirlenir
    float speed;    // Piksel/saniye hareket hizi
};

Actor pac; // Pacman karakteri

#define NUM_GHOSTS 3
// Ghost yapisi - hayaletlerin AI ve durum bilgilerini tutar
struct Ghost {
    Actor a;            // Ortak hareket bilgileri
    int type;           // Hayalet tipi: 0=Blinky(chase), 1=Pinky(oncelmeli), 2=Inky(karisik)
    int mode;           // 0=chase (kovar), 1=scared (kacar), 2=eaten (eve doner)
    uint32_t scaredUntil; // Scared modunun bitis zamani (millis) - power pellet suresi
    uint16_t color;     // Hayaletin rengi (normal modda)
    int lastTileC;      // Son islenen kare sutunu (tekrar islemeyi onler)
    int lastTileR;      // Son islenen kare satiri
} ghosts[NUM_GHOSTS];

// Arduino IDE'nin prototip uretirken struct'i gorememe hatasini asmak icin manuel prototipler
void moveGhost(Ghost &g, float dt);
void drawGhost(Ghost &g);

// ============================================================
//  resetActors — Pacman ve hayaletleri baslangic konumlarina koyar
//  Hizlar level'a gore artar (level*1.5 Pacman, level*1.0 hayalet).
// ============================================================
void resetActors() {
    // Pacman baslangic karesi (9,11) - tam ortada alt sira
    pac.x = 9 * TILE + HALF_TILE;
    pac.y = 11 * TILE + HALF_TILE; 
    pac.dx = -1; pac.dy = 0;       // Baslangic yonu: sola
    pac.ndx = -1; pac.ndy = 0;
    pac.speed = 40.0f + (level * 1.5f); // Daha dengeli Pacman hizi (level basina +1.5)

    // Hayalet renkleri: kirmizi, pembe, cyan
    uint16_t cols[] = {tft.color565(255, 0, 0), tft.color565(255, 180, 255), tft.color565(0, 255, 255)};
    for(int i=0; i<NUM_GHOSTS; i++) {
        // Tüm hayaletleri evin dışında (Row 7) başlatıyoruz ki içeride takılmasınlar
        ghosts[i].a.x = (8 + i) * TILE + HALF_TILE; 
        ghosts[i].a.y = 7 * TILE + HALF_TILE; 
        
        ghosts[i].a.dx = (i==1) ? 1 : -1; ghosts[i].a.dy = 0; // Ortadaki saga, digerleri sola
        ghosts[i].a.speed = 22.0f + (level * 1.0f); // Hayalet hizi dusuruldu (takilmadiklari icin cok hizlanmislardi)
        ghosts[i].type = i;        // Hayalet tipi = indeks (0,1,2)
        ghosts[i].mode = 0;        // Baslangicta chase modda
        ghosts[i].color = cols[i];
        ghosts[i].lastTileC = -1;  // Henuz bir kare islenmedi
        ghosts[i].lastTileR = -1;
    }
}

// ============================================================
//  resetLevel — Bolumu (veya tum oyunu) sifirlar
//  fullReset: true ise skor/can/level sifirlanir (yeni oyun),
//             false ise sadece bir sonraki bolum (level++ sonrasi).
//  Dot'lar yeniden doldurulur (fullReset veya bolum tamamlandiysa).
// ============================================================
void resetLevel(bool fullReset) {
    if (fullReset) {
        score = 0;
        lives = 3;
        level = 1;
    }
    
    // Haritayı her yeni bölümde (veya oyunda) sıfırlamamız lazım
    if (fullReset || dotsLeft == 0) {
        dotsLeft = 0;
        // PROGMEM_MAP'ten gameMap'e kopyala ve dot sayisini say
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                gameMap[r][c] = PROGMEM_MAP[r][c];
                if (gameMap[r][c] == 2 || gameMap[r][c] == 3) dotsLeft++; // dot ve power say
            }
        }
    }
    
    resetActors();         // Karakterleri baslangica koy
    state = READY;         // "HAZIR!" geri sayimina gec
    stateTimer = millis();
}

// ============================================================
//  applyPacmanInput — Joystick okuyarak Pacman'in istek (next)
//  yonunu (ndx, ndy) belirler. Hareket hemen yapilmaz, kare
//  merkezinde movePacman tarafindan uygulanir.
//  dead=400: joystick deadzone (ortada titremeyi onler)
// ============================================================
void applyPacmanInput() {
    int jx = analogRead(JOY_X) - joyCenterX; // Merkezlenmis X ekseni
    int jy = analogRead(JOY_Y) - joyCenterY; // Merkezlenmis Y ekseni
    int dead = 400; // Deadzone: 0-4095 araliginda merkez civari ~400
    if (abs(jx) > dead || abs(jy) > dead) {
        // Baskin ekseni sec (diagonal egimde daha buyuk olan kazanir)
        if (abs(jx) > abs(jy)) {
            pac.ndx = (jx > 0) ? 1 : -1; pac.ndy = 0;
        } else {
            pac.ndy = (jy > 0) ? 1 : -1; pac.ndx = 0;
        }
    }
}

// ============================================================
//  movePacman — Pacman'i dt ile hareket ettirir
//  Kare merkezlerinde yon degisimi ve duvar kontrolu yapilir.
//  1.5f: kare merkezine tolerans (piksel) - yumuşak yakalama icin.
//  Tunel: ekran sol/sag kenarindan gecis (SCR_W wrap).
// ============================================================
void movePacman(float dt) {
    // Bulundugu karenin merkez koordinati
    int cx = ((int)pac.x / TILE) * TILE + HALF_TILE;
    int cy = ((int)pac.y / TILE) * TILE + HALF_TILE;
    
    // Kare merkezine yakinsa yon degisimi uygula
    if (abs(pac.x - cx) <= 1.5f && abs(pac.y - cy) <= 1.5f) {
        if (pac.ndx != pac.dx || pac.ndy != pac.dy) {
            // Istek yonunde kare duvar degilse yon degistir
            int nc = ((int)cx / TILE) + pac.ndx;
            int nr = ((int)cy / TILE) + pac.ndy;
            if (!isWall(nc, nr)) {
                pac.dx = pac.ndx; pac.dy = pac.ndy;
                pac.x = cx; pac.y = cy; // Kareye tam hizala
            }
        }
    }
    
    // Onundeki kare duvar mi kontrol et
    int nc = ((int)pac.x / TILE) + pac.dx;
    int nr = ((int)pac.y / TILE) + pac.dy;
    
    if (abs(pac.x - cx) <= 1.5f && abs(pac.y - cy) <= 1.5f && isWall(nc, nr)) {
        pac.x = cx; pac.y = cy; // Duvar varsa merkezde dur
    } else {
        // dt ile orantili hareket (FPS bagimsiz)
        pac.x += pac.dx * pac.speed * dt;
        pac.y += pac.dy * pac.speed * dt;
    }
    
    // Yatay tunnel: soldan cikinca sagdan, sagdan cikinca soldan gir
    if (pac.x < 0) pac.x += SCR_W;
    if (pac.x >= SCR_W) pac.x -= SCR_W;
}

// ============================================================
//  moveGhost — Hayaletin AI hareket mantigi
//  Kare merkezlerinde, hedefe en kisa (Manhattan benzeri) mesafeyi
//  veren yon secilir. Geri donus yasaktir (180° engeli).
//
//  mode (g.a modu):
//    0 = CHASE  -> Pacman'i kovalar (hedef tip'e gore degisir)
//    1 = SCARED -> Rastgele kacar (power pellet yendi)
//    2 = EATEN  -> Eve (9,9) doner, varinca chase'e gecer
//
//  type (sadece mode=0 chase icin):
//    0 = Blinky -> Pacman'in tam konumu
//    1 = Pinky  -> Pacman'in 3 kare onu (yon ile)
//    2 = Inky   -> 3 saniyede bir Pacman/sol-ust degisir (karisik)
//
//  Hiz carpani: scared *0.6 (yavas), eaten *2.0 (hizli eve donus)
// ============================================================
void moveGhost(Ghost &g, float dt) {
    // Bulundugu karenin merkezi ve kare indeksi
    int cx = ((int)g.a.x / TILE) * TILE + HALF_TILE;
    int cy = ((int)g.a.y / TILE) * TILE + HALF_TILE;
    int curC = cx / TILE;
    int curR = cy / TILE;
    
    // Kare merkezine yakinsa ve yeni bir kareye girdiyse AI calisir
    if (abs(g.a.x - cx) <= 1.5f && abs(g.a.y - cy) <= 1.5f) {
        if (g.lastTileC != curC || g.lastTileR != curR) {
            g.lastTileC = curC;
            g.lastTileR = curR;
            g.a.x = cx; g.a.y = cy; // Kareye tam hizala
            
            int bestDx = g.a.dx, bestDy = g.a.dy;
            float minDist = 99999.0f; // Sonsuz benzeri baslangic
            int tc = 0, tr = 0;       // Hedef kare (target col/row)
            
            // Hedefi belirle (mode'a gore)
            if (g.mode == 2) { 
                // Eaten: eve (9,9) don - varinca chase moduna gec
                tc = 9; tr = 9; 
                if (((int)g.a.x/TILE) == 9 && ((int)g.a.y/TILE) == 9) g.mode = 0; 
            } else if (g.mode == 1) { 
                // Scared: rastgele kac - sure dolunca chase'e don
                tc = random(0, COLS); tr = random(0, ROWS);
                if (millis() > g.scaredUntil) g.mode = 0;
            } else { 
                // Chase: tip'e gore hedef belirle
                if (g.type == 0) { 
                    // Blinky: Pacman'in tam konumu
                    tc = (int)pac.x/TILE; tr = (int)pac.y/TILE; 
                }
                else if (g.type == 1) { 
                    // Pinky: Pacman'in 3 kare onu (ambush)
                    tc = ((int)pac.x/TILE) + pac.dx*3; tr = ((int)pac.y/TILE) + pac.dy*3; 
                }
                else { 
                    // Inky: 3 sn'de bir Pacman veya sol-ust kose (karisik davranis)
                    tc = (millis()/3000)%2==0 ? (int)pac.x/TILE : 1; tr = 1; 
                }
            }
            
            // 4 yonu dene: sag, sol, asagi, yukari
            int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
            bool foundMove = false;
            for (int i=0; i<4; i++) {
                int dx = dirs[i][0]; int dy = dirs[i][1];
                // Geri donus yasak (180°) - cikmaz yoksa uygulanmaz
                if (dx == -g.a.dx && dy == -g.a.dy && (g.a.dx!=0 || g.a.dy!=0)) continue;
                int nc = ((int)g.a.x/TILE) + dx;
                int nr = ((int)g.a.y/TILE) + dy;
                if (isWall(nc, nr)) continue; // Duvar olan yonu atla
                
                // Hedefe olan mesafe (karesel - Manhattan benzeri)
                float dist = sq(nc - tc) + sq(nr - tr);
                if (dist < minDist) {
                    minDist = dist;
                    bestDx = dx; bestDy = dy;
                    foundMove = true;
                }
            }
            if (!foundMove) {
                // Cikmaz sokak: geri donmek zorunda
                bestDx = -g.a.dx; bestDy = -g.a.dy;
            }
            g.a.dx = bestDx; g.a.dy = bestDy;
        }
    }
    
    // Mod'a gore hiz carpani uygula
    float spd = g.a.speed;
    if (g.mode == 1) spd *= 0.6f;  // Scared: yavaslat
    if (g.mode == 2) spd *= 2.0f;  // Eaten: eve hizli don
    
    // dt ile orantili hareket
    g.a.x += g.a.dx * spd * dt;
    g.a.y += g.a.dy * spd * dt;
    // Yatay tunnel
    if (g.a.x < 0) g.a.x += SCR_W;
    if (g.a.x >= SCR_W) g.a.x -= SCR_W;
}

// ============================================================
//  drawMap — gameMap'i canvas'a cizer (duvar/dot/power)
//  Her kare tipine gore farkli sekil ve renk kullanilir:
//    1=duvar (kare+cicevre), 2=dot (kucuk nokta), 3=power (daire)
// ============================================================
void drawMap() {
    canvas.fillSprite(C_BG); // Oncelikle arka plani temizle
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int x = c * TILE;
            int y = MAP_Y + r * TILE;
            if (gameMap[r][c] == 1) {
                // Duvar: dolu kare + ici acik mavi cerceve (derinlik hissi)
                canvas.fillRect(x, y, TILE, TILE, C_WALL);
                canvas.drawRect(x+1, y+1, TILE-2, TILE-2, tft.color565(60,60,250));
            } else if (gameMap[r][c] == 2) {
                // Dot: karenin merkezinde 2x2 piksel nokta
                canvas.fillRect(x + HALF_TILE - 1, y + HALF_TILE - 1, 2, 2, C_DOT);
            } else if (gameMap[r][c] == 3) {
                // Power pellet: merkezde yari cap 3 daire
                canvas.fillCircle(x + HALF_TILE, y + HALF_TILE, 3, C_POWER);
            }
        }
    }
}

// ============================================================
//  drawHUD — Ust bilgi seridini cizer (skor, level, canlar)
// ============================================================
void drawHUD() {
    // HUD arka plani (koyu mavi) + alt cizgi
    canvas.fillRect(0, 0, SCR_W, HUD_H, tft.color565(10,10,40));
    canvas.drawFastHLine(0, HUD_H-1, SCR_W, C_WALL);
    
    // Skor
    canvas.setTextSize(1);
    canvas.setTextColor(C_PAC);
    canvas.setCursor(2, 4);
    canvas.printf("SC:%05d", score); // 5 haneli sifir dolgulu skor
    
    // Level
    canvas.setTextColor(tft.color565(180,180,180));
    canvas.setCursor(65, 4);
    canvas.printf("LV:%d", level);

    // Canlar - sagda kalan can sayisi kadar sarı daire
    for (int i=0; i<lives; i++) {
        canvas.fillCircle(150 - i*8, 7, 3, C_PAC); // Her can 8px aralikla
    }
}

// ============================================================
//  drawPacman — Pacman'i cizer (agiz animasyonlu)
//  x,y: piksel konum (y'ye MAP_Y eklenir), dx,dy: yon
//  mouthOpen: 150ms'de bir ac/kapat -> yeme animasyonu
// ============================================================
void drawPacman(float x, float y, int dx, int dy) {
    bool mouthOpen = (millis() / 150) % 2 == 0; // Agiz acik/kapali toggle
    canvas.fillCircle(x, y + MAP_Y, 3, C_PAC);  // Govde (sari daire)
    if (mouthOpen) {
        // Yone gore agiz ucgeni (arka plan rengi ile "kes")
        if (dx == 1) canvas.fillTriangle(x, y+MAP_Y, x+4, y+MAP_Y-3, x+4, y+MAP_Y+3, C_BG);
        else if (dx == -1) canvas.fillTriangle(x, y+MAP_Y, x-4, y+MAP_Y-3, x-4, y+MAP_Y+3, C_BG);
        else if (dy == 1) canvas.fillTriangle(x, y+MAP_Y, x-3, y+MAP_Y+4, x+3, y+MAP_Y+4, C_BG);
        else if (dy == -1) canvas.fillTriangle(x, y+MAP_Y, x-3, y+MAP_Y-4, x+3, y+MAP_Y-4, C_BG);
    }
}

// ============================================================
//  drawGhost — Hayaleti cizer (mod'a gore renk degisir)
//  mode 1 (scared): mavi, son 2 sn beyaz yanip soner
//  mode 2 (eaten):  sadece govde (goz yok), acik mavi
// ============================================================
void drawGhost(Ghost &g) {
    uint16_t c = g.color;
    if (g.mode == 1) {
        // Scared: son 2 saniye beyaz yanip soner (uyari)
        if (g.scaredUntil - millis() < 2000 && (millis()/200)%2 == 0) c = TFT_WHITE;
        else c = tft.color565(50,50,255); // Mavi
    } else if (g.mode == 2) {
        c = tft.color565(100,200,255);  // Eaten: acik mavi (sadece govde)
    }
    
    int ix = (int)g.a.x;
    int iy = (int)g.a.y + MAP_Y; // Harita offseti
    
    // Govde: kare + ust yari daire (kubbe)
    canvas.fillRect(ix-3, iy-1, 7, 5, c);
    canvas.fillCircle(ix, iy-2, 3, c);
    // Alt etek girintileri (3 dikey parca)
    canvas.fillRect(ix-3, iy+4, 2, 2, c);
    canvas.fillRect(ix, iy+4, 2, 2, c);
    canvas.fillRect(ix+2, iy+4, 2, 2, c);
    
    // Eaten degilse gozleri ciz (beyaz + mavi bebek)
    if (g.mode != 2) {
        canvas.fillRect(ix-2, iy-3, 2, 2, TFT_WHITE);
        canvas.fillRect(ix+1, iy-3, 2, 2, TFT_WHITE);
        // Bebekler yon vektorune gore kayar (hareket hissi)
        canvas.drawPixel(ix-1+g.a.dx, iy-2+g.a.dy, TFT_BLUE);
        canvas.drawPixel(ix+2+g.a.dx, iy-2+g.a.dy, TFT_BLUE);
    }
}

// ============================================================
//  setup — Donanimi baslatir, oyun ilk ayarlarini yapar
//  Sira: buzzer sustur -> OLED kapat -> buton pinleri -> TFT +
//  MADCTL RGB modu -> canvas -> NVS ayarlar -> joystick merkezi
// ============================================================
void setup() {
    // OLED Kapatma ve Buzzer Susturma (Hızlı Başlatma için)
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
    // OLED'i I2C uzerinden kapat (acilista TFT oncelikli olsun)
    Wire.begin(8, 9);                // I2C: SDA=8, SCL=9
    Wire.beginTransmission(0x3C);    // SH1106 I2C adresi (0x3C)
    Wire.write(0x00);                // Komut register'i (Co/D=0)
    Wire.write(0xAE); // Display OFF // 0xAE = Display OFF komutu
    Wire.endTransmission();

    // Boot partition'i OS'e yonlendir (oyundan cikisi hizlandirir)
    const esp_partition_t *os_part = esp_ota_get_next_update_partition(NULL);
    if (os_part) esp_ota_set_boot_partition(os_part);
    
    // Buton ve joystick pinleri (pull-up, basili = LOW)
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP);
    pinMode(BTN_D, INPUT_PULLUP);
    pinMode(JOY_SW, INPUT_PULLUP);
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
    
    // SPI ve TFT baslatma
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
    initDevTools(tft);               // Dev tools (screenshot vb.) hazirla
    tft.init(); tft.setRotation(1);  // 1 = yatay (landscape) oryantasyon
    // TFT donanimini RGB moduna gecir (standart RGB565 sabitler icin)
    tft.startWrite();
    tft.writecommand(0x36);  // MADCTL - Memory Access Control register
    tft.writedata(0xA0);     // MY|MV, BGR bit kapali (RGB modu) - 0xA0 = MY|MV bitleri
    tft.endWrite();
    setScreenshotMode(SCR_RGB_SWAP); tft.setSwapBytes(true); // Byte siralama swap
    
    // Off-screen canvas (double buffer) - 16 bit renk derinligi
    canvas.setColorDepth(16);
    canvas.createSprite(SCR_W, SCR_H);
    
    // NVS: ses ayarini oku (varsayilan true)
    prefs.begin("os", true);
    soundEnabled = prefs.getBool("sound_en", true);
    prefs.end();
    
    // NVS: pacman rekorunu oku (varsayilan 0)
    prefs.begin("os", true);
    highScore = prefs.getInt("hs_pacman", 0);
    prefs.end();
    
    // Joystick başlangıçta dokunulma ihtimaline karşı sabit merkez kullanıyoruz
    joyCenterX = 2048;  // ADC 12-bit orta degeri
    joyCenterY = 2048;
}

// ============================================================
//  loop — Ana oyun dongusu (her frame cagrılır)
//  1) PAUSE toggle (joystick butonu)
//  2) Ton sure kontrolu (noTone)
//  3) Delta-time hesabi + 60 FPS frame kilidi
//  4) BTN_A kenar algilama
//  5) State'e gore ilgili blok calsa da cizim
// ============================================================
void loop() {
    // --- PAUSE toggle: joystick butonuna basildiginda PLAYING -> PAUSE ---
    static bool prevJoySw = true;
    bool currJoySw = digitalRead(JOY_SW);
    if (prevJoySw && !currJoySw) { // Dusey kenar (basildi)
        if (state == PLAYING) {
            state = PAUSE;
            playSound(400, 50); // 400 Hz kisa bip
        }
    }
    prevJoySw = currJoySw;

    // --- Ton sure kontrolu: ESP32 tone() sure desteklemedigi icin manuel ---
    uint32_t now = millis();
    if (soundEndTime > 0 && now > soundEndTime) {
        noTone(BUZZER);            // Tonu durdur
        digitalWrite(BUZZER, LOW); // Pini sifirla
        soundEndTime = 0;          // Tekrar islemeyi onle
    }
    
    // --- Delta-time hesabi ve 60 FPS frame kilidi ---
    static uint32_t lastFrame = 0;
    float dt = (now - lastFrame) / 1000.0f; // Gecen sure (saniye)
    if (dt > DT_CAP) dt = DT_CAP;  // FPS60: Lag spike korumasi - dt max 50ms
    if (dt < FRAME_SEC) return;    // FPS60: 60 FPS frame kilidi, dt yeterince birikmeden islem yapma
    lastFrame = now;
    
    // --- BTN_A kenar algilama (basildigi an yakala) ---
    int btnA = digitalRead(BTN_A);
    bool pressA = (btnA == LOW && prevBtnA == HIGH);
    prevBtnA = btnA;
    
    // ============================================================
    //  STATE: TITLE - Baslik/acilis ekrani
    // ============================================================
    if (state == TITLE) {
        canvas.fillSprite(C_BG);
        
        // PACMAN basligi: golgeli (koyu sari altta, sari ustte)
        canvas.setTextSize(2);
        canvas.setTextColor(tft.color565(50, 50, 0)); // Golve rengi
        canvas.setCursor(45, 11);
        canvas.print("PACMAN");
        canvas.setTextColor(TFT_YELLOW);              // Ana yazi
        canvas.setCursor(44, 10);
        canvas.print("PACMAN");

        // Buyuk Pacman logosu (agiz animasyonlu)
        bool mouthOpen = (millis() / 200) % 2 == 0;
        canvas.fillCircle(80, 50, 15, C_PAC);
        if (mouthOpen) canvas.fillTriangle(80, 50, 100, 35, 100, 65, TFT_BLACK);

        // Menu secenekleri
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(10, 95);
        canvas.print("[A] Basla");

        canvas.setTextColor(0xBDF7);
        canvas.setCursor(90, 95);
        canvas.print("[B] OS Menu");

        canvas.setTextColor(TFT_GREEN);
        canvas.setCursor(10, 110);
        canvas.print("[JOY] Hareket");

        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(92, 110);
        canvas.printf("Rekor: %d", highScore);

        checkScreenshot(canvas); // Dev tools: ekran goruntusu yakala
        canvas.pushSprite(0,0);  // Canvas'i TFT'ye aktar (tek push, titremesiz)
        if (pressA) {
            playSound(880, 50); // Baslama sesi
            resetLevel(true);   // Tamamen yeni oyun baslat
        }
        if (!digitalRead(BTN_B)) { delay(50); returnToOS(); } // FPS60: Debounce 200ms->50ms
        return;
    }
    
    // ============================================================
    //  STATE: GAMEOVER / WIN - Sonuc ekrani
    // ============================================================
    if (state == GAMEOVER || state == WIN) {
        canvas.fillSprite(TFT_BLACK);

        // Sonuc kutusu (kirmizi = gameover, yesil = win)
        canvas.fillRoundRect(15, 6, 130, 120, 5, tft.color565(33, 4, 4));
        canvas.drawRoundRect(15, 6, 130, 120, 5, state==WIN ? TFT_GREEN : TFT_RED);
        canvas.drawRoundRect(16, 7, 128, 118, 4, state==WIN ? tft.color565(0, 128, 0) : 0x8000);

        canvas.setTextSize(2);
        canvas.setTextColor(state==WIN ? TFT_GREEN : TFT_RED);
        canvas.setCursor(state==WIN ? 24 : 20, 14); 
        canvas.print(state==WIN ? "KAZANDIN!" : "OYUN BITTI");

        // Skor gosterimi
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(30, 48);
        canvas.print("Skor:   ");
        canvas.setTextColor(TFT_YELLOW);
        canvas.setTextSize(2);
        canvas.setCursor(75, 42);
        canvas.print(score);

        // Rekor gosterimi
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(30, 68);
        canvas.print("Rekor:  ");
        canvas.setTextColor(TFT_GREEN);
        canvas.setTextSize(2);
        canvas.setCursor(75, 62);
        canvas.print(highScore);

        // Yeni rekor bildirimi (skor >= rekor ve sifirdan buyuk)
        if (score >= highScore && score > 0) {
            canvas.setTextSize(1);
            canvas.setTextColor(TFT_MAGENTA);
            canvas.setCursor(47, 88);
            canvas.print("YENI REKOR!");
        }

        // Menu secenekleri
        canvas.setTextSize(1);
        canvas.setTextColor(0xBDF7);
        canvas.setCursor(30, 104);
        canvas.print("[A] Tekrar Oyna");
        
        canvas.setCursor(30, 114);
        canvas.print("[B] OS Menu");
        
        checkScreenshot(canvas); // Dev tools: ekran goruntusu yakala
        canvas.pushSprite(0,0);
        if (pressA) { delay(50); resetLevel(true); } // FPS60: Debounce 200ms->50ms
        if (!digitalRead(BTN_B)) { delay(50); returnToOS(); } // FPS60: Debounce 200ms->50ms
        return;
    }
    
    // ============================================================
    //  STATE: READY - "HAZIR!" geri sayim (2 sn) sonra PLAYING
    // ============================================================
    if (state == READY) {
        drawMap(); drawHUD();
        drawPacman(pac.x, pac.y, pac.dx, pac.dy);
        for(int i=0; i<NUM_GHOSTS; i++) drawGhost(ghosts[i]);
        canvas.setTextColor(TFT_YELLOW); canvas.setTextSize(1);
        canvas.setCursor(65, 80); canvas.print("HAZIR!");
        checkScreenshot(canvas); // Dev tools: ekran goruntusu yakala
        canvas.pushSprite(0,0);
        if (now - stateTimer > 2000) state = PLAYING; // 2 saniye sonra basla
        return;
    }
    
    // ============================================================
    //  STATE: DYING - Pacman olum animasyonu (kuculen daire)
    //  1.5 sn sonra can azalt, can bittiyse GAMEOVER + rekor kaydet
    // ============================================================
    if (state == DYING) {
        drawMap(); drawHUD();
        // Daire kuculur (3 -> 0 piksel yari cap), ~300ms basina 1 birim
        int r = 3 - (now - stateTimer) / 300;
        if (r > 0) canvas.fillCircle(pac.x, pac.y+MAP_Y, r, C_PAC);
        checkScreenshot(canvas); // Dev tools: ekran goruntusu yakala
        canvas.pushSprite(0,0);
        if (now - stateTimer > 1500) {
            lives--;
            if (lives <= 0) {
                // Rekor yenilendi ise NVS'e kaydet
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
                // Hala can var - karakterleri sifirla, READY'ye don
                resetActors();
                state = READY;
                stateTimer = now;
            }
        }
        return;
    }
    
    // ============================================================
    //  STATE: PLAYING - Aktif oynanis (girdi, hareket, cizim)
    // ============================================================
    if (state == PLAYING) {
        applyPacmanInput(); // Joystick -> istek yonu
        movePacman(dt);     // Hareketi uygula
        
        // Pacman'in bulundugu kareyi kontrol et (dot/power yeme)
        int pc = (int)pac.x / TILE;
        int pr = (int)pac.y / TILE;
        if (pr >= 0 && pr < ROWS && pc >= 0 && pc < COLS) {
            if (gameMap[pr][pc] == 2) {
                // Dot yendi: 10 puan
                gameMap[pr][pc] = 0; score += 10; dotsLeft--;
                playSound(1000, 20); // Kisa yuksek bip
            } else if (gameMap[pr][pc] == 3) {
                // Power pellet yendi: 50 puan + hayaletleri scared yap
                gameMap[pr][pc] = 0; score += 50; dotsLeft--;
                playSound(1500, 100);
                for(int i=0; i<NUM_GHOSTS; i++) {
                    if (ghosts[i].mode != 2) { // Eaten degilse korkut
                        ghosts[i].mode = 1;
                        ghosts[i].scaredUntil = now + 7000; // 7 saniye scared mod
                    }
                }
            }
        }
        
        // Tum dot'lar yenildi -> bolum gec, level artir
        if (dotsLeft == 0) {
            level++;
            // Kullanici istegi uzerine bolum gecis sesi tamamen kaldirildi
            resetLevel(false);
            return;
        }
        
        // Hayaletleri hareket ettir + carpisma kontrolu
        for(int i=0; i<NUM_GHOSTS; i++) {
            moveGhost(ghosts[i], dt);
            
            // Yakinlik kontrolu (~5 piksel = carpisma)
            if (abs(pac.x - ghosts[i].a.x) < 5.0f && abs(pac.y - ghosts[i].a.y) < 5.0f) {
                if (ghosts[i].mode == 1) {
                    // Scared hayalet yenir: 200 puan, eaten moduna gec
                    ghosts[i].mode = 2; 
                    score += 200;
                    playSound(2000, 150); // Yeme sesi
                } else if (ghosts[i].mode == 0) {
                    // Chase hayalet Pacman'i yakaladi -> olum
                    state = DYING;
                    stateTimer = now;
                    playSound(200, 300); // Daha kisa ve net olum sesi
                    break; // Diger hayaletleri kontrol etmeyi birak (Bip bip onleme)
                }
            }
        }
        
        // Cizim (her frame)
        drawMap(); drawHUD();
        drawPacman(pac.x, pac.y, pac.dx, pac.dy);
        for(int i=0; i<NUM_GHOSTS; i++) drawGhost(ghosts[i]);
        checkScreenshot(canvas); // Dev tools: ekran goruntusu yakala
        canvas.pushSprite(0,0);  // Canvas -> TFT (tek push)
        return;
    }
    
    // ============================================================
    //  STATE: PAUSE - Duraklatilmis, karartma + menu
    //  [A] ile devam, [B] ile OS'e don
    // ============================================================
    if (state == PAUSE) {
        // Oncelikle oyun sahnesini ciz
        drawMap(); drawHUD();
        drawPacman(pac.x, pac.y, pac.dx, pac.dy);
        for(int i=0; i<NUM_GHOSTS; i++) drawGhost(ghosts[i]);
        
        // Üzerine karartma ve menü
        canvas.fillRect(30, 36, 100, 56, tft.color565(10, 10, 15)); // Karartma kutusu
        canvas.drawRect(30, 36, 100, 56, TFT_GREEN);                // Yesil cerceve
        
        canvas.setTextSize(2);
        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(50, 42); canvas.print("PAUSE");
        
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(44, 64); canvas.print("[A] Devam Et");
        canvas.setTextColor(tft.color565(180, 180, 180));
        canvas.setCursor(47, 78); canvas.print("[B] OS Menu");
        
        checkScreenshot(canvas); // Dev tools: ekran goruntusu yakala
        canvas.pushSprite(0,0);
        
        // [A] ile devam et
        if (!digitalRead(BTN_A)) {
            playSound(800, 50);
            delay(50); // FPS60: Debounce 200ms->50ms
            state = PLAYING;
        }
        // [B] ile OS'e don
        if (!digitalRead(BTN_B)) {
            playSound(400, 50);
            delay(50); // FPS60: Debounce 200ms->50ms
            returnToOS();
        }
        return;
    }
}

