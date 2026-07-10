#pragma once
// ============================================================
//  E-OS MODE7 RACING — Renderer.h
//  Tum grafik cizim fonksiyonlari: Mode7 pseudo-3D, sprite'lar,
//  HUD, OLED radar, baslik/bitis/pause ekranlari.
// ============================================================

#include <TFT_eSPI.h>
#include <U8g2lib.h>
#include "Config.h"
#include "../hardware_config.h"

// ============================================================
//  renderMode7 — Pseudo-3D yol perspektifi (gokyuzu + tepeler + yol)
//  fb       : Sprite frame buffer pointer'i
//  player   : Oyuncu konumu/acisi
//  animTick : Animasyon sayaci (yol seridi hareketi icin)
// ============================================================
inline void renderMode7(uint16_t* fb, const Racer& player, uint32_t animTick) {
    if (!fb) return;

    float cosA = cosf(player.angle);
    float sinA = sinf(player.angle);

    for (int y = 0; y < HORIZON; y++) {
        float t = (float)y / HORIZON;
        uint16_t skyC = RGB(
            (int)(100 - t * 40),
            (int)(180 - t * 50),
            255
        );
        for (int x = 0; x < SW; x++) fb[y * SW + x] = skyC;
    }

    const float HILL_FOV = 1.4f;
    for (int x = 0; x < SW; x++) {
        float dir = player.angle + ((float)x / SW - 0.5f) * HILL_FOV;
        float h = sinf(dir * 3.0f) * 4.0f + sinf(dir * 7.0f + 1.3f) * 2.5f;
        int hillH = (int)(7.0f + h);
        if (hillH < 1) hillH = 1;
        int hy = HORIZON - hillH;
        if (hy < 0) hy = 0;
        for (int y = hy; y < HORIZON; y++) fb[y * SW + x] = RGB(70, 95, 130);
    }

    float camH = 15.0f;
    float focal = 40.0f;
    float maxDepth = 120.0f;

    for (int y = HORIZON; y < ROAD_H; y++) {
        float row = (float)(y - HORIZON + 1);
        float depth = (camH * focal) / row;

        bool isFoggy = false;
        if (depth > maxDepth) {
            depth = maxDepth;
            isFoggy = true;
        }

        float halfW = depth * 1.0f;

        float wx = player.x + cosA * depth + sinA * halfW;
        float wy = player.y + sinA * depth - cosA * halfW;

        float dx = (-sinA * halfW * 2.0f) / (float)SW;
        float dy = ( cosA * halfW * 2.0f) / (float)SW;

        bool stripe = ((int)(depth * 0.2f + animTick * player.speed * 0.025f)) % 2;

        for (int x = 0; x < SW; x++) {
            if (isFoggy) {
                fb[y * SW + x] = COL_SKY_BOT;
            } else {
                bool outOfBounds = (wx < 0 || wx >= MAP_W || wy < 0 || wy >= MAP_H);
                int mapX = (int)wx;
                int mapY = (int)wy;

                uint16_t col;
                if (outOfBounds) {
                    bool checker = (((int)(wx + 10000) / 4) + ((int)(wy + 10000) / 4)) % 2;
                    col = checker ? COL_GRASS_A : COL_GRASS_B;
                } else {
                    bool onRoad = trackMap[mapY * MAP_W + mapX];
                    if (onRoad) {
                        col = stripe ? COL_ROAD_A : COL_ROAD_B;
                        bool edge = false;
                        int nx1 = constrain(mapX + 1, 0, MAP_W - 1);
                        int nx2 = constrain(mapX - 1, 0, MAP_W - 1);
                        int ny1 = constrain(mapY + 1, 0, MAP_H - 1);
                        int ny2 = constrain(mapY - 1, 0, MAP_H - 1);
                        if (!trackMap[mapY * MAP_W + nx1] || !trackMap[mapY * MAP_W + nx2] ||
                            !trackMap[ny1 * MAP_W + mapX] || !trackMap[ny2 * MAP_W + mapX]) {
                            edge = true;
                        }
                        if (edge) col = stripe ? COL_EDGE_A : COL_EDGE_B;
                    } else {
                        bool checker = ((mapX / 4) + (mapY / 4)) % 2;
                        col = checker ? COL_GRASS_A : COL_GRASS_B;
                    }
                }

                if (depth > 60.0f) col = (col >> 1) & COL_DARKEN_MASK;
                if (depth > 90.0f) col = (col >> 1) & COL_DARKEN_MASK;

                fb[y * SW + x] = col;
            }
            wx += dx;
            wy += dy;
        }
    }
}

