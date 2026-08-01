#pragma once
// ============================================================
//  Renderer.h — Tum TFT canvas ve OLED cizim fonksiyonlari
// ============================================================

#include <TFT_eSPI.h>
#include <U8g2lib.h>
#include "Config.h"
#include "Math3D.h"

// ============================================================
//  drawMesh — 3D wireframe mesh cizimi (kup/piramit/elmas)
// ============================================================
inline void drawMesh(TFT_eSprite &canvas,
                     const float verts[][3], int nVerts,
                     const uint8_t edges[][2], int nEdges,
                     Vec3 worldPos, float rot, float scale,
                     uint16_t color, float camYaw, float camPitch) {
    int sx[12], sy[12];
    bool visible[12];

    for (int i = 0; i < nVerts && i < 12; i++) {
        Vec3 v = {verts[i][0], verts[i][1], verts[i][2]};
        v = v3mul(v, scale);
        v = rotateY(v, rot);
        v = v3add(v, worldPos);
        v = rotateY(v, -camYaw);
        v = rotateX(v, -camPitch);
        visible[i] = project(v, sx[i], sy[i]);
    }

    for (int i = 0; i < nEdges; i++) {
        int a = edges[i][0], b = edges[i][1];
        if (visible[a] && visible[b]) {
            canvas.drawLine(sx[a], sy[a], sx[b], sy[b], color);
        }
    }
}

// ============================================================
//  drawStars — Yildiz arka plan (parallax ile)
// ============================================================
inline void drawStars(TFT_eSprite &canvas, Star *stars, int nStars,
                      float camYaw, float camPitch) {
    for (int i = 0; i < nStars; i++) {
        int sx = (stars[i].x - (int)(camYaw * 30)) % SCR_W;
        if (sx < 0) sx += SCR_W;
        int sy = (stars[i].y - (int)(camPitch * 20)) % (SCR_H - HUD_H);
        if (sy < 0) sy += (SCR_H - HUD_H);
        uint16_t c = RGB(stars[i].bright, stars[i].bright, stars[i].bright + 30);
        canvas.drawPixel(sx, sy, c);
    }
}

// ============================================================
//  drawSpaceDust — Uzay tozu parcaciklari (yon hissi)
// ============================================================
inline void drawSpaceDust(TFT_eSprite &canvas, SpaceDust *dust, int nDust,
                          float camYaw, float camPitch) {
    for (int i = 0; i < nDust; i++) {
        Vec3 p = {dust[i].x, dust[i].y, dust[i].z};
        p = rotateY(p, -camYaw);
        p = rotateX(p, -camPitch);
        int sx, sy;
        if (project(p, sx, sy) && sx >= 0 && sx < SCR_W && sy >= 0 && sy < SCR_H - HUD_H) {
            uint8_t b = (uint8_t)(120.0f / dust[i].z * 5.0f);
            b = constrain(b, 10, 80);
            canvas.drawPixel(sx, sy, RGB(b / 2, b / 2, b));
        }
    }
}

// ============================================================
//  drawObjects — Tum aktif dusmanlari canvas'a ciz
// ============================================================
inline void drawObjects(TFT_eSprite &canvas, Object3D *objects, int nObjects,
                        float camYaw, float camPitch) {
    for (int i = 0; i < nObjects; i++) {
        if (!objects[i].active) continue;
        Object3D &o = objects[i];

        uint16_t color;
        if (o.hitTimer > 0) {
            color = COL_HIT;
        } else {
            switch (o.type) {
                case OBJ_CUBE:    color = COL_WIRE_CYAN;  break;
                case OBJ_PYRAMID: color = COL_WIRE_GREEN; break;
                case OBJ_DIAMOND: color = COL_WIRE_GOLD;  break;
            }
        }

        switch (o.type) {
            case OBJ_CUBE:
                drawMesh(canvas, cubeVerts, CUBE_VERTS, cubeEdges, CUBE_EDGES,
                         o.pos, o.rotAngle, o.scale, color, camYaw, camPitch);
                break;
            case OBJ_PYRAMID:
                drawMesh(canvas, pyrVerts, PYR_VERTS, pyrEdges, PYR_EDGES,
                         o.pos, o.rotAngle, o.scale, color, camYaw, camPitch);
                break;
            case OBJ_DIAMOND:
                drawMesh(canvas, diamVerts, DIAM_VERTS, diamEdges, DIAM_EDGES,
                         o.pos, o.rotAngle, o.scale, color, camYaw, camPitch);
                break;
        }

        // HP gostergesi (cismin ustunde)
        Vec3 hpPos = o.pos;
        hpPos.y -= o.scale * 2.0f;
        Vec3 hpCam = rotateY(hpPos, -camYaw);
        hpCam = rotateX(hpCam, -camPitch);
        int hsx, hsy;
        if (project(hpCam, hsx, hsy)) {
            for (int h = 0; h < o.hp; h++) {
                canvas.fillRect(hsx - o.hp * 2 + h * 4, hsy, 3, 2, COL_WIRE_RED);
            }
        }
    }
}

