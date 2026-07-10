#pragma once
// ============================================================
//  Enemies.h — E-OS DOOM: Düşman yapay zekası, animasyon
//  durum makinesi, mermi fiziği ve varil patlaması
// ============================================================

#include "Config.h"

// ============================================================
//  getEnemyTexID — Sprite'ın tür ve animasyon durumuna göre
//  texture atlas indeksini döndürür.
// ============================================================
inline int getEnemyTexID(int spriteIndex) {
    Sprite &s = sprites[spriteIndex];
    if (s.type == ST_ZOMBIE) return ZOMBIE_FRAMES[s.animState][s.animFrame & 1];
    if (s.type == ST_PINKY)  return PINKY_FRAMES[s.animState][s.animFrame & 1];
    if (s.type == ST_BARON)  return BARON_FRAMES[s.animState][s.animFrame & 1];
    if (s.type == ST_BARREL) return VARIL_FRAMES[s.animState][s.animFrame & 1];
    if (s.type == ST_CORPSE) return 33;   // dekor ceset: pinky cesedi yeniden kullanılır
    return s.type;
}

// ============================================================
//  initSprite — Sprite başlatma: konum, tür, can değeri atar.
//  Türüne göre hp: zombi=30, pinky=60, baron=150, varil=10.
// ============================================================
inline void initSprite(int i, float x, float y, int type, int state) {
    sprites[i].x = x; sprites[i].y = y;
    sprites[i].type = type; sprites[i].state = state;
    sprites[i].dx = 0; sprites[i].dy = 0;
    sprites[i].animState = ANIM_IDLE;
    sprites[i].animFrame = 0;
    sprites[i].animTimer = 0;
    sprites[i].lastFireTime = 0;

    if (type == ST_ZOMBIE)      sprites[i].hp = 30;
    else if (type == ST_PINKY)  sprites[i].hp = 60;
    else if (type == ST_BARON)  sprites[i].hp = 150;
    else if (type == ST_BARREL) sprites[i].hp = 10;
    else                        sprites[i].hp = 0;
}

// ============================================================
//  explodeBarrel — Varili patlatır, zincirleme etki uygular.
//  2.5 birim yarıçaptaki düşmanlara 50 hasar, oyuncuya
//  mesafeye bağlı azalan hasar verir.
// ============================================================
inline void explodeBarrel(int barrelIdx, uint32_t now) {
    if (sprites[barrelIdx].animState == ANIM_DYING || sprites[barrelIdx].animState == ANIM_DEAD) return;
    float bx = sprites[barrelIdx].x, by = sprites[barrelIdx].y;
    sprites[barrelIdx].animState = ANIM_DYING;
    sprites[barrelIdx].animFrame = 0;
    sprites[barrelIdx].animTimer = now;
    playSound(NOTE_E3, 120);

    for (int i = 0; i < NUM_SPRITES; i++) {
        if (i == barrelIdx || sprites[i].state <= 0 || sprites[i].animState == ANIM_DEAD) continue;
        float dx = sprites[i].x - bx, dy = sprites[i].y - by;
        float distSq = dx * dx + dy * dy;
        if (distSq < 6.25f) {
            if (sprites[i].type == ST_BARREL) {
                explodeBarrel(i, now);
            } else if (isMonsterType(sprites[i].type)) {
                sprites[i].hp -= 50;
                if (sprites[i].hp <= 0) sprites[i].animState = ANIM_DYING;
                else                    sprites[i].animState = ANIM_HIT;
                sprites[i].animFrame = 0;
                sprites[i].animTimer = now;
            }
        }
    }

    float pdx = px - bx, pdy = py - by, pdistSq = pdx * pdx + pdy * pdy;
    if (pdistSq < 6.25f && !lastShieldState) {
        float pdist = sqrt(pdistSq);
        int dmg = (int)(40 * (1.0f - pdist / 2.5f));
        if (armor > 0) {
            int absorb = min(armor, (dmg + 1) / 2);
            armor -= absorb;
            hp -= (dmg - absorb);
        } else {
            hp -= dmg;
        }
        lastDamageTime = now;
    }
}

