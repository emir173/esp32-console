#pragma once
// ============================================================
//  E-OS ARKANOID — Ball.h
//  Top: Vektorel hiz, duvar sekme, dinamik raket fizigi,
//  kuyruklu top efekti (ball trail)
// ============================================================
#include <math.h>
#include "Config.h"

class Ball {
    float x, y;        // Guncel pozisyon
    float vx, vy;      // Hiz vektoru (piksel/sn)
    float speed;       // Skaler hiz
    float prevX, prevY; // Onceki frame pozisyonu (tugla carpisma hesaplamasi icin)
    bool stuck;        // Top cubuga yapisik mi?

    // Kuyruklu top (trail) tamponu — shift register
    float trailX[TRAIL_LEN];
    float trailY[TRAIL_LEN];
    int trailCount;

public:
    void init(float startX, float startY, float spd) {
        x = startX;
        y = startY;
        vx = 0.0f;
        vy = 0.0f;
        speed = spd;
        if (speed > MAX_BALL_SPD) speed = MAX_BALL_SPD;
        prevX = x;
        prevY = y;
        stuck = true;
        trailCount = 0;
        for (int i = 0; i < TRAIL_LEN; i++) {
            trailX[i] = x;
            trailY[i] = y;
        }
    }

    void stickTo(float paddleX) {
        stuck = true;
        x = paddleX;
        y = PADDLE_Y - BALL_R - 1;
        vx = 0.0f;
        vy = 0.0f;
        prevX = x;
        prevY = y;
        clearTrail();
    }

    void launch() {
        stuck = false;
        float dir = (random(0, 2) == 0) ? -1.0f : 1.0f;
        float angle = 0.45f + (float)random(0, 20) / 100.0f;  // ~26-37 derece
        vx = dir * speed * sinf(angle);
        vy = -speed * cosf(angle);
    }

    // Donus degeri:
    //   0 = normal, 1 = yatay duvar carpma, 2 = dikey duvar carpma, -1 = top altta dustu
    int update(float dt) {
        if (stuck) return 0;

        prevX = x;
        prevY = y;

        x += vx * dt;
        y += vy * dt;

        // Top altta ekrandan dustu mu?
        if (y - BALL_R > (float)SCR_H) return -1;

        int flags = 0;

        // Sol duvar
        if (x - BALL_R <= 1.0f) {
            x = BALL_R + 1.0f;
            vx = fabsf(vx);
            flags |= 1;
        }
        // Sag duvar
        if (x + BALL_R >= (float)(SCR_W - 1)) {
            x = (float)(SCR_W - 1 - BALL_R);
            vx = -fabsf(vx);
            flags |= 1;
        }
        // Ust duvar (HUD alti)
        if (y - BALL_R <= (float)(HUD_H + 1)) {
            y = (float)(HUD_H + 1 + BALL_R);
            vy = fabsf(vy);
            flags |= 2;
        }

        // Kuyruk tamponuna ekle
        pushTrail();

        return flags;
    }

    // Dinamik raket fizigi: carptigi yere gore aci hesaplar
    // Donus: true = rakete carpti
    bool handlePaddleCollision(float paddleX) {
        if (vy <= 0.0f) return false;  // Sadece top asagi inerken kontrol et

        float halfW = PADDLE_W / 2.0f;
        if (y + BALL_R >= (float)PADDLE_Y &&
            y + BALL_R < (float)(PADDLE_Y + PADDLE_H + 3) &&
            x >= paddleX - halfW - BALL_R &&
            x <= paddleX + halfW + BALL_R) {

            // Carptigi nokta (-1: sol kenar, 0: orta, +1: sag kenar)
            float hitPos = (x - paddleX) / (halfW + (float)BALL_R);
            if (hitPos > 0.9f)  hitPos = 0.9f;
            if (hitPos < -0.9f) hitPos = -0.9f;

            // Aciya donustur (maks ±60 derece = ±1.05 rad)
            float angle = hitPos * 1.05f;
            vx = speed * sinf(angle);
            vy = -speed * cosf(angle);

            // Topu cubugun ustune yerlestir (girismeyi onle)
            y = (float)(PADDLE_Y - BALL_R);
            return true;
        }
        return false;
    }

    void bounceX() { vx = -vx; }
    void bounceY() { vy = -vy; }

    void setSpeed(float spd) { speed = spd; }

    // --- Kuyruklu top (Trail) ---
    void pushTrail() {
        // Shift register: yeni pozisyon basa, eskiler saga kayar
        for (int i = TRAIL_LEN - 1; i > 0; i--) {
            trailX[i] = trailX[i - 1];
            trailY[i] = trailY[i - 1];
        }
        trailX[0] = x;
        trailY[0] = y;
        if (trailCount < TRAIL_LEN) trailCount++;
    }

    void draw(TFT_eSprite& canvas) const {
        // Kuyruk (en eskiden yeniye, topun altinda kalacak sekilde)
        for (int i = TRAIL_LEN - 1; i >= 2; i--) {
            if (i >= trailCount) continue;
            int r = BALL_R - i + 1;
            if (r <= 0) {
                canvas.drawPixel((int)trailX[i], (int)trailY[i], COL_BALL);
            } else {
                canvas.fillCircle((int)trailX[i], (int)trailY[i], r, COL_BALL);
            }
        }

        // Topun kendisi
        int bx = (int)x;
        int by = (int)y;
        canvas.fillCircle(bx, by, BALL_R, COL_BALL);
        // Parlaklik noktasi (sol-ust)
        canvas.drawPixel(bx - 1, by - 1, COL_BALL_GLOW);
    }

    void clearTrail() {
        trailCount = 0;
        for (int i = 0; i < TRAIL_LEN; i++) {
            trailX[i] = x;
            trailY[i] = y;
        }
    }

    // --- Getter'lar ---
    float getX()     const { return x; }
    float getY()     const { return y; }
    float getVX()    const { return vx; }
    float getVY()    const { return vy; }
    float getSpeed() const { return speed; }
    float getPrevX() const { return prevX; }
    float getPrevY() const { return prevY; }
    bool  isStuck()  const { return stuck; }
};
