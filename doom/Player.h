#pragma once
// ============================================================
//  Player.h — E-OS DOOM: Oyuncu hareket, kalkan, silah,
//  eşya toplama ve etkileşim mantığı
// ============================================================

#include "Config.h"

// ════════════════════════════════════════════════════════════
//  updateShieldState — Kalkan durumunu ve parry penceresini
//  günceller. BTN_D algılama + kalkan cooldown yönetimi.
// ════════════════════════════════════════════════════════════
inline void updateShieldState(uint32_t now) {
    bool currentShieldState = !digitalRead(BTN_D);
    if (currentShieldState && !lastShieldState && (now - shieldStartTime > 500))
        shieldStartTime = now;
    lastShieldState = currentShieldState;
}

// ════════════════════════════════════════════════════════════
//  updateMovement — Joystick ile kamera dönüşü ve oyuncu
//  harita üzerinde hareket. Çarpışma tespiti margin ile.
// ════════════════════════════════════════════════════════════
inline void updateMovement(float dt) {
    int jx = joyRawX - joyCenterX;
    int jy = joyRawY - joyCenterY;
    if (abs(jx) < 300) jx = 0;
    if (abs(jy) < 300) jy = 0;

    if (jx) {
        float rs = (jx / 2000.0f) * 2.5f * dt, s = rs, c = 1.0f - rs * rs * 0.5f, odx = dirX;
        dirX = dirX * c - dirY * s;
        dirY = odx * s + dirY * c;
        float opx = planeX;
        planeX = planeX * c - planeY * s;
        planeY = opx * s + planeY * c;
    }

    if (jy) {
        float mv = (-jy / 2000.0f) * (lastShieldState ? 1.5f : 3.0f) * dt;
        float moveX = dirX * mv;
        float moveY = dirY * mv;
        float marginX = (moveX > 0) ? 0.2f : -0.2f;
        float marginY = (moveY > 0) ? 0.2f : -0.2f;

        if (MAP[(int)py][(int)(px + moveX + marginX)] == 0) px += moveX;
        if (MAP[(int)(py + moveY + marginY)][(int)px] == 0) py += moveY;
    }
}

// ════════════════════════════════════════════════════════════
//  handleUseAction — BTN_B: Kapı açma, kilitli kapı, çıkış,
//  gizli duvar, yakın dövüş. 300ms cooldown.
// ════════════════════════════════════════════════════════════
inline void handleUseAction(uint32_t now, float dt) {
    if (digitalRead(BTN_B) || (now - sonKullanma <= 300)) return;

    bool kapiAcildi = false;
    for (float d = 0.1f; d <= 2.0f; d += 0.2f) {
        int tx = (int)(px + dirX * d), ty = (int)(py + dirY * d);
        if (tx >= 0 && tx < MW && ty >= 0 && ty < MH) {
            int block = MAP[ty][tx];
            if (block >= 6) {
                if (block == 6 || (block == 7 && hasKey)) {
                    // Zaten açılmakta olan kapıda B tekrar tetiklemesin
                    if (doorAnimT(tx, ty) < 0) {
                        if (block == 7) { hasKey = false; playSound(NOTE_E5, 50); }
                        else            { playSound(NOTE_C4, 80); }
                        startDoorAnim(tx, ty);
                    }
                } else if (block == 8) {
                    // Çıkış switch'i basılır; TaskEngine flip karesini gösterip
                    // intermission/victory ekranını çizer (loadLevel de orada).
                    exitPressed = true;
                    levelDone = true;
                    kapiAcildi = true;
                    break;
                } else if (block == 9) {
                    // Asansör: kabin sekansı (kapanış + ışınlanma) TaskEngine'de
                    elevSrcX = (int8_t)tx; elevSrcY = (int8_t)ty;
                    elevatorPending = true;
                }
                kapiAcildi = true;
                break;
            } else if (block == 4) {
                MAP[ty][tx] = 5;
                for (int y = 0; y < MH; y++) {
                    for (int x = 0; x < MW; x++) {
                        if (MAP[y][x] == 31) MAP[y][x] = 0;
                    }
                }
                for (int i = 0; i < NUM_SPRITES; i++) {
                    if (sprites[i].state == -1) sprites[i].state = 1;
                }
                playSound(NOTE_E3, 120);
                kapiAcildi = true;
                break;
            } else if (block > 0 && block < 6) {
                break;
            }
        }
    }

    if (!kapiAcildi && !lastShieldState) {
        bool dusmanVar = false;
        int hedefID = -1;

        for (int i = 0; i < NUM_SPRITES; i++) {
            if (sprites[i].state >= 1 && isMonsterType(sprites[i].type) &&
                sprites[i].animState != ANIM_DEAD) {
                float dx = sprites[i].x - px, dy = sprites[i].y - py, dist = sqrt(dx * dx + dy * dy);
                if (dist < 0.001f) continue;
                float ang = atan2(dy, dx) - atan2(dirY, dirX);
                while (ang > PI) ang -= 2 * PI;
                while (ang < -PI) ang += 2 * PI;
                if (checkLOS(px, py, dx, dy, dist) && abs(ang) < 0.5f && dist < 1.8f) {
                    dusmanVar = true;
                    hedefID = i;
                    break;
                }
            }
        }

        if (dusmanVar) {
            meleeTimer = now;
            playSound(NOTE_F4, 50);
            sprites[hedefID].hp -= 25;
            if (sprites[hedefID].hp <= 0) {
                sprites[hedefID].animState = ANIM_DYING;
                hp += 10;
                if (hp > 100) hp = 100;
                playSound(NOTE_B3, 80);
            } else {
                sprites[hedefID].animState = ANIM_HIT;
                playSound(NOTE_B3, 40);
            }
            sprites[hedefID].animFrame = 0;
            sprites[hedefID].animTimer = now;
        }
    }
    sonKullanma = now;
}

