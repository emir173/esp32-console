#pragma once
// ============================================================
//  Renderer.h — Tum TFT_eSprite cizim fonksiyonlari
// ============================================================
#include "Config.h"

// drawStars — Arkaplan yildizlarini canvas'a cizer
inline void drawStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        uint16_t c = (stars[i].layer == 0) ? COL_STAR_DIM :
                     (stars[i].layer == 1) ? COL_STAR_BRI : COL_STAR_GOLD;
        canvas.drawPixel((int)stars[i].x, (int)stars[i].y, c);
    }
}

// drawShip — Oyuncu uzay gemisini canvas'a cizer
inline void drawShip() {
    int sx = (int)ship.x;
    int sy = (int)ship.y;

    if (ship.invincTimer > 0.0f && ((int)(ship.invincTimer * 10.0f) % 2)) return;

    // 1) Kanatlar (Geriye kivrik aerodinamik yapi)
    canvas.fillTriangle(sx, sy - 3, sx - 6, sy + 4, sx + 6, sy + 4, COL_SHIP_A);
    canvas.drawTriangle(sx, sy - 3, sx - 6, sy + 4, sx + 6, sy + 4, COL_SHIP_B);

    // 2) Ana Govde (Merkez)
    canvas.fillRect(sx - 1, sy - 6, 3, 10, COL_SHIP_B);
    canvas.drawPixel(sx, sy - 7, COL_SHIP_A); // Sivri burun

    // 3) Kokpit cami
    canvas.fillRect(sx - 1, sy - 2, 3, 3, RGB(200, 230, 255));
    canvas.drawPixel(sx, sy - 1, RGB(255, 255, 255)); // Cama vuran parlama

    // 4) Kanat ucu lazer namlulari
    canvas.drawFastVLine(sx - 6, sy, 3, RGB(255, 80, 80));
    canvas.drawFastVLine(sx + 6, sy, 3, RGB(255, 80, 80));

    // 5) Motor atesi (Animasyonlu ve iki katmanli)
    if (animFrame % 3 != 0) {
        int flameH = 2 + (animFrame % 2);
        canvas.fillRect(sx - 1, sy + 4, 3, flameH, COL_SHIP_ENG);
        canvas.drawPixel(sx, sy + 4 + flameH, RGB(255, 255, 100)); // Sari ates ucu
    }

    if (ship.shieldTimer > 0.0f) {
        canvas.drawCircle(sx, sy, 8, RGB(60, 160, 255));
        if (animFrame % 4 < 2) canvas.drawCircle(sx, sy, 9, RGB(30, 100, 200));
    }
}

// drawBullets — Tum aktif mermileri canvas'a cizer
inline void drawBullets() {
    for (int i = 0; i < MAX_P_BULLETS; i++) {
        if (!pBullets[i].active) continue;
        canvas.fillRect((int)pBullets[i].x - 1, (int)pBullets[i].y - 1, 2, 3, COL_BULLET_P);
    }
    for (int i = 0; i < MAX_E_BULLETS; i++) {
        if (!eBullets[i].active) continue;
        canvas.fillCircle((int)eBullets[i].x, (int)eBullets[i].y, 2, COL_BULLET_E);
    }
}

// drawEnemies — Tum aktif dusmanlari tiplerine gore canvas'a cizer
inline void drawEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        Enemy& e = enemies[i];
        int sx = (int)e.x;
        int sy = (int)e.y;

        switch (e.type) {
            case EN_BASIC:
                canvas.fillTriangle(sx, sy + 4, sx - 4, sy - 3, sx + 4, sy - 3, COL_EN_BASIC);
                canvas.drawPixel(sx - 1, sy, COL_WHITE);
                canvas.drawPixel(sx + 1, sy, COL_WHITE);
                break;
            case EN_FAST:
                canvas.fillTriangle(sx, sy + 3, sx - 3, sy - 3, sx + 3, sy - 3, COL_EN_FAST);
                canvas.drawPixel(sx, sy, COL_WHITE);
                break;
            case EN_TANK:
                canvas.fillRect(sx - 4, sy - 4, 9, 8, COL_EN_TANK);
                canvas.drawRect(sx - 4, sy - 4, 9, 8, RGB(60, 150, 60));
                canvas.fillRect(sx - 1, sy + 2, 3, 3, RGB(200, 200, 200));
                break;
            case EN_BOSS:
                canvas.fillRect(sx - 8, sy - 6, 17, 12, COL_EN_BOSS);
                canvas.drawRect(sx - 8, sy - 6, 17, 12, RGB(150, 40, 150));
                canvas.fillRect(sx - 4, sy - 3, 3, 3, RGB(255, 80, 80));
                canvas.fillRect(sx + 2, sy - 3, 3, 3, RGB(255, 80, 80));
                int bw = 16 * e.hp / e.maxHp;
                canvas.fillRect(sx - 8, sy - 9, bw, 2, RGB(255, 60, 60));
                canvas.drawRect(sx - 8, sy - 9, 16, 2, RGB(100, 100, 100));
                break;
        }
    }
}