// ============================================================
//  drawBullets — Tum aktif mermileri ciz
// ============================================================
inline void drawBullets(TFT_eSprite &canvas, Bullet3D *bullets, int nBullets,
                        float camYaw, float camPitch) {
    for (int i = 0; i < nBullets; i++) {
        if (!bullets[i].active) continue;
        Vec3 p = rotateY(bullets[i].pos, -camYaw);
        p = rotateX(p, -camPitch);
        int sx, sy;
        if (project(p, sx, sy)) {
            canvas.fillCircle(sx, sy, 2, COL_BULLET);
        }
    }
}

// ============================================================
//  drawBooms — Patlamalari ciz (genisleyen halka)
// ============================================================
inline void drawBooms(TFT_eSprite &canvas, Boom *booms, int nBooms,
                      float camYaw, float camPitch) {
    for (int i = 0; i < nBooms; i++) {
        if (!booms[i].active) continue;
        Vec3 p = rotateY(booms[i].pos, -camYaw);
        p = rotateX(p, -camPitch);
        int sx, sy;
        if (project(p, sx, sy)) {
            int r = 3 + (int)(booms[i].frame * 12.5f);
            uint16_t c = (booms[i].frame < 0.24f) ? COL_BOOM :
                         (booms[i].frame < 0.4f)  ? COL_WIRE_RED
                                                  : RGB(100, 40, 20);
            canvas.drawCircle(sx, sy, r, c);
            if (booms[i].frame < 0.32f) canvas.drawCircle(sx, sy, r / 2, COL_BOOM);
        }
    }
}

// ============================================================
//  drawCrosshair — Ekran ortasina nisan arti isareti
// ============================================================
inline void drawCrosshair(TFT_eSprite &canvas) {
    int cx = SCR_W / 2;
    int cy = (SCR_H - HUD_H) / 2;
    canvas.drawFastHLine(cx - 6, cy, 4, COL_CROSS);
    canvas.drawFastHLine(cx + 3, cy, 4, COL_CROSS);
    canvas.drawFastVLine(cx, cy - 6, 4, COL_CROSS);
    canvas.drawFastVLine(cx, cy + 3, 4, COL_CROSS);
    canvas.drawPixel(cx, cy, COL_CROSS);
}

// ============================================================
//  drawHUD — Alt seride skor/can/dalga/kill bilgisi
// ============================================================
inline void drawHUD(TFT_eSprite &canvas, int score, int hp, int wave, int kills) {
    int hy = SCR_H - HUD_H;
    canvas.fillRect(0, hy, SCR_W, HUD_H, COL_HUD_BG);
    canvas.drawFastHLine(0, hy, SCR_W, RGB(40, 40, 80));

    canvas.setTextSize(1);
    canvas.setTextColor(COL_HUD_TXT);

    canvas.setCursor(2, hy + 2);
    canvas.printf("SCR:%d", score);

    for (int i = 0; i < hp; i++) {
        canvas.fillRect(60 + i * 8, hy + 4, 5, 5, COL_WIRE_RED);
    }

    canvas.setTextColor(RGB(100, 255, 100));
    canvas.setCursor(110, hy + 2);
    canvas.printf("W:%d", wave + 1);

    canvas.setTextColor(RGB(180, 180, 180));
    canvas.setCursor(136, hy + 2);
    canvas.printf("K:%d", kills);
}