// ════════════════════════════════════════════════════════════
//  handleWeaponSwitch — BTN_C: Silah değiştirme
//  0=tabanca ↔ 1=pompalı (edge detection ile)
// ════════════════════════════════════════════════════════════
inline void handleWeaponSwitch(uint32_t now) {
    static bool btnC_prev = true;
    bool btnC_curr = digitalRead(BTN_C);
    if (btnC_prev && !btnC_curr) {
        weaponType = (weaponType + 1) % 2;
        if (weaponType == 1) { playSound(NOTE_A4, 40); }
        else                 { playSound(NOTE_E4, 40); }
    }
    btnC_prev = btnC_curr;
}

// ════════════════════════════════════════════════════════════
//  handleShooting — BTN_A: Tabanca (tek, hızlı, 20 hasar)
//  veya Pompalı (saçma, yavaş, 45 hasar, çok hedefli).
// ════════════════════════════════════════════════════════════
inline void handleShooting(uint32_t now) {
    bool btnA_curr = !digitalRead(BTN_A);
    if (!btnA_curr || lastShieldState || (now - meleeTimer < 300)) return;

    // --- TABANCA ---
    if (weaponType == 0 && ammo > 0 && now - fireT > 250) {
        ammo--;
        playSound(NOTE_E4, 50);
        fireT = now;
        for (int i = 0; i < NUM_SPRITES; i++) {
            if (sprites[i].state >= 1 &&
                (isMonsterType(sprites[i].type) || sprites[i].type == ST_BARREL) &&
                sprites[i].animState != ANIM_DEAD) {
                float dx = sprites[i].x - px, dy = sprites[i].y - py, dist = sqrt(dx * dx + dy * dy);
                if (dist < 0.001f) continue;
                float ang = atan2(dy, dx) - atan2(dirY, dirX);
                while (ang > PI) ang -= 2 * PI;
                while (ang < -PI) ang += 2 * PI;
                if (checkLOS(px, py, dx, dy, dist) && abs(ang) < 0.2f && dist < 8.0f) {
                    if (sprites[i].type == ST_BARREL) {
                        explodeBarrel(i, now);
                    } else {
                        sprites[i].hp -= 20;
                        if (sprites[i].hp <= 0) {
                            sprites[i].animState = ANIM_DYING;
                            armor += (sprites[i].type == ST_BARON ? 30 : 15);
                            if (armor > 100) armor = 100;
                            playSound(NOTE_B3, 80);
                        } else {
                            sprites[i].animState = ANIM_HIT;
                            playSound(NOTE_E4, 40);
                        }
                        sprites[i].animFrame = 0;
                        sprites[i].animTimer = now;
                    }
                    break;
                }
            }
        }
    }
    // --- POMPALI ---
    else if (weaponType == 1 && ammo > 0 && now - fireT > 600) {
        int cost = ammo >= 3 ? 3 : ammo;
        ammo -= cost;
        playSound(NOTE_E3, 100);
        fireT = now;
        int hits = 0;
        for (int i = 0; i < NUM_SPRITES; i++) {
            if (sprites[i].state >= 1 &&
                (isMonsterType(sprites[i].type) || sprites[i].type == ST_BARREL) &&
                sprites[i].animState != ANIM_DEAD) {
                float dx = sprites[i].x - px, dy = sprites[i].y - py, dist = sqrt(dx * dx + dy * dy);
                if (dist < 0.001f) continue;
                float ang = atan2(dy, dx) - atan2(dirY, dirX);
                while (ang > PI) ang -= 2 * PI;
                while (ang < -PI) ang += 2 * PI;
                if (checkLOS(px, py, dx, dy, dist) && abs(ang) < 0.5f && dist < 4.5f) {
                    if (sprites[i].type == ST_BARREL) {
                        explodeBarrel(i, now);
                    } else {
                        sprites[i].hp -= 45;
                        if (sprites[i].hp <= 0) {
                            sprites[i].animState = ANIM_DYING;
                            armor += (sprites[i].type == ST_BARON ? 30 : 15);
                            if (armor > 100) armor = 100;
                            playSound(NOTE_B3, 80);
                        } else {
                            sprites[i].animState = ANIM_HIT;
                            playSound(NOTE_E4, 40);
                        }
                        sprites[i].animFrame = 0;
                        sprites[i].animTimer = now;
                    }
                    hits++;
                    if (hits >= 3) break;
                }
            }
        }
    }
}

// ════════════════════════════════════════════════════════════
//  dusukCanKalpAtisi — HP ≤ 25 iken ritmik kalp atışı sesi
// ════════════════════════════════════════════════════════════
inline void dusukCanKalpAtisi(uint32_t now) {
    static int lastBeat = 0;
    if (hp > 0 && hp <= 25) {
        int currentBeat = (now % 1000) / 250;
        if (currentBeat == 0 && lastBeat != 0)       { playSound(NOTE_G3, 80);  lastBeat = 0; }
        else if (currentBeat == 1 && lastBeat != 1)  { playSound(NOTE_E3, 100); lastBeat = 1; }
        else if (currentBeat == 2 && lastBeat != 2)  { lastBeat = 2; }
        else if (currentBeat == 3 && lastBeat != 3)  { lastBeat = 3; }
    }
}