// ============================================================
//  updateAnimations — Tüm sprite'ların animasyon durum
//  makinesini ilerletir (WALK/ATTACK/HIT/DYING kare geçişleri).
// ============================================================
inline void updateAnimations(uint32_t now) {
    const uint16_t WALK_MS[]  = {200, 150, 300};
    const uint16_t DYING_MS   = 250;

    for (int i = 0; i < NUM_SPRITES; i++) {
        Sprite &s = sprites[i];
        if (s.state <= 0 || s.animState == ANIM_DEAD) continue;

        uint32_t elapsed = now - s.animTimer;
        if (isMonsterType(s.type)) {
            int tIdx = (s.type == ST_ZOMBIE) ? 0 : (s.type == ST_PINKY) ? 1 : 2;
            if (s.animState == ANIM_WALK) {
                if (elapsed > WALK_MS[tIdx]) { s.animFrame = 1 - s.animFrame; s.animTimer = now; }
            } else if (s.animState == ANIM_ATTACK) {
                if (elapsed > 300) { s.animState = ANIM_WALK; s.animFrame = 0; s.animTimer = now; }
                else if (s.type == ST_PINKY && elapsed > 150 && s.animFrame == 0) { s.animFrame = 1; }
            } else if (s.animState == ANIM_HIT) {
                if (elapsed > 150) {
                    if (s.hp <= 0) { s.animState = ANIM_DYING; s.animFrame = 0; s.animTimer = now; }
                    else            { s.animState = ANIM_WALK;  s.animFrame = 0; s.animTimer = now; }
                }
            } else if (s.animState == ANIM_DYING) {
                if (s.animFrame == 0 && elapsed > DYING_MS) {
                    s.animFrame = 1; s.animTimer = now;
                } else if (s.animFrame == 1 && elapsed > DYING_MS) {
                    s.animState = ANIM_DEAD;
                }
            }
        } else if (s.type == ST_BARREL) {
            if (s.animState == ANIM_DYING) {
                if (s.animFrame == 0 && elapsed > 150) {
                    s.animFrame = 1; s.animTimer = now;
                } else if (s.animFrame == 1 && elapsed > 150) {
                    s.animState = ANIM_DEAD;
                    s.state = 0;
                }
            }
        }
    }
}

// ============================================================
//  knockbackSprite — Düşmanı (mx,my) kadar iter; hedef hücre
//  dolu olan eksen uygulanmaz. Kalkan/parry geri tepmesi bunsuz
//  düşmanı duvarın içine gömüp kilitleyebiliyordu.
// ============================================================
inline void knockbackSprite(int i, float mx, float my) {
    float tx = sprites[i].x + mx, ty = sprites[i].y + my;
    if ((int)tx >= 0 && (int)tx < MW && MAP[(int)sprites[i].y][(int)tx] == 0) sprites[i].x = tx;
    if ((int)ty >= 0 && (int)ty < MH && MAP[(int)ty][(int)sprites[i].x] == 0) sprites[i].y = ty;
}

