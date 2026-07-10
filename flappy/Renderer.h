#pragma once

#include "Config.h"
#include "Bird.h"
#include "Pipes.h"

// ============================================================
//  SKY BACKGROUND
// ============================================================
inline void drawSky(TFT_eSprite& canvas) {
    canvas.fillSprite(COL_SKY);
}

// ============================================================
//  SCROLLING CLOUDS
// ============================================================
inline void drawClouds(TFT_eSprite& canvas, float cloudOffset) {
    int off = (int)cloudOffset;
    int cx1 = 200 - (off % 220);
    int cx2 = 330 - (off % 350);

    canvas.fillCircle(cx1,      18, 7, TFT_WHITE);
    canvas.fillCircle(cx1 + 10, 15, 9, TFT_WHITE);
    canvas.fillCircle(cx1 + 20, 19, 6, TFT_WHITE);

    canvas.fillCircle(cx2,      34, 5, TFT_WHITE);
    canvas.fillCircle(cx2 + 8,  31, 7, TFT_WHITE);
    canvas.fillCircle(cx2 + 15, 35, 5, TFT_WHITE);
}

// ============================================================
//  SCROLLING GROUND (Seamless Loop)
// ============================================================
inline void drawGround(TFT_eSprite& canvas, float groundOffset) {
    canvas.fillRect(0, GROUND_Y, SCR_W, GROUND_H, COL_GROUND);

    int off = ((int)groundOffset % GROUND_PATTERN_PERIOD + GROUND_PATTERN_PERIOD) % GROUND_PATTERN_PERIOD;

    for (int x = -off; x < SCR_W; x += 3) {
        if (x >= 0 && x < SCR_W) {
            int realX = x + off;
            int h = 2 + ((realX % 7 == 0) ? 2 : 0);
            canvas.drawFastVLine(x, GROUND_Y, h, COL_GRASS);
        }
    }
}

// ============================================================
//  BIRD (with rotation-based wing/beak positioning)
// ============================================================
inline void drawBird(TFT_eSprite& canvas, int x, int y, float angle, bool isDead = false) {
    canvas.fillCircle(x, y, BIRD_R, COL_BIRD);

    // --- Wing ---
    int wingDY;
    if (angle < -15.0f) {
        wingDY = 2;
    } else if (angle > 60.0f) {
        wingDY = -3;
    } else {
        wingDY = (angle > 30.0f) ? -1 : 0;
    }
    canvas.fillCircle(x - 3, y + wingDY, 3, COL_WING);

    // --- Eye ---
    float rad = angle * 0.0174533f;
    float ec = cosf(rad);
    float es = sinf(rad);
    // Eye local position: slightly forward and up from center
    float eyeLocalX = 2.5f;
    float eyeLocalY = -3.0f;
    int eyeX = x + (int)(eyeLocalX * ec - eyeLocalY * es);
    int eyeY = y + (int)(eyeLocalX * es + eyeLocalY * ec);

    if (isDead) {
        // Dead eye: draw an X
        canvas.drawLine(eyeX - 2, eyeY - 2, eyeX + 2, eyeY + 2, TFT_BLACK);
        canvas.drawLine(eyeX + 2, eyeY - 2, eyeX - 2, eyeY + 2, TFT_BLACK);
        // Draw thicker X by offsetting by 1 pixel
        canvas.drawLine(eyeX - 2, eyeY - 1, eyeX + 2, eyeY + 3, TFT_BLACK);
        canvas.drawLine(eyeX + 2, eyeY - 1, eyeX - 2, eyeY + 3, TFT_BLACK);
    } else {
        // Alive eye: white circle + black pupil
        canvas.fillCircle(eyeX, eyeY, 2, TFT_WHITE);
        canvas.fillCircle(eyeX + 1, eyeY, 1, TFT_BLACK);
    }

    // --- Beak (proper triangle that never collapses) ---
    // Define 3 local points relative to bird center (0,0):
    //   Base1: on bird edge, offset +25° from forward direction
    //   Base2: on bird edge, offset -25° from forward direction
    //   Tip:   extended straight ahead from bird center
    // This creates a wide triangle that maintains its shape at any rotation.
    float baseAngle = 25.0f * 0.0174533f;  // 25 degrees spread
    float tipDist = (float)BIRD_R + 5.0f;  // tip distance from center
    float baseDist = (float)BIRD_R - 0.5f; // base sits at bird edge

    // Base1 (upper jaw edge)
    float b1x = baseDist * cosf(rad - baseAngle);
    float b1y = baseDist * sinf(rad - baseAngle);
    // Base2 (lower jaw edge)
    float b2x = baseDist * cosf(rad + baseAngle);
    float b2y = baseDist * sinf(rad + baseAngle);
    // Tip (straight ahead)
    float tipx = tipDist * ec;
    float tipy = tipDist * es;

    canvas.fillTriangle(
        x + (int)b1x, y + (int)b1y,
        x + (int)b2x, y + (int)b2y,
        x + (int)tipx, y + (int)tipy,
        COL_BEAK
    );
}

