#pragma once
// ============================================================
//  Renderer.h — TFT_eSprite cizim fonksiyonlari
//  Harita, HUD, Pacman ve hayaletleri canvas'a cizer.
// ============================================================

#include "Config.h"
#include "Player.h"
#include "Ghosts.h"

// ============================================================
//  drawMap — gameMap'i canvas'a cizer (duvar/dot/power)
//  CELL_WALL = dolu kare + ici acik mavi cerceve
//  CELL_DOT  = karenin merkezinde 2x2 piksel nokta
//  CELL_POWER = merkezde yari cap 3 daire
// ============================================================
inline void drawMap() {
    canvas.fillSprite(COL_BG);
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int x = c * TILE;
            int y = MAP_Y + r * TILE;
            if (gameMap[r][c] == CELL_WALL) {
                canvas.fillRect(x, y, TILE, TILE, COL_WALL);
                canvas.drawRect(x + 1, y + 1, TILE - 2, TILE - 2, COL_WALL_INNER);
            } else if (gameMap[r][c] == CELL_DOT) {
                canvas.fillRect(x + HALF_TILE - 1, y + HALF_TILE - 1, 2, 2, COL_DOT);
            } else if (gameMap[r][c] == CELL_POWER) {
                canvas.fillCircle(x + HALF_TILE, y + HALF_TILE, 3, COL_POWER);
            }
        }
    }
}

// ============================================================
//  drawHUD — Ust bilgi seridini cizer (skor, level, canlar)
// ============================================================
inline void drawHUD() {
    canvas.fillRect(0, 0, SCR_W, HUD_H, COL_HUD_BG);
    canvas.drawFastHLine(0, HUD_H - 1, SCR_W, COL_WALL);

    canvas.setTextSize(1);
    canvas.setTextColor(COL_PAC);
    canvas.setCursor(2, 4);
    canvas.printf("SC:%05d", score);

    canvas.setTextColor(COL_LIGHTGREY);
    canvas.setCursor(65, 4);
    canvas.printf("LV:%d", level);

    for (int i = 0; i < lives; i++) {
        canvas.fillCircle(SCR_W - 10 - i * 8, 7, 3, COL_PAC);
    }
}

// ============================================================
//  drawPacman — Pacman'i cizer (agiz animasyonlu)
//  150ms'de bir agiz ac/kapat
// ============================================================
inline void drawPacman(float x, float y, int dx, int dy) {
    bool mouthOpen = (millis() / 150) % 2 == 0;
    canvas.fillCircle(x, y + MAP_Y, 3, COL_PAC);
    if (mouthOpen) {
        if (dx == 1)       canvas.fillTriangle(x, y + MAP_Y, x + 4, y + MAP_Y - 3, x + 4, y + MAP_Y + 3, COL_BG);
        else if (dx == -1) canvas.fillTriangle(x, y + MAP_Y, x - 4, y + MAP_Y - 3, x - 4, y + MAP_Y + 3, COL_BG);
        else if (dy == 1)  canvas.fillTriangle(x, y + MAP_Y, x - 3, y + MAP_Y + 4, x + 3, y + MAP_Y + 4, COL_BG);
        else if (dy == -1) canvas.fillTriangle(x, y + MAP_Y, x - 3, y + MAP_Y - 4, x + 3, y + MAP_Y - 4, COL_BG);
    }
}

// ============================================================
//  drawGhost — Hayaleti cizer (moda gore renk degisir)
//  SCARED: mavi govde + son 2 sn beyaz yanip soner
//  EATEN:  acik mavi govde, goz yok
// ============================================================
inline void drawGhost(const Ghost &g) {
    uint16_t c = g.color;
    if (g.mode == GHOST_SCARED) {
        if (g.scaredUntil - millis() < SCARED_WARN && (millis() / 200) % 2 == 0)
            c = TFT_WHITE;
        else
            c = COL_SCARED_BLUE;
    } else if (g.mode == GHOST_EATEN) {
        c = COL_EATEN_BLUE;
    }

    int ix = (int)g.a.x;
    int iy = (int)g.a.y + MAP_Y;

    // Govde
    canvas.fillRect(ix - 3, iy - 1, 7, 5, c);
    canvas.fillCircle(ix, iy - 2, 3, c);
    canvas.fillRect(ix - 3, iy + 4, 2, 2, c);
    canvas.fillRect(ix, iy + 4, 2, 2, c);
    canvas.fillRect(ix + 2, iy + 4, 2, 2, c);

    // Gozler (eaten modda yok)
    if (g.mode != GHOST_EATEN) {
        canvas.fillRect(ix - 2, iy - 3, 2, 2, TFT_WHITE);
        canvas.fillRect(ix + 1, iy - 3, 2, 2, TFT_WHITE);
        canvas.drawPixel(ix - 1 + g.a.dx, iy - 2 + g.a.dy, TFT_BLUE);
        canvas.drawPixel(ix + 2 + g.a.dx, iy - 2 + g.a.dy, TFT_BLUE);
    }
}
