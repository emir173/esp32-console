#pragma once
// ============================================================
//  Renderer.h — E-OS DOOM: Tüm raycasting, sprite, HUD,
//  efekt, menü ve BMP yükleme işlemleri
// ============================================================

#include "Config.h"

// ============================================================
//  Yumuşak sis yardımcıları — 32 seviyeli light diminishing.
//  fogF: mesafe → parlaklık faktörü (FOG_MIN..32).
//  fogShade: RGB565 pikseli f/32 ile ölçekler (2 çarpma).
// ============================================================
inline uint8_t fogF(float d) {
    if (d <= FOG_START) return 32;
    int f = 32 - (int)((d - FOG_START) * FOG_SLOPE);
    if (f < FOG_MIN) f = FOG_MIN;
    return (uint8_t)f;
}
inline uint16_t fogShade(uint16_t c, uint8_t f) {
    if (f >= 32) return c;
    uint32_t rb = c & 0xF81Fu, g = c & 0x07E0u;
    return (uint16_t)((((rb * f) >> 5) & 0xF81Fu) | (((g * f) >> 5) & 0x07E0u));
}

// ============================================================
//  renderWalls — DDA Raycasting ile duvar sütunlarını çizer.
//  activeFB: yazılacak framebuffer. pitch: hasar sarsıntısı.
//  Her piksel sütununda ışın gönderir, DDA ile duvar bulur,
//  texture mapping yapar, tavan/zemin doldurur.
//  zBuffer[x]'i sprite derinlik testi için doldurur.
// ============================================================
inline void renderWalls(uint16_t* activeFB, int pitch) {
    // Zemin/tavan flat casting satır LUT'ları: perp mesafe ve sis SADECE
    // satıra (y) bağlı — kare başına bir kez hesaplanır, piksel başına
    // maliyet 2 çarpma + texture okumasına iner.
    static float   rowDist[SH];
    static uint8_t rowF[SH];
    int hz = SH / 2 + pitch;                      // ufuk satırı (sarsıntı dahil)
    for (int y = 0; y < SH; y++) {
        int k = (y < hz) ? (hz - y) : (y - hz);
        float d = (k > 0) ? (0.5f * SH) / k : 999.0f;
        rowDist[y] = d;
        rowF[y] = fogF(d);
    }
    const uint16_t* texF = tex[TEX_FLOOR];
    const uint16_t* texC = tex[TEX_CEIL];
    for (int x = 0; x < SW; x++) {
        float camX  = camXTable[x];
        float rayX  = dirX + planeX * camX;
        float rayY  = dirY + planeY * camX;
        int mx = (int)px, my = (int)py;
        float ddx = fabs(1 / rayX), ddy = fabs(1 / rayY), sdx, sdy;
        int sx, sy, hit = 0, side, ht = 1, lh = 1;
        // Animasyonlu kapı: ışın kapıyı deler, arkadaki duvar da bulunur;
        // sütun iki parça çizilir (üstte yükselen kanat, altında arkası).
        int   doorHit = 0, doorSide = 0, doorTx = 0, doorHt = 6;
        float doorPerp = 0, doorT = 0;
        if (rayX < 0) { sx = -1; sdx = (px - mx) * ddx; }
        else          { sx =  1; sdx = (mx + 1 - px) * ddx; }
        if (rayY < 0) { sy = -1; sdy = (py - my) * ddy; }
        else          { sy =  1; sdy = (my + 1 - py) * ddy; }
        while (!hit) {
            if (sdx < sdy) { sdx += ddx; mx += sx; side = 0; }
            else           { sdy += ddy; my += sy; side = 1; }
            if (mx < 0 || mx >= MW || my < 0 || my >= MH) break;
            int cell = MAP[my][mx];
            if (cell > 0) {
                float t = doorHit ? -1.0f : doorAnimT(mx, my);
                if (t >= 0) {   // ilk animasyonlu kapı: kaydet, ışına devam
                    doorHit = 1; doorT = t; doorHt = cell; doorSide = side;
                    doorPerp = (side == 0) ? (sdx - ddx) : (sdy - ddy);
                    float wxD = (side == 0) ? py + doorPerp * rayY : px + doorPerp * rayX;
                    wxD -= floor(wxD);
                    doorTx = (int)(wxD * TEX_W);
                } else { hit = 1; ht = cell; }
            }
        }
        if (ht == 8 && exitPressed) ht = TEX_EXIT_ON;   // çıkış switch'i basılı hali
        if (ht == 9) ht = TEX_ELEV;                     // asansör kapı dokusu
        int ds, de, tx = 0;
        uint8_t fw = 32;
        float st = 0, tp = 0;
        if (!hit) {
            zBuffer[x] = 999.0f;
            ds = de = min(SH, max(0, hz));  // duvar yok: sütun tamamen tavan+zemin
        } else {
            float perp = (side == 0) ? (sdx - ddx) : (sdy - ddy);
            zBuffer[x] = perp;
            lh  = (int)(SH / perp);
            ds  = max(0, -lh / 2 + SH / 2 + pitch);
            de  = min(SH - 1, lh / 2 + SH / 2 + pitch);
            float wx = (side == 0) ? py + perp * rayY : px + perp * rayX;
            wx -= floor(wx);
            tx = (int)(wx * TEX_W);
            st = 1.0f * TEX_H / lh;
            tp = (ds - SH / 2 - pitch + lh / 2) * st;
            // Yumuşak sis; side==1 karartması (eski +1 kademe = %50) sonda
            fw = fogF(perp);
            if (side == 1) { fw >>= 1; if (fw < FOG_MIN) fw = FOG_MIN; }
        }
        if (!doorHit) {
            // Tavan flat casting: dünya koordinatı = pos + rowDist * rayDir
            for (int y = 0; y < ds; y++) {
                float rd = rowDist[y];
                int fx = (int)((px + rd * rayX) * TEX_W) & (TEX_W - 1);
                int fy = (int)((py + rd * rayY) * TEX_W) & (TEX_W - 1);
                activeFB[y * SW + x] = fogShade(texC[fy * TEX_W + fx], rowF[y]);
            }
            for (int y = ds; y < de; y++) {
                int ty = ((int)tp) & 63;
                tp += st;
                activeFB[y * SW + x] = fogShade(tex[ht][ty * TEX_W + tx], fw);
            }
            // Zemin flat casting (tavanla simetrik)
            for (int y = de; y < SH; y++) {
                float rd = rowDist[y];
                int fx = (int)((px + rd * rayX) * TEX_W) & (TEX_W - 1);
                int fy = (int)((py + rd * rayY) * TEX_W) & (TEX_W - 1);
                activeFB[y * SW + x] = fogShade(texF[fy * TEX_W + fx], rowF[y]);
            }
        } else {
            // ── Animasyonlu kapı sütunu ──
            // Kanat, açıklığın üst t kısmına kaymış durumda: ekranda
            // [dsD, doorB) kanadı gösterir, doku dikeyde t*TEX_H kaydırılır
            // (kanat dokusuyla birlikte tavana kalkar, orijinal Doom gibi).
            int lhD   = (int)(SH / doorPerp);
            int dsD   = max(0, -lhD / 2 + SH / 2 + pitch);
            int deD   = min(SH - 1, lhD / 2 + SH / 2 + pitch);
            int doorB = (int)(lhD / 2 + SH / 2 + pitch - doorT * lhD);
            if (doorB > deD) doorB = deD;
            if (doorB < dsD) doorB = dsD;
            uint8_t fwD = fogF(doorPerp);
            if (doorSide == 1) { fwD >>= 1; if (fwD < FOG_MIN) fwD = FOG_MIN; }
            float stD = 1.0f * TEX_H / lhD;
            float tpD = (dsD - SH / 2 - pitch + lhD / 2) * stD + doorT * TEX_H;
            zBuffer[x] = doorPerp;   // aralıktaki sprite'lar kapı düzlemine kırpılır

            for (int y = 0; y < dsD; y++) {          // tavan
                float rd = rowDist[y];
                int fx = (int)((px + rd * rayX) * TEX_W) & (TEX_W - 1);
                int fy = (int)((py + rd * rayY) * TEX_W) & (TEX_W - 1);
                activeFB[y * SW + x] = fogShade(texC[fy * TEX_W + fx], rowF[y]);
            }
            for (int y = dsD; y < doorB; y++) {      // yükselen kanat
                int ty = ((int)tpD) & 63;
                tpD += stD;
                activeFB[y * SW + x] = fogShade(tex[doorHt][ty * TEX_W + doorTx], fwD);
            }
            int wallS = max(ds, doorB), wallE = max(de, doorB);
            for (int y = doorB; y < wallS; y++) {    // aralıktan görünen tavan
                float rd = rowDist[y];
                int fx = (int)((px + rd * rayX) * TEX_W) & (TEX_W - 1);
                int fy = (int)((py + rd * rayY) * TEX_W) & (TEX_W - 1);
                activeFB[y * SW + x] = fogShade(texC[fy * TEX_W + fx], rowF[y]);
            }
            float tp2 = (wallS - SH / 2 - pitch + lh / 2) * st;
            for (int y = wallS; y < wallE; y++) {    // arkadaki duvar
                int ty = ((int)tp2) & 63;
                tp2 += st;
                activeFB[y * SW + x] = fogShade(tex[ht][ty * TEX_W + tx], fw);
            }
            for (int y = wallE; y < SH; y++) {       // zemin
                float rd = rowDist[y];
                int fx = (int)((px + rd * rayX) * TEX_W) & (TEX_W - 1);
                int fy = (int)((py + rd * rayY) * TEX_W) & (TEX_W - 1);
                activeFB[y * SW + x] = fogShade(texF[fy * TEX_W + fx], rowF[y]);
            }
        }
    }
}