// ============================================================
//  updateAllEnemies — Tüm düşman AI, mermi fiziği ve eşya
//  toplama döngüsü. Oyuncuya mesafe + görüş hattı (LOS) ile
//  chase/patrol/ateş kararları. Kalkan etkileşimleri.
// ============================================================
inline void updateAllEnemies(float dt, uint32_t now, bool isParrying) {
    int jy = joyRawY - joyCenterY;
    if (abs(jy) < 300) jy = 0;

    for (int i = 0; i < NUM_SPRITES; i++) {
        if (sprites[i].state <= 0) continue;

        float dx = px - sprites[i].x, dy = py - sprites[i].y;
        float dist = sqrt(dx * dx + dy * dy);
        if (dist < 0.001f) continue;

        bool hasLOS = checkLOS(sprites[i].x, sprites[i].y, dx, dy, dist);

        // --- DÜŞMAN AI (zombi, baron, pinky) ---
        if (isMonsterType(sprites[i].type)) {
            if (sprites[i].animState == ANIM_DEAD || sprites[i].animState == ANIM_DYING) continue;

            // Fark-etme: ALERT_DIST (9.0) silah menzillerinden geniş — düşman,
            // oyuncu ateş edip HIT'e geçirmeden ÖNCE görüşle uyanır ve ses verir.
            // Notalar doom'un başka hiçbir sesiyle çakışmaz (vuruş=G3/B3, ölüm=E3):
            // baron=A3 pes / zombi=C4 orta / pinky=F4 tiz. Global 250ms süzgeç
            // aynı anda uyanan sürüyü tek bip'e indirir.
            if (sprites[i].animState == ANIM_IDLE && dist < ALERT_DIST && dist > 0.6f && hasLOS) {
                sprites[i].animState = ANIM_WALK;
                sprites[i].animTimer = now;
                static uint32_t lastAlertMs = 0;
                if (now - lastAlertMs > 250) {
                    if (sprites[i].type == ST_BARON)      playSound(NOTE_A3, 150);
                    else if (sprites[i].type == ST_PINKY) playSound(NOTE_F4, 80);
                    else                                  playSound(NOTE_C4, 100);
                    lastAlertMs = now;
                }
            }

            if (dist < 6.0f && dist > 0.6f) {
                if (sprites[i].type != ST_PINKY && hasLOS && dist > 2.5f &&
                    random(0, 100) < 2 && now - sprites[i].lastFireTime > 3000) {
                    sprites[i].animState = ANIM_ATTACK;
                    sprites[i].animFrame = 0;
                    sprites[i].animTimer = now;
                    for (int j = 0; j < NUM_SPRITES; j++) {
                        if (sprites[j].state == 0) {
                            initSprite(j, sprites[i].x, sprites[i].y, ST_FIREBALL, 1);
                            sprites[j].dx = (dx / dist);
                            sprites[j].dy = (dy / dist);
                            sprites[i].lastFireTime = now;
                            playSound(NOTE_E4, 60);
                            break;
                        }
                    }
                }

                float spd; float moveX, moveY;
                if (sprites[i].type == ST_PINKY) {
                    spd = 1.2f * dt;
                    float zigzag = sin(now / 250.0f) * 0.3f;
                    moveX = (dx / dist) * spd + (-dy / dist) * zigzag * spd;
                    moveY = (dy / dist) * spd + (dx / dist) * zigzag * spd;
                } else {
                    spd = (sprites[i].type == ST_BARON ? 0.3f : 0.5f) * dt;
                    moveX = (dx / dist) * spd;
                    moveY = (dy / dist) * spd;
                }
                int nx = (int)(sprites[i].x + moveX), ny = (int)(sprites[i].y);
                if (nx >= 0 && nx < MW && ny >= 0 && ny < MH && MAP[ny][nx] == 0) sprites[i].x += moveX;
                nx = (int)(sprites[i].x); ny = (int)(sprites[i].y + moveY);
                if (nx >= 0 && nx < MW && ny >= 0 && ny < MH && MAP[ny][nx] == 0) sprites[i].y += moveY;
            }

            if (hasLOS && lastShieldState && (jy < 0) && dist <= 0.9f && now - shieldSawTime > 400) {
                sprites[i].hp -= 40;
                if (sprites[i].hp <= 0) { sprites[i].animState = ANIM_DYING; playSound(NOTE_E3, 80); }
                else                     { sprites[i].animState = ANIM_HIT;   playSound(NOTE_G3, 100); }
                sprites[i].animFrame = 0; sprites[i].animTimer = now; shieldSawTime = now;
            } else if (hasLOS && dist <= 0.6f && now - lastDamageTime > 1000) {
                sprites[i].animState = ANIM_ATTACK;
                sprites[i].animFrame = 0;
                sprites[i].animTimer = now;
                if (isParrying) {
                    playSound(NOTE_E5, 40);
                    knockbackSprite(i, -dx * 1.5f, -dy * 1.5f);
                    lastDamageTime = now - 200;
                } else if (lastShieldState) {
                    playSound(NOTE_G4, 40);
                    knockbackSprite(i, -dx * 0.5f, -dy * 0.5f);
                    lastDamageTime = now - 600;
                } else {
                    int dmg = (sprites[i].type == ST_BARON) ? 30 : ((sprites[i].type == ST_PINKY) ? 25 : 20);
                    if (armor > 0) {
                        int absorb = min(armor, (dmg + 1) / 2);
                        armor -= absorb;
                        hp -= (dmg - absorb);
                    } else {
                        hp -= dmg;
                    }
                    lastDamageTime = now;
                    playSound(NOTE_G3, 100);
                }
            }
        }
        // --- DÜŞMAN MERMİSİ (ateş topu) ---
        else if (sprites[i].type == ST_FIREBALL) {
            float projSpeed = 4.0f * dt;
            sprites[i].x += sprites[i].dx * projSpeed;
            sprites[i].y += sprites[i].dy * projSpeed;
            float f_dx = px - sprites[i].x, f_dy = py - sprites[i].y, f_dist = sqrt(f_dx * f_dx + f_dy * f_dy);
            int bx = (int)sprites[i].x, by = (int)sprites[i].y;
            if (bx < 0 || bx >= MW || by < 0 || by >= MH || MAP[by][bx] > 0) {
                sprites[i].state = 0;
            } else if (f_dist < 0.5f) {
                if (isParrying) {
                    sprites[i].dx *= -1.5f; sprites[i].dy *= -1.5f;
                    sprites[i].type = ST_PARRYBALL;
                    playSound(NOTE_E5, 40);
                } else if (lastShieldState) {
                    sprites[i].state = 0;
                    playSound(NOTE_G4, 40);
                } else {
                    sprites[i].state = 0;
                    int dmg = 15;
                    if (armor > 0) {
                        int absorb = min(armor, (dmg + 1) / 2);
                        armor -= absorb;
                        hp -= (dmg - absorb);
                    } else {
                        hp -= dmg;
                    }
                    lastDamageTime = now;
                    playSound(NOTE_G3, 100);
                }
            }
        }
        // --- SEKME MERMİSİ (parry ile geri dönen) ---
        else if (sprites[i].type == ST_PARRYBALL) {
            float projSpeed = 4.0f * dt;
            sprites[i].x += sprites[i].dx * projSpeed;
            sprites[i].y += sprites[i].dy * projSpeed;
            int bx = (int)sprites[i].x, by = (int)sprites[i].y;
            if (bx < 0 || bx >= MW || by < 0 || by >= MH || MAP[by][bx] > 0) {
                sprites[i].state = 0;
            } else {
                for (int j = 0; j < NUM_SPRITES; j++) {
                    if (sprites[j].state >= 1 && isMonsterType(sprites[j].type) &&
                        sprites[j].animState != ANIM_DEAD) {
                        float diffX = sprites[j].x - sprites[i].x;
                        float diffY = sprites[j].y - sprites[i].y;
                        if ((diffX * diffX + diffY * diffY) < 0.25f) {
                            sprites[j].hp -= 40;
                            if (sprites[j].hp <= 0) sprites[j].animState = ANIM_DYING;
                            else                     sprites[j].animState = ANIM_HIT;
                            sprites[j].animFrame = 0; sprites[j].animTimer = now;
                            sprites[i].state = 0;
                            playSound(NOTE_E5, 40);
                            break;
                        }
                    }
                }
            }
        }
        // --- EŞYA TOPLAMA ---
        else if (dist < 0.8f) {
            if (isItemType(sprites[i].type)) {
                if (sprites[i].type == ST_AMMO)        { ammo += 15; if (ammo > AMMO_MAX) ammo = AMMO_MAX; }
                else if (sprites[i].type == ST_HEALTH) { hp += 25; if (hp > 100) hp = 100; }
                else if (sprites[i].type == ST_KEY)    { hasKey = true; }
                else if (sprites[i].type == ST_ARMOR)  { armor += 50; if (armor > 100) armor = 100; }
                sprites[i].state = 0;
                playSound(NOTE_C5, 50);
            }
        }
    }
}
