#pragma once

#include <TFT_eSPI.h>
#include <stdio.h>
#include "Config.h"
#include "Tetromino.h"
#include "Board.h"

// ============================================================
//  RENDERER — Tum cizim fonksiyonlari
//  Hedef her zaman canvas sprite'idir (cift tamponlama),
//  dogrudan TFT'ye cizim YAPILMAZ.
// ============================================================

// Grid hucresinin ekran pikseli
inline int cellPx(int cx) { return BOARD_PX_X + cx * CELL; }
inline int cellPy(int cy) { return BOARD_PX_Y + cy * CELL; }

// ------------------------------------------------------------
//  Tek blok cizimi — 6x6 px, ust-sol acik / alt-sag koyu kenar
//  ile hafif 3D gorunum
// ------------------------------------------------------------
inline void drawBlock(TFT_eSprite &s, int px, int py, uint8_t type) {
    s.fillRect(px, py, CELL, CELL, PIECE_COLORS[type]);
    s.drawFastHLine(px, py, CELL, PIECE_COLORS_LT[type]);
    s.drawFastVLine(px, py, CELL, PIECE_COLORS_LT[type]);
    s.drawFastHLine(px, py + CELL - 1, CELL, PIECE_COLORS_DK[type]);
    s.drawFastVLine(px + CELL - 1, py, CELL, PIECE_COLORS_DK[type]);
}

// ------------------------------------------------------------
//  Oyun alani cercevesi + ic grid cizgileri
// ------------------------------------------------------------
inline void drawBoardFrame(TFT_eSprite &s) {
    s.drawRect(BOARD_PX_X - 2, BOARD_PX_Y - 2,
               BOARD_PX_W + 4, BOARD_PX_H + 4, COLOR_FRAME);
    // Soluk ic cizgiler (bloklar uzerini kapatir)
    for (int c = 1; c < BOARD_COLS; c++)
        s.drawFastVLine(BOARD_PX_X + c * CELL, BOARD_PX_Y, BOARD_PX_H, COLOR_GRID);
    for (int r = 1; r < BOARD_ROWS; r++)
        s.drawFastHLine(BOARD_PX_X, BOARD_PX_Y + r * CELL, BOARD_PX_W, COLOR_GRID);
}

// ------------------------------------------------------------
//  Kilitli bloklar (yigin) — temizlenen satirlar flas yapar
//  clearRows/numClear: su an temizlenen satirlar
//  flashOn: true ise temizlenen satirlar beyaz, false ise bos
// ------------------------------------------------------------
inline void drawStack(TFT_eSprite &s, const Board &b,
                      const uint8_t *clearRows, int numClear, bool flashOn) {
    for (int y = 0; y < BOARD_ROWS; y++) {
        // Bu satir temizleniyor mu?
        bool isClearing = false;
        for (int i = 0; i < numClear; i++) {
            if (clearRows[i] == y) { isClearing = true; break; }
        }
        if (isClearing) {
            if (flashOn)
                s.fillRect(cellPx(0), cellPy(y), BOARD_PX_W, CELL, COLOR_FLASH);
            continue;   // flas kapali fazda satir bos gorunur
        }
        for (int x = 0; x < BOARD_COLS; x++) {
            if (b.cells[y][x])
                drawBlock(s, cellPx(x), cellPy(y), b.cells[y][x] - 1);
        }
    }
}

// ------------------------------------------------------------
//  Aktif parca
// ------------------------------------------------------------
inline void drawPiece(TFT_eSprite &s, uint8_t type, uint8_t rot, int px, int py) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (!pieceCell(type, rot, i, j)) continue;
            int cy = py + i;
            if (cy < 0) continue;   // tavan ustu gorunmez
            drawBlock(s, cellPx(px + j), cellPy(cy), type);
        }
    }
}