// ============================================================
//  drawTitle — Baslik ekrani (demo kup + baslik + menu + rekor)
// ============================================================
inline void drawTitle(TFT_eSprite &canvas, Star *stars, int nStars,
                      float camYaw, float camPitch, int highScore,
                      unsigned long nowMs) {
    canvas.fillSprite(COL_BG);
    drawStars(canvas, stars, nStars, camYaw, camPitch);

    float demoRot = nowMs * 0.00125f;
    Vec3 demoPos = {0, -0.4f, 8.5f};
    drawMesh(canvas, cubeVerts, CUBE_VERTS, cubeEdges, CUBE_EDGES,
             demoPos, demoRot, 1.7f, COL_WIRE_CYAN, 0, 0);

    canvas.setTextSize(2);
    canvas.setTextColor(RGB(20, 80, 120));
    canvas.setCursor(39, 13);
    canvas.print("WIRE 3D");
    canvas.setTextColor(COL_WIRE_CYAN);
    canvas.setCursor(38, 12);
    canvas.print("WIRE 3D");

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(53, 95);
    canvas.print("[A] Start");

    canvas.setCursor(47, 105);
    canvas.print("[B] OS Menu");

    canvas.setCursor(50, 115);
    canvas.print("[JOY] Move");

    // Rekor
    canvas.setTextColor(TFT_YELLOW);
    char rekorBuf[20];
    snprintf(rekorBuf, sizeof(rekorBuf), "Best: %d", highScore);
    int txtW = strlen(rekorBuf) * 6;
    canvas.setCursor((160 - txtW) / 2, 140);
    canvas.print(rekorBuf);
}

// ============================================================
//  drawGameOver — Game over screen
// ============================================================
inline void drawGameOver(TFT_eSprite &canvas, Star *stars, int nStars,
                         float camYaw, float camPitch,
                         int score, int kills, int wave, int highScore) {
    canvas.fillSprite(COL_BG);
    drawStars(canvas, stars, nStars, camYaw, camPitch);

    // Ortak OS game-over: 4 satirlik tablo (: hizali). Yildiz alani arka planda.
    char sb[12], kb[12], wb[12], hb[12];
    snprintf(sb, sizeof(sb), "%d", score);
    snprintf(kb, sizeof(kb), "%d", kills);
    snprintf(wb, sizeof(wb), "%d", wave + 1);
    snprintf(hb, sizeof(hb), "%d", highScore);
    OsStat rows[4] = {
        { "Score", sb, COL_WHITE, COL_WHITE     },
        { "Kills", kb, COL_WHITE, COL_WIRE_CYAN },
        { "Wave",  wb, COL_WHITE, COL_WIRE_GREEN},
        { "Best",  hb, COL_WIRE_GOLD, COL_WIRE_GOLD },
    };
    osDrawGameOver(canvas, false, rows, 4);
}

// ============================================================
//  drawRadar — OLED radar ekrani (dusman konumlari + tarama)
// ============================================================
inline void drawRadar(U8G2 &oled, Object3D *objects, int nObjects,
                      float camYaw, unsigned long nowMs) {
    oled.clearBuffer();

    const int cx = 64, cy = 32, radius = 30;
    oled.drawCircle(cx, cy, radius);
    oled.drawCircle(cx, cy, radius / 2);
    oled.drawHLine(cx - radius, cy, radius * 2);
    oled.drawVLine(cx, cy - radius, radius * 2);

    oled.drawTriangle(cx, cy - 4, cx - 4, cy + 3, cx + 4, cy + 3);

    for (int i = 0; i < nObjects; i++) {
        if (!objects[i].active) continue;
        Vec3 rel = rotateY(objects[i].pos, -camYaw);
        float dist = v3len(objects[i].pos);
        float rScale = (dist / 50.0f) * radius;
        if (rScale > radius) rScale = radius;

        Vec3 dirN = v3norm(rel);
        int rx = cx + (int)(dirN.x * rScale);
        int ry = cy - (int)(dirN.z * rScale);
        rx = constrain(rx, cx - radius + 2, cx + radius - 2);
        ry = constrain(ry, cy - radius + 2, cy + radius - 2);

        int boxSize = 1;
        if (dist < 20.0f) boxSize = 5;
        else if (dist < 35.0f) boxSize = 3;
        oled.drawBox(rx - boxSize / 2, ry - boxSize / 2, boxSize, boxSize);
    }

    float sweepAngle = (nowMs % 3000) / 3000.0f * PI * 2.0f;
    int sx = cx + (int)(sinf(sweepAngle) * radius);
    int sy = cy - (int)(cosf(sweepAngle) * radius);
    oled.drawLine(cx, cy, sx, sy);

    oled.sendBuffer();
}
