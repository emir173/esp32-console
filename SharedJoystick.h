#pragma once

#include <math.h>
#include "hardware_config.h"

// ============================================================
//  Advanced Joystick Processor
//  4 improvements over the original EMA + hysteresis pipeline:
//
//    1. Radial Deadzone       — sqrt(jx^2 + jy^2) circular gate
//    2. Dynamic EMA Alpha     — flick=1.0 / stable=0.1 adaptive
//    3. Octagonal Gating      — atan2() 4 equal angular cones
//    4. Auto-Center Drift     — silent background recalibration
// ============================================================

namespace JoySettings {
    constexpr int   DEADZONE_RADIUS    = 500;
    constexpr float EMA_ALPHA_MIN      = 0.10f;
    constexpr float EMA_ALPHA_MAX      = 1.00f;
    constexpr int   FLICK_THRESHOLD    = 350;
    constexpr float CONE_HALF_ANGLE    = 45.0f;
    constexpr float CONE_DEAD_DEG      = 5.0f;
    constexpr unsigned long DIR_COOLDOWN_MS      = 60;
    constexpr unsigned long AUTO_CAL_IDLE_MS     = 5000;
    constexpr unsigned long AUTO_CAL_INTERVAL_MS = 500;
    constexpr int   AUTO_CAL_MAX_DRIFT = 200;
}

// Radians -> degrees (ESP32 Arduino.h already #defines RAD_TO_DEG)
constexpr float JOY_RAD2DEG = 180.0f / 3.14159265358979323846f;

struct JoystickProcessor {

    // ---- Center calibration ----
    int centerX, centerY;          // current (drift-adjusted) center
    int origCenterX, origCenterY;  // startup values (safety limit)

    // ---- EMA filter state ----
    float emaX, emaY;
    float alpha;                   // current adaptive alpha

    // ---- Auto-center drift ----
    unsigned long lastActiveMs;    // last time stick was deflected
    unsigned long lastCalCheckMs;  // last calibration step

    // ---- Direction state ----
    int currentDir;                // -1 = deadzone
    int prevDir;                   // previous frame (edge detection)
    unsigned long lastDirChangeMs; // cooldown timer

    // ---- Init with startup calibration values ----
    void init(int cx, int cy) {
        centerX = cx;
        centerY = cy;
        origCenterX = cx;
        origCenterY = cy;
        emaX = 0.0f;
        emaY = 0.0f;
        alpha = JoySettings::EMA_ALPHA_MIN;
        currentDir = -1;
        prevDir = -1;
        lastActiveMs = millis();
        lastCalCheckMs = millis();
        lastDirChangeMs = 0;
    }

    // ---- Reset filter + direction (new game / state change) ----
    void reset() {
        emaX = 0.0f;
        emaY = 0.0f;
        currentDir = -1;
        prevDir = -1;
    }

    // ---- Main update: read -> filter -> deadzone -> angle -> direction ----
    // Returns currentDir (-1 if inside radial deadzone)
    int update() {
        unsigned long now = millis();

        int rawX = analogRead(JOY_X) - centerX;
        int rawY = analogRead(JOY_Y) - centerY;

        // ====== 2. Dynamic EMA Alpha (Adaptive Smoothing) ======
        float deltaX = (float)rawX - emaX;
        float deltaY = (float)rawY - emaY;
        float flickMag = sqrtf(deltaX * deltaX + deltaY * deltaY);

        float t = flickMag / (float)JoySettings::FLICK_THRESHOLD;
        if (t > 1.0f) t = 1.0f;
        alpha = JoySettings::EMA_ALPHA_MIN + (JoySettings::EMA_ALPHA_MAX - JoySettings::EMA_ALPHA_MIN) * t;

        emaX = alpha * (float)rawX + (1.0f - alpha) * emaX;
        emaY = alpha * (float)rawY + (1.0f - alpha) * emaY;

        // ====== 1. Radial Deadzone (Pythagorean) ======
        int iemaX = (int)emaX;
        int iemaY = (int)emaY;
        float mag = sqrtf((float)iemaX * iemaX + (float)iemaY * iemaY);

        if (mag < (float)JoySettings::DEADZONE_RADIUS) {
            currentDir = -1;
        } else {
            lastActiveMs = now;

            // ====== 3. Octagonal Gating (atan2 — 4 equal cones) ======
            float angle = atan2f((float)iemaY, (float)iemaX) * JOY_RAD2DEG;
            float absAngle = fabsf(angle);

            int newDir;
            if (currentDir == -1) {
                newDir = coneExact(absAngle, angle);
            } else {
                newDir = coneHysteresis(absAngle, angle);
                if (newDir == -1) newDir = currentDir;
            }

            // Cooldown guard on direction changes
            if (newDir != currentDir) {
                if (now - lastDirChangeMs > JoySettings::DIR_COOLDOWN_MS) {
                    currentDir = newDir;
                    lastDirChangeMs = now;
                }
            }
        }

        // ====== 4. Auto-Center Drift Calibration ======
        updateAutoCenter(now, mag);

        return currentDir;
    }

    int coneExact(float absAngle, float angle) const {
        if (absAngle < JoySettings::CONE_HALF_ANGLE) {
            return DIR_RIGHT;
        } else if (absAngle < 3.0f * JoySettings::CONE_HALF_ANGLE) {
            return (angle > 0.0f) ? DIR_DOWN : DIR_UP;
        } else {
            return DIR_LEFT;
        }
    }

    int coneHysteresis(float absAngle, float angle) const {
        float b1 = JoySettings::CONE_HALF_ANGLE - JoySettings::CONE_DEAD_DEG;
        float b2 = JoySettings::CONE_HALF_ANGLE + JoySettings::CONE_DEAD_DEG;
        float b3 = 3.0f * JoySettings::CONE_HALF_ANGLE - JoySettings::CONE_DEAD_DEG;
        float b4 = 3.0f * JoySettings::CONE_HALF_ANGLE + JoySettings::CONE_DEAD_DEG;

        if (absAngle < b1) {
            return DIR_RIGHT;
        } else if (absAngle > b2 && absAngle < b3) {
            return (angle > 0.0f) ? DIR_DOWN : DIR_UP;
        } else if (absAngle > b4) {
            return DIR_LEFT;
        }
        return -1;
    }

    void updateAutoCenter(unsigned long now, float currentMag) {
        if (currentMag > (float)JoySettings::DEADZONE_RADIUS) return;
        if (now - lastActiveMs < JoySettings::AUTO_CAL_IDLE_MS) return;
        if (now - lastCalCheckMs < JoySettings::AUTO_CAL_INTERVAL_MS) return;
        lastCalCheckMs = now;

        long sumX = 0, sumY = 0;
        for (int i = 0; i < 4; i++) {
            sumX += analogRead(JOY_X);
            sumY += analogRead(JOY_Y);
        }
        int avgX = sumX / 4;
        int avgY = sumY / 4;

        if (avgX > centerX && centerX < origCenterX + JoySettings::AUTO_CAL_MAX_DRIFT)
            centerX++;
        else if (avgX < centerX && centerX > origCenterX - JoySettings::AUTO_CAL_MAX_DRIFT)
            centerX--;

        if (avgY > centerY && centerY < origCenterY + JoySettings::AUTO_CAL_MAX_DRIFT)
            centerY++;
        else if (avgY < centerY && centerY > origCenterY - JoySettings::AUTO_CAL_MAX_DRIFT)
            centerY--;
    }
};
