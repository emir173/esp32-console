#pragma once
// ============================================================
//  NoteManager.h — Rhythm Beats oyun mantigi
//  Nota spawn/dusme, vurus eslestirme (en yakin nota), otomatik
//  miss, puan + combo + saglik takibi ve yargilama geri bildirimi.
//  Zamanin sahibi .ino'dur (songTime ms); burasi sadece isler.
// ============================================================

#include "Config.h"
#include "Songs.h"

struct NoteManager {
    // ---- Aktif sarki ----
    const SongInfo *song = nullptr;
    uint16_t nextIdx = 0;          // chart'ta siradaki spawn edilecek nota
    float    travelMs = 0;         // spawn -> hit line yolculuk suresi (ms)
    float    okWindowMs = 0;       // OK penceresi ms karsiligi

    // ---- Nota havuzu ----
    Note pool[MAX_NOTES];

    // ---- Skor / combo / saglik ----
    long score = 0;
    int  combo = 0;
    int  maxCombo = 0;
    int  health = HEALTH_START;
    int  cntPerfect = 0, cntGood = 0, cntOk = 0, cntMiss = 0;

    // ---- Yargilama geri bildirimi (renderer okur) ----
    HitGrade      lastGrade = GRADE_NONE;
    uint8_t       lastGradeLane = 0;
    unsigned long lastGradeMs = 0;   // millis() — yazi 400 ms gorunur

    // ---- Bu frame'de olusan otomatik miss sayisi (.ino ses/shake icin) ----
    uint8_t autoMissThisFrame = 0;

    // ------------------------------------------------------------
    //  Yeni sarki baslat — tum sayaclari sifirla
    // ------------------------------------------------------------
    void begin(const SongInfo *s) {
        song = s;
        nextIdx = 0;
        travelMs   = ((float)HIT_Y - SPAWN_Y) / s->speed * 1000.0f;
        okWindowMs = WIN_OK_PX / s->speed * 1000.0f;
        for (int i = 0; i < MAX_NOTES; i++) pool[i].active = false;
        score = 0; combo = 0; maxCombo = 0;
        health = HEALTH_START;
        cntPerfect = cntGood = cntOk = cntMiss = 0;
        lastGrade = GRADE_NONE;
        autoMissThisFrame = 0;
    }

    // ------------------------------------------------------------
    //  Frame guncellemesi: spawn + Y hesabi + otomatik miss
    //  songTime: sarki basindan ms (pause'da ilerlemez)
    // ------------------------------------------------------------
    void update(uint32_t songTime, unsigned long nowMs) {
        autoMissThisFrame = 0;

        // Spawn: nota, hit line'a "travelMs" kala ekrana girer
        while (nextIdx < song->noteCount &&
               (float)song->notes[nextIdx].timeMs - (float)songTime <= travelMs) {
            spawnNote(song->notes[nextIdx]);
            nextIdx++;
        }

        // Y pozisyonu zamandan hesaplanir (birikimli hata olmaz) + otomatik miss
        for (int i = 0; i < MAX_NOTES; i++) {
            Note &n = pool[i];
            if (!n.active) continue;
            float dtMs = (float)n.targetTime - (float)songTime;   // + = henuz ustte
            n.y = (float)HIT_Y - dtMs * song->speed / 1000.0f;

            if (!n.hit && dtMs < -okWindowMs) {
                // Pencereyi basilmadan gecti -> MISS
                n.active = false;
                applyMiss(n.lane, nowMs);
                autoMissThisFrame++;
            } else if (n.y > (float)SCR_H + NOTE_H) {
                n.active = false;   // ekran disi temizlik
            }
        }
    }

    // ------------------------------------------------------------
    //  Buton basildi: o seritteki en yakin notayi bul, derecelendir.
    //  Donus: verilen derece (GRADE_NONE = seritte nota yok, yoksay)
    // ------------------------------------------------------------
    HitGrade press(uint8_t lane, uint32_t songTime, unsigned long nowMs) {
        int   bestIdx = -1;
        float bestAbs = 1e9f;
        for (int i = 0; i < MAX_NOTES; i++) {
            Note &n = pool[i];
            if (!n.active || n.hit || n.lane != lane) continue;
            float d = fabsf((float)n.targetTime - (float)songTime);
            if (d < bestAbs) { bestAbs = d; bestIdx = i; }
        }
        if (bestIdx < 0) return GRADE_NONE;   // seritte hic nota yok — bos basis

        // ms cinsinden mesafeyi px'e cevir, pencere derecesini bul
        float distPx = bestAbs * song->speed / 1000.0f;
        if (distPx > WIN_OK_PX) {
            // En yakin nota bile pencere disinda -> hatali basis = MISS
            applyMiss(lane, nowMs);
            return GRADE_MISS;
        }

        Note &n = pool[bestIdx];
        n.hit = true;
        n.active = false;

        HitGrade g;
        int mult = comboMultiplier(combo + 1);   // bu vurus dahil combo
        if (distPx <= WIN_PERFECT_PX) {
            g = GRADE_PERFECT; cntPerfect++;
            score += (long)SCORE_PERFECT * mult;
            health += HP_PERFECT;
        } else if (distPx <= WIN_GOOD_PX) {
            g = GRADE_GOOD; cntGood++;
            score += (long)SCORE_GOOD * mult;
            health += HP_GOOD;
        } else {
            g = GRADE_OK; cntOk++;
            score += SCORE_OK;   // OK carpansiz
        }
        if (health > HEALTH_MAX) health = HEALTH_MAX;

        combo++;
        if (combo > maxCombo) maxCombo = combo;

        lastGrade = g;
        lastGradeLane = lane;
        lastGradeMs = nowMs;
        return g;
    }

    // ------------------------------------------------------------
    //  MISS uygula (otomatik veya hatali basis)
    // ------------------------------------------------------------
    void applyMiss(uint8_t lane, unsigned long nowMs) {
        cntMiss++;
        combo = 0;
        health += HP_MISS;
        if (health < 0) health = 0;
        lastGrade = GRADE_MISS;
        lastGradeLane = lane;
        lastGradeMs = nowMs;
    }

    // ------------------------------------------------------------
    //  Sarki bitti mi? (tum notalar islendi + son notadan 2 sn sonra)
    // ------------------------------------------------------------
    bool songFinished(uint32_t songTime) const {
        if (nextIdx < song->noteCount) return false;
        for (int i = 0; i < MAX_NOTES; i++) {
            if (pool[i].active) return false;
        }
        uint32_t lastTime = song->notes[song->noteCount - 1].timeMs;
        return songTime > lastTime + SONG_END_PAD_MS;
    }

    bool dead() const { return health <= 0; }

private:
    // Havuzdan bos yuva bul, notayi yerlestir
    void spawnNote(const SongNote &sn) {
        for (int i = 0; i < MAX_NOTES; i++) {
            if (!pool[i].active) {
                pool[i].lane = sn.lane;
                pool[i].y = SPAWN_Y;
                pool[i].targetTime = sn.timeMs;
                pool[i].active = true;
                pool[i].hit = false;
                return;
            }
        }
        // Havuz dolu (olmamali — MAX_NOTES yogun sarkiya gore secildi)
    }
};