// ============================================================
//  renderSprites — Tüm aktif sprite'ları mesafeye göre
//  uzaktan yakına sıralayıp framebuffer'a çizer.
//  Chroma-key (COL_CHROMA) atlanır, HIT efekti parlaklık artırır.
// ============================================================
inline void renderSprites(uint16_t* activeFB, int pitch) {
    int order[NUM_SPRITES];
    float dists[NUM_SPRITES];
    int aliveCount = 0;
    for (int i = 0; i < NUM_SPRITES; i++) {
        if (sprites[i].state <= 0 && sprites[i].type != ST_BARREL) continue;
        if (sprites[i].type == ST_BARREL && sprites[i].state <= 0 && sprites[i].animState == ANIM_DEAD) continue;
        float dx_s = px - sprites[i].x;
        float dy_s = py - sprites[i].y;
        dists[i] = dx_s * dx_s + dy_s * dy_s;
        order[aliveCount] = i;
        int j = aliveCount;
        while (j > 0 && dists[order[j - 1]] < dists[order[j]]) {
            int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
            j--;
        }
        aliveCount++;
    }
    for (int i = 0; i < aliveCount; i++) {
        int idx = order[i];
        if (sprites[idx].state <= 0 && sprites[idx].type != ST_BARREL) continue;
        if (sprites[idx].type == ST_BARREL && sprites[idx].state <= 0 && sprites[idx].animState == ANIM_DEAD) continue;
        float sx = sprites[idx].x - px, sy = sprites[idx].y - py;
        float det = 1.0f / (planeX * dirY - dirX * planeY);
        float tx = det * (dirY * sx - dirX * sy);
        float ty = det * (-planeY * sx + planeX * sy);
        if (ty > 0) {
            int ssx = (int)((SW / 2) * (1 + tx / ty));
            int sh  = abs((int)(SH / ty));
            int dyS = max(0, -sh / 2 + SH / 2 + pitch);
            int dyE = min(SH - 1, sh / 2 + SH / 2 + pitch);
            int dw  = abs((int)(SH / ty));
            int dxS = max(0, -dw / 2 + ssx);
            int dxE = min(SW - 1, dw / 2 + ssx);

            int texID = getEnemyTexID(idx);
            if (!tex[texID]) continue;

            // Mesafe sisi: ışıklı mermiler muaf, HIT parlaması sisi ezer.
            // Duvarlardan farklı olarak TEK kademe: 160x128'de uzak düşman
            // zaten birkaç piksel, tam karartma oynanabilirliği bozar
            // (bilinçli sapma).
            int fogLevel = 0;
            if (sprites[idx].type != ST_FIREBALL && sprites[idx].type != ST_PARRYBALL) {
                if (ty > FOG_SPRITE) fogLevel = 1;
            }

            for (int b = dxS; b < dxE; b++) {
                if (ty >= zBuffer[b]) continue;
                int tX = (int)(256 * (b - (-dw / 2 + ssx)) * TEX_W / dw) / 256;
                if (tX < 0) tX = 0;
                else if (tX >= TEX_W) tX = TEX_W - 1;

                for (int y = dyS; y < dyE; y++) {
                    int d  = y * 256 - SH * 128 - pitch * 256 + sh * 128;
                    int tY = ((d * TEX_H) / sh) / 256;
                    if (tY < 0) tY = 0;
                    else if (tY >= TEX_H) tY = TEX_H - 1;

                    uint16_t col = tex[texID][tY * TEX_W + tX];
                    if (col != COL_CHROMA) {
                        if (sprites[idx].animState == ANIM_HIT) {
                            uint8_t r = ((col >> 11) & 0x1F), g = ((col >> 5) & 0x3F), b_col = (col & 0x1F);
                            r = ((int)r + 12 > 31) ? 31 : r + 12;
                            g = ((int)g + 24 > 63) ? 63 : g + 24;
                            b_col = ((int)b_col + 12 > 31) ? 31 : b_col + 12;
                            col = (r << 11) | (g << 5) | b_col;
                        } else {
                            for (int k = 0; k < fogLevel; k++) col = ((col >> 1) & COL_DARKEN_MASK);
                        }
                        activeFB[y * SW + b] = col;
                    }
                }
            }
        }
    }
}

