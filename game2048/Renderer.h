#pragma once

#include "Config.h"
#include "Board.h"

// ============================================================
//  Renderer — tum cizim fonksiyonlari (TFT_eSprite canvas'a)
//  Ortak overlay'ler (osDrawPause/osDrawGameOver) GameBase.h'de;
//  .ino dogrudan cagirir.
// ============================================================

inline uint16_t tileColor(uint8_t v) {
    return TILE_COLORS[(v <= 16) ? v : 16];
}

// 2 ve 4 acik zeminli -> koyu rakam; digerleri beyaz
inline uint16_t tileTextColor(uint8_t v) {
    return (v <= 2) ? COL_TXT_DARK : TFT_WHITE;
}

// Us -> ekrana yazilacak deger ("2".."2048", 16384 -> "16K")
inline void tileValueStr(uint8_t v, char *buf, size_t n) {
    unsigned long val = 1UL << v;
    if (val >= 10000) snprintf(buf, n, "%luK", val / 1000);
    else              snprintf(buf, n, "%lu", val);
}

inline int tilePX(int c) { return BOARD_X + GAP + c * (CELL + GAP); }
inline int tilePY(int r) { return BOARD_Y + GAP + r * (CELL + GAP); }

// ------------------------------------------------------------
//  drawTile — tek karo (px,py = hucrenin sol-ust koses i)
//  inflate: >0 birlesme pop'u (buyur), <0 spawn (kucukten buyur)
// ------------------------------------------------------------
inline void drawTile(TFT_eSprite &cv, int px, int py, uint8_t v, int inflate = 0) {
    if (!v) return;
    int s = CELL + 2 * inflate;
    if (s < 7) s = 7;
    int x = px + (CELL - s) / 2;
    int y = py + (CELL - s) / 2;
    cv.fillRoundRect(x, y, s, s, 4, tileColor(v));

    if (s < CELL - 6) return;   // cok kucukken rakam sigmaz, cizme

    char buf[8];
    tileValueStr(v, buf, sizeof(buf));
    int ts = (strlen(buf) <= 2) ? 2 : 1;
    cv.setTextSize(ts);
    cv.setTextColor(tileTextColor(v));
    int tw = cv.textWidth(buf);
    int th = ts * 8;
    cv.setCursor(px + (CELL - tw) / 2, py + (CELL - th) / 2 + 1);
    cv.print(buf);
}

// ------------------------------------------------------------
//  drawBoardBg — arka plan + tahta cercevesi + bos hucreler
// ------------------------------------------------------------
inline void drawBoardBg(TFT_eSprite &cv) {
    cv.fillSprite(COL_BG);
    cv.fillRoundRect(BOARD_X - 2, BOARD_Y - 2, BOARD_W + 4, BOARD_W + 4, 5, COL_BOARD_FRAME);
    cv.fillRoundRect(BOARD_X, BOARD_Y, BOARD_W, BOARD_W, 4, COL_BOARD_BG);
    for (int r = 0; r < BOARD_N; r++)
        for (int c = 0; c < BOARD_N; c++)
            cv.fillRoundRect(tilePX(c), tilePY(r), CELL, CELL, 4, COL_CELL_EMPTY);
}

// ------------------------------------------------------------
//  drawStaticTiles — sabit tahta (pop/spawn animasyonlariyla)
// ------------------------------------------------------------
inline void drawStaticTiles(TFT_eSprite &cv, const uint8_t grid[BOARD_N][BOARD_N],
                            const unsigned long mergeMs[BOARD_N][BOARD_N],
                            const unsigned long spawnMs[BOARD_N][BOARD_N],
                            unsigned long now) {
    for (int r = 0; r < BOARD_N; r++) {
        for (int c = 0; c < BOARD_N; c++) {
            uint8_t v = grid[r][c];
            if (!v) continue;
            int inflate = 0;
            if (spawnMs[r][c] && now - spawnMs[r][c] < SPAWN_MS) {
                float t = (float)(now - spawnMs[r][c]) / (float)SPAWN_MS;
                inflate = -(int)((1.0f - t) * 8.0f);
            } else if (mergeMs[r][c] && now - mergeMs[r][c] < POP_MS) {
                float t = (float)(now - mergeMs[r][c]) / (float)POP_MS;
                inflate = (int)((1.0f - t) * 3.0f);
            }
            drawTile(cv, tilePX(c), tilePY(r), v, inflate);
        }
    }
}

// ------------------------------------------------------------
//  drawAnimTiles — kayan karolar (t: 0..1, smoothstep ease)
// ------------------------------------------------------------
inline void drawAnimTiles(TFT_eSprite &cv, const AnimTile *anims, uint8_t count, float t) {
    float tt = t * t * (3.0f - 2.0f * t);
    for (uint8_t i = 0; i < count; i++) {
        const AnimTile &a = anims[i];
        int fx = tilePX(a.fc), fy = tilePY(a.fr);
        int tx = tilePX(a.tc), ty = tilePY(a.tr);
        int px = fx + (int)((tx - fx) * tt);
        int py = fy + (int)((ty - fy) * tt);
        drawTile(cv, px, py, a.v);
    }
}

