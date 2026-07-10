#pragma once
// ============================================================
//  E-OS SPACE INVADERS — Renderer.h
//  Tum cizim fonksiyonlari, parallax yildiz tarlasi,
//  parcacik patlama sistemi, ekran titremesi ve flas
// ============================================================
#include <math.h>
#include "Config.h"
#include "Player.h"
#include "Invaders.h"
#include "Projectiles.h"
#include "Bunker.h"
#include "../SharedParticles.h"

// ============================================================
//  PARALLAX YILDIZ TARLASI (3 Katman — float tabanli kayma)
// ============================================================
struct StarLayer {
    int   x[20];
    float y[20];
    int count;
    uint16_t color;
    float speed;
};

struct Starfield {
    StarLayer far, mid, near;

    void init() {
        far.count  = STAR_FAR_COUNT;
        far.color  = COL_STAR_FAR;
        far.speed  = STAR_FAR_SPD;
        mid.count  = STAR_MID_COUNT;
        mid.color  = COL_STAR_MID;
        mid.speed  = STAR_MID_SPD;
        near.count = STAR_NEAR_COUNT;
        near.color = COL_STAR_NEAR;
        near.speed = STAR_NEAR_SPD;
        resetLayer(far);
        resetLayer(mid);
        resetLayer(near);
    }

    void resetLayer(StarLayer &l) {
        for (int i = 0; i < l.count; i++) {
            l.x[i] = random(0, SCR_W);
            l.y[i] = (float)random(HUD_H + 1, SCR_H);
        }
    }

    void update(float dt) {
        scrollLayer(far,  dt);
        scrollLayer(mid,  dt);
        scrollLayer(near, dt);
    }

    void scrollLayer(StarLayer &l, float dt) {
        for (int i = 0; i < l.count; i++) {
            l.y[i] += l.speed * dt;
            if (l.y[i] >= (float)SCR_H) {
                l.y[i] = (float)(HUD_H + 1);
                l.x[i] = random(0, SCR_W);
            }
        }
    }

    void draw(TFT_eSprite &canvas) const {
        drawLayer(canvas, far);
        drawLayer(canvas, mid);
        drawLayer(canvas, near);
    }

    void drawLayer(TFT_eSprite &canvas, const StarLayer &l) const {
        bool isNear = (&l == &near);
        for (int i = 0; i < l.count; i++) {
            int iy = (int)l.y[i];
            if (iy < HUD_H) continue;
            if (isNear) {
                canvas.fillRect(l.x[i], iy, 2, 2, l.color);
            } else {
                canvas.drawPixel(l.x[i], iy, l.color);
            }
        }
    }
};

// ============================================================
//  PARCACIK PATLAMA SISTEMI
// ============================================================
struct ParticleSystem : public SharedParticleSystem<MAX_PARTICLES> {

    void spawn(float px, float py, uint16_t color, int count) {
        for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
            if (!particles[i].active) {
                particles[i].x = px;
                particles[i].y = py;
                float angle = random(0, 628) / 100.0f;
                float spd   = random(30, 100);
                particles[i].vx = cosf(angle) * spd;
                particles[i].vy = sinf(angle) * spd - 25.0f;
                particles[i].color = color;
                particles[i].life = random(30, 60) / 100.0f;
                particles[i].active = true;
                count--;
            }
        }
    }

    void update(float dt) {
        SharedParticleSystem<MAX_PARTICLES>::update(dt, 15.0f, (float)SCR_H);
    }

    void draw(TFT_eSprite &canvas) const {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].active) continue;
            int px = (int)particles[i].x;
            int py = (int)particles[i].y;
            float alpha = particles[i].life / 0.6f;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;

            if (alpha > 0.6f) {
                canvas.fillRect(px, py, 2, 2, particles[i].color);
            } else if (alpha > 0.3f) {
                canvas.drawPixel(px, py, particles[i].color);
            }
        }
    }
};

// ============================================================
//  EKRAN TITREMESI (Screen Shake)
// ============================================================
struct ScreenShake {
    float shakeX, shakeY;
    float intensity;

    void reset() { shakeX = 0.0f; shakeY = 0.0f; intensity = 0.0f; }

