#pragma once
// ============================================================
//  Renderer.h — Rhythm Beats TFT cizimleri
//  Menu, oyun sahnesi (seritler, notalar, hit line, HUD),
//  geri sayim overlay'i. Tum cizimler TFT_eSprite canvas'a.
// ============================================================

#include "Config.h"
#include "Songs.h"
#include "NoteManager.h"

// ------------------------------------------------------------
//  Arka plan + serit ayirici cizgiler
//  hype: combo >= 20 iken daha canli fon
// ------------------------------------------------------------
inline void drawBackground(TFT_eSprite &canvas, bool hype) {
    canvas.fillSprite(hype ? COL_BG_HYPE : COL_BG);
    for (int i = 1; i < LANE_COUNT; i++) {
        canvas.drawFastVLine(i * LANE_W, 12, HIT_Y - 10, COL_LANE_LINE);
    }
}

// ------------------------------------------------------------
//  Ust serit basliklari (y=0..10) — serit harfleri, kendi renginde
// ------------------------------------------------------------
inline void drawLaneHeaders(TFT_eSprite &canvas) {
    canvas.setTextSize(1);
    for (int i = 0; i < LANE_COUNT; i++) {
        canvas.setTextColor(COL_LANES[i]);
        canvas.setCursor(LANE_CENTER[i] - 3, 2);
        canvas.print(LANE_LABELS[i]);
    }
}

// ------------------------------------------------------------
//  Vurus cizgisi + serit isaretleri
//  songTime/beatMs: beat nabiz efekti, flash: PERFECT beyaz flash
//  pressedMask: basili butonlarin seritleri (bit 0-3) dolu daire
// ------------------------------------------------------------
inline void drawHitLine(TFT_eSprite &canvas, uint32_t songTime, uint16_t beatMs,
                        bool flash, uint8_t pressedMask) {
    bool pulse = (songTime % beatMs) < 90;   // her beat'te kisa parlama

    if (flash) {
        // PERFECT ani: cizginin tamami parlak yanar
        canvas.fillRect(0, HIT_Y - 3, SCR_W, 7, COL_HIT_LINE);
    } else {
        if (pulse) {
            canvas.drawFastHLine(0, HIT_Y - 3, SCR_W, COL_HIT_GLOW);
            canvas.drawFastHLine(0, HIT_Y + 3, SCR_W, COL_HIT_GLOW);
        }
        canvas.fillRect(0, HIT_Y - 1, SCR_W, 3, pulse ? COL_HIT_LINE : COL_HIT_GLOW);
        canvas.drawFastHLine(0, HIT_Y, SCR_W, COL_HIT_LINE);
    }

    // Serit isaretleri: bos halka, buton basiliyken dolu daire
    for (int i = 0; i < LANE_COUNT; i++) {
        if (pressedMask & (1 << i)) {
            canvas.fillCircle(LANE_CENTER[i], HIT_Y, 5, COL_LANES[i]);
        } else {
            canvas.drawCircle(LANE_CENTER[i], HIT_Y, 4, COL_LANES[i]);
        }
    }
}

// ------------------------------------------------------------
//  Dusen notalar — yuvarlak dikdortgen + 1px parilti cercevesi
// ------------------------------------------------------------
inline void drawNotes(TFT_eSprite &canvas, const Note *pool) {
    for (int i = 0; i < MAX_NOTES; i++) {
        const Note &n = pool[i];
        if (!n.active) continue;
        int y = (int)n.y;
        if (y < -NOTE_H || y > SCR_H + NOTE_H) continue;
        int x = LANE_CENTER[n.lane] - NOTE_W / 2;
        canvas.drawRoundRect(x - 1, y - NOTE_H / 2 - 1, NOTE_W + 2, NOTE_H + 2, 3,
                             COL_LANES_GLOW[n.lane]);
        canvas.fillRoundRect(x, y - NOTE_H / 2, NOTE_W, NOTE_H, 2, COL_LANES[n.lane]);
    }
}

