#pragma once
// ============================================================
//  Math3D.h — 3D matematik, vektor islemleri, projeksiyon,
//             veri yapilari (Object3D, Bullet3D, Boom, vb.)
// ============================================================

#include "Config.h"

// ============ Vec3 — 3B vektor ============
struct Vec3 {
    float x, y, z;
};

// v3add — Iki vektorun toplamini dondurur (a + b)
inline Vec3 v3add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

// v3sub — Iki vektorun farkini dondurur (a - b)
inline Vec3 v3sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

// v3mul — Vektoru skaler ile carp (a * s)
inline Vec3 v3mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }

// v3dot — Skaler carpim
inline float v3dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// v3len — Vektorun uzunlugu (norm)
inline float v3len(Vec3 a) { return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); }

// v3norm — Birim vektor (yon) dondurur
inline Vec3 v3norm(Vec3 a) {
    float l = v3len(a);
    if (l < 0.001f) return {0, 0, 1};
    return v3mul(a, 1.0f / l);
}

// rotateY — Y ekseni etrafinda donus (yaw)
inline Vec3 rotateY(Vec3 v, float angle) {
    float c = cosf(angle), s = sinf(angle);
    return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

// rotateX — X ekseni etrafinda donus (pitch)
inline Vec3 rotateX(Vec3 v, float angle) {
    float c = cosf(angle), s = sinf(angle);
    return {v.x, v.y * c - v.z * s, v.y * s + v.z * c};
}

// project — 3D -> 2D perspektif projeksiyon
// p: kamera uzayindaki nokta, sx/sy: cikis ekran koordinatlari
// return: true = cizilebilir
inline bool project(Vec3 p, int &sx, int &sy) {
    if (p.z < NEAR_CLIP) return false;
    sx = SCR_W / 2 + (int)(p.x * FOCAL / p.z);
    sy = (SCR_H - HUD_H) / 2 + (int)(p.y * FOCAL / p.z);
    return (sx >= -50 && sx < SCR_W + 50 && sy >= -50 && sy < SCR_H + 50);
}

// ============ Yildiz (Arka Plan) ============
struct Star {
    int x, y;
    uint8_t bright;
};

// ============ Object3D — Uzaydaki dusman cismi ============
struct Object3D {
    Vec3 pos, vel;
    float rotAngle, rotSpeed, scale;
    ObjType type;
    int hp;
    float hitTimer;
    bool active;
};

// ============ Bullet3D — Oyuncu mermisi ============
struct Bullet3D {
    Vec3 pos, vel;
    float life;
    bool active;
};

// ============ Boom — Patlama efekti ============
struct Boom {
    Vec3 pos;
    float frame;
    bool active;
};

// ============ SpaceDust — Uzay tozu (hiz hissi) ============
struct SpaceDust {
    float x, y, z;
};