    void trigger(int mag) {
        if ((float)mag > intensity) intensity = (float)mag;
    }

    void update() {
        if (intensity < 0.3f) { shakeX = 0.0f; shakeY = 0.0f; intensity = 0.0f; return; }
        shakeX = (float)random((int)(-intensity), (int)(intensity + 1.0f));
        shakeY = (float)random((int)(-intensity), (int)(intensity + 1.0f));
        intensity *= SHAKE_DECAY;
    }
};

// ============================================================
//  EKRAN FLASI (Screen Flash)
// ============================================================
struct ScreenFlash {
    int timer;

    void reset() { timer = 0; }

    void trigger() { timer = FLASH_DURATION; }

    void update() { if (timer > 0) timer--; }

    void draw(TFT_eSprite &canvas) const {
        if (timer > 0) {
            for (int y = 0; y < SCR_H; y += 2) {
                canvas.drawFastHLine(0, y, SCR_W, COL_FLASH_RED);
            }
        }
    }
};

// ============================================================
//  CIzIM FONKSIYONLARI
// ============================================================

inline void drawStars(TFT_eSprite &canvas, Starfield &stars) {
    stars.draw(canvas);
}

inline void drawAlien(TFT_eSprite &canvas, int row, int col, bool animFrame, const InvaderGrid &grid) {
    int x = (int)grid.alienX(col);
    int y = (int)grid.alienY(row);
    uint16_t c = COL_ALIEN_ROW[row & 3];
    int type = row % 3;

    if (type == 0) {
        canvas.fillRect(x + 3, y, 3, 2, c);
        canvas.fillRect(x + 1, y + 2, 7, 3, c);
        canvas.fillRect(x, y + 4, 9, 2, c);
        canvas.drawPixel(x + 3, y + 3, TFT_BLACK);
        canvas.drawPixel(x + 5, y + 3, TFT_BLACK);
        if (animFrame) {
            canvas.drawPixel(x + 1, y + 6, c);
            canvas.drawPixel(x + 7, y + 6, c);
        } else {
            canvas.drawPixel(x, y + 6, c);
            canvas.drawPixel(x + 8, y + 6, c);
        }
    } else if (type == 1) {
        canvas.fillRect(x + 1, y, 7, 6, c);
        canvas.fillRect(x, y + 1, 9, 4, c);
        canvas.fillRect(x + 2, y + 2, 2, 2, TFT_BLACK);
        canvas.fillRect(x + 5, y + 2, 2, 2, TFT_BLACK);
        if (animFrame) {
            canvas.drawPixel(x, y, c);
            canvas.drawPixel(x + 8, y, c);
        } else {
            canvas.drawPixel(x, y + 5, c);
            canvas.drawPixel(x + 8, y + 5, c);
            canvas.drawPixel(x, y + 6, c);
            canvas.drawPixel(x + 8, y + 6, c);
        }
    } else {
        canvas.fillRect(x + 2, y, 5, 2, c);
        canvas.fillRect(x + 1, y + 2, 7, 2, c);
        canvas.fillRect(x, y + 3, 9, 2, c);
        canvas.fillRect(x + 1, y + 5, 7, 1, c);
        canvas.drawPixel(x + 3, y + 3, TFT_BLACK);
        canvas.drawPixel(x + 5, y + 3, TFT_BLACK);
        if (animFrame) {
            canvas.drawPixel(x + 2, y + 6, c);
            canvas.drawPixel(x + 4, y + 6, c);
            canvas.drawPixel(x + 6, y + 6, c);
        } else {
            canvas.drawPixel(x + 1, y + 6, c);
            canvas.drawPixel(x + 3, y + 6, c);
            canvas.drawPixel(x + 5, y + 6, c);
            canvas.drawPixel(x + 7, y + 6, c);
        }
    }
}

inline void drawAlienGrid(TFT_eSprite &canvas, const InvaderGrid &grid) {
    for (int r = 0; r < ALIEN_ROWS; r++)
        for (int c = 0; c < ALIEN_COLS; c++)
            if (grid.aliens[r][c])
                drawAlien(canvas, r, c, grid.animFrame, grid);
}

