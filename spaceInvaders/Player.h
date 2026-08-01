#pragma once
// ============================================================
//  E-OS SPACE INVADERS — Player.h
//  Oyuncu gemisi: pozisyon, hareket, ates, dokunulmazlik
// ============================================================
#include "Config.h"

struct Player {
    float x;
    int lives;
    unsigned long hitTime;
    unsigned long lastFireMs;

    void init() {
        x = SCR_W / 2.0f;
        lives = START_LIVES;
        hitTime = 0;
        lastFireMs = 0;
    }

    bool isInvincible() const {
        return (millis() - hitTime < (unsigned long)INVINCIBLE_MS);
    }

    void takeDamage() {
        lives--;
        hitTime = millis();
    }

    void move(float rawJoyX, int joyCenterX, float dt) {
        float jx = rawJoyX - (float)joyCenterX;
        float ax = (jx > 0.0f) ? jx : -jx;
        if (ax > (float)DEADZONE) {
            float factor = (ax - (float)DEADZONE) / (2048.0f - (float)DEADZONE);
            if (factor > 1.0f) factor = 1.0f;
            float spd = 30.0f + factor * 75.0f;
            x += ((jx > 0.0f) ? spd : -spd) * dt;
        }
        float halfW = (float)PLAYER_W / 2.0f;
        if (x < halfW + 1.0f)      x = halfW + 1.0f;
        if (x > SCR_W - halfW - 1.0f) x = (float)SCR_W - halfW - 1.0f;
    }

    bool canFire() const {
        return (millis() - lastFireMs >= (unsigned long)FIRE_COOLDOWN);
    }

    void markFired() {
        lastFireMs = millis();
    }

    float gunTipX() const { return x; }
    float gunTipY() const { return (float)PLAYER_Y - 2.0f; }

    float hitboxLeft()   const { return x - (float)PLAYER_W / 2.0f; }
    float hitboxRight()  const { return x + (float)PLAYER_W / 2.0f; }
    float hitboxTop()    const { return (float)PLAYER_Y; }
    float hitboxBottom() const { return (float)PLAYER_Y + (float)PLAYER_H; }
};
