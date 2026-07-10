#pragma once

#include "Config.h"

struct Bird {
    float y;
    float vel;
    float angle;

    void init() {
        y = SCR_H / 2.0f;
        vel = 0;
        angle = 0;
    }

    void update(float dt) {
        vel += GRAVITY * dt;
        y += vel * dt;

        // --- Target angle based on vertical velocity ---
        // vel < 0 means bird is RISING (going UP on screen)   → nose UP   (negative angle)
        // vel > 0 means bird is FALLING (going DOWN on screen) → nose DOWN (positive angle)
        //
        // Phase 1: Rising fast       → nose locked at -30° (up)
        // Phase 2: Apex grace zone   → nose gradually returns from -30° to 0°
        // Phase 3: Falling           → nose tilts down 0° → 90° (quadratic ease)

        float targetAngle;

        if (vel <= -54.0f) {
            // Phase 1: Strong upward velocity → nose fully up
            targetAngle = BIRD_MIN_ANGLE;           // -30°
        }
        else if (vel < 50.0f) {
            // Phase 2: Apex grace zone (vel -54 to +50)
            // Maps linearly: vel=-54 → -30°, vel=0 → ~-14°, vel=+50 → 0°
            float t = (vel + 54.0f) / (50.0f + 54.0f);   // 0.0 → 1.0
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            targetAngle = BIRD_MIN_ANGLE * (1.0f - t);    // -30 → 0
        }
        else {
            // Phase 3: Falling (vel >= 50) → quadratic ease-in toward 90°
            float fallRatio = (vel - 50.0f) / 150.0f;
            if (fallRatio > 1.0f) fallRatio = 1.0f;
            targetAngle = BIRD_MAX_ANGLE * fallRatio * fallRatio;  // 0 → 90°
        }

        // --- Asymmetric rotation speed ---
        //   Going nose-UP (target < current):  fast  (360°/s) — responsive jump
        //   Going nose-DOWN (target > current): slow  (160°/s) — graceful fall
        float rotSpeed = (targetAngle < angle) ? BIRD_ANGLE_SPEED : 160.0f;
        float step = rotSpeed * dt;

        if (angle < targetAngle) {
            angle += step;
            if (angle > targetAngle) angle = targetAngle;
        } else if (angle > targetAngle) {
            angle -= step;
            if (angle < targetAngle) angle = targetAngle;
        }
    }

    void jump() {
        vel = JUMP_VEL;                   // -90 px/s (upward)
        angle = BIRD_MIN_ANGLE;           // Immediate nose-up kick (no 1-frame lag)
    }

    void dieFall(float dt) {
        vel += GRAVITY * dt;
        y += vel * dt;
        angle = BIRD_MAX_ANGLE;
        if (y + BIRD_R > GROUND_Y) {
            y = GROUND_Y - BIRD_R;
            vel = 0;
        }
    }

    bool hitsGround() const {
        return (y + BIRD_R >= GROUND_Y);
    }
};