// ============================================================
//  renderPlayerCar — Oyuncu araci sprite (ekran alt-ortada sabit)
//  fb       : Sprite frame buffer pointer'i
//  steer    : Direksiyon degeri (-1..1), arac yatay kaymasi
//  isBraking: Fren yapiliyor mu (stop lambasi icin)
// ============================================================
inline void renderPlayerCar(uint16_t* fb, float steer, bool isBraking) {
    int baseY = ROAD_H - 2;
    int cx = SW / 2 + (int)(steer * 12.0f);

    for (int y = baseY - 6; y < baseY; y++) {
        if (y < HORIZON || y >= ROAD_H) continue;
        for (int x = cx - 12; x <= cx + 12; x++) {
            if (x < 0 || x >= SW) continue;
            if (x < cx - 6 || x > cx + 6) {
                fb[y * SW + x] = RGB(15, 15, 15);
            }
        }
    }

    for (int y = baseY - 5; y < baseY; y++) {
        if (y < HORIZON || y >= ROAD_H) continue;
        for (int x = cx - 8; x <= cx + 8; x++) {
            if (x < 0 || x >= SW) continue;
            fb[y * SW + x] = COL_CAR_BODY;
        }
    }

    uint16_t tailColor = isBraking ? RGB(255, 50, 50) : RGB(100, 0, 0);
    for (int x = cx - 7; x <= cx - 4; x++) {
        if (x >= 0 && x < SW) fb[(baseY - 4) * SW + x] = tailColor;
    }
    for (int x = cx + 4; x <= cx + 7; x++) {
        if (x >= 0 && x < SW) fb[(baseY - 4) * SW + x] = tailColor;
    }

    for (int y = baseY - 10; y < baseY - 5; y++) {
        if (y < HORIZON || y >= ROAD_H) continue;
        for (int x = cx - 5; x <= cx + 5; x++) {
            if (x < 0 || x >= SW) continue;
            if (y < baseY - 7) fb[y * SW + x] = RGB(30, 30, 40);
            else fb[y * SW + x] = COL_CAR_BODY;
        }
    }

    int spY = baseY - 11;
    if (spY >= HORIZON) {
        for (int x = cx - 10; x <= cx + 10; x++) {
            if (x >= 0 && x < SW) fb[spY * SW + x] = RGB(220, 220, 220);
        }
        if (cx - 6 >= 0 && cx - 6 < SW) fb[(spY + 1) * SW + cx - 6] = RGB(100, 100, 100);
        if (cx + 6 >= 0 && cx + 6 < SW) fb[(spY + 1) * SW + cx + 6] = RGB(100, 100, 100);
    }
}

