#pragma once

#include <math.h>
#include "Config.h"
#include "Snake.h"
#include "Food.h"
#include "Particles.h"

// ============================================================
//  EKRAN TITREMESI (Screen Shake)
//  Siddete gore azalan titreme: yem(1), gold(3), olum(8)
// ============================================================
struct ScreenShake {
    float intensity;
    int offsetX, offsetY;

    void reset() {
        intensity = 0.0f;
        offsetX = 0;
        offsetY = 0;
    }

    void trigger(float mag) {
        if (mag > intensity) intensity = mag;
    }

    void update() {
        if (intensity < 0.2f) {
            reset();
            return;
        }
        int mag = (int)intensity;
        offsetX = random(-mag, mag + 1);
        offsetY = random(-mag, mag + 1);
        intensity *= 0.85f;
    }
};

// ============================================================
//  RGB565 parlaklik olcekleme (alpha yerine — trail icin)
// ============================================================
inline uint16_t scaleColor565(uint16_t c, float f) {
    if (f <= 0.0f) return 0;
    if (f >= 1.0f) return c;
    int r = (c >> 11) & 0x1F;
    int g = (c >> 5) & 0x3F;
    int b = c & 0x1F;
    r = (int)(r * f);
    g = (int)(g * f);
    b = (int)(b * f);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// ============================================================
//  Pre-render static background into bgSprite
// ============================================================
inline void renderBackground(TFT_eSprite& bg, int level, int arenaInset = 0) {
    bg.fillSprite(COL_BG_A);

    // Oyun alani
    bg.fillRect(0, OFFSET_Y, SCR_W, ROWS * GRID, COL_BG_A);

    int ax = arenaInset * GRID;
    int ay = OFFSET_Y + arenaInset * GRID;
    int aw = (COLS - 2 * arenaInset) * GRID;
    int ah = (ROWS - 2 * arenaInset) * GRID;

    // Dead zone (arena disi) — koyu kirmili dolgu
    if (arenaInset > 0) {
        bg.fillRect(0, OFFSET_Y, SCR_W, arenaInset * GRID, COL_ARENA_DEAD);             // üst
        bg.fillRect(0, ay + ah, SCR_W, arenaInset * GRID, COL_ARENA_DEAD);              // alt
        bg.fillRect(0, ay, arenaInset * GRID, ah, COL_ARENA_DEAD);                      // sol
        bg.fillRect(ax + aw, ay, arenaInset * GRID, ah, COL_ARENA_DEAD);                // sag
    }

    // Eski damali yapi (Checkerboard — sadece arena içi)
    for (int gy = arenaInset; gy < ROWS - arenaInset; gy++) {
        for (int gx = arenaInset; gx < COLS - arenaInset; gx++) {
            if ((gx + gy) & 1) {
                bg.fillRect(gx * GRID, OFFSET_Y + gy * GRID, GRID, GRID, COL_BG_B);
            }
        }
    }

    // Alt bosluk + arena cerçevesi
    int bottomY = OFFSET_Y + ROWS * GRID;
    bg.fillRect(0, bottomY, SCR_W, SCR_H - bottomY, COL_BG_A);
    bg.drawRect(ax, ay - 1, aw, ah + 2, COL_BORDER);
}

// ============================================================
//  Copy pre-rendered background to canvas
// ============================================================
inline void drawBackground(TFT_eSprite& canvas, TFT_eSprite& bg) {
    bg.pushToSprite(&canvas, 0, 0);
}

// ============================================================
//  Arena walls (daralan alan — kirmizi pulse kenar)
//  arenaInset > 0 iken electric/dikenli duvar, shrink sonrasi parlak
// ============================================================
inline void drawArenaWalls(TFT_eSprite& canvas, int arenaInset, unsigned long shrinkFlashEndMs, bool aboutToShrink) {
    if (arenaInset > 0) {
        int ax = arenaInset * GRID;
        int ay = OFFSET_Y + arenaInset * GRID;
        int aw = (COLS - 2 * arenaInset) * GRID;
        int ah = (ROWS - 2 * arenaInset) * GRID;

        unsigned long now = millis();
        float pulse = 0.5f + 0.5f * sinf((float)now * 0.010f);
        bool flashing = (now < shrinkFlashEndMs);
        float base = flashing ? (0.55f + 0.45f * pulse)
                              : (0.25f + 0.35f * pulse);
        uint16_t wallCol = scaleColor565(TFT_RED, base);

        canvas.drawRect(ax - 1, ay - 1, aw + 2, ah + 2, wallCol);
    }

    if (aboutToShrink) {
        unsigned long now = millis();
        if ((now / 150) % 2 == 0) {
            int nx = arenaInset * GRID;
            int ny = OFFSET_Y + arenaInset * GRID;
            int nw = (COLS - 2 * arenaInset) * GRID;
            int nh = (ROWS - 2 * arenaInset) * GRID;
            
            // Draw red warning overlay on the perimeter that's about to shrink
            // Top
            canvas.fillRect(nx, ny, nw, GRID, COL_RED_DARK); // COL_RED_DARK is Dark Red
            // Bottom
            canvas.fillRect(nx, ny + nh - GRID, nw, GRID, COL_RED_DARK);
            // Left
            canvas.fillRect(nx, ny + GRID, GRID, nh - 2 * GRID, COL_RED_DARK);
            // Right
            canvas.fillRect(nx + nw - GRID, ny + GRID, GRID, nh - 2 * GRID, COL_RED_DARK);
        }
    }
}

// ============================================================
//  Draw snake eyes
// ============================================================
inline void drawEyes(TFT_eSprite& canvas, int px, int py, int d) {
    int ex1, ey1, ex2, ey2;
    switch (d) {
        case DIR_UP:    ex1 = px + 1; ey1 = py + 1; ex2 = px + 5; ey2 = py + 1; break;
        case DIR_RIGHT: ex1 = px + 5; ey1 = py + 1; ex2 = px + 5; ey2 = py + 5; break;
        case DIR_DOWN:  ex1 = px + 1; ey1 = py + 5; ex2 = px + 5; ey2 = py + 5; break;
        case DIR_LEFT:  ex1 = px + 1; ey1 = py + 1; ex2 = px + 1; ey2 = py + 5; break;
        default: return;
    }
    int ox = (d == DIR_RIGHT) ? 1 : (d == DIR_LEFT) ? 0 : 0;
    int oy = (d == DIR_DOWN)  ? 1 : (d == DIR_UP)   ? 0 : 0;
    canvas.fillRect(ex1, ey1, 2, 2, TFT_WHITE);
    canvas.fillRect(ex2, ey2, 2, 2, TFT_WHITE);
    canvas.drawPixel(ex1 + ox, ey1 + oy, TFT_BLACK);
    canvas.drawPixel(ex2 + ox, ey2 + oy, TFT_BLACK);
}

// ============================================================
//  Draw one snake cell at grid position
// ============================================================
inline void drawCell(TFT_eSprite& canvas, int gx, int gy, uint16_t fill, uint16_t border) {
    int px = gx * GRID;
    int py = OFFSET_Y + gy * GRID;
    canvas.fillRect(px + 1, py + 1, GRID - 2, GRID - 2, fill);
    canvas.drawRect(px, py, GRID, GRID, border);
}

// ============================================================
//  Erase a snake cell (fill with background color)
// ============================================================
inline void eraseCell(TFT_eSprite& canvas, int gx, int gy, int level) {
    int px = gx * GRID;
    int py = OFFSET_Y + gy * GRID;
    if (FoodManager::isObstacle(gx, gy, level)) {
        canvas.fillRect(px, py, GRID, GRID, TFT_DARKGREY);
    } else {
        canvas.fillRect(px + 1, py + 1, GRID - 2, GRID - 2, COL_BG_A);
        canvas.drawRect(px, py, GRID, GRID, COL_BG_A);
    }
}

// ============================================================
//  Squash & Stretch kafa (yiyecek aninda 80ms elastik deformasyon)
// ============================================================
inline void drawCellHeadSquash(TFT_eSprite& canvas, int gx, int gy, int dir, float squashT, uint16_t fill, uint16_t border) {
    int px = gx * GRID;
    int py = OFFSET_Y + gy * GRID;
    int w = GRID, h = GRID;

    if (squashT >= 0.0f && squashT < 1.0f) {
        float env = sinf(squashT * 3.14159265358979323846f);
        float osc = cosf(squashT * 3.14159265358979323846f);
        int dw = (int)lroundf(env * osc * SQUASH_AMP);
        w = GRID + dw;
        h = GRID - dw;
        if (w < 4) w = 4;
        if (h < 4) h = 4;
    }

    int rx = px - (w - GRID) / 2;
    int ry = py - (h - GRID) / 2;
    canvas.fillRect(rx + 1, ry + 1, w - 2, h - 2, fill);
    canvas.drawRect(rx, ry, w, h, border);
    drawEyes(canvas, rx, ry, dir);
}

// ============================================================
//  Full snake draw (after reset)
// ============================================================
inline void drawSnakeFull(TFT_eSprite& canvas, const Snake& s, float squashT = 2.0f, bool ghostActive = false) {
    uint16_t hFill = ghostActive ? scaleColor565(COL_HEAD, 0.5f) : COL_HEAD;
    uint16_t hBrd  = ghostActive ? scaleColor565(COL_HEAD_DK, 0.5f) : COL_HEAD_DK;
    uint16_t bBrd  = ghostActive ? scaleColor565(COL_BODY_BRD, 0.5f) : COL_BODY_BRD;
    for (int i = s.len - 1; i >= 0; i--) {
        if (i == 0) {
            drawCellHeadSquash(canvas, s.x[i], s.y[i], s.dir, squashT, hFill, hBrd);
        } else {
            uint16_t bf = ghostActive ? scaleColor565(s.bodyColor(i), 0.5f) : s.bodyColor(i);
            drawCell(canvas, s.x[i], s.y[i], bf, bBrd);
        }
    }
}

// ============================================================
//  Partial snake draw (incremental, only changed cells)
// ============================================================
inline void drawSnakePartial(TFT_eSprite& canvas, const Snake& s, int level, float squashT = 2.0f, bool ghostActive = false) {
    uint16_t hFill = ghostActive ? scaleColor565(COL_HEAD, 0.5f) : COL_HEAD;
    uint16_t hBrd  = ghostActive ? scaleColor565(COL_HEAD_DK, 0.5f) : COL_HEAD_DK;
    uint16_t bBrd  = ghostActive ? scaleColor565(COL_BODY_BRD, 0.5f) : COL_BODY_BRD;

    if (!s.grew) {
        // Erase old tail
        eraseCell(canvas, s.oldTailX, s.oldTailY, level);

        // Erase extra cell on shrink
        if (s.shrank && s.extraErase) {
            eraseCell(canvas, s.extraEraseX, s.extraEraseY, level);
        }
    }

    // Redraw old head as body (use OLD position, which is now index 1)
    if (s.len >= 2) {
        uint16_t bf = ghostActive ? scaleColor565(s.bodyColor(1), 0.5f) : s.bodyColor(1);
        drawCell(canvas, s.oldHeadX, s.oldHeadY, bf, bBrd);
    }

    // Draw new head
    drawCellHeadSquash(canvas, s.x[0], s.y[0], s.dir, squashT, hFill, hBrd);
}

// ============================================================
//  Trail (kafa izi — yilan ciziminden ONCE cizilir)
//  Ring buffer'dan en eski->en yeni, azalan parlaklik
// ============================================================
inline void drawTrail(TFT_eSprite& canvas, const Snake& s, int activeLen) {
    if (s.trailLen <= 0 || activeLen <= 0) return;
    int n = (s.trailLen < activeLen) ? s.trailLen : activeLen;
    static const float fracLow[4]  = {0.10f, 0.20f, 0.40f, 0.60f};
    static const float fracHigh[8] = {0.05f, 0.08f, 0.10f, 0.15f,
                                      0.20f, 0.30f, 0.45f, 0.60f};
    const float* fr = (activeLen >= 8) ? fracHigh : fracLow;
    int start = (s.trailHead - n + 8) % 8;
    for (int i = 0; i < n; i++) {
        int idx = (start + i) % 8;
        uint16_t col = scaleColor565(COL_HEAD, fr[i]);
        int px = s.trailX[idx] * GRID;
        int py = OFFSET_Y + s.trailY[idx] * GRID;
        canvas.fillRect(px, py, GRID, GRID, col);
    }
}

// ============================================================
//  Draw food + poison + power-up
// ============================================================
inline void drawFood(TFT_eSprite& canvas, const FoodManager& food) {
    bool bright = (millis() / 300) % 2 == 0;
    int cx = (int)food.foodPixelX;
    int cy = (int)food.foodPixelY;

    // Ghost power-up (mavi halka + daire)
    if (food.foodType == FOOD_GHOST) {
        uint16_t col = bright ? COL_GHOST : COL_GHOST_DIM;
        canvas.fillCircle(cx, cy, 3, col);
        canvas.drawCircle(cx, cy, 4, COL_GHOST_HL);
        return;
    }

    // Magnet power-up (mor kare + dis cerceve)
    if (food.foodType == FOOD_MAGNET) {
        uint16_t col = bright ? COL_MAGNET : COL_MAGNET_DIM;
        canvas.fillRect(cx - 2, cy - 2, 5, 5, col);
        canvas.drawRect(cx - 3, cy - 3, 7, 7, COL_MAGNET_HL);
        return;
    }

    // Normal / Gold food
    uint16_t mainCol, hlCol;
    if (food.foodType == FOOD_GOLD) {
        mainCol = bright ? COL_GOLD : COL_GOLD_DIM;
        hlCol   = COL_GOLD_HL;
    } else {
        mainCol = bright ? COL_FOOD : COL_FOOD_DIM;
        hlCol   = COL_FOOD_HL;
    }

    canvas.fillCircle(cx, cy, 3, mainCol);
    canvas.drawPixel(cx - 1, cy - 1, hlCol);
    canvas.drawFastVLine(cx, cy - 4, 2, COL_FOOD_STEM);
    canvas.drawPixel(cx + 1, cy - 4, COL_FOOD_LEAF);
    canvas.drawPixel(cx + 2, cy - 3, COL_FOOD_LEAF);

    // Poison mushroom
    if (food.isPoisonVisible()) {
        int px = food.poisonX * GRID + GRID / 2;
        int py = OFFSET_Y + food.poisonY * GRID + GRID / 2;
        canvas.fillCircle(px, py, 3, bright ? COL_POISON : COL_POISON_DIM);
        canvas.drawPixel(px - 1, py - 1, COL_POISON_HL);
        canvas.drawPixel(px + 1, py - 1, TFT_WHITE);
        canvas.drawPixel(px - 1, py + 1, TFT_WHITE);
    }
}

// ============================================================
//  Draw power-up status (aktif cerceve + sarj bar)
// ============================================================
inline void drawPowerUpStatus(TFT_eSprite& canvas, float ghostCharge, float magnetCharge,
                              bool ghostActive, bool magnetActive, float ghostActiveFrac, float magnetActiveFrac) {
    // Aktif cerceveler (ic ice — ikisi ayni anda olabilir)
    if (ghostActive) {
        canvas.drawRect(0, OFFSET_Y - 1, SCR_W, ROWS * GRID + 2, COL_GHOST);
    }
    if (magnetActive) {
        canvas.drawRect(2, OFFSET_Y + 1, SCR_W - 4, ROWS * GRID - 2, COL_MAGNET);
    }

    // Barlar (HUD icinde saga hizali cizgiler)
    int barW = 30;
    int startX = SCR_W - barW - 2;

    float gFrac = ghostActive ? ghostActiveFrac : ghostCharge;
    if (gFrac > 0.0f || ghostActive) { // Draw empty bar even if 0 when active
        int w = (int)(gFrac * barW);
        canvas.fillRect(startX, 2, barW, 2, COL_GHOST_DIM);
        if (w > 0) canvas.fillRect(startX, 2, w, 2, ghostActive ? COL_GHOST_HL : COL_GHOST);
    }
    
    float mFrac = magnetActive ? magnetActiveFrac : magnetCharge;
    if (mFrac > 0.0f || magnetActive) {
        int w = (int)(mFrac * barW);
        canvas.fillRect(startX, 5, barW, 2, COL_MAGNET_DIM);
        if (w > 0) canvas.fillRect(startX, 5, w, 2, magnetActive ? COL_MAGNET_HL : COL_MAGNET);
    }
}

// ============================================================
//  Draw floating score popup
// ============================================================
inline void drawPopup(TFT_eSprite& canvas, int px, int py, float timer, int pts) {
    if (timer <= 0.0f) return;
    int floatY = py - (int)(15.0f - timer);
    if (floatY < HUD_H) floatY = HUD_H;
    canvas.setTextSize(1);

    // Clamp X coordinate to prevent text wrapping on the right edge of the screen
    int textW = (pts >= 10 || pts <= -10) ? 18 : 12; // "+20" is 3 chars = 18px. "+5" is 2 chars = 12px.
    int startX = px - 6;
    if (startX + textW > SCR_W) startX = SCR_W - textW;
    if (startX < 0) startX = 0;

    // Text Shadow
    canvas.setTextColor(TFT_BLACK);
    canvas.setCursor(startX + 1, floatY + 1);
    if (pts > 0) canvas.print("+");
    canvas.print(pts);

    // Text
    if (pts < 0)       canvas.setTextColor(COL_POISON);
    else if (pts == 50) canvas.setTextColor(COL_GOLD);
    else                canvas.setTextColor(TFT_YELLOW);

    canvas.setCursor(startX, floatY);
    if (pts > 0) canvas.print("+");
    canvas.print(pts);
}

// ============================================================
//  Draw HUD (score, FPS)
//  Returns true if HUD was drawn (caller should track dirty)
// ============================================================
inline void drawHUD(TFT_eSprite& canvas, int score, int fps, bool showFps) {
    canvas.setTextSize(1);

    // Score (left)
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(2, 2);
    canvas.print("SCORE:");
    canvas.setTextColor(TFT_WHITE);
    canvas.print(score);

    // FPS (right, left of power-up bars)
    if (showFps) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "FPS:%d", fps);
        int fpsW = len * 6;
        int barW = 30; // Width of the power-up bars

        canvas.setTextColor(COL_HUD_TEXT);
        canvas.setCursor(SCR_W - barW - fpsW - 6, 2);
        canvas.print("FPS:");
        canvas.setTextColor(TFT_GREEN);
        canvas.print(fps);
    }

    // Divider line
    canvas.drawFastHLine(0, HUD_H, SCR_W, COL_HUD_LINE);
}