// ------------------------------------------------------------
//  Alt bar (y=112..127) — serit etiketleri, basiliyken parlak kutu
// ------------------------------------------------------------
inline void drawBottomBar(TFT_eSprite &canvas, uint8_t pressedMask) {
    canvas.fillRect(0, BOTTOM_BAR_Y, SCR_W, SCR_H - BOTTOM_BAR_Y, COL_LANE_LINE);
    canvas.setTextSize(1);
    for (int i = 0; i < LANE_COUNT; i++) {
        int cx = LANE_CENTER[i];
        if (pressedMask & (1 << i)) {
            canvas.fillRoundRect(cx - 9, BOTTOM_BAR_Y + 2, 18, 12, 2, COL_LANES[i]);
            canvas.setTextColor(COL_BG);
        } else {
            canvas.drawRoundRect(cx - 9, BOTTOM_BAR_Y + 2, 18, 12, 2, COL_LANES[i]);
            canvas.setTextColor(COL_LANES[i]);
        }
        canvas.setCursor(cx - 3, BOTTOM_BAR_Y + 4);
        canvas.print(LANE_LABELS[i]);
    }
}

// ------------------------------------------------------------
//  Saglik bari — sol kenar dikey (2 px), renk esiklere gore
//  < 15 iken kirmizi yanip soner
// ------------------------------------------------------------
inline void drawHealthBar(TFT_eSprite &canvas, int health, unsigned long now) {
    const int barH = HEALTH_BAR_BOT - HEALTH_BAR_TOP;
    canvas.drawFastVLine(HEALTH_BAR_X + HEALTH_BAR_W, HEALTH_BAR_TOP, barH, COL_LANE_LINE);

    uint16_t col;
    if (health > 60)      col = COL_HP_HIGH;
    else if (health >= 30) col = COL_HP_MID;
    else                   col = COL_HP_LOW;
    if (health < 15 && (now / 200) % 2 == 0) return;   // kritik: yanip sonme

    int fillH = health * barH / HEALTH_MAX;
    if (fillH <= 0) return;
    canvas.fillRect(HEALTH_BAR_X, HEALTH_BAR_BOT - fillH, HEALTH_BAR_W, fillH, col);
}

// ------------------------------------------------------------
//  Combo sayaci — >= 5 iken buyuk font, >= 10 iken renk animasyonu
// ------------------------------------------------------------
inline void drawCombo(TFT_eSprite &canvas, int combo, unsigned long now) {
    if (combo < 5) return;
    char buf[12];
    snprintf(buf, sizeof(buf), "%dx", combo);
    canvas.setTextSize(2);
    uint16_t col = TFT_WHITE;
    if (combo >= 10) {
        // Yanip sonen neon renk dongusu
        static const uint16_t CYCLE[3] = { COL_PERFECT, COL_LANES[1], COL_LANES[3] };
        col = CYCLE[(now / 150) % 3];
    }
    canvas.setTextColor(col);
    canvas.setCursor(80 - canvas.textWidth(buf) / 2, 16);
    canvas.print(buf);

    canvas.setTextSize(1);
    canvas.setTextColor(COL_GRAY_TXT);
    char mbuf[8];
    snprintf(mbuf, sizeof(mbuf), "x%d", comboMultiplier(combo));
    canvas.setCursor(80 - canvas.textWidth(mbuf) / 2, 34);
    canvas.print(mbuf);
}

// ------------------------------------------------------------
//  Yargilama yazisi (PERFECT/GOOD/OK/MISS) — hit line ustunde 400 ms
// ------------------------------------------------------------
inline void drawJudgement(TFT_eSprite &canvas, const NoteManager &nm, unsigned long now) {
    if (nm.lastGrade == GRADE_NONE) return;
    if (now - nm.lastGradeMs > JUDGE_SHOW_MS) return;

    const char *txt;
    uint16_t col;
    switch (nm.lastGrade) {
        case GRADE_PERFECT: txt = "PERFECT"; col = COL_PERFECT; break;
        case GRADE_GOOD:    txt = "GOOD";    col = COL_GOOD;    break;
        case GRADE_OK:      txt = "OK";      col = COL_OK;      break;
        default:            txt = "MISS";    col = COL_MISS;    break;
    }

    bool big = (nm.lastGrade == GRADE_PERFECT);
    canvas.setTextSize(big ? 2 : 1);
    int w = canvas.textWidth(txt);
    // Vurulan seridin merkezine hizala, ekran disina tasma
    int x = LANE_CENTER[nm.lastGradeLane] - w / 2;
    if (x < 2) x = 2;
    if (x + w > SCR_W - 2) x = SCR_W - 2 - w;
    canvas.setTextColor(col);
    canvas.setCursor(x, big ? 82 : 90);
    canvas.print(txt);
}