// drawExplosions — Patlama efektlerini canvas'a cizer
inline void drawExplosions() {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) continue;
        int f = (int)(explosions[i].elapsed * 30.0f);
        int r = 3 + f;
        uint16_t c = (f < 4) ? COL_BOOM_A : (f < 8) ? COL_BOOM_B : COL_BOOM_C;
        canvas.drawCircle((int)explosions[i].x, (int)explosions[i].y, r, c);
        if (f < 6) canvas.fillCircle((int)explosions[i].x, (int)explosions[i].y, r/2, c);
    }
}

// drawPowerUps — Power-up'lari canvas'a cizer
inline void drawPowerUps() {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerUps[i].active) continue;
        int sx = (int)powerUps[i].x;
        int sy = (int)powerUps[i].y;
        uint16_t c;
        char label;
        switch (powerUps[i].type) {
            case PWR_TRIPLE: c = COL_PWR_TRIPLE; label = 'T'; break;
            case PWR_SHIELD: c = COL_PWR_SHIELD; label = 'S'; break;
            case PWR_LIFE:   c = COL_PWR_LIFE;   label = '+'; break;
            default:         c = COL_WHITE;         label = '?'; break;
        }
        canvas.fillRect(sx - 4, sy - 4, 9, 9, c);
        canvas.drawRect(sx - 4, sy - 4, 9, 9, COL_WHITE);
        canvas.setTextColor(COL_BLACK);
        canvas.setCursor(sx - 2, sy - 3);
        canvas.print(label);
    }
}

// drawHUD — Alt bilgi cubugunu (HUD) canvas'a cizer
inline void drawHUD() {
    canvas.fillRect(0, SCR_H - HUD_H, SCR_W, HUD_H, COL_HUD_BG);
    canvas.drawFastHLine(0, SCR_H - HUD_H, SCR_W, RGB(40, 40, 80));

    canvas.setTextSize(1);
    canvas.setTextColor(COL_HUD_TXT);

    canvas.setCursor(2, SCR_H - HUD_H + 2);
    canvas.printf("SCR:%d", ship.score);

    for (int i = 0; i < ship.hp; i++) {
        canvas.fillRect(50 + i * 7, SCR_H - HUD_H + 4, 5, 5, RGB(255, 60, 60));
    }

    char wStr[16];
    snprintf(wStr, sizeof(wStr), "W:%d", curWave + 1);
    int wLen = strlen(wStr) * 6;
    canvas.setTextColor(RGB(100, 255, 100));
    canvas.setCursor(158 - wLen, SCR_H - HUD_H + 2);
    canvas.print(wStr);

    if (showFps) {
        char fpsStr[16];
        snprintf(fpsStr, sizeof(fpsStr), "FPS:%d", currentFPS);
        int fpsW = strlen(fpsStr) * 6;
        int fpsX = 158 - wLen - fpsW - 4;
        canvas.setTextColor(RGB(100, 255, 100));
        canvas.setCursor(fpsX, SCR_H - HUD_H + 2);
        canvas.print(fpsStr);
    }

    if (ship.tripleTimer > 0.0f) {
        canvas.fillRect(88, SCR_H - HUD_H + 4, 3, 5, COL_PWR_TRIPLE);
    }
    if (ship.shieldTimer > 0.0f) {
        canvas.fillRect(93, SCR_H - HUD_H + 4, 3, 5, COL_PWR_SHIELD);
    }
}