// ============================================================
//  Draw combo bar in HUD
// ============================================================
inline void drawComboBar(TFT_eSprite& canvas, int comboCount, float comboFrac) {
    if (comboCount <= 1) return;
    if (comboFrac < 0.0f) comboFrac = 0.0f;
    if (comboFrac > 1.0f) comboFrac = 1.0f;
    
    // Barlar (HUD icinde saga hizali cizgiler)
    int barW = 30;
    int startX = SCR_W - barW - 2;

    int w = (int)(comboFrac * barW);
    uint16_t col, dimCol;
    if (comboCount >= 7) {
        col = TFT_RED;
        dimCol = COL_RED_DARK; // Koyu kirmizi
    } else if (comboCount >= 4) {
        col = TFT_ORANGE;
        dimCol = COL_ORANGE_DARK; // Koyu turuncu
    } else {
        col = TFT_YELLOW;
        dimCol = COL_YELLOW_DARK; // Koyu sari
    }

    canvas.fillRect(startX, 8, barW, 2, dimCol);
    if (w > 0) canvas.fillRect(startX, 8, w, 2, col);
}

// ============================================================
//  Draw main menu
// ============================================================
inline void drawMenu(TFT_eSprite& canvas, int highScore) {
    canvas.fillSprite(COL_BG_A);

    // Title shadow
    canvas.setTextSize(2);
    canvas.setTextColor(COL_BODY_BRD);
    canvas.setCursor(MENU_TITLE_SH_X, MENU_TITLE_SH_Y);
    canvas.print("SNAKE");

    // Title
    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(MENU_TITLE_X, MENU_TITLE_Y);
    canvas.print("SNAKE");

    // Decorative snake body
    for (int i = 6; i >= 1; i--) {
        int dx = 30 + i * 10;
        uint16_t col = (i & 1) ? COL_BODY_B : COL_BODY_A;
        canvas.fillRect(dx + 1, MENU_DEMO_Y + 1, GRID - 2, GRID - 2, col);
        canvas.drawRect(dx, MENU_DEMO_Y, GRID, GRID, COL_BODY_BRD);
    }

    // Decorative snake head
    canvas.fillRect(MENU_DEMO_HEAD_X + 1, MENU_DEMO_Y + 1, GRID - 2, GRID - 2, COL_HEAD);
    canvas.drawRect(MENU_DEMO_HEAD_X, MENU_DEMO_Y, GRID, GRID, COL_HEAD_DK);
    canvas.fillRect(MENU_DEMO_HEAD_X + 5, MENU_DEMO_Y + 1, 2, 2, TFT_WHITE);
    canvas.fillRect(MENU_DEMO_HEAD_X + 5, MENU_DEMO_Y + 5, 2, 2, TFT_WHITE);

    // Decorative apple
    canvas.fillCircle(MENU_APPLE_X, MENU_DEMO_Y + 4, 3, COL_FOOD);
    canvas.drawPixel(MENU_APPLE_X - 1, MENU_DEMO_Y + 3, COL_FOOD_HL);
    canvas.drawFastVLine(MENU_APPLE_X, MENU_DEMO_Y, 2, COL_FOOD_STEM);
    canvas.drawPixel(MENU_APPLE_X + 1, MENU_DEMO_Y, COL_FOOD_LEAF);

    // Button hints
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(MENU_BTN_A_X, MENU_BTN_A_Y);
    canvas.print("[A] Start");

    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(MENU_BTN_B_X, MENU_BTN_B_Y);
    canvas.print("[B] OS Menu");

    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(MENU_JOY_X, MENU_JOY_Y);
    canvas.print("[JOY] Move");

    // High score
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(MENU_RECORD_X, MENU_RECORD_Y);
    canvas.printf("Best: %d", highScore);
}