// ------------------------------------------------------------
//  Hayalet parca — parcanin dusecegi yeri koyu cerceve ile goster
// ------------------------------------------------------------
inline void drawGhost(TFT_eSprite &s, uint8_t type, uint8_t rot, int px, int py) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (!pieceCell(type, rot, i, j)) continue;
            int cy = py + i;
            if (cy < 0) continue;
            s.drawRect(cellPx(px + j), cellPy(cy), CELL, CELL, PIECE_COLORS_DK[type]);
        }
    }
}

// ------------------------------------------------------------
//  Siradaki parca onizlemesi — kutu icinde ortalanmis
// ------------------------------------------------------------
inline void drawNextPreview(TFT_eSprite &s, uint8_t type) {
    s.drawRoundRect(NEXT_BOX_X, NEXT_BOX_Y, NEXT_BOX_W, NEXT_BOX_H, 2, COLOR_FRAME);

    // Parcanin 4x4 kutudaki gercek sinirlarini bul (ortalamak icin)
    int minI = 4, maxI = -1, minJ = 4, maxJ = -1;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (!pieceCell(type, 0, i, j)) continue;
            if (i < minI) minI = i;
            if (i > maxI) maxI = i;
            if (j < minJ) minJ = j;
            if (j > maxJ) maxJ = j;
        }
    }
    int w = (maxJ - minJ + 1) * CELL;
    int h = (maxI - minI + 1) * CELL;
    int ox = NEXT_BOX_X + (NEXT_BOX_W - w) / 2 - minJ * CELL;
    int oy = NEXT_BOX_Y + (NEXT_BOX_H - h) / 2 - minI * CELL;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (pieceCell(type, 0, i, j))
                drawBlock(s, ox + j * CELL, oy + i * CELL, type);
        }
    }
}

// ------------------------------------------------------------
//  Sag panel — siradaki parca + skor + satir + seviye + rekor
// ------------------------------------------------------------
inline void drawPanel(TFT_eSprite &s, uint8_t nextType,
                      int score, int level, int lines, int highScore,
                      int fps, bool showFps) {
    char buf[16];
    s.drawFastVLine(PANEL_DIV_X, 0, SCR_H, COLOR_FRAME);
    s.setTextSize(1);

    // Siradaki parca
    s.setTextColor(COLOR_TEXT_DIM);
    s.setCursor(PANEL_X, 6);
    s.print("NEXT");
    drawNextPreview(s, nextType);

    // Skor
    s.setTextColor(COLOR_TEXT_DIM);
    s.setCursor(PANEL_X, 48);
    s.print("SCORE");
    s.setTextColor(TFT_WHITE);
    s.setCursor(PANEL_X, 57);
    snprintf(buf, sizeof(buf), "%d", score);
    s.print(buf);

    // Satir
    s.setTextColor(COLOR_TEXT_DIM);
    s.setCursor(PANEL_X, 68);
    s.print("LINES");
    s.setTextColor(TFT_WHITE);
    s.setCursor(PANEL_X, 77);
    snprintf(buf, sizeof(buf), "%d", lines);
    s.print(buf);

    // Seviye
    s.setTextColor(COLOR_TEXT_DIM);
    s.setCursor(PANEL_X, 88);
    s.print("LEVEL");
    s.setTextColor(TFT_WHITE);
    s.setCursor(PANEL_X, 97);
    snprintf(buf, sizeof(buf), "%d", level + 1);
    s.print(buf);

    // Rekor
    s.setTextColor(COLOR_TEXT_DIM);
    s.setCursor(PANEL_X, 108);
    s.print("BEST");
    s.setTextColor(TFT_YELLOW);
    s.setCursor(PANEL_X, 117);
    snprintf(buf, sizeof(buf), "%d", highScore);
    s.print(buf);

    // FPS (ayarlardan acildiysa)
    if (showFps) {
        s.setTextColor(COLOR_TEXT_DIM);
        s.setCursor(PANEL_X, 120);
        snprintf(buf, sizeof(buf), "FPS:%d", fps);
        s.print(buf);
    }
}

