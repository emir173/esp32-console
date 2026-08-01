#pragma once
// ============================================================
//  E-OS PLATFORMER — Renderer.h
//  Tüm TFT_eSprite çizim fonksiyonları.
// ============================================================
#include "Physics.h"
#include "../dev_tools.h"

// ============ Extern (Renderer'a özel) ============
extern uint32_t animTick;
extern int currentFPS;
extern bool showFps;

// ============ Tile Çizimi ============
inline void drawTiles() {
    int startCol = camX / TILE;
    int endCol = min(startCol + SCR_W / TILE + 2, MAP_W);

    for (int r = 0; r < MAP_H; r++) {
        int sy = r * TILE;
        if (sy + TILE < 0 || sy >= SCR_H) continue;

        for (int c = startCol; c < endCol; c++) {
            int sx = c * TILE - camX;
            if (sx + TILE < 0 || sx >= SCR_W) continue;

            uint8_t t = mapData[r][c];
            if (t == T_AIR) continue;

            if (t == T_GROUND) {
                canvas.fillRect(sx, sy, TILE, TILE, COL_GRASS);
                canvas.drawFastHLine(sx, sy, TILE, COL_GRASS_TOP);
                canvas.drawFastHLine(sx, sy + 1, TILE, COL_GRASS_TOP);
                if ((c + r) % 3 == 0) canvas.drawPixel(sx + 3, sy, RGB(90, 230, 110));
            }
            else if (t == T_BRICK) {
                canvas.fillRect(sx, sy, TILE, TILE, COL_BRICK_A);
                canvas.drawFastHLine(sx, sy + 3, TILE, COL_BRICK_B);
                canvas.drawFastVLine(sx + 3, sy, 3, COL_BRICK_B);
                canvas.drawFastVLine(sx + 6, sy + 4, 4, COL_BRICK_B);
            }
            else if (t == T_SPIKE) {
                for (int dy = 0; dy < TILE; dy++) {
                    int hw = dy * TILE / (2 * TILE);
                    int cx = sx + TILE / 2;
                    canvas.drawFastHLine(cx - hw, sy + dy, hw * 2 + 1, COL_SPIKE);
                }
                canvas.drawPixel(sx + TILE/2, sy, COL_SPIKE_TIP);
                canvas.drawPixel(sx + TILE/2, sy + 1, COL_SPIKE_TIP);
            }
            else if (t == T_COIN) {
                bool sparkle = (animTick / 12) % 2;
                uint16_t cc = sparkle ? COL_COIN_B : COL_COIN_A;
                canvas.fillCircle(sx + 3, sy + 3, 2, cc);
                canvas.drawPixel(sx + 2, sy + 2, COL_COIN_B);
            }
            else if (t == T_FLAG) {
                canvas.drawFastVLine(sx + 1, sy, TILE, COL_FLAG_POLE);
                int wave = (animTick / 8) % 2;
                canvas.fillRect(sx + 2, sy + wave, 5, 4, COL_FLAG_RED);
            }
        }
    }
}

// ============ Oyuncu Çizimi ============
inline void drawPlayer() {
    int sx = (int)plr.x - camX;
    int sy = (int)plr.y;

    if (plr.invincTimer > 0.0f && ((int)plr.invincTimer / 3) % 2) return;

    canvas.fillRect(sx, sy, PW, PH, COL_PLR_A);
    canvas.drawRect(sx, sy, PW, PH, COL_PLR_B);

    int eyeX = plr.facingRight ? sx + 3 : sx + 1;
    canvas.fillRect(eyeX, sy + 2, 2, 2, COL_WHITE);
    canvas.drawPixel(eyeX + (plr.facingRight ? 1 : 0), sy + 3, COL_BLACK);
}

// ============ Düşman Çizimi ============
inline void drawEnemies() {
    for (int i = 0; i < numEnemies; i++) {
        if (!enemies[i].active) continue;
        Enemy& e = enemies[i];
        int sx = (int)e.x - camX;
        int sy = (int)e.y;
        if (sx < -8 || sx > SCR_W + 8) continue;

        canvas.fillRect(sx, sy, 7, 7, COL_ENEMY_A);
        canvas.drawRect(sx, sy, 7, 7, COL_ENEMY_B);
        canvas.fillRect(sx + 1, sy + 2, 2, 2, COL_WHITE);
        canvas.fillRect(sx + 4, sy + 2, 2, 2, COL_WHITE);
        canvas.drawPixel(sx + 2, sy + 3, COL_BLACK);
        canvas.drawPixel(sx + 5, sy + 3, COL_BLACK);
    }
}