// ============================================================
//  renderAICar — AI rakip araci perspektif olcegiyle ciz
//  fb       : Sprite frame buffer pointer'i
//  ai       : AI Racer referansi
//  player   : Oyuncu referansi (kamera uzayi donusumu icin)
//  color    : AI arac rengi (uint16_t)
// ============================================================
inline void renderAICar(uint16_t* fb, const Racer& ai, const Racer& player, uint16_t color) {
    float cosA = cosf(player.angle);
    float sinA = sinf(player.angle);

    float dx = ai.x - player.x;
    float dy = ai.y - player.y;

    float tz = cosA * dx + sinA * dy;
    float tx = -sinA * dx + cosA * dy;

    if (tz < 5.0f || tz > 120.0f) return;

    float focal = 40.0f;
    float camH = 15.0f;
    float focalX = SW / 2.0f;

    int sx = (int)(SW / 2 + (tx / tz) * focalX);
    float scaleY = focal / tz;
    float scaleX = focalX / tz;

    int cw = (int)(scaleX * 5.0f);
    int ch = (int)(scaleY * 3.0f);
    int tireW = (int)(scaleX * 1.3f);
    int cabW = (int)(scaleX * 2.8f);
    cw = constrain(cw, 3, 18);
    ch = constrain(ch, 2, 8);
    tireW = constrain(tireW, 1, 4);
    cabW = constrain(cabW, 2, 10);

    int sy = (int)(HORIZON + (camH * focal) / tz);
    sy = constrain(sy, HORIZON + 5, ROAD_H - 1);

    for (int py = sy - ch; py <= sy; py++) {
        if (py < HORIZON || py >= ROAD_H) continue;
        for (int px = sx - cw / 2; px <= sx + cw / 2; px++) {
            if (px < 0 || px >= SW) continue;
            if (px < sx - cw / 2 + tireW || px > sx + cw / 2 - tireW) {
                fb[py * SW + px] = RGB(15, 15, 15);
            } else {
                fb[py * SW + px] = color;
            }
        }
    }

    int cabH = constrain((int)(scaleY * 2.5f), 1, 5);
    int cabTop = sy - ch - cabH;
    for (int py = cabTop; py < sy - ch; py++) {
        if (py < HORIZON || py >= ROAD_H) continue;
        for (int px = sx - cabW / 2; px <= sx + cabW / 2; px++) {
            if (px < 0 || px >= SW) continue;
            if (py == cabTop) fb[py * SW + px] = color;
            else fb[py * SW + px] = RGB(30, 30, 40);
        }
    }

    int spY = cabTop - constrain((int)(scaleY * 1.5f), 1, 6);
    if (spY >= HORIZON && spY < ROAD_H) {
        for (int px = sx - cw / 2; px <= sx + cw / 2; px++) {
            if (px >= 0 && px < SW) fb[spY * SW + px] = ((color >> 1) & COL_DARKEN_MASK);
        }
        if (spY + 1 >= HORIZON && spY + 1 < ROAD_H) {
            int leg1 = sx - cabW / 2;
            int leg2 = sx + cabW / 2;
            if (leg1 >= 0 && leg1 < SW) fb[(spY + 1) * SW + leg1] = RGB(100, 100, 100);
            if (leg2 >= 0 && leg2 < SW) fb[(spY + 1) * SW + leg2] = RGB(100, 100, 100);
        }
    }
}

// ============================================================
//  renderPosts — Yol kenari direkleri (kirmizi-beyaz cizgili)
//  fb     : Sprite frame buffer pointer'i
//  player : Oyuncu referansi (kamera uzayi icin)
// ============================================================
inline void renderPosts(uint16_t* fb, const Racer& player) {
    float cosA = cosf(player.angle);
    float sinA = sinf(player.angle);

    for (int i = 0; i < NUM_POSTS; i++) {
        float dx = posts[i].x - player.x;
        float dy = posts[i].y - player.y;
        float tz = cosA * dx + sinA * dy;
        if (tz < 3.0f || tz > 120.0f) continue;
        float tx = -sinA * dx + cosA * dy;

        int sx = (int)(SW / 2 + (tx / tz) * (SW / 2.0f));
        if (sx < -4 || sx >= SW + 4) continue;

        int baseY = (int)(HORIZON + (15.0f * 40.0f) / tz);
        baseY = constrain(baseY, HORIZON, ROAD_H - 1);
        int ph = constrain((int)((40.0f / tz) * 7.0f), 2, 32);
        int pw = constrain((int)((SW / 2.0f / tz) * 0.9f), 1, 4);
        int topY = baseY - ph;
        if (topY < HORIZON) topY = HORIZON;

        for (int y = topY; y <= baseY; y++) {
            uint16_t c = (((y / 3) & 1) ? COL_EDGE_A : COL_EDGE_B);
            for (int x = sx - pw / 2; x <= sx + pw / 2; x++) {
                if (x >= 0 && x < SW) fb[y * SW + x] = c;
            }
        }
    }
}