// ============================================================
//  renderWeapon — Silah/görünüm modelini framebuffer'a çizer.
//  Duruma göre doğru silah texture'ını seçer (tabanca,
//  pompalı, kalkan, yakın dövüş). Hareket halinde sallanır.
// ============================================================
inline void renderWeapon(uint16_t* activeFB, int pitch, uint32_t now) {
    int gW = 64, gH = 64;
    int gX = (SW - gW) / 2;
    int gY = SH - gH + 5 + pitch;

    int jx = joyRawX - joyCenterX;
    int jy = joyRawY - joyCenterY;
    if (abs(jx) < 300) jx = 0;
    if (abs(jy) < 300) jy = 0;
    if (jx || jy) gY += sin(now / 150.0f) * 4;
    if (weaponType == 0) gY += 15;

    bool isParrying = lastShieldState && (now - shieldStartTime < 300);
    int wTex = 14;

    if (now - meleeTimer < 300) {
        wTex = 21;
    } else if (lastShieldState) {
        if (now - shieldSawTime < 200) wTex = 19;
        else if (isParrying)          wTex = 20;
        else                          wTex = 18;
    } else {
        if (weaponType == 0) {
            uint32_t elapsed = now - fireT;
            if (elapsed < 100) { wTex = 15; gY += 10; }
            else               { wTex = 14; }
        } else if (weaponType == 1) {
            uint32_t elapsed = now - fireT;
            if      (elapsed < 80)  { wTex = 17; gY += 8; }
            else if (elapsed < 200) wTex = 47;
            else if (elapsed < 400) wTex = 48;
            else if (elapsed < 520) wTex = 47;
            else                    wTex = 16;
        }
    }

    for (int sy = 0; sy < gH; sy++) {
        int drawY = gY + sy;
        if (drawY < 0 || drawY >= SH) continue;
        for (int sx = 0; sx < gW; sx++) {
            int drawX = gX + sx;
            if (drawX < 0 || drawX >= SW) continue;
            int c = tex[wTex][sy * TEX_W + sx];
            if (c != COL_CHROMA) { activeFB[drawY * SW + drawX] = c; }
        }
    }
}