inline void drawPlayerShip(TFT_eSprite &canvas, const Player &p) {
    if (p.isInvincible() && (millis() / 100) % 2 == 0) return;
    int x = (int)p.x;
    int y = PLAYER_Y;

    canvas.fillRect(x - PLAYER_W / 2, y + 4, PLAYER_W, PLAYER_H - 4, COL_PLAYER);
    canvas.fillRect(x - PLAYER_W / 2 + 2, y + 2, PLAYER_W - 4, 3, COL_PLAYER);
    canvas.fillRect(x - 1, y, 3, 3, COL_PLAYER);
    canvas.drawFastHLine(x - PLAYER_W / 2 + 1, y + 5, PLAYER_W - 2, COL_PLAYER_HL);
    canvas.fillRect(x - 2, y + 3, 5, 2, COL_PLAYER_CABIN);
}

inline void drawBunkers(TFT_eSprite &canvas, const BunkerManager &bm) {
    for (int i = 0; i < BUNKER_COUNT; i++) {
        const Bunker &b = bm.bunkers[i];
        for (int r = 0; r < BUNKER_ROWS; r++) {
            for (int c = 0; c < BUNKER_COLS; c++) {
                if (!b.blocks[r][c]) continue;
                int bx = b.blockX(c);
                int by = b.blockY(r);
                canvas.fillRect(bx + 1, by + 1, BUNKER_BLOCK_W - 2, BUNKER_BLOCK_H - 2, COL_BUNKER);
                canvas.drawPixel(bx, by, COL_BUNKER_HL);
            }
        }
    }
}

inline void drawBullets(TFT_eSprite &canvas, const ProjectileManager &pm) {
    for (int i = 0; i < MAX_PBULLETS; i++) {
        if (!pm.pBullets[i].active) continue;
        int bx = (int)pm.pBullets[i].x;
        int by = (int)pm.pBullets[i].y;
        canvas.fillRect(bx - 1, by - 2, 2, 5, COL_PBULLET);
        canvas.drawPixel(bx, by - 2, COL_PBULLET_HL);
    }
    for (int i = 0; i < MAX_EBULLETS; i++) {
        if (!pm.eBullets[i].active) continue;
        int bx = (int)pm.eBullets[i].x;
        int by = (int)pm.eBullets[i].y;
        uint16_t bc = pm.eBullets[i].color;
        canvas.fillRect(bx - 1, by - 2, 2, 5, bc);
        canvas.drawPixel(bx, by, COL_WHITE);
    }
}

inline void drawHUD(TFT_eSprite &canvas, int score, int wave, int lives, int fps, bool showFps) {
    canvas.setTextSize(1);
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(2, 1);
    canvas.print("S:");
    canvas.setTextColor(TFT_WHITE);
    canvas.print(score);

    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(SCR_W / 2 - 8, 1);
    canvas.print("W:");
    canvas.setTextColor(TFT_YELLOW);
    canvas.print(wave);

    for (int i = 0; i < lives; i++) {
        int lx = SCR_W - 10 - i * 12;
        canvas.fillTriangle(lx, 8, lx + 4, 1, lx + 8, 8, COL_PLAYER);
    }

    if (showFps) {
        char fpsStr[16];
        int len = snprintf(fpsStr, sizeof(fpsStr), "FPS:%d", fps);
        canvas.setTextColor(TFT_GREEN);
        canvas.setCursor(SCR_W - len * 6 - 2, 1);
        canvas.print(fpsStr);
    }

    canvas.drawFastHLine(0, HUD_H, SCR_W, COL_HUD_LINE);
}