// ============================================================
//  drawHUD — Alt bilgi seridi (hiz, tur, pozisyon, sure, FPS)
//  canvas        : TFT sprite
//  player        : Oyuncu Racer referansi
//  playerLap     : Oyuncunun tamamladigi tur sayisi
//  playerNextCP  : Oyuncunun siradaki checkpoint index'i
//  aiLap         : AI tur dizisi (const int[NUM_AI])
//  aiCheckpoint  : AI checkpoint dizisi (const int[NUM_AI])
//  lastLapTime   : Son tur suresi (ms)
//  bestLapTime   : En iyi tur suresi (ms, 0 = yok)
//  currentFPS    : FPS degeri
//  showFps       : FPS gosterilsin mi
// ============================================================
inline void drawHUD(TFT_eSprite& canvas, const Racer& player, int playerLap,
                    int playerNextCP, const int* aiLap, const int* aiCheckpoint,
                    uint32_t lastLapTime, uint32_t bestLapTime, int currentFPS, bool showFps) {
    canvas.fillRect(0, ROAD_H, SW, HUD_H, COL_HUD_BG);
    canvas.drawFastHLine(0, ROAD_H, SW, RGB(60, 60, 80));

    canvas.setTextSize(1);

    int spdPct = (int)(player.speed / MAX_SPEED * 100);
    spdPct = constrain(spdPct, 0, 100);
    canvas.setTextColor(RGB(80, 255, 80), COL_HUD_BG);

    char spdStr[16];
    snprintf(spdStr, sizeof(spdStr), "%d%%", spdPct);
    int spdW = strlen(spdStr) * 6;
    int spdX = 4 + (35 - spdW) / 2;

    canvas.setCursor(spdX, ROAD_H + 3);
    canvas.print(spdStr);

    int barW = spdPct * 35 / 100;
    canvas.fillRect(4, ROAD_H + 15, barW, 3, RGB(80, 255, 80));
    canvas.fillRect(4 + barW, ROAD_H + 15, 35 - barW, 3, RGB(30, 50, 30));

    canvas.setTextColor(RGB(255, 220, 80), COL_HUD_BG);
    canvas.setCursor(44, ROAD_H + 3);
    int displayedLap = (playerLap + 1 < TOTAL_LAPS) ? (playerLap + 1) : TOTAL_LAPS;
    canvas.printf("LAP:%d/%d", displayedLap, TOTAL_LAPS);

    int pos = 1;
    for (int i = 0; i < NUM_AI; i++) {
        if (aiLap[i] > playerLap) pos++;
        else if (aiLap[i] == playerLap && aiCheckpoint[i] > playerNextCP) pos++;
    }
    canvas.setTextColor(pos == 1 ? RGB(255, 220, 0) : RGB(200, 200, 200), COL_HUD_BG);
    canvas.setCursor(44, ROAD_H + 13);
    canvas.printf("POS:%d/%d", pos, NUM_AI + 1);

    char lapStr[32];
    snprintf(lapStr, sizeof(lapStr), "Lap: %02lu.%02lus", lastLapTime / 1000, (lastLapTime % 1000) / 10);
    int lw = strlen(lapStr) * 6;
    canvas.setTextColor(RGB(180, 180, 180), COL_HUD_BG);
    canvas.setCursor(SW - lw - 2, ROAD_H + 3);
    canvas.print(lapStr);

    char bestStr[32];
    if (bestLapTime > 0) {
        snprintf(bestStr, sizeof(bestStr), "Best:%02lu.%02lus", bestLapTime / 1000, (bestLapTime % 1000) / 10);
    } else {
        snprintf(bestStr, sizeof(bestStr), "Best:--.--s");
    }
    int bw = strlen(bestStr) * 6;
    canvas.setTextColor(RGB(255, 100, 255), COL_HUD_BG);
    canvas.setCursor(SW - bw - 2, ROAD_H + 13);
    canvas.print(bestStr);

    if (showFps) {
        char fpsStr[16];
        snprintf(fpsStr, sizeof(fpsStr), "FPS:%d", currentFPS);
        int fpsW = strlen(fpsStr) * 6;
        canvas.setTextColor(RGB(255, 50, 50));
        canvas.setCursor(SW - fpsW - 2, 2);
        canvas.print(fpsStr);
    }
}

