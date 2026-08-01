#pragma once
// ============================================================
//  E-OS MODE7 RACING — Player.h
//  Oyuncu araci fizigi, motor sesi, AI rakip guncelleme ve
//  arac-arac carpisma mantigi.
// ============================================================

#include "Config.h"
#include "../hardware_config.h"

// ============================================================
//  updatePlayerPhysics — Oyuncu arac fizigi (delta-time tabanli)
//  p       : Oyuncu Racer referansi (konum, aci, hiz guncellenir)
//  dt      : Delta-time (saniye)
//  gas     : Gaza basili mi
//  brake   : Frene basili mi
//  steer   : Direksiyon inputu (-1..1)
// ============================================================
inline void updatePlayerPhysics(Racer& p, float dt, bool gas, bool brake, float steer) {
    if (gas)      p.speed += ACCEL * dt;
    else          p.speed -= DRAG * dt;

    if (brake)    p.speed -= BRAKE_FORCE * dt;

    p.speed = constrain(p.speed, 0.0f, MAX_SPEED);

    float steerAmount = steer * STEER_RATE * (p.speed / MAX_SPEED + 0.3f);
    p.angle += steerAmount * dt;

    p.x += cosf(p.angle) * p.speed * dt;
    p.y += sinf(p.angle) * p.speed * dt;

    int mx = (int)p.x; if (mx < 0) mx = 0; else if (mx >= MAP_W) mx = MAP_W - 1;
    int my = (int)p.y; if (my < 0) my = 0; else if (my >= MAP_H) my = MAP_H - 1;
    if (!trackMap[my * MAP_W + mx]) {
        p.speed -= GRASS_SLOW * dt;
        if (p.speed < 0) p.speed = 0;
    }

    p.x = constrain(p.x, 1.0f, (float)MAP_W - 2);
    p.y = constrain(p.y, 1.0f, (float)MAP_H - 2);
}

// ============================================================
//  updateMotorSound — Motor sesi (surekli buzzer tonu)
//  p            : Oyuncu Racer referansi
//  gas          : Gaza basili mi
//  soundEnabled : Global ses acik/kapali
//  animTick     : Animasyon sayaci (dalgalanma icin)
// ============================================================
inline void updateMotorSound(const Racer& p, bool gas, bool soundEnabled, uint32_t animTick) {
    if (soundEnabled && p.speed > 1.0f) {
        int baseFreq = gas ? MOTOR_GAS_FREQ : MOTOR_IDLE_FREQ;
        int freq = baseFreq + (int)(p.speed / MAX_SPEED * 15);
        freq += (animTick % 3) * 6;
        osBuzzerTone(freq);   // LEDC + volume (surekli motor tonu)
    } else {
        osBuzzerOff();
    }
}

// ============================================================
//  updateAI — AI rakip guncelleme (checkpoint takibi + rubber-band hiz)
//  r       : AI Racer referansi
//  nextCP  : AI'nin siradaki checkpoint index'i (guncellenir)
//  lap     : AI'nin tur sayisi (guncellenir)
//  dt      : Delta-time (saniye)
//  player  : Oyuncu referansi (rubber-band mesafe hesabi icin)
// ============================================================
inline void updateAI(Racer& r, int& nextCP, int& lap, float dt, const Racer& player) {
    Checkpoint& cp = checkpoints[nextCP % NUM_CHECKPOINTS];

    float dx = cp.x - r.x;
    float dy = cp.y - r.y;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist < cp.radius) {
        nextCP = (nextCP + 1) % NUM_CHECKPOINTS;
        if (nextCP == 0) lap++;
    }

    float targetAngle = atan2f(dy, dx);
    float angleDiff = targetAngle - r.angle;
    while (angleDiff > M_PI)  angleDiff -= 2 * M_PI;
    while (angleDiff < -M_PI) angleDiff += 2 * M_PI;

    r.angle += angleDiff * 3.0f * dt;

    float targetSpeed = AI_NORMAL_SPEED;
    float pDist = sqrtf((r.x - player.x) * (r.x - player.x) +
                        (r.y - player.y) * (r.y - player.y));
    if (pDist > 15) targetSpeed = AI_FAST_SPEED;
    if (pDist < 5)  targetSpeed = AI_SLOW_SPEED;

    r.speed += (targetSpeed - r.speed) * 2.0f * dt;

    r.x += cosf(r.angle) * r.speed * dt;
    r.y += sinf(r.angle) * r.speed * dt;

    r.x = constrain(r.x, 1.0f, (float)MAP_W - 2);
    r.y = constrain(r.y, 1.0f, (float)MAP_H - 2);
}

// ============================================================
//  handlePlayerAICollision — Oyuncu vs AI carpisma (itme + yavaslama)
//  player       : Oyuncu Racer referansi
//  ai           : AI Racer referansi
//  collisionSfx : Carpisma sesi calinmali mi (cikti parametresi)
// ============================================================
inline void handlePlayerAICollision(Racer& player, Racer& ai, bool& collisionSfx) {
    float ddx = player.x - ai.x;
    float ddy = player.y - ai.y;
    float d2 = ddx * ddx + ddy * ddy;

    if (d2 < 16.0f) {
        float d = sqrtf(d2); if (d < 0.001f) d = 0.001f;
        float push = 4.0f - d;
        player.x += (ddx / d) * push;
        player.y += (ddy / d) * push;
        ai.x     -= (ddx / d) * push * 0.5f;
        ai.y     -= (ddy / d) * push * 0.5f;
        player.speed *= 0.6f;
        collisionSfx = true;
    } else {
        collisionSfx = false;
    }
}