// ============================================================
//  renderDamageEffect — Hasar/düşük can ekran kenarı vinyeti.
//  Hasar alınmışsa (son 200ms) veya hp<=25 (yanıp sönen)
//  kenarlara kırmızı gradient çizer.
// ============================================================
inline void renderDamageEffect(uint16_t* activeFB, uint32_t now) {
    bool lowHpFlash = (hp > 0 && hp <= 25) && ((now % 1000) < 150);
    if ((now - lastDamageTime < 200 && !lastShieldState) || lowHpFlash) {
        uint16_t redOut = COL_RED;
        uint16_t redIn  = COL_DARKRED;
        for (int y = 0; y < SH; y++) {
            activeFB[y * SW]          = redOut;
            activeFB[y * SW + SW - 1] = redOut;
            activeFB[y * SW + 1]      = redIn;
            activeFB[y * SW + SW - 2] = redIn;
        }
        for (int x = 0; x < SW; x++) {
            activeFB[x]                 = redOut;
            activeFB[(SH - 1) * SW + x] = redOut;
            activeFB[SW + x]            = redIn;
            activeFB[(SH - 2) * SW + x] = redIn;
        }
    }
}

// ============================================================
//  drawStaticHUD — Oyun altındaki sabit HUD çerçevesini
//  TFT'ye bir kere çizer (AMMO / HEALTH / ARMOR etiketleri).
// ============================================================
inline void drawStaticHUD() {
    int hy = SH;
    tft.fillRect(0, hy, 160, 24, COL_HUDBG);
    tft.drawFastHLine(0, hy, 160, COL_HUDLINE);
    tft.drawFastHLine(0, 127, 160, COL_HUDDARK);
    tft.drawFastVLine(50, hy, 24, COL_HUDVDARK);
    tft.drawFastVLine(51, hy, 24, COL_HUDLINE);
    tft.drawFastVLine(105, hy, 24, COL_HUDVDARK);
    tft.drawFastVLine(106, hy, 24, COL_HUDLINE);
    tft.setTextSize(1);
    tft.setTextColor(COL_AMMO_LABEL);
    tft.setCursor(12, hy + 4);   tft.print("AMMO");
    tft.setCursor(62, hy + 4);   tft.print("HEALTH");
    tft.setCursor(118, hy + 4);  tft.print("ARMOR");
}

