// ============================================================
//  WIRE-FRAME 3D ENGINE — TİP TANIMLARI
//  Arduino IDE'nin otomatik prototip üretimi nedeniyle
//  struct'lar ayrı header dosyasında tanımlanmalıdır.
// ============================================================
#ifndef TYPES_H
#define TYPES_H

#include <math.h>

// ============================================================
//  3D MATEMATİK — Vec3, Vec2, Mat4
// ============================================================
struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

// 4×4 matris (satır-majör)
struct Mat4 { float m[4][4]; };

// ============================================================
//  MESH TANIMI
//  Vertices + Edge listesi (wire-frame için yeterli)
// ============================================================
#define MAX_VERTS 32
#define MAX_EDGES 60
#define MAX_FACES 20

struct Mesh {
    Vec3  verts[MAX_VERTS];
    int   edges[MAX_EDGES][2];
    int   faces[MAX_FACES][4];  // Her yüz 3-4 köşe (index, -1 = son)
    int   numVerts, numEdges, numFaces;
};

// ============================================================
//  KAMERA (6-DOF)
// ============================================================
struct Camera6DOF {
    Vec3  pos;           // Dünya pozisyonu
    Vec3  forward;       // Baktığı yön (normalize)
    Vec3  up;            // Yukarı vektör
    Vec3  right;         // Sağ vektör (cross(forward, up))
    float rollAngle;     // Görsel roll (ui için)
};

// ============================================================
//  SAHNE NESNESİ
// ============================================================
struct SceneObject {
    Mesh*    mesh;
    Vec3     pos;           // Dünya pozisyonu
    Vec3     rot;           // Euler açıları (rx, ry, rz)
    Vec3     rotSpeed;      // Dönüş hızı (radyan/saniye)
    float    scale;
    uint16_t color;
    bool     active;
    int      hp;            // Sağlık (düşmanlar için)
    int      hitTimer;      // Hasar flash timer
    Vec3     vel;           // Hız vektörü (AI hareketi)
};

// ============================================================
//  MERMİ
// ============================================================
struct Bullet {
    Vec3  pos;
    Vec3  vel;    // Normalize * hız
    bool  active;
    int   life;   // Frame sayacı
};

// ============================================================
//  PATLAMA
// ============================================================
struct Explosion {
    Vec3  pos;
    int   timer;    // 0 = bitti
    int   maxTimer;
};

// ============================================================
//  YILDIZ
// ============================================================
struct Star {
    int x, y;
    uint16_t color;
};

// ============================================================
//  SAHNE ENUM
// ============================================================
enum Scene { SCENE_TITLE, SCENE_SPACE, SCENE_TUNNEL, SCENE_GAMEOVER };

// ============================================================
//  TÜNEL ENGELİ
// ============================================================
struct TunnelObstacle {
    float z;          // Derinlik pozisyonu
    float angle;      // Açısal pozisyon (radyan)
    float gapAngle;   // Boşluğun açısı
    float gapSize;    // Boşluk büyüklüğü (radyan)
    bool  active;
    bool  passed;     // Oyuncu geçti mi (skor için)
};

#endif
