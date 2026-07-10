#pragma once
// ============================================================
//  Player.h — Oyuncu uzay gemisi mantigi (ates etme, timer guncelleme)
// ============================================================
#include "Config.h"

// firePlayer — Oyuncu gemisinden mermi atesler
//  Ates bekleme suresi (shootCD) kontrol eder; triple shot aktifse
//  ek 2 mermi (sag-sol capraz) daha atesler. Ses efekti calar.
inline void firePlayer() {
    if (ship.shootCD > 0.0f) return;
    ship.shootCD = SHOOT_COOLDOWN;

    for (int i = 0; i < MAX_P_BULLETS; i++) {
        if (!pBullets[i].active) {
            pBullets[i] = {ship.x, ship.y - 5, 0.0f, -120.0f, true};
            break;
        }
    }

    if (ship.tripleTimer > 0.0f) {
        for (int s = 0; s < 2; s++) {
            float vxOff = (s == 0) ? -45.0f : 45.0f;
            for (int i = 0; i < MAX_P_BULLETS; i++) {
                if (!pBullets[i].active) {
                    pBullets[i] = {ship.x, ship.y - 3, vxOff, -105.0f, true};
                    break;
                }
            }
        }
    }
    playSound(NOTE_A4, 25);
}