// drawTitle — Baslik ekranini cizer
inline void drawTitle() {
    canvas.fillSprite(COL_BG);
    drawStars();

    canvas.setTextSize(2);
    const char* t1 = "GALACTIC";
    const char* t2 = "STRIKE";
    int w1 = strlen(t1) * 12;
    int w2 = strlen(t2) * 12;

    canvas.setTextColor(RGB(20, 60, 120));
    canvas.setCursor((SCR_W - w1) / 2 + 1, 16);
    canvas.print(t1);

    canvas.setTextColor(RGB(80, 200, 255));
    canvas.setCursor((SCR_W - w1) / 2, 15);
    canvas.print(t1);

    canvas.setTextColor(RGB(255, 200, 60));
    canvas.setCursor((SCR_W - w2) / 2, 38);
    canvas.print(t2);

    int dx = SCR_W / 2, dy = 64;
    // 1) Kanatlar (Geriye kivrik aerodinamik yapi)
    canvas.fillTriangle(dx, dy - 3, dx - 6, dy + 4, dx + 6, dy + 4, COL_SHIP_A);
    canvas.drawTriangle(dx, dy - 3, dx - 6, dy + 4, dx + 6, dy + 4, COL_SHIP_B);

    // 2) Ana Govde (Merkez)
    canvas.fillRect(dx - 1, dy - 6, 3, 10, COL_SHIP_B);
    canvas.drawPixel(dx, dy - 7, COL_SHIP_A); // Sivri burun

    // 3) Kokpit cami
    canvas.fillRect(dx - 1, dy - 2, 3, 3, RGB(200, 230, 255));
    canvas.drawPixel(dx, dy - 1, RGB(255, 255, 255)); // Cama vuran parlama

    // 4) Kanat ucu lazer namlulari
    canvas.drawFastVLine(dx - 6, dy, 3, RGB(255, 80, 80));
    canvas.drawFastVLine(dx + 6, dy, 3, RGB(255, 80, 80));

    // 5) Motor atesi (Animasyonlu ve iki katmanli)
    if (animFrame % 3 != 0) {
        int flameH = 2 + (animFrame % 2);
        canvas.fillRect(dx - 1, dy + 4, 3, flameH, COL_SHIP_ENG);
        canvas.drawPixel(dx, dy + 4 + flameH, RGB(255, 255, 100)); // Sari ates ucu
    }

    canvas.setTextSize(1);
    canvas.setTextColor(COL_HUD_BRIGHT);
    canvas.setCursor(53, 95);
    canvas.print("[A] Start");

    canvas.setCursor(47, 105);
    canvas.print("[B] OS Menu");

    canvas.setCursor(50, 115);
    canvas.print("[JOY] Move");

    canvas.setTextColor(TFT_YELLOW);
    char rekorBuf[20];
    snprintf(rekorBuf, sizeof(rekorBuf), "Best: %d", highScore);
    int txtW = strlen(rekorBuf) * 6;
    canvas.setCursor((160 - txtW) / 2, 140);
    canvas.print(rekorBuf);

    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}

// drawGameOver — Oyun bitti ekranini cizer
inline void drawGameOver() {
    canvas.fillSprite(COL_BG);
    drawStars();

    // Ortak OS game-over: Score/Wave/Best tablosu + NEW BEST rozeti.
    // Yildiz alani arka planda kalir.
    char sb[12], wb[12], hb[12];
    snprintf(sb, sizeof(sb), "%d", ship.score);
    snprintf(wb, sizeof(wb), "%d", curWave + 1);
    snprintf(hb, sizeof(hb), "%d", highScore);
    OsStat rows[3] = {
        { "Score", sb, TFT_WHITE, TFT_YELLOW },
        { "Wave",  wb, TFT_WHITE, TFT_CYAN   },
        { "Best",  hb, TFT_WHITE, TFT_GREEN  },
    };
    bool newRecord = (ship.score >= highScore && ship.score > 0);
    osDrawGameOver(canvas, false, rows, 3, newRecord ? "NEW BEST!" : nullptr);

    checkScreenshot(canvas);
    canvas.pushSprite(0, 0);
}

// updateStars — Yildizlari asagiya kaydirir (parallax arkaplan)
//  dt: Gecen sure (s)
inline void updateStars(float dt) {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].y += stars[i].speed * dt;
        if (stars[i].y > SCR_H) {
            stars[i].y = 0;
            stars[i].x = random(0, SCR_W);
        }
    }
}
