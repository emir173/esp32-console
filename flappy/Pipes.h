#pragma once

#include "Config.h"

struct Pipe {
    float x;
    int gapCenter;
    bool scored;
};

struct Pipes {
    Pipe p[NUM_PIPES];

    void init() {
        for (int i = 0; i < NUM_PIPES; i++) {
            p[i].x = SCR_W + 30 + i * PIPE_DIST;
            p[i].gapCenter = random(PIPE_GAP / 2 + 10, GROUND_Y - PIPE_GAP / 2 - 10);
            p[i].scored = false;
        }
    }

    int update(float dt, float speed) {
        int newlyScored = 0;
        for (int i = 0; i < NUM_PIPES; i++) {
            p[i].x -= speed * dt;
        }

        for (int i = 0; i < NUM_PIPES; i++) {
            // Award only after the entire bird has cleared the pipe lip.
            if (!p[i].scored && p[i].x + PIPE_W + PIPE_LIP_OVERHANG < BIRD_X - BIRD_R) {
                p[i].scored = true;
                newlyScored++;
            }

            if (p[i].x + PIPE_W + PIPE_LIP_OVERHANG < -5) {
                float maxX = 0;
                for (int j = 0; j < NUM_PIPES; j++) {
                    if (p[j].x > maxX) maxX = p[j].x;
                }
                p[i].x = maxX + PIPE_DIST;
                p[i].gapCenter = random(PIPE_GAP / 2 + 10, GROUND_Y - PIPE_GAP / 2 - 10);
                p[i].scored = false;
            }
        }
        return newlyScored;
    }

    bool collidesWith(float birdY, int birdR) const {
        for (int i = 0; i < NUM_PIPES; i++) {
            int px = (int)p[i].x;
            int gapTop = p[i].gapCenter - PIPE_GAP / 2;
            int gapBot = p[i].gapCenter + PIPE_GAP / 2;
            bool bodyOverlap = BIRD_X + birdR > px && BIRD_X - birdR < px + PIPE_W;
            if (bodyOverlap && (birdY - birdR < gapTop || birdY + birdR > gapBot)) return true;

            bool lipOverlap = BIRD_X + birdR > px - PIPE_LIP_OVERHANG &&
                              BIRD_X - birdR < px + PIPE_W + PIPE_LIP_OVERHANG;
            if (lipOverlap &&
                ((birdY + birdR > gapTop - PIPE_LIP_H && birdY - birdR < gapTop) ||
                 (birdY + birdR > gapBot && birdY - birdR < gapBot + PIPE_LIP_H))) return true;
        }
        return false;
    }
};
