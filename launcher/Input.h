#pragma once
#include <Arduino.h>

// Read every loop, consume one stable press. A held key on screen entry is
// disarmed until released, so opening/closing a popup cannot trigger an action.
struct LauncherButton {
    int raw = HIGH;
    int stable = HIGH;
    uint32_t changedMs = 0;
    bool pressed = false;
    bool armed = true;
    bool active = false;

    void reset(int value, uint32_t now) {
        raw = stable = value;
        changedMs = now;
        pressed = false;
        active = false;
        armed = (value == HIGH);
    }

    void update(int value, uint32_t now) {
        pressed = false;
        if (value != raw) {
            raw = value;
            changedMs = now;
        }
        if (stable != raw && now - changedMs >= 20) {
            stable = raw;
            if (stable == HIGH) { armed = true; active = false; }
            else if (armed) { pressed = true; active = true; armed = false; }
        }
    }

    bool down() const { return stable == LOW; }
    bool held() const { return active && down(); }
};

struct LauncherInput {
    LauncherButton a, b, c, d, joy;
    void reset(uint32_t now) {
        a.reset(digitalRead(BTN_A), now);
        b.reset(digitalRead(BTN_B), now);
        c.reset(digitalRead(BTN_C), now);
        d.reset(digitalRead(BTN_D), now);
        joy.reset(digitalRead(JOY_SW), now);
    }
    void update(uint32_t now) {
        a.update(digitalRead(BTN_A), now);
        b.update(digitalRead(BTN_B), now);
        c.update(digitalRead(BTN_C), now);
        d.update(digitalRead(BTN_D), now);
        joy.update(digitalRead(JOY_SW), now);
    }
};
