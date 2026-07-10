#pragma once
// ============================================================
//  E-OS ARKANOID — Renderer.h
//  Tum TFT_eSprite cizim fonksiyonlari:
//    Parcacik Patlama Sistemi (Particle System)
//    Ekran Titremesi (Screen Shake)
//    HUD, Duvarlar, Menu, Dalga Gecisi, Oyun Bitti, Pause
// ============================================================
#include <math.h>
#include "Config.h"
#include "Paddle.h"
#include "Ball.h"
#include "Bricks.h"
#include "../SharedParticles.h"

// ============================================================
//  PARCACIK PATLAMA SISTEMI
//  Tugla kirildiginda kendi renginde 8-10 parcacik etrafa sacilir,
//  yercekimi ile asagi duser ve sonumlenir.
// ============================================================
struct ParticleSystem : public SharedParticleSystem<MAX_PARTICLES> {

    void emit(float px, float py, uint16_t color, int count) {
        for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
            if (!particles[i].active) {
                particles[i].x = px;
                particles[i].y = py;
                // Rastgele aci ve hiz (radyal patlama)
                float angle = (float)random(0, 628) / 100.0f;  // 0 .. 2*PI
                float spd   = (float)random(40, 121);
                particles[i].vx = cosf(angle) * spd;
                particles[i].vy = sinf(angle) * spd - 40.0f;  // Yukari bias
                particles[i].color = color;
                particles[i].life  = 0.4f + (float)random(0, 20) / 100.0f;
                particles[i].active = true;
                count--;
            }
        }
    }

    void update(float dt) {
        SharedParticleSystem<MAX_PARTICLES>::update(dt, 35.0f, (float)SCR_H + 5.0f);
    }

    void draw(TFT_eSprite& canvas) const {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].active) continue;
            int px = (int)particles[i].x;
            int py = (int)particles[i].y;
            if (px < 0 || px >= SCR_W) continue;
            if (py < HUD_H || py >= SCR_H) continue;

            // Omur azaldikca kuculen parcacik
            if (particles[i].life > 0.25f) {
                canvas.fillRect(px, py, 2, 2, particles[i].color);
            } else {
                canvas.drawPixel(px, py, particles[i].color);
            }
        }
    }
};

// ============================================================
//  EKRAN TITREMESI (Screen Shake)
//  Farkli siddetlerde: tugla kirilma(1.5), raket vurus(0.5), can kaybi(4.0)
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
        intensity *= 0.85f;   // Sonumlenme katsayisi
    }
};

// ============================================================
//  CIzIM FONKSIYONLARI
// ============================================================

inline void drawWalls(TFT_eSprite& canvas) {
    canvas.drawFastVLine(0, HUD_H, SCR_H - HUD_H, COL_WALL);
    canvas.drawFastVLine(SCR_W - 1, HUD_H, SCR_H - HUD_H, COL_WALL);
    canvas.drawFastHLine(0, HUD_H, SCR_W, COL_WALL);
}

inline void drawHUD(TFT_eSprite& canvas, int score, int wave, int lives,
                    int fps, bool showFps) {
    canvas.setTextSize(1);

    // Skor (sol)
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(2, 1);
    canvas.print("S:");
    canvas.setTextColor(TFT_WHITE);
    canvas.print(score);

    // Dalga (orta)
    canvas.setTextColor(COL_HUD_TEXT);
    canvas.setCursor(SCR_W / 2 - 8, 1);
    canvas.print("W:");
    canvas.setTextColor(TFT_YELLOW);
    canvas.print(wave);

    // FPS (sag tarafa, canlardan once — 128 sinirini kullan)
    if (showFps) {
        char buf[12];
        int len = snprintf(buf, sizeof(buf), "FPS:%d", fps);
        int totalW = len * 6;
        int fpsX = 128 - totalW - 4;  // Canlar 128+ alani kaplar

        canvas.setTextColor(COL_HUD_TEXT);
        canvas.setCursor(fpsX, 1);
        canvas.print("FPS:");
        canvas.setTextColor(TFT_GREEN);
        canvas.print(fps);
    }

    // Canlar (sag — minik toplar)
    for (int i = 0; i < lives; i++) {
        canvas.fillCircle(SCR_W - 6 - i * 10, 5, 3, COL_BALL);
    }

    // Ayirici cizgi
    canvas.drawFastHLine(0, HUD_H, SCR_W, COL_HUD_LINE);
}

