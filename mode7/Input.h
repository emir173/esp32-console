#pragma once

#include <Arduino.h>

struct Mode7Button {
    int raw = HIGH;
    int stable = HIGH;
    uint32_t changedMs = 0;

    void reset(int value, uint32_t now) {
        raw = stable = value;
        changedMs = now;
    }

    bool update(int value, uint32_t now) {
        if (value != raw) { raw = value; changedMs = now; }
        if (stable == raw || now - changedMs < 20) return false;
        stable = raw;
        return stable == LOW;
    }

    bool down() const { return stable == LOW; }
};
