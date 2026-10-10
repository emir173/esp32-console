#pragma once

#include "Config.h"

// CP0 is the start/finish line at the top of the track. All cars must visit
// CP1..CP7 before returning to CP0 to complete a lap.
inline Vec2 trackPoint(float rad) {
    float wob = TRK_WOB(rad);
    return {MAP_W / 2.0f + (MAP_W / 2.0f - 26) * wob * cosf(rad),
            MAP_H / 2.0f + (MAP_H / 2.0f - 26) * wob * sinf(rad)};
}

inline bool advanceCheckpoint(const Racer& racer, int& nextCP, int& lap) {
    const Checkpoint& cp = checkpoints[nextCP];
    float dx = racer.x - cp.x;
    float dy = racer.y - cp.y;
    if (dx * dx + dy * dy >= cp.radius * cp.radius) return false;

    bool completedLap = (nextCP == 0);
    nextCP = (nextCP + 1) % NUM_CHECKPOINTS;
    if (completedLap) ++lap;
    return completedLap;
}

inline int checkpointProgress(int nextCP) {
    return nextCP == 0 ? NUM_CHECKPOINTS : nextCP;
}

inline int racePosition(const Racer& player, int playerLap, int playerNextCP,
                        const Racer* aiCars, const int* aiLap, const int* aiCP) {
    int pos = 1;
    const Checkpoint& cp = checkpoints[playerNextCP];
    float dx = player.x - cp.x, dy = player.y - cp.y;
    float playerDistance = dx * dx + dy * dy;
    for (int i = 0; i < NUM_AI; ++i) {
        if (aiLap[i] > playerLap) { ++pos; continue; }
        if (aiLap[i] != playerLap) continue;
        int aiProgress = checkpointProgress(aiCP[i]);
        int progress = checkpointProgress(playerNextCP);
        if (aiProgress > progress) { ++pos; continue; }
        if (aiProgress == progress) {
            float ax = aiCars[i].x - cp.x, ay = aiCars[i].y - cp.y;
            if (ax * ax + ay * ay < playerDistance) ++pos;
        }
    }
    return pos;
}
