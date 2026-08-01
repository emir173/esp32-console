#pragma once
// ============================================================
//  E-OS ARKANOID — Bricks.h
//  Tugla izgarasi: Farkli renk/puan seviyeleri,
//  AABB circle-dikdortgen carpisma tespiti
// ============================================================
#include <math.h>
#include "Config.h"

class Bricks {
    bool alive[BRICK_ROWS][BRICK_COLS];
    int count;

public:
    void init() {
        for (int r = 0; r < BRICK_ROWS; r++)
            for (int c = 0; c < BRICK_COLS; c++)
                alive[r][c] = true;
        count = BRICK_ROWS * BRICK_COLS;
    }

    int getBrickX(int col) const {
        return BRICK_START_X + col * (BRICK_W + BRICK_GAP_X);
    }

    int getBrickY(int row) const {
        return BRICK_START_Y + row * (BRICK_H + BRICK_GAP_Y);
    }

    // AABB daire-dikdortgen carpisma testi
    // bx, by: topun guncel pozisyonu
    // prevBX, prevBY: topun onceki frame pozisyonu (carpisma yonu hesabi icin)
    // Cikti: outRow, outCol = carpilan tugla indeksi
    //        outBounceX, outBounceY = hangi eksen(ler)de sekme yapmali
    bool checkHit(float bx, float by, float prevBX, float prevBY,
                  float BALL_RADIUS,
                  int& outRow, int& outCol,
                  bool& outBounceX, bool& outBounceY) const {

        for (int r = 0; r < BRICK_ROWS; r++) {
            for (int c = 0; c < BRICK_COLS; c++) {
                if (!alive[r][c]) continue;

                int bwx = getBrickX(c);
                int bwy = getBrickY(r);

                // En yakin noktayi bul (dikdortgene sinirla)
                float closestX = fmaxf((float)bwx, fminf(bx, (float)(bwx + BRICK_W)));
                float closestY = fmaxf((float)bwy, fminf(by, (float)(bwy + BRICK_H)));

                float dx = bx - closestX;
                float dy = by - closestY;

                if (dx * dx + dy * dy < BALL_RADIUS * BALL_RADIUS) {
                    outRow = r;
                    outCol = c;

                    // Onceki pozisyona bakarak hangi yonden geldigini bul
                    bool wasOutV = (prevBY + BALL_RADIUS <= (float)bwy ||
                                    prevBY - BALL_RADIUS >= (float)(bwy + BRICK_H));
                    bool wasOutH = (prevBX + BALL_RADIUS <= (float)bwx ||
                                    prevBX - BALL_RADIUS >= (float)(bwx + BRICK_W));

                    if (wasOutV && !wasOutH) {
                        outBounceY = true;
                        outBounceX = false;
                    } else if (wasOutH && !wasOutV) {
                        outBounceX = true;
                        outBounceY = false;
                    } else {
                        // Kose carpismasi — her iki eksende sek
                        outBounceX = true;
                        outBounceY = true;
                    }
                    return true;
                }
            }
        }
        return false;
    }

    int breakAt(int r, int c) {
        alive[r][c] = false;
        count--;
        return BRICK_POINTS[r];
    }

    bool isEmpty()  const { return count == 0; }
    int  getCount() const { return count; }
    bool isAlive(int r, int c) const { return alive[r][c]; }

    void draw(TFT_eSprite& canvas) const {
        for (int r = 0; r < BRICK_ROWS; r++)
            for (int c = 0; c < BRICK_COLS; c++)
                if (alive[r][c])
                    drawOne(canvas, r, c);
    }

private:
    void drawOne(TFT_eSprite& canvas, int r, int c) const {
        int bx = getBrickX(c);
        int by = getBrickY(r);
        uint16_t color = BRICK_COLORS[r];
        uint16_t hl    = BRICK_HL[r];

        // Ana govde
        canvas.fillRect(bx, by, BRICK_W, BRICK_H, color);
        // Ust + sol kenar vurgusu (3D efekti)
        canvas.drawFastHLine(bx, by, BRICK_W - 1, hl);
        canvas.drawFastVLine(bx, by, BRICK_H - 1, hl);
        // Alt + sag kenar golgesi
        canvas.drawFastHLine(bx + 1, by + BRICK_H - 1, BRICK_W - 1, TFT_BLACK);
        canvas.drawFastVLine(bx + BRICK_W - 1, by + 1, BRICK_H - 1, TFT_BLACK);
    }
};
