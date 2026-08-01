#pragma once
// ============================================================
//  Ghosts.h — Hayalet yapisi ve AI hareket mantigi
// ============================================================

#include "Config.h"
#include "Player.h"

// ============================================================
//  Ghost — Hayalet yapisi
//  type:  Blinky(chase), Pinky(oncelmeli), Inky(karisik)
//  mode:  chase (kovar), scared (kacar), eaten (eve doner)
// ============================================================
struct Ghost {
    Actor a;
    int type;
    int mode;
    uint32_t scaredUntil;
    uint16_t color;
    int lastTileC;
    int lastTileR;
};

extern Ghost ghosts[NUM_GHOSTS];

// ============================================================
//  moveGhost — Hayaletin AI hareket mantigi
//  Kare merkezlerinde hedefe en kisa mesafe secilir.
//  Geri donus (180°) yasaktir.
//
//  CHASE:  tip'e gore hedef (Blinky/Pinky/Inky)
//  SCARED: rastgele kacar
//  EATEN:  eve (9,9) doner, varinca chase'e gecer
// ============================================================
inline void moveGhost(Ghost &g, float dt) {
    int cx = ((int)g.a.x / TILE) * TILE + HALF_TILE;
    int cy = ((int)g.a.y / TILE) * TILE + HALF_TILE;
    int curC = cx / TILE;
    int curR = cy / TILE;

    if (abs(g.a.x - cx) <= SNAP_TOLERANCE && abs(g.a.y - cy) <= SNAP_TOLERANCE) {
        if (g.lastTileC != curC || g.lastTileR != curR) {
            g.lastTileC = curC;
            g.lastTileR = curR;
            g.a.x = cx; g.a.y = cy;

            int bestDx = g.a.dx, bestDy = g.a.dy;
            float minDist = 99999.0f;
            int tc = 0, tr = 0;

            // Hedef belirle
            if (g.mode == GHOST_EATEN) {
                tc = 9; tr = 9;
                if (((int)g.a.x / TILE) == 9 && ((int)g.a.y / TILE) == 9) g.mode = GHOST_CHASE;
            } else if (g.mode == GHOST_SCARED) {
                tc = random(0, COLS); tr = random(0, ROWS);
                if (millis() > g.scaredUntil) g.mode = GHOST_CHASE;
            } else {
                if (g.type == GHOST_TYPE_BLINKY) {
                    tc = (int)pac.x / TILE; tr = (int)pac.y / TILE;
                } else if (g.type == GHOST_TYPE_PINKY) {
                    tc = ((int)pac.x / TILE) + pac.dx * 3;
                    tr = ((int)pac.y / TILE) + pac.dy * 3;
                } else {
                    tc = (millis() / 3000) % 2 == 0 ? (int)pac.x / TILE : 1; tr = 1;
                }
            }

            // 4 yonu dene
            int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            bool foundMove = false;
            for (int i = 0; i < 4; i++) {
                int dx = dirs[i][0]; int dy = dirs[i][1];
                if (dx == -g.a.dx && dy == -g.a.dy && (g.a.dx != 0 || g.a.dy != 0)) continue;
                int nc = ((int)g.a.x / TILE) + dx;
                int nr = ((int)g.a.y / TILE) + dy;
                if (isWall(nc, nr)) continue;
                float dist = sq(nc - tc) + sq(nr - tr);
                if (dist < minDist) {
                    minDist = dist;
                    bestDx = dx; bestDy = dy;
                    foundMove = true;
                }
            }
            if (!foundMove) {
                bestDx = -g.a.dx; bestDy = -g.a.dy;
            }
            g.a.dx = bestDx; g.a.dy = bestDy;
        }
    }

    float spd = g.a.speed;
    if (g.mode == GHOST_SCARED) spd *= SCARED_SPEED_MUL;
    if (g.mode == GHOST_EATEN)  spd *= EATEN_SPEED_MUL;

    g.a.x += g.a.dx * spd * dt;
    g.a.y += g.a.dy * spd * dt;
    if (g.a.x < 0) g.a.x += SCR_W;
    if (g.a.x >= SCR_W) g.a.x -= SCR_W;
}
