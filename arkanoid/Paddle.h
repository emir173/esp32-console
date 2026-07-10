#pragma once
// ============================================================
//  E-OS ARKANOID — Paddle.h
//  Raket: X ekseninde puruzsuz analog hareket,
//  olum bolge (deadzone) ve ekran siniri clamp
// ============================================================
#include "Config.h"
#include "../hardware_config.h"

class Paddle {
    float x;
    int centerX;

public:
    void init(int cx) {
        centerX = cx;
        x = SCR_W / 2.0f;
    }

    void reset() {
        x = SCR_W / 2.0f;
    }

    void update(float dt) {
        int raw = analogRead(JOY_X) - centerX;
        float spd = 0.0f;

        if (abs(raw) > DEADZONE) {
            float sign = (raw > 0) ? 1.0f : -1.0f;
            float factor = (float)(abs(raw) - DEADZONE) / (float)(2048 - DEADZONE);
            if (factor > 1.0f) factor = 1.0f;
            spd = sign * (36.0f + factor * 90.0f);  // 36 ~ 126 piksel/sn
        }

        x += spd * dt;

        // Ekran siniri clamp (1px duvar payi birak)
        float halfW = (float)PADDLE_W / 2.0f;
        if (x < halfW + 1.0f) x = halfW + 1.0f;
        if (x > (float)SCR_W - halfW - 1.0f) x = (float)SCR_W - halfW - 1.0f;
    }

    void draw(TFT_eSprite& canvas) const {
        int px = (int)x - PADDLE_W / 2;
        int py = PADDLE_Y;

        // Ana govde
        canvas.fillRect(px, py, PADDLE_W, PADDLE_H, COL_PADDLE);
        // Ust vurgu
        canvas.drawFastHLine(px, py, PADDLE_W, COL_PADDLE_HL);
        // Alt golge
        canvas.drawFastHLine(px, py + PADDLE_H - 1, PADDLE_W, COL_PADDLE_DK);
        // Orta dekoratif cizgi
        canvas.drawFastVLine((int)x, py + 1, PADDLE_H - 2, COL_PADDLE_HL);
    }

    float getX()    const { return x; }
    float getHalfW() const { return (float)PADDLE_W / 2.0f; }
};
