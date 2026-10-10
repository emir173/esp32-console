#pragma once

#include <math.h>
#include "Config.h"
#include "Snake.h"

struct FoodManager {
    int foodX, foodY;
    bool foodActive;
    FoodType foodType;
    int poisonX, poisonY;
    bool poisonActive;
    unsigned long poisonSpawnMs;
    float foodPixelX, foodPixelY;
    unsigned long spawnMs;

    void init() {
        foodActive = false;
        foodX = foodY = -1;
        foodType = FOOD_NORMAL;
        poisonActive = false;
        poisonX = -1;
        poisonY = -1;
        poisonSpawnMs = 0;
        foodPixelX = -1.0f;
        foodPixelY = -1.0f;
        spawnMs = 0;
    }

    static bool isObstacle(int gx, int gy, int level, int arenaInset = 0) {
        if (gx < arenaInset || gx >= COLS - arenaInset ||
            gy < arenaInset || gy >= ROWS - arenaInset) return true;
        if (level >= 2) {
            if (gx >= COLS / 2 - 1 && gx <= COLS / 2 &&
                gy >= ROWS / 2 - 1 && gy <= ROWS / 2) return true;
        }
        return false;
    }

    // Tum hucreleri tara; dolu hucreye dusme ve 1000 deneme siniri yok.
    bool findFreeCell(const Snake& snake, int level, int arenaInset,
                      int avoidX, int avoidY, int& outX, int& outY) const {
        int freeCount = 0;
        for (int gy = arenaInset; gy < ROWS - arenaInset; gy++) {
            for (int gx = arenaInset; gx < COLS - arenaInset; gx++) {
                if (snake.isOnCell(gx, gy) || isObstacle(gx, gy, level, arenaInset) ||
                    (gx == avoidX && gy == avoidY)) continue;
                // Reservoir sampling: her bos hucre esit olasilikla secilir.
                if (random(++freeCount) == 0) {
                    outX = gx;
                    outY = gy;
                }
            }
        }
        return freeCount > 0;
    }

    bool spawnFood(const Snake& snake, int level, int comboCount, int arenaInset) {
        foodActive = false;

        bool placed = false;

        // Stratejik spawn: combo aktifken yilanin 5-8 hücre ilerisinde
        if (comboCount > 1) {
            int aheadDist = random(5, 9);
            int fx = snake.x[0];
            int fy = snake.y[0];
            switch (snake.dir) {
                case DIR_UP:    fy -= aheadDist; break;
                case DIR_RIGHT: fx += aheadDist; break;
                case DIR_DOWN:  fy += aheadDist; break;
                case DIR_LEFT:  fx -= aheadDist; break;
            }
            if (fx >= arenaInset && fx < COLS - arenaInset &&
                fy >= arenaInset && fy < ROWS - arenaInset &&
                !snake.isOnCell(fx, fy) &&
                !isObstacle(fx, fy, level, arenaInset) &&
                !(poisonActive && fx == poisonX && fy == poisonY)) {
                foodX = fx;
                foodY = fy;
                placed = true;
            }
        }

        // Rastgele (combo yoksa veya stratejik yer doluysa)
        if (!placed) {
            placed = findFreeCell(snake, level, arenaInset,
                                 poisonActive ? poisonX : -1,
                                 poisonActive ? poisonY : -1, foodX, foodY);
            // Son bos hucre zehirse ilerlemeyi engellemesin.
            if (!placed && poisonActive) {
                poisonActive = false;
                placed = findFreeCell(snake, level, arenaInset, -1, -1, foodX, foodY);
            }
        }
        if (!placed) {
            foodX = foodY = -1;
            return false;
        }
        foodActive = true;

        // Tur belirle: %5 power-up, geri kalan %65 normal / %35 gold
        int r = random(0, 100);
        if (r < POWERUP_SPAWN_CHANCE) {
            foodType = (random(0, 2) == 0) ? FOOD_GHOST : FOOD_MAGNET;
        } else {
            foodType = (r < 65) ? FOOD_NORMAL : FOOD_GOLD;
        }

        // Poison: combo >3 iken %40 -> %15 (adil kir, RNG hatasi olmasin)
        if (!poisonActive && random(0, 100) < (comboCount > 3 ? 15 : 40)) {
            spawnPoison(snake, level, arenaInset);
        }

        // Pixel pozisyonu grid'e sabitle (magnet kapaliyken)
        foodPixelX = foodX * GRID + GRID / 2.0f;
        foodPixelY = OFFSET_Y + foodY * GRID + GRID / 2.0f;
        
        spawnMs = millis();
        return true;
    }

    // Daralma / yeni engel sonrasi yiyecekler erisilebilir alanda kalsin.
    bool validatePlacement(const Snake& snake, int level, int comboCount, int arenaInset) {
        if (poisonActive && isObstacle(poisonX, poisonY, level, arenaInset))
            poisonActive = false;
        if (!foodActive || isObstacle(foodX, foodY, level, arenaInset))
            return spawnFood(snake, level, comboCount, arenaInset);
        return true;
    }

    void updateMagnetPixels(int headGx, int headGy, bool magnetActive, float dt) {
        if (!foodActive) return;
        if (foodPixelX < 0.0f) {
            foodPixelX = foodX * GRID + GRID / 2.0f;
            foodPixelY = OFFSET_Y + foodY * GRID + GRID / 2.0f;
        }
        float tx = magnetActive ? (headGx * GRID + GRID / 2.0f)
                                : (foodX * GRID + GRID / 2.0f);
        float ty = magnetActive ? (OFFSET_Y + headGy * GRID + GRID / 2.0f)
                                : (OFFSET_Y + foodY * GRID + GRID / 2.0f);
        
        float dx = tx - foodPixelX;
        float dy = ty - foodPixelY;
        float dist = sqrt(dx * dx + dy * dy);
        
        bool canMagnetize = magnetActive && (millis() - spawnMs > 500);
        
        // Eger hedefe ulasmamissa ve (aktif degilken Geri donuyorsa VEYA 50 pikselden yakinsa)
        if (dist > 0 && (!magnetActive || (canMagnetize && dist < 50.0f))) {
            float speed = 100.0f * dt; // 100 pixels per second (sabit hiz)
            
            float moveDist = speed;
            if (moveDist > dist) moveDist = dist; // Hedefi asma
            
            foodPixelX += (dx / dist) * moveDist;
            foodPixelY += (dy / dist) * moveDist;
        }
    }

    void spawnPoison(const Snake& snake, int level, int arenaInset) {
        poisonActive = findFreeCell(snake, level, arenaInset,
                                    foodActive ? foodX : -1,
                                    foodActive ? foodY : -1, poisonX, poisonY);
        if (poisonActive) poisonSpawnMs = millis();
    }

    void updatePoison() {
        if (!poisonActive) return;
        if (millis() - poisonSpawnMs > POISON_LIFETIME) {
            poisonActive = false;
        }
    }

    bool isPoisonVisible() const {
        if (!poisonActive) return false;
        if (millis() - poisonSpawnMs > POISON_BLINK_MS &&
            (millis() / 150) % 2 == 0) return false;
        return true;
    }
};