// ============================================================
//  makeTex — SD gerektirmeyen prosedürel texture üretir.
//  id=8: yeşil zemin, id=9-11: siyah boş, id=12: ateş topu,
//  id=13: sekme mermisi (camgöbeği).
// ============================================================
inline void makeTex(int id) {
    if (id == 9 || id == 10 || id == 11) {
        for (int i = 0; i < TEX_W * TEX_H; i++) tex[id][i] = COL_CHROMA;
        return;
    }
    for (int y = 0; y < TEX_H; y++) {
        for (int x = 0; x < TEX_W; x++) {
            if (id == 8) {
                tex[8][y * TEX_W + x] = COL_GREEN_ITEM;
            } else if (id == 12) {
                float dx = x - 32, dy = y - 32;
                float r = sqrt(dx * dx + dy * dy);
                if (r < 18)      tex[12][y * TEX_W + x] = COL_RGB(255, (uint8_t)(255 - (r * 12)), 0);
                else if (r < 26) tex[12][y * TEX_W + x] = COL_RGB(255, 0, 0);
                else             tex[12][y * TEX_W + x] = COL_CHROMA;
            } else if (id == 13) {
                float dx = x - 32, dy = y - 32;
                float r = sqrt(dx * dx + dy * dy);
                if (r < 18)      tex[13][y * TEX_W + x] = COL_RGB(0, (uint8_t)(255 - (r * 12)), 255);
                else if (r < 26) tex[13][y * TEX_W + x] = COL_RGB(0, 0, 255);
                else             tex[13][y * TEX_W + x] = COL_CHROMA;
            }
        }
    }
}

// ============================================================
//  makeFlatTexes — zemin/tavan flat'lerinin prosedürel fallback'i.
//  SD'de /doom/zemin.bmp ve /doom/tavan.bmp varsa loadBMP üzerine yazar.
//  Zemin: kahverengi benek (orijinal Doom kahve flat taklidi),
//  tavan: soğuk gri-mavi + 32'lik panel derzi.
// ============================================================
inline void makeFlatTexes() {
    for (int y = 0; y < TEX_H; y++) {
        for (int x = 0; x < TEX_W; x++) {
            uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            int n = (int)((h >> 24) & 31) - 16;                    // -16..15 benek
            int g = 80 + n;
            tex[TEX_FLOOR][y * TEX_W + x] = COL_RGB(g + (g >> 2), g, (g * 2) / 3);
            int c = 44 + n / 2;
            uint16_t col = COL_RGB((c * 3) / 4, (c * 3) / 4, c);
            if ((x & 31) == 0 || (y & 31) == 0) col = COL_RGB(c / 2, c / 2, (c * 2) / 3);
            tex[TEX_CEIL][y * TEX_W + x] = col;
        }
    }
}