inline void drawMenu(TFT_eSprite &canvas, Starfield &stars, int highScore) {
    canvas.fillSprite(COL_SPACE);
    stars.draw(canvas);

    canvas.setTextSize(2);
    canvas.setTextColor(COL_TITLE_SHADOW);
    canvas.setCursor(53, 7);
    canvas.print("SPACE");
    canvas.setCursor(35, 25);
    canvas.print("INVADERS");
    canvas.setTextColor(TFT_CYAN);
    canvas.setCursor(52, 6);
    canvas.print("SPACE");
    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(34, 24);
    canvas.print("INVADERS");

    for (int i = 0; i < 4; i++) {
        int ax = 35 + i * 24;
        int ay = 52;
        uint16_t ac = COL_ALIEN_ROW[i & 3];
        canvas.fillRect(ax + 1, ay, 7, 6, ac);
        canvas.fillRect(ax, ay + 1, 9, 4, ac);
        canvas.drawPixel(ax + 2, ay + 2, TFT_BLACK);
        canvas.drawPixel(ax + 6, ay + 2, TFT_BLACK);
        bool animF = (millis() / 500) % 2;
        if (animF) {
            canvas.drawPixel(ax, ay + 6, ac);
            canvas.drawPixel(ax + 8, ay + 6, ac);
        }
    }

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(53, 95);
    canvas.print("[A] Start");

    canvas.setCursor(47, 105);
    canvas.print("[B] OS Menu");

    canvas.setCursor(50, 115);
    canvas.print("[JOY] Move");

    // Rekor
    canvas.setTextColor(TFT_YELLOW);
    char rekorBuf[20];
    snprintf(rekorBuf, sizeof(rekorBuf), "Best: %d", highScore);
    int txtW = strlen(rekorBuf) * 6;
    canvas.setCursor((160 - txtW) / 2, 140);
    canvas.print(rekorBuf);
}

inline void drawWaveClear(TFT_eSprite &canvas, Starfield &stars, int wave, int score) {
    canvas.fillSprite(COL_SPACE);
    stars.draw(canvas);

    canvas.setTextSize(2);
    canvas.setTextColor(TFT_GREEN);
    char waveStr[16];
    snprintf(waveStr, sizeof(waveStr), "WAVE %d", wave);
    int txtW = strlen(waveStr) * 12; // size 2
    canvas.setCursor((160 - txtW) / 2, 36);
    canvas.print(waveStr);

    canvas.setTextColor(TFT_CYAN);
    const char *cz = "CLEARED!";
    int cw = (int)strlen(cz) * 12;
    canvas.setCursor((160 - cw) / 2, 58);
    canvas.print(cz);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_YELLOW);    // Skor
    char scoreStr[16];
    snprintf(scoreStr, sizeof(scoreStr), "Score: %d", score);
    txtW = strlen(scoreStr) * 6; // size 1
    canvas.setCursor((160 - txtW) / 2, 84);
    canvas.print(scoreStr);
}

inline void drawGameOver(TFT_eSprite &canvas, Starfield &stars, int score, int highScore, bool newRecord) {
    canvas.fillSprite(COL_SPACE);
    stars.draw(canvas);

    // Ortak OS game-over: Score/Best tablosu + NEW BEST rozeti.
    // Yildiz alani arka planda kalir.
    char sb[12], hb[12];
    snprintf(sb, sizeof(sb), "%d", score);
    snprintf(hb, sizeof(hb), "%d", highScore);
    OsStat rows[2] = {
        { "Score", sb, TFT_WHITE, TFT_YELLOW },
        { "Best",  hb, TFT_WHITE, TFT_GREEN  },
    };
    osDrawGameOver(canvas, false, rows, 2, newRecord ? "NEW BEST!" : nullptr);
}

inline void drawPauseOverlay(TFT_eSprite &canvas) {
    osDrawPause(canvas, TFT_GREEN);   // ortak OS pause kutusu (EN, yesil tema)
}

inline void drawParticles(TFT_eSprite &canvas, ParticleSystem &ps) {
    ps.draw(canvas);
}

inline void renderGameScene(TFT_eSprite &canvas, Starfield &stars,
                            const InvaderGrid &invaders, const Player &player,
                            const BunkerManager &bunkers, const ProjectileManager &pm,
                            ParticleSystem &particles, ScreenFlash &flash,
                            int score, int wave, int lives, int fps, bool showFps) {
    canvas.fillSprite(COL_SPACE);
    stars.draw(canvas);
    drawAlienGrid(canvas, invaders);
    drawBunkers(canvas, bunkers);
    drawBullets(canvas, pm);
    drawPlayerShip(canvas, player);
    drawParticles(canvas, particles);
    drawHUD(canvas, score, wave, lives, fps, showFps);
    flash.draw(canvas);
}
