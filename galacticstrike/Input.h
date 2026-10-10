#pragma once
#include <Arduino.h>

// Poll between rendered frames; retain each debounced press until consumed.
struct StrikeButton {
    int raw = HIGH;
    int stable = HIGH;
    uint32_t changedMs = 0;
    bool pending = false;

    void reset(int value, uint32_t now) {
        raw = stable = value;
        changedMs = now;
        pending = false;
    }

    void update(int value, uint32_t now) {
        if (value != raw) {
            raw = value;
            changedMs = now;
        }
        if (stable != raw && now - changedMs >= 8) {
            stable = raw;
            if (stable == LOW) pending = true;
        }
    }

    bool takePress() {
        bool pressed = pending;
        pending = false;
        return pressed;
    }
};