// ============ HUD Çizimi ============
inline void drawHUD() {
    canvas.fillRect(0, 0, SCR_W, HUD_H, COL_HUD_BG);
    canvas.setTextSize(1);

    canvas.setTextColor(COL_HUD_TXT);
    char scrBuf[16];
    snprintf(scrBuf, sizeof(scrBuf), "SCOR:%d", plr.score);
    int scrW = strlen(scrBuf) * 6;
    canvas.setCursor(2, 1);
    canvas.print(scrBuf);

    canvas.setTextColor(COL_HUD_COIN);
    int coinX = 2 + scrW + 12;
    canvas.setCursor(coinX, 1);
    canvas.printf("x%d", plr.coins);
    canvas.fillCircle(coinX - 5, 4, 2, COL_COIN_A);
    canvas.fillCircle(coinX - 5, 4, 1, COL_COIN_B);
    
    if (showFps) {
        canvas.setTextColor(COL_DEAD_TEXT);
        canvas.setCursor(80, 1);
        canvas.printf("FPS:%d", currentFPS);
    }

    for (int i = 0; i < plr.lives; i++) {
        canvas.fillRect(125 + i * 10, 2, 6, 6, RGB(255, 60, 60));
    }

    canvas.drawFastHLine(0, HUD_H - 1, SCR_W, COL_HUD_LINE);
}

// ============ Başlık/Menü Ekranı ============
inline void drawTitleScreen() {
    canvas.fillSprite(COL_SKY);

    canvas.setTextSize(2);
    const char* title = "PLATFORM";
    int tw = strlen(title) * 12;
    canvas.setTextColor(RGB(20, 60, 120));
    canvas.setCursor((SCR_W - tw)/2 + 1, 21);
    canvas.print(title);

    canvas.setTextColor(COL_PLR_A);
    canvas.setCursor((SCR_W - tw)/2, 20);
    canvas.print(title);

    int demoX = (SCR_W - (PW * 2)) / 2;
    int demoY = 55;
    canvas.fillRect(demoX, demoY, PW * 2, PH * 2, COL_PLR_A);
    canvas.drawRect(demoX, demoY, PW * 2, PH * 2, COL_PLR_B);
    canvas.fillRect(demoX + 7, demoY + 3, 3, 3, COL_WHITE);
    canvas.drawPixel(demoX + 9, demoY + 5, COL_BLACK);

    for (int x = 0; x < SCR_W; x += TILE) {
        canvas.fillRect(x, 78, TILE, TILE, COL_GRASS);
        canvas.drawFastHLine(x, 78, TILE, COL_GRASS_TOP);
    }

    canvas.setTextSize(1);
    canvas.setTextColor(COL_WHITE);
    canvas.setCursor(53, 95);
    canvas.print("[A] Start");

    canvas.setTextColor(COL_BTN_B);
    canvas.setCursor(47, 105);
    canvas.print("[B] OS Menu");

    if (highScore > 0) {
        canvas.setTextColor(COL_HUD_COIN);
        char rekorBuf[20];
        snprintf(rekorBuf, sizeof(rekorBuf), "Best: %d", highScore);
        int txtW = strlen(rekorBuf) * 6;
        canvas.setCursor((160 - txtW) / 2, 115);
        canvas.print(rekorBuf);
    }
}

// ============ Game Over Ekranı ============
inline void drawGameOverScreen() {
    canvas.fillSprite(COL_BLACK);

    // Ortak OS game-over: Score/Gold/Best tablosu + NEW BEST rozeti
    char sb[12], gb[12], hb[12];
    snprintf(sb, sizeof(sb), "%d", plr.score);
    snprintf(gb, sizeof(gb), "%d", plr.coins);
    snprintf(hb, sizeof(hb), "%d", highScore);
    OsStat rows[3] = {
        { "Score", sb, COL_WHITE, COL_HUD_TXT        },
        { "Gold",  gb, COL_WHITE, COL_HUD_COIN       },
        { "Best",  hb, COL_WHITE, RGB(100, 255, 100) },
    };
    bool newRecord = (plr.score >= highScore && plr.score > 0);
    osDrawGameOver(canvas, false, rows, 3, newRecord ? "NEW BEST!" : nullptr);
}

// ============ Kazanma Ekranı ============
inline void drawWinScreen() {
    canvas.fillSprite(COL_BLACK);

    // Ortak OS win ekrani (yesil cerceve): Score/Gold/Best tablosu + NEW BEST
    char sb[12], gb[12], hb[12];
    snprintf(sb, sizeof(sb), "%d", plr.score);
    snprintf(gb, sizeof(gb), "%d", plr.coins);
    snprintf(hb, sizeof(hb), "%d", highScore);
    OsStat rows[3] = {
        { "Score", sb, COL_WHITE, COL_HUD_TXT      },
        { "Gold",  gb, COL_WHITE, COL_HUD_COIN     },
        { "Best",  hb, COL_WHITE, RGB(255, 220, 0) },
    };
    bool newRecord = (plr.score >= highScore && plr.score > 0);
    osDrawGameOver(canvas, true, rows, 3, newRecord ? "NEW BEST!" : nullptr);
}

// ============ Pause Overlay ============
inline void drawPauseOverlay() {
    osDrawPause(canvas, TFT_GREEN);   // ortak OS pause kutusu (EN, yesil tema)
}
