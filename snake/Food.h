#pragma once

#include "Config.h"
#include "Snake.h"

struct FoodManager {
    int foodX, foodY;
    FoodType foodType;
    int poisonX, poisonY;
    bool poisonActive;
    unsigned long poisonSpawnMs;
    float foodPixelX, foodPixelY;
    unsigned long spawnMs;

    void init() {
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

    void spawnFood(const Snake& snake, int level, int comboCount, int arenaInset) {
        if (snake.len >= COLS * ROWS - 1) return;

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
            int tries = 0;
            do {
                foodX = random(arenaInset, COLS - arenaInset);
                foodY = random(arenaInset, ROWS - arenaInset);
                tries++;
            } while (tries < 1000 &&
                     (snake.isOnCell(foodX, foodY) ||
                      isObstacle(foodX, foodY, level, arenaInset) ||
                      (poisonActive && foodX == poisonX && foodY == poisonY)));
        }

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
    }

    void updateMagnetPixels(int headGx, int headGy, bool magnetActive, float dt) {
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
        int tries = 0;
        do {
            poisonX = random(arenaInset, COLS - arenaInset);
            poisonY = random(arenaInset, ROWS - arenaInset);
            tries++;
        } while (tries < 1000 &&
                 (snake.isOnCell(poisonX, poisonY) ||
                  isObstacle(poisonX, poisonY, level, arenaInset) ||
                  (poisonX == foodX && poisonY == foodY)));

        poisonActive = true;
        poisonSpawnMs = millis();
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
