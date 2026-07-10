#pragma once

#include "Config.h"
#include "../SharedParticles.h"

struct ParticleSystem : public SharedParticleSystem<MAX_PARTICLES> {

    void spawn(float px, float py, uint16_t color, int count) {
        for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
            if (!particles[i].active) {
                particles[i].x = px;
                particles[i].y = py;
                particles[i].vx = random(-75, 73);
                particles[i].vy = random(-75, 28);
                particles[i].color = color;
                particles[i].life = random(8, 16) / 30.0f;
                particles[i].active = true;
                count--;
            }
        }
    }

    void update(float dt) {
        SharedParticleSystem<MAX_PARTICLES>::update(dt, 3.6f, SCR_H);
    }
};