// ------------------------------------------------------------
//  Ana menu
// ------------------------------------------------------------
inline void drawMenu(TFT_eSprite &s, int highScore) {
    char buf[20];
    s.fillSprite(TFT_BLACK);

    // Baslik: her harf farkli parca renginde (golgeli)
    const char *title = "TETRIS";
    s.setTextSize(2);
    for (int i = 0; i < 6; i++) {
        char c[2] = { title[i], 0 };
        s.setTextColor(COLOR_GRID);                   // golge
        s.setCursor(45 + i * 12, 13);
        s.print(c);
        s.setTextColor(MENU_TITLE_COLORS[i]);
        s.setCursor(44 + i * 12, 12);
        s.print(c);
    }

    s.setTextSize(1);
    s.setTextColor(TFT_WHITE);
    s.setCursor(53, 55);
    s.print("[A] Start");

    s.setCursor(47, 75);
    s.print("[B] OS Menu");

    // Ipuclari + rekor
    s.setTextColor(COLOR_TEXT_DIM);
    s.setCursor(35, 95);
    s.print("[JOY] Move/Drop");
    
    s.setTextColor(TFT_YELLOW);
    snprintf(buf, sizeof(buf), "BEST: %d", highScore);
    s.setCursor(80 - (int)strlen(buf) * 3, 110);   // ortala (6 px/karakter)
    s.print(buf);

    // Dekoratif blok seridi (alt kenar — tetris yigini hissi)
    for (int i = 0; i < 27; i++) {
        if ((i * 7 + 3) % 5 == 0) continue;        // rastgele bosluk hissi
        drawBlock(s, i * CELL, SCR_H - CELL, (i * 3 + 1) % 7);
    }
}

// ------------------------------------------------------------
//  Game Over paneli (oyun sahnesinin uzerine cizilir)
// ------------------------------------------------------------
inline void drawGameOverPanel(TFT_eSprite &s, int score, int lines,
                              int highScore, bool newRecord) {
    // Ortak OS game-over: Score/Lines/Best tablosu + NEW BEST rozeti.
    // Oyun sahnesinin uzerine panel olarak cizilir (fillSprite yok).
    char sb[12], lb[12], hb[12];
    snprintf(sb, sizeof(sb), "%d", score);
    snprintf(lb, sizeof(lb), "%d", lines);
    snprintf(hb, sizeof(hb), "%d", highScore);
    OsStat rows[3] = {
        { "Score", sb, COLOR_TEXT_DIM, TFT_WHITE  },
        { "Lines", lb, COLOR_TEXT_DIM, TFT_WHITE  },
        { "Best",  hb, COLOR_TEXT_DIM, TFT_YELLOW },
    };
    osDrawGameOver(s, false, rows, 3, newRecord ? "NEW BEST!" : nullptr);
}

// ------------------------------------------------------------
//  Pause paneli (oyun sahnesinin uzerine cizilir)
// ------------------------------------------------------------
inline void drawPauseOverlay(TFT_eSprite &s) {
    // Manuel EN pause kutusu. Kontrol semasi: [A] devam, [B] OS menu.
    // (Ortak osDrawPause kullanilmiyor: metinler tetris'e ozel el ile hizali.)
    s.fillRoundRect(30, 38, 100, 52, 5, COLOR_PANEL_BG);
    s.drawRoundRect(30, 38, 100, 52, 5, COLOR_FRAME);
    s.setTextSize(1);
    s.setTextColor(TFT_WHITE);
    const char* t = "PAUSED";
    s.setCursor(80 - (strlen(t) * 6) / 2, 46);
    s.print(t);
    const char* a = "[A] Continue";
    s.setCursor(80 - (strlen(a) * 6) / 2, 62);
    s.print(a);
    s.setTextColor(COLOR_TEXT_DIM);
    const char* b = "[B] OS Menu";
    s.setCursor(80 - (strlen(b) * 6) / 2, 76);
    s.print(b);
}
