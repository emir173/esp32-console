#pragma once
// ============================================================
//  Player.h — Pacman oyuncu yapisi ve hareket mantigi
// ============================================================

#include "Config.h"

// ============================================================
//  Actor — Ortak hareket yapisi (Pacman ve hayaletler kullanir)
// ============================================================
struct Actor {
    float x, y;     // Piksel cinsinden konum (yumusak hareket icin float)
    int dx, dy;     // Su anki yon vektoru (-1/0/+1)
    int ndx, ndy;   // Istek (next) yon vektoru — oyuncu girisi ile belirlenir
    float speed;    // Piksel/saniye hareket hizi
};

extern Actor pac;

// ============================================================
//  applyPacmanInput — Joystick okuyarak Pacman'in istek yonunu belirler
//  Baskin eksen mantigi: diagonal egimde daha buyuk olan kazanir
// ============================================================
inline void applyPacmanInput() {
    int jx = analogRead(JOY_X) - joyCenterX;
    int jy = analogRead(JOY_Y) - joyCenterY;
    if (abs(jx) > DEADZONE || abs(jy) > DEADZONE) {
        if (abs(jx) > abs(jy)) {
            pac.ndx = (jx > 0) ? 1 : -1; pac.ndy = 0;
        } else {
            pac.ndy = (jy > 0) ? 1 : -1; pac.ndx = 0;
        }
    }
}

// ============================================================
//  movePacman — Pacman'i dt ile hareket ettirir
//  Kare merkezlerinde yon degisimi ve duvar kontrolu yapilir.
//  Tunel: ekran sol/sag kenarindan gecis (SCR_W wrap).
// ============================================================
inline void movePacman(float dt) {
    int cx = ((int)pac.x / TILE) * TILE + HALF_TILE;
    int cy = ((int)pac.y / TILE) * TILE + HALF_TILE;

    if (abs(pac.x - cx) <= SNAP_TOLERANCE && abs(pac.y - cy) <= SNAP_TOLERANCE) {
        if (pac.ndx != pac.dx || pac.ndy != pac.dy) {
            int nc = ((int)cx / TILE) + pac.ndx;
            int nr = ((int)cy / TILE) + pac.ndy;
            if (!isWall(nc, nr)) {
                pac.dx = pac.ndx; pac.dy = pac.ndy;
                pac.x = cx; pac.y = cy;
            }
        }
    }

    int nc = ((int)pac.x / TILE) + pac.dx;
    int nr = ((int)pac.y / TILE) + pac.dy;

    if (abs(pac.x - cx) <= SNAP_TOLERANCE && abs(pac.y - cy) <= SNAP_TOLERANCE && isWall(nc, nr)) {
        pac.x = cx; pac.y = cy;
    } else {
        pac.x += pac.dx * pac.speed * dt;
        pac.y += pac.dy * pac.speed * dt;
    }

    if (pac.x < 0) pac.x += SCR_W;
    if (pac.x >= SCR_W) pac.x -= SCR_W;
}