// ============================================================
//  3D PIPE (with border + highlight shading)
// ============================================================
inline void drawPipe(TFT_eSprite& canvas, const Pipe& p) {
    int px = (int)p.x;
    if (px > SCR_W + 6 || px + PIPE_W < -6) return;

    int gapTop = p.gapCenter - PIPE_GAP / 2;
    int gapBot = p.gapCenter + PIPE_GAP / 2;
    int lipW = 4;

    // --- Upper Pipe ---
    if (gapTop > 0) {
        int lipY = gapTop - 5;
        if (lipY < 0) lipY = 0;

        // Body fill
        canvas.fillRect(px, 0, PIPE_W, gapTop, COL_PIPE);
        // Left border (dark)
        canvas.drawFastVLine(px, 0, gapTop, COL_PIPE_BORDER);
        // Right border (dark)
        canvas.drawFastVLine(px + PIPE_W - 1, 0, gapTop, COL_PIPE_BORDER);
        // Highlight (bright strip on left side)
        if (PIPE_W >= 6) {
            canvas.drawFastVLine(px + 2, 0, gapTop, COL_PIPE_HIGHLIGHT);
        }

        // Lip
        canvas.fillRect(px - 2, lipY, PIPE_W + lipW, gapTop - lipY, COL_PIPE_LIP);
        canvas.drawRect(px - 2, lipY, PIPE_W + lipW, gapTop - lipY, COL_PIPE_BORDER);
        canvas.drawFastVLine(px + 2, lipY + 1, (gapTop - lipY) - 2, COL_PIPE_HIGHLIGHT);
    }

    // --- Lower Pipe ---
    if (gapBot < GROUND_Y) {
        int lipH = 5;
        int bodyH = GROUND_Y - gapBot;
        if (gapBot + lipH > GROUND_Y) lipH = GROUND_Y - gapBot;

        // Body fill
        canvas.fillRect(px, gapBot, PIPE_W, bodyH, COL_PIPE);
        // Left border
        canvas.drawFastVLine(px, gapBot, bodyH, COL_PIPE_BORDER);
        // Right border
        canvas.drawFastVLine(px + PIPE_W - 1, gapBot, bodyH, COL_PIPE_BORDER);
        // Highlight
        if (PIPE_W >= 6) {
            canvas.drawFastVLine(px + 2, gapBot, bodyH, COL_PIPE_HIGHLIGHT);
        }

        // Lip
        canvas.fillRect(px - 2, gapBot, PIPE_W + lipW, lipH, COL_PIPE_LIP);
        canvas.drawRect(px - 2, gapBot, PIPE_W + lipW, lipH, COL_PIPE_BORDER);
        canvas.drawFastVLine(px + 2, gapBot + 1, lipH - 2, COL_PIPE_HIGHLIGHT);
    }
}

// ============================================================
//  DRAW ALL PIPES
// ============================================================
inline void drawAllPipes(TFT_eSprite& canvas, const Pipes& pipes) {
    for (int i = 0; i < NUM_PIPES; i++) {
        drawPipe(canvas, pipes.p[i]);
    }
}