inline void drawMenu(TFT_eSprite& canvas, int highScore) {
    canvas.fillSprite(COL_BG);

    // Baslik golgesi
    canvas.setTextSize(2);
    canvas.setTextColor(COL_TITLE_SHADOW);
    canvas.setCursor(33, 7);
    canvas.print("ARKANOID");

    // Baslik
    canvas.setTextColor(TFT_CYAN);
    canvas.setCursor(32, 6);
    canvas.print("ARKANOID");

    // Dekoratif tuglalar
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 6; c++) {
            int dx = 20 + c * 20;
            int dy = 30 + r * 8;
            uint16_t col = BRICK_COLORS[(r + c) % BRICK_ROWS];
            uint16_t hl  = BRICK_HL[(r + c) % BRICK_ROWS];
            canvas.fillRect(dx, dy, 18, 6, col);
            canvas.drawFastHLine(dx, dy, 17, hl);
        }
    }

    // Dekoratif cubuk + animasyonlu top
    int demoPX = SCR_W / 2;
    int demoPY = 64;
    canvas.fillRect(demoPX - PADDLE_W / 2, demoPY, PADDLE_W, PADDLE_H, COL_PADDLE);
    canvas.drawFastHLine(demoPX - PADDLE_W / 2, demoPY, PADDLE_W, COL_PADDLE_HL);
    float demoBY = 58.0f + sinf((float)millis() / 200.0f) * 3.0f;
    canvas.fillCircle(demoPX, (int)demoBY, BALL_R, COL_BALL);

    // Buton ipuclari
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(53, 95);
    canvas.print("[A] Start");

    canvas.setCursor(47, 105);
    canvas.print("[B] OS Menu");

    canvas.setCursor(50, 115);
    canvas.print("[JOY] Move");

    // Rekor (Sari)
    canvas.setTextColor(TFT_YELLOW);
    char rekorBuf[20];
    snprintf(rekorBuf, sizeof(rekorBuf), "Best: %d", highScore);
    int txtW = strlen(rekorBuf) * 6;
    canvas.setCursor((160 - txtW) / 2, 140);
    canvas.print(rekorBuf);
}

inline void drawLevelClear(TFT_eSprite& canvas, int wave, int score) {
    canvas.fillSprite(COL_BG);

    canvas.setTextSize(2);
    canvas.setTextColor(TFT_GREEN);
    char waveStr[16];
    snprintf(waveStr, sizeof(waveStr), "WAVE %d", wave);
    int txtW = strlen(waveStr) * 12; // size 2
    canvas.setCursor((160 - txtW) / 2, 40);
    canvas.print(waveStr);

    canvas.setTextColor(TFT_CYAN);
    const char *cz = "CLEARED!";
    int cw = (int)strlen(cz) * 12;
    canvas.setCursor((160 - cw) / 2, 55);
    canvas.print(cz);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_YELLOW);
    char scoreStr[16];
    snprintf(scoreStr, sizeof(scoreStr), "Score: %d", score);
    txtW = strlen(scoreStr) * 6; // size 1
    canvas.setCursor((160 - txtW) / 2, 80);
    canvas.print(scoreStr);

    // Kutlama parcaciklari
    for (int i = 0; i < 20; i++) {
        int px = random(10, SCR_W - 10);
        int py = random(20, SCR_H - 20);
        uint16_t pc = BRICK_COLORS[random(0, BRICK_ROWS)];
        canvas.fillRect(px, py, 3, 3, pc);
    }
}

inline void drawGameOver(TFT_eSprite& canvas, int score, int wave,
                         int highScore, bool newRecord) {
    canvas.fillSprite(COL_BG);

    // Ortak OS game-over: 3 satirlik tablo (: hizali) + NEW BEST rozeti
    char sb[12], wb[12], hb[12];
    snprintf(sb, sizeof(sb), "%d", score);
    snprintf(wb, sizeof(wb), "%d", wave);
    snprintf(hb, sizeof(hb), "%d", highScore);
    OsStat rows[3] = {
        { "Score", sb, TFT_WHITE, TFT_YELLOW },
        { "Wave",  wb, TFT_WHITE, TFT_CYAN   },
        { "Best",  hb, TFT_WHITE, TFT_GREEN  },
    };
    osDrawGameOver(canvas, false, rows, 3, newRecord ? "NEW BEST!" : nullptr);
}

inline void drawPauseOverlay(TFT_eSprite& canvas) {
    osDrawPause(canvas, TFT_GREEN);   // ortak OS pause kutusu (EN, yesil tema)
}