// ============================================================
//  drawTitleScreen — Baslik ekrani (oyun adi + araba ikonu + menu)
//  canvas      : TFT sprite
//  bestLapTime : En iyi tur suresi (ms, 0 = yok)
// ============================================================
inline void drawTitleScreen(TFT_eSprite& canvas, uint32_t bestLapTime) {
    canvas.fillSprite(TFT_BLACK);

    canvas.setTextSize(2);
    const char* title = "MODE 7";
    int titleW = strlen(title) * 12;
    canvas.setTextColor(RGB(255, 200, 0));
    canvas.setCursor((SW - titleW) / 2, 20);
    canvas.print(title);

    int cx = SW / 2;
    canvas.fillTriangle(cx, 45, cx - 7, 58, cx + 7, 58, COL_CAR_BODY);

    canvas.setTextSize(1);

    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(10, 95);
    canvas.print("[A] Start");

    canvas.setTextColor(COL_HUD_BRIGHT);
    canvas.setCursor(90, 95);
    canvas.print("[B] OS Menu");

    if (bestLapTime > 0) {
        canvas.setTextColor(TFT_YELLOW);
        char bestStr[32];
        snprintf(bestStr, sizeof(bestStr), "Best: %lu.%02lu", bestLapTime / 1000, (bestLapTime % 1000) / 10);
        int bestW = strlen(bestStr) * 6;
        canvas.setCursor((SW - bestW) / 2, 110);
        canvas.print(bestStr);
    } else {
        canvas.setTextColor(TFT_YELLOW);
        const char* bestStr = "Best: --.--";
        int bestW = strlen(bestStr) * 6;
        canvas.setCursor((SW - bestW) / 2, 110);
        canvas.print(bestStr);
    }
}

// ============================================================
//  drawFinishScreen — Finish screen (results + best lap + menu)
//  canvas      : TFT sprite
//  pos         : Player finish position (1 = 1st)
//  bestLapTime : Best lap time (ms, 0 = none)
//  newRecord   : New record achieved (for highlight)
// ============================================================
inline void drawFinishScreen(TFT_eSprite& canvas, int pos, uint32_t bestLapTime, bool newRecord) {
    canvas.fillSprite(TFT_BLACK);

    canvas.fillRoundRect(15, 12, 130, 108, 5, COL_PANEL);
    canvas.drawRoundRect(15, 12, 130, 108, 5, TFT_RED);
    canvas.drawRoundRect(16, 13, 128, 106, 4, COL_RED_DARK);

    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    const char* title = "FINISH!";
    int titleW = strlen(title) * 12;
    canvas.setCursor((SW - titleW) / 2, 20);
    canvas.print(title);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 48);
    canvas.print("Pos:   ");
    canvas.setTextColor(TFT_YELLOW);
    canvas.setTextSize(2);
    canvas.setCursor(75, 42);
    canvas.printf("%d/%d", pos, NUM_AI + 1);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 70);
    canvas.print("Best:  ");
    canvas.setTextColor(TFT_GREEN);
    canvas.setTextSize(2);
    canvas.setCursor(75, 64);
    if (bestLapTime > 0)
        canvas.printf("%lu.%01lu", bestLapTime / 1000, (bestLapTime % 1000) / 100);
    else
        canvas.print("--.-");

    // New record achieved, show gold highlight
    if (newRecord && bestLapTime > 0) {
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_YELLOW);
        const char* rec = "NEW RECORD!";
        canvas.setCursor((SW - strlen(rec) * 6) / 2, 84);
        canvas.print(rec);
    }

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(30, 98);
    canvas.print("[A] Retry");
    canvas.setTextColor(COL_HUD_BRIGHT);
    canvas.setCursor(30, 108);
    canvas.print("[B] OS Menu");
}