// ============================================================
//  loadBMP — SD'den 24bpp BMP'yi tex[id]'ye yükler.
//  64x64, chroma-key (mor) → saydam, siyah → koyu gri.
// ============================================================
inline bool loadBMP(const char* filename, int id, uint8_t* fileBuf) {
    if (!tex[id]) return false;

    File f = SD.open(filename, FILE_READ);
    if (!f) return false;

    size_t fSize = f.size();
    if (fSize > 80000) { f.close(); return false; }
    f.read(fileBuf, fSize);
    f.close();

    uint8_t* hdr = fileBuf;
    if (hdr[0] != 'B' || hdr[1] != 'M') { return false; }
    uint32_t dataOfs = (uint32_t)hdr[10] | ((uint32_t)hdr[11] << 8) | ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
    int32_t w  = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19] << 8) | ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24));
    int32_t h  = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8) | ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24));
    uint16_t bpp = (uint16_t)(hdr[28] | (hdr[29] << 8));
    bool bottomUp = (h > 0);
    int32_t absH = bottomUp ? h : -h;
    if (w != TEX_W || absH != TEX_H || bpp != 24) { return false; }

    uint32_t rowSz = ((w * 3 + 3) & ~3);
    for (int row = 0; row < TEX_H; row++) {
        int tgtY = bottomUp ? (TEX_H - 1 - row) : row;
        uint8_t* rowBuf = fileBuf + dataOfs + (row * rowSz);
        for (int x = 0; x < TEX_W; x++) {
            uint8_t b = rowBuf[x * 3], g = rowBuf[x * 3 + 1], r = rowBuf[x * 3 + 2];
            uint16_t c = COL_RGB(r, g, b);
            if (r > 200 && g < 55 && b > 200) c = COL_CHROMA;
            else if (r == 0 && g == 0 && b == 0) c = COL_NEARBLACK;
            tex[id][tgtY * TEX_W + x] = c;
        }
    }
    return true;
}

// ============================================================
//  loadTitlePic — SD'den /doom/TITLEPIC.bmp (160x128) okur.
//  Menü arka planı olarak kullanılır.
// ============================================================
inline bool loadTitlePic(uint8_t* fileBuf) {
    if (!titlePicBuf) return false;

    File f = SD.open("/doom/TITLEPIC.bmp", FILE_READ);
    if (!f) return false;

    size_t fSize = f.size();
    if (fSize > 80000) { f.close(); return false; }
    f.read(fileBuf, fSize);
    f.close();

    uint8_t* hdr = fileBuf;
    if (hdr[0] != 'B' || hdr[1] != 'M') { return false; }
    uint32_t dataOfs = hdr[10] | (hdr[11] << 8) | (hdr[12] << 16) | (hdr[13] << 24);
    int32_t w  = hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24);
    int32_t h  = hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24);
    uint16_t bpp = hdr[28] | (hdr[29] << 8);
    bool bottomUp = (h > 0);
    int32_t absH = bottomUp ? h : -h;
    if (w != 160 || absH != 128 || bpp != 24) { return false; }

    uint32_t rowSz = ((w * 3 + 3) & ~3);
    for (int row = 0; row < 128; row++) {
        int tgtY = bottomUp ? (128 - 1 - row) : row;
        uint8_t* rowBuf = fileBuf + dataOfs + (row * rowSz);
        for (int x = 0; x < 160; x++) {
            uint8_t b = rowBuf[x * 3], g = rowBuf[x * 3 + 1], r = rowBuf[x * 3 + 2];
            titlePicBuf[tgtY * 160 + x] = COL_RGB(r, g, b);
        }
    }
    return true;
}

// ============================================================
//  drawIconDoom — DOOM ikonu: artı işareti + daire
// ============================================================
inline void drawIconDoom(int x, int y, bool sel) {
    uint16_t c = sel ? COL_MENU_TITLE : COL_RGB(80, 20, 0);
    tft.drawCircle(x + 12, y + 12, 8, c);
    tft.drawFastHLine(x + 2, y + 12, 8, c);
    tft.drawFastHLine(x + 15, y + 12, 8, c);
    tft.drawFastVLine(x + 12, y + 2, 8, c);
    tft.drawFastVLine(x + 12, y + 15, 8, c);
    tft.drawPixel(x + 12, y + 12, c);
}

