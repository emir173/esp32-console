#pragma once
#include <TFT_eSPI.h>
#include <cmath>

struct Particle {
    float x, y, vx, vy;
    uint16_t color;
    float life;
    bool active;
};

template <int MAX_PART>
struct SharedParticleSystem {
    static_assert(MAX_PART > 0, "Particle pool must not be empty");
    Particle particles[MAX_PART] = {};

    void clear() {
        for (int i = 0; i < MAX_PART; i++) {
            particles[i].active = false;
        }
    }

    void update(float dt, float gravity = 0.0f, float maxY = 9999.0f) {
        if (!std::isfinite(dt) || dt <= 0.0f) return;
        for (int i = 0; i < MAX_PART; i++) {
            if (!particles[i].active) continue;
            
            particles[i].vy += gravity * dt;
            particles[i].x += particles[i].vx * dt;
            particles[i].y += particles[i].vy * dt;
            particles[i].life -= dt;
            
            // Eğer ömrü bittiyse veya ekrandan (maxY) çıktıysa deaktif et
            if (particles[i].life <= 0.0f || particles[i].y >= maxY) {
                particles[i].active = false;
            }
        }
    }

    void draw(TFT_eSprite& canvas) const {
        for (int i = 0; i < MAX_PART; i++) {
            if (particles[i].active) {
                canvas.drawPixel((int)particles[i].x, (int)particles[i].y, particles[i].color);
            }
        }
    }
};