// ------------------------------------------------------------
//  Panel yardimcisi — metni panel seridinde yatay ortala
// ------------------------------------------------------------
inline void panelCenter(TFT_eSprite &cv, const char *s, int y, uint16_t col) {
    cv.setTextSize(1);
    cv.setTextColor(col);
    int tw = cv.textWidth(s);
    cv.setCursor(PANEL_X + (PANEL_W - tw) / 2, y);
    cv.print(s);
}

// ------------------------------------------------------------
//  drawPanel — sag serit: SCORE / BEST / +N / TOP mini karo
// ------------------------------------------------------------
inline void drawPanel(TFT_eSprite &cv, long score, long best, uint8_t maxV,
                      int gainVal, unsigned long gainMs, unsigned long now) {
    char buf[12];

    panelCenter(cv, "SCORE", 10, COL_GRAY_TXT);
    snprintf(buf, sizeof(buf), "%ld", score);
    panelCenter(cv, buf, 20, TFT_WHITE);

    panelCenter(cv, "BEST", 36, COL_GRAY_TXT);
    long shownBest = (score > best) ? score : best;
    snprintf(buf, sizeof(buf), "%ld", shownBest);
    panelCenter(cv, buf, 46, (score > best && score > 0) ? TFT_GREEN : COL_GOLD);

    // Son hamlenin puani (kisa sure gorunur)
    if (gainVal > 0 && now - gainMs < GAIN_POPUP_MS) {
        snprintf(buf, sizeof(buf), "+%d", gainVal);
        panelCenter(cv, buf, 62, TFT_YELLOW);
    }

    // En buyuk karo — mini onizleme
    if (maxV > 0) {
        panelCenter(cv, "TOP", 82, COL_GRAY_TXT);
        int mx = PANEL_X + (PANEL_W - MINI_TILE) / 2;
        int my = 93;
        cv.fillRoundRect(mx, my, MINI_TILE, MINI_TILE, 4, tileColor(maxV));
        tileValueStr(maxV, buf, sizeof(buf));
        cv.setTextSize(1);
        cv.setTextColor(tileTextColor(maxV));
        int tw = cv.textWidth(buf);
        cv.setCursor(mx + (MINI_TILE - tw) / 2, my + (MINI_TILE - 8) / 2 + 1);
        cv.print(buf);
    }
}

// ------------------------------------------------------------
//  drawMenu — baslik karolari + ipuclari + rekor
// ------------------------------------------------------------
inline void drawMenu(TFT_eSprite &cv, long best) {
    cv.fillSprite(COL_BG);

    // Baslik: "2 0 4 8" — dort renkli karo
    const char digits[4] = { '2', '0', '4', '8' };
    const uint16_t cols[4] = { tileColor(1), COL_GOLD, tileColor(4), tileColor(6) };
    const uint16_t txts[4] = { COL_TXT_DARK, TFT_WHITE, TFT_WHITE, TFT_WHITE };
    int ts = 30, gap = 4;
    int x0 = (SCR_W - (4 * ts + 3 * gap)) / 2;
    for (int i = 0; i < 4; i++) {
        int x = x0 + i * (ts + gap);
        cv.fillRoundRect(x, 14, ts, ts, 5, cols[i]);
        cv.setTextSize(2);
        cv.setTextColor(txts[i]);
        cv.setCursor(x + (ts - 12) / 2 + 1, 14 + (ts - 14) / 2);
        cv.print(digits[i]);
    }

    cv.setTextSize(1);
    cv.setTextColor(COL_GRAY_TXT);
    const char *hint = "Join tiles, reach 2048!";
    cv.setCursor((SCR_W - cv.textWidth(hint)) / 2, 58);
    cv.print(hint);

    if (best > 0) {
        char buf[20];
        snprintf(buf, sizeof(buf), "Best: %ld", best);
        cv.setTextColor(TFT_GREEN);
        cv.setCursor((SCR_W - cv.textWidth(buf)) / 2, 74);
        cv.print(buf);
    }

    // Buton ipuclari — overlay'lerle ayni hizalama stili (ortak sol-x)
    int menuX = 80 - cv.textWidth("[B] OS Menu") / 2;
    cv.setTextColor(TFT_WHITE);
    cv.setCursor(menuX, 96);
    cv.print("[A] Start");
    cv.setTextColor(TFT_LIGHTGREY);
    cv.setCursor(menuX, 110);
    cv.print("[B] OS Menu");
}