// ============================================================
//  Draw game over screen
// ============================================================
inline void drawGameOver(TFT_eSprite& canvas, int score, int highScore, bool newRecord) {
    canvas.fillSprite(COL_BG_A);

    // Checkerboard background
    for (int gy = 0; gy < SCR_H / GRID; gy++)
        for (int gx = 0; gx < SCR_W / GRID; gx++)
            if ((gx + gy) & 1)
                canvas.fillRect(gx * GRID, gy * GRID, GRID, GRID, COL_BG_B);

    // Ortak OS game-over: 3 satirlik tablo (: hizali) + NEW BEST rozeti.
    // Checkerboard arka plan uzerine cizilir.
    char sb[12], fb[12], hb[12];
    snprintf(sb, sizeof(sb), "%d", score);
    snprintf(fb, sizeof(fb), "%d", score / 10);
    snprintf(hb, sizeof(hb), "%d", highScore);
    OsStat rows[3] = {
        { "Score", sb, TFT_WHITE, TFT_YELLOW },
        { "Food",  fb, TFT_WHITE, COL_FOOD   },
        { "Best",  hb, TFT_WHITE, TFT_GREEN  },
    };
    osDrawGameOver(canvas, false, rows, 3, newRecord ? "NEW BEST!" : nullptr);
}

// ============================================================
//  Draw pause overlay
// ============================================================
inline void drawPauseOverlay(TFT_eSprite& canvas) {
    osDrawPause(canvas, TFT_GREEN);   // ortak OS pause kutusu (EN, yesil tema)
}