// ============================================================
//  drawPauseScreen — Pause screen (green framed panel)
//  canvas : TFT sprite
// ============================================================
inline void drawPauseScreen(TFT_eSprite& canvas) {
    osDrawPause(canvas, TFT_GREEN);   // common OS pause box (EN, green theme)
}

// ============================================================
//  drawRadar — OLED secondary screen mini map (128x64)
//  oled       : SH1106 OLED reference
//  player     : Player Racer reference
//  aiCars     : AI opponent array
//  playerLap  : Player lap count
//  aiLap      : AI lap array
// ============================================================
inline void drawRadar(U8G2_SH1106_128X64_NONAME_F_HW_I2C& oled, const Racer& player,
                      const Racer* aiCars, int playerLap, const int* aiLap) {
    (void)aiLap;
    // Dirty flag: only generate I2C traffic if values change
    static int lastLap = -1, lastSpeed = -1, lastPX = -1, lastPY = -1;
    static int lastAIX[NUM_AI] = {-1, -1}, lastAIY[NUM_AI] = {-1, -1};
    int spd = (int)(player.speed / MAX_SPEED * 100);
    int px = (int)player.x, py = (int)player.y;

    bool changed = (lastLap != playerLap || lastSpeed != spd ||
                    abs(lastPX - px) > 2 || abs(lastPY - py) > 2);
    if (!changed) {
        for (int i = 0; i < NUM_AI; i++) {
            int aix = (int)aiCars[i].x, aiy = (int)aiCars[i].y;
            if (abs(lastAIX[i] - aix) > 2 || abs(lastAIY[i] - aiy) > 2) {
                changed = true; break;
            }
        }
    }
    if (!changed) return;

    lastLap = playerLap; lastSpeed = spd;
    lastPX = px; lastPY = py;
    for (int i = 0; i < NUM_AI; i++) {
        lastAIX[i] = (int)aiCars[i].x; lastAIY[i] = (int)aiCars[i].y;
    }
    oled.clearBuffer();

    float cx = MAP_W / 2.0f, cy = MAP_H / 2.0f;
    float ra = MAP_W / 2.0f - 26, rb = MAP_H / 2.0f - 26;
    const int ox = 64, oy = 25;
    const float scX = 0.40f;
    const float scY = 0.30f;

    for (int deg = 0; deg < 360; deg += 3) {
        float rad = deg * M_PI / 180.0f;
        float wob = TRK_WOB(rad);
        int px = ox + (int)((ra * wob * cosf(rad)) * scX);
        int py = oy + (int)((rb * wob * sinf(rad)) * scY);
        if (px >= 0 && px < 128 && py >= 0 && py < 50)
            oled.drawPixel(px, py);
    }

    for (int i = 0; i < NUM_AI; i++) {
        int ax = ox + (int)((aiCars[i].x - cx) * scX);
        int ay = oy + (int)((aiCars[i].y - cy) * scY);
        if (ax >= 1 && ax < 127 && ay >= 1 && ay < 49)
            oled.drawCircle(ax, ay, 2);
    }

    int ppx = ox + (int)((player.x - cx) * scX);
    int ppy = oy + (int)((player.y - cy) * scY);
    oled.drawBox(ppx - 2, ppy - 2, 5, 5);

    oled.drawHLine(0, 51, 128);
    oled.setFont(u8g2_font_6x12_tr);
    oled.setCursor(2, 63);
    int displayedLap = (playerLap + 1 < TOTAL_LAPS) ? (playerLap + 1) : TOTAL_LAPS;
    oled.printf("LAP %d/%d", displayedLap, TOTAL_LAPS);
    oled.setCursor(80, 63);
    oled.printf("SPD %d%%", constrain(spd, 0, 100));

    oled.sendBuffer();
}