// ------------------------------------------------------------
//  Oyun sahnesi — PLAYING/PAUSE/GAMEOVER arkaplani tek cagri
// ------------------------------------------------------------
inline void drawPlayScene(TFT_eSprite &canvas, const NoteManager &nm, uint32_t songTime,
                          uint8_t pressedMask, bool flash, unsigned long now) {
    drawBackground(canvas, nm.combo >= 20);
    drawLaneHeaders(canvas);
    drawNotes(canvas, nm.pool);
    drawHitLine(canvas, songTime, nm.song->beatMs, flash, pressedMask);
    drawBottomBar(canvas, pressedMask);
    drawHealthBar(canvas, nm.health, now);
    drawCombo(canvas, nm.combo, now);
    drawJudgement(canvas, nm, now);
}

// ------------------------------------------------------------
//  Geri sayim overlay'i — buyuk "3" / "2" / "1" / "GO!"
//  step: 0..3
// ------------------------------------------------------------
inline void drawCountdown(TFT_eSprite &canvas, uint8_t step) {
    static const char *TEXTS[4] = { "3", "2", "1", "GO!" };
    const char *t = TEXTS[step > 3 ? 3 : step];
    canvas.setTextSize(3);
    canvas.setTextColor(step >= 3 ? COL_GOOD : TFT_WHITE);
    canvas.setCursor(80 - canvas.textWidth(t) / 2, 50);
    canvas.print(t);
}

// ------------------------------------------------------------
//  Sarki secim menusu
// ------------------------------------------------------------
inline void drawMenu(TFT_eSprite &canvas, int sel, long best, unsigned long now) {
    canvas.fillSprite(COL_BG);

    // Baslik + dekoratif nota kutulari
    canvas.setTextSize(2);
    canvas.setTextColor(COL_LANES[1]);
    canvas.setCursor(80 - canvas.textWidth("RHYTHM") / 2, 6);
    canvas.print("RHYTHM");
    canvas.setTextColor(COL_LANES[3]);
    canvas.setCursor(80 - canvas.textWidth("BEATS") / 2, 24);
    canvas.print("BEATS");
    // Basligin iki yaninda "dusen nota" susleri (hafif salinim)
    int wob = (int)((now / 300) % 2);
    for (int i = 0; i < LANE_COUNT; i++) {
        int x = (i < 2) ? 10 + i * 14 : 122 + (i - 2) * 14;
        canvas.fillRoundRect(x, 12 + ((i + wob) % 2) * 8, 10, 6, 2, COL_LANES[i]);
    }

    // Sarki listesi — secili satir dolgulu, zorluk yildizlari
    canvas.setTextSize(1);
    for (int i = 0; i < SONG_COUNT; i++) {
        int y = 46 + i * 16;
        if (i == sel) {
            canvas.fillRoundRect(14, y - 3, 132, 14, 3, COL_LANE_LINE);
            canvas.drawRoundRect(14, y - 3, 132, 14, 3, COL_LANES[i]);
            canvas.setTextColor(TFT_WHITE);
        } else {
            canvas.setTextColor(COL_GRAY_TXT);
        }
        canvas.setCursor(20, y);
        canvas.print(SONGS[i].name);
        // Yildizlar: dolu = sarkinin temasi, bos = soluk
        for (int s = 0; s < 3; s++) {
            uint16_t c = (s < SONGS[i].stars) ? COL_LANES[3] : COL_LANE_LINE;
            canvas.fillCircle(120 + s * 8, y + 3, 2, c);
        }
    }

    // Rekor + kontroller
    canvas.setTextColor(COL_PERFECT);
    char bbuf[24];
    snprintf(bbuf, sizeof(bbuf), "Best: %ld", best);
    canvas.setCursor(80 - canvas.textWidth(bbuf) / 2, 98);
    canvas.print(bbuf);

    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(80 - canvas.textWidth("[A] Play   [B] OS Menu") / 2, 114);
    canvas.print("[A] Play   [B] OS Menu");
}