// ============================================================
//  drawIconSnake — SNAKE ikonu: yılan başı + gövde + göz
// ============================================================
inline void drawIconSnake(int x, int y, bool sel) {
    uint16_t c = sel ? COL_MENU_SNAKE : COL_RGB(0, 80, 20);
    tft.fillRect(x + 4, y + 14, 16, 6, c);
    tft.fillRect(x + 14, y + 6, 6, 14, c);
    tft.fillRect(x + 6, y + 6, 10, 6, c);
    tft.drawPixel(x + 8, y + 8, TFT_BLACK);
}

// ============================================================
//  drawMasterMenu — Ana OS menüsü (DOOM / SNAKE)
// ============================================================
inline void drawMasterMenu(bool fullRedraw) {
    if (fullRedraw) {
        tft.fillScreen(TFT_BLACK);
        tft.fillRect(0, 0, 160, 22, COL_RGB(15, 15, 30));
        tft.setTextColor(COL_ARMOR_VAL);
        tft.setTextSize(1);
        tft.setCursor(68, 7);
        tft.print("E-OS");
        tft.drawFastHLine(0, 22, 160, COL_MENU_SYS);
        masterMenuDrawn = true;
    }

    struct { const char* label; const char* sub; uint16_t color; } items[] = {
        { "DOOM",  "FPS Klasigi",  COL_MENU_TITLE },
        { "SNAKE", "Retro Yilan",  COL_MENU_SNAKE }
    };

    int yBase = 32;
    for (int i = 0; i < 2; i++) {
        bool sel = (i == masterMenuSel);
        int cy = yBase + i * 32;

        uint16_t bg = sel ? COL_RGB(30, 30, 30) : TFT_BLACK;
        uint16_t border = sel ? items[i].color : COL_DARKGRAY;

        tft.fillRect(4, cy, 152, 28, bg);
        tft.drawRect(4, cy, 152, 28, border);
        tft.drawRect(6, cy + 2, 24, 24, border);

        if (i == 0) drawIconDoom(6, cy + 2, sel);
        else        drawIconSnake(6, cy + 2, sel);

        tft.setTextSize(1);
        tft.setTextColor(sel ? items[i].color : COL_RGB(120, 120, 120), bg);
        tft.setCursor(38, cy + 5);
        tft.print(items[i].label);

        tft.setTextColor(sel ? COL_RGB(200, 200, 200) : COL_RGB(80, 80, 80), bg);
        tft.setCursor(38, cy + 16);
        tft.print(items[i].sub);
    }
}

// ============================================================
//  snakeDraw — Yılan oyununu TFT'ye çizer (CS=5 piksel/hücre)
// ============================================================
inline void snakeDraw() {
    const int CS = 5;
    const int OX = 0, OY = 0;

    if (!snakeDrawn) {
        tft.fillScreen(COL_SNAKE_BG);
        tft.drawFastHLine(0, SNAKE_H * CS, 160, COL_SNAKE_HLINE);
        snakeDrawn = true;
    }

    tft.setTextSize(1);
    tft.setTextColor(COL_RGB(0, 200, 0), COL_SNAKE_BG);
    tft.setCursor(4, SNAKE_H * CS + 4);
    tft.printf("SCORE: %04d", snakeScore);

    tft.fillRect(OX + snakeFoodX * CS, OY + snakeFoodY * CS, CS - 1, CS - 1, COL_SNAKE_FOOD);

    for (int i = 1; i < snakeLen; i++) {
        uint8_t g = 100 + (i * 100 / snakeLen);
        tft.fillRect(OX + snakeBody[i].x * CS, OY + snakeBody[i].y * CS, CS - 1, CS - 1, COL_RGB(0, g, 0));
    }
    tft.fillRect(OX + snakeBody[0].x * CS, OY + snakeBody[0].y * CS, CS - 1, CS - 1, COL_SNAKE_HEAD);

    if (snakeDead) {
        tft.setTextSize(2);
        tft.setTextColor(COL_GAMEOVER);
        tft.setCursor(26, 55);
        tft.print("GAME OVER");
        tft.setTextSize(1);
        tft.setTextColor(COL_RGB(180, 180, 180));
        tft.setCursor(45, 95);
        tft.print("BTN_A: Retry");
        tft.setCursor(45, 110);
        tft.print("BTN_B: Exit");
    }
}