// ============================================================
//  HUD: SCORE + POPUP ANIMATION + FPS
// ============================================================
inline void drawScore(TFT_eSprite& canvas, int score, float popupTimer, int currentFPS, bool showFps, int birdY) {
    char buf[8];
    sprintf(buf, "%d", score);
    int sx = SCR_W / 2 - (strlen(buf) * 6);

    // Normal score (shadow + text)
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_BLACK);
    canvas.setCursor(sx + 1, 6);
    canvas.print(buf);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(sx, 5);
    canvas.print(buf);

    // Score popup animation
    if (popupTimer > 0) {
        float t = popupTimer;
        if (t > SCORE_POPUP_DUR) t = SCORE_POPUP_DUR;
        float alpha = t / SCORE_POPUP_DUR;  // 1.0 = just scored, 0.0 = fading out
        int floatY = birdY - 15 - (int)((1.0f - alpha) * 20.0f);
        // Use size 2 for first 60% of duration, then size 1 for fade-out
        int popSize = (alpha > 0.4f) ? 2 : 1;

        canvas.setTextSize(popSize);
        canvas.setTextColor(TFT_BLACK);
        canvas.setCursor(BIRD_X + 13, floatY + 1);
        canvas.print("+1");
        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(BIRD_X + 12, floatY);
        canvas.print("+1");
    }

    // FPS
    if (showFps) {
        char fpsStr[12];
        sprintf(fpsStr, "FPS:%d", currentFPS);
        int fpsW = strlen(fpsStr) * 6;
        canvas.setTextSize(1);
        canvas.setTextColor(COL_HUD_TEXT);
        canvas.setCursor(SCR_W - fpsW - 2, 5);
        canvas.print("FPS:");
        canvas.setTextColor(TFT_GREEN);
        canvas.print(currentFPS);
    }
}

// ============================================================
//  MAIN MENU SCREEN
// ============================================================
inline void drawMenu(TFT_eSprite& canvas, int highScore, float cloudOffset) {
    drawSky(canvas);
    drawClouds(canvas, cloudOffset);
    drawGround(canvas, 0);

    // Title shadow
    canvas.setTextSize(2);
    canvas.setTextColor(COL_FLAPPY_SHADOW);
    canvas.setCursor(15, 11);
    canvas.print("FLAPPY BIRD");
    // Title
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(14, 10);
    canvas.print("FLAPPY BIRD");

    // Animated bird
    float menuBirdY = 55.0f + sinf((float)millis() / 200.0f) * 8.0f;
    drawBird(canvas, SCR_W / 2, (int)menuBirdY, -1.0f);

    // Buttons
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(10, 95);
    canvas.print("[A] Start");

    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(85, 95);
    canvas.print("[B] OS Menu");

    // High score
    char rekorBuf[20];
    sprintf(rekorBuf, "Best: %d", highScore);
    int rekorW = strlen(rekorBuf) * 6;
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(SCR_W / 2 - (rekorW / 2), 107);
    canvas.print(rekorBuf);
}

// ============================================================
//  GAME OVER SCREEN
// ============================================================
inline void drawGameOver(TFT_eSprite& canvas, int score, int highScore, bool newRecord) {
    canvas.fillSprite(TFT_BLACK);

    // Ortak OS game-over: Score/Best tablosu + NEW BEST rozeti
    char sb[12], hb[12];
    snprintf(sb, sizeof(sb), "%d", score);
    snprintf(hb, sizeof(hb), "%d", highScore);
    OsStat rows[2] = {
        { "Score", sb, TFT_WHITE, TFT_YELLOW },
        { "Best",  hb, TFT_WHITE, TFT_GREEN  },
    };
    osDrawGameOver(canvas, false, rows, 2, newRecord ? "NEW BEST!" : nullptr);
}

// ============================================================
//  PAUSE OVERLAY
// ============================================================
inline void drawPauseOverlay(TFT_eSprite& canvas) {
    osDrawPause(canvas, TFT_GREEN);   // ortak OS pause kutusu (EN, yesil tema)
}
