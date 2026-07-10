// ============================================================
//  ESP32 FLIGHT TRACKER V1.1 — UI Render (TFT + OLED)
//
//  TFT 160x128:
//    - Map background + grid + continents
//    - Aircraft dots (arrow shaped pointing to heading)
//    - Status bar (FLIGHT TRACKER + connection + aircraft count)
//    - Selected aircraft highlight + info box
//
//  OLED 128x64:
//    - Total aircraft count + last update
//    - Selected aircraft details (callsign, altitude, speed, heading, country)
// ============================================================

#include "flight_internal.h"
#include "flight_plane_frames.h"

// ─── Aircraft sprite system (Pre-rendered via Python) ───
// NO rotation artifacts — every direction is sharp pixel-art.
#define PLANE_SIZE   15

// (buildPlaneSprites function removed because we draw directly from header)

// ─── Render state ───
static bool s_oledReady = false;
static bool s_dashboardDrawn = false;
// Previous viewport — if not changed, don't redraw map
static float s_prevCenterLat = 999;
static float s_prevCenterLon = 999;
static int   s_prevZoom = -1;
static int   s_prevSelected = -2;  // track selected aircraft changes
static int   s_prevAircraftCount = -1;

// ══════════════════════════════════════════════════════════════
//  interpolateAircraft — Extrapolate aircraft's current position
//  from heading+speed. OpenSky data arrives every 120 sec, but we
//  calculate where the aircraft is heading on every render (100ms)
//  and slide it on screen to give a "live" movement feel.
//
//  Logic:
//    delta_t = now - last_update (seconds)
//    distance = velocity * delta_t (meters)
//    Earth radius 6371000m, 1 degree ~111km
//    lat_offset = distance * cos(heading) / 111000
//    lon_offset = distance * sin(heading) / (111000 * cos(lat))
//    interp_lat = lat + lat_offset
//    interp_lon = lon + lon_offset
// ══════════════════════════════════════════════════════════════
static void interpolateAircraft(Aircraft &a) {
    if (a.on_ground || a.velocity < 1.0f) {
        // On ground or very slow — don't interpolate
        a.interp_lat = a.lat;
        a.interp_lon = a.lon;
        return;
    }

    uint32_t now = millis();
    float deltaSec = (now - a.last_update_ms) / 1000.0f;
    if (deltaSec > 600) deltaSec = 600;  // 10 min cap — aircraft might be lost if data is too old

    // Movement over earth — meters → degrees
    float distance = a.velocity * deltaSec;  // meters
    float headingRad = a.heading * PI / 180.0f;

    // North axis: heading 0 = north, 90 = east
    // lat delta: cos(heading) * distance
    // lon delta: sin(heading) * distance
    float latDegPerM = 1.0f / 111000.0f;  // 1 degree latitude ~111km
    float lonDegPerM = 1.0f / (111000.0f * cosf(a.lat * PI / 180.0f));

    a.interp_lat = a.lat + cosf(headingRad) * distance * latDegPerM;
    a.interp_lon = a.lon + sinf(headingRad) * distance * lonDegPerM;

    // Boundary check
    if (a.interp_lat > 90) a.interp_lat = 90;
    if (a.interp_lat < -90) a.interp_lat = -90;
    if (a.interp_lon > 180) a.interp_lon -= 360;
    if (a.interp_lon < -180) a.interp_lon += 360;
}

// ══════════════════════════════════════════════════════════════
//  renderSetup — Init OLED
// ══════════════════════════════════════════════════════════════
void renderSetup() {
    oled.begin();
    renderMarkOLEDReady();
}

// ══════════════════════════════════════════════════════════════
//  renderDrawStatusBar — Top status bar
//  Left: ▶ FLIGHT TRACKER  Right: aircraft count + connection dot
// ══════════════════════════════════════════════════════════════
void renderDrawStatusBar() {
    spr.fillRect(0, 0, 160, 12, RGB_FIX(12, 12, 28));

    // Total Width: Triangle (4px) + Space (4px) + "FLIGHT TRACKER" (84px) = 92px
    // 160 - 92 = 68 -> X start = 34
    
    // Play triangle (Left of centered group)
    spr.fillTriangle(34, 3, 34, 9, 38, 6, COL_ACCENT);
    
    // FLIGHT TRACKER text
    spr.setTextSize(1);
    spr.setTextColor(COL_WHITE, RGB_FIX(12, 12, 28));
    spr.setCursor(42, 2);
    spr.print("FLIGHT TRACKER");

    // UPDATING indicator — far right
    static uint32_t lastBlink = 0;
    static bool blinkOn = false;
    
    if (g_polling) {
        // Blinking red/orange light while updating
        if (millis() - lastBlink > 300) {
            lastBlink = millis();
            blinkOn = !blinkOn;
        }
        if (blinkOn) {
            spr.fillRect(150, 3, 4, 6, COL_DANGER);
        } else {
            spr.fillRect(150, 3, 4, 6, RGB_FIX(50, 0, 0));
        }
    } else {
        // Sleep color (gray/navy) when not updating
        spr.fillRect(150, 3, 4, 6, RGB_FIX(30, 40, 50));
    }

    // Accent bottom line
    spr.drawFastHLine(0, 12, 160, RGB_FIX(30, 70, 100));
    spr.drawFastHLine(40, 12, 80, COL_ACCENT);
}

// ══════════════════════════════════════════════════════════════
//  renderDrawAircraft — Draw a single aircraft on screen
//  Passenger jet silhouette — pointed nose, swept wings, integrated tail
//  Color: jet (high alt) → yellow, prop (low alt) → green
//         grounded → gray, selected → red
// ══════════════════════════════════════════════════════════════
void renderDrawAircraft(const Aircraft &a, bool selected) {
    if (!a.on_screen) return;

    int cx = a.screen_x;
    int cy = a.screen_y;
    if (cx < 0 || cx >= SCR_W || cy < 0 || cy >= SCR_H) return;

    // 100% Original FlightRadar24 Logo (High Res with Anti-Aliasing)
    int angle = (int)round(a.heading);
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    
    int frameIdx = angle / 5; // 72 frames in 5-degree steps
    if (frameIdx >= 72) frameIdx = 0;

    int w = 15;
    int h = 15;
    int startX = cx - w / 2;
    int startY = cy - h / 2;

    // Color selection based on altitude (OpenSky style)
    int colorIdx = 0;
    float altFt = a.altitude * 3.281f; // meters -> feet
    if (altFt >= 35000.0f) colorIdx = 3;      // Purple (> 35k ft)
    else if (altFt >= 25000.0f) colorIdx = 2; // Blue (25k - 35k ft)
    else if (altFt >= 15000.0f) colorIdx = 1; // Green (15k - 25k ft)
    // else 0 (Yellow, < 15k ft)

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sx = startX + x;
            int sy = startY + y;
            
            if (sx < 0 || sx >= SCR_W || sy < 0 || sy >= SCR_H) continue;
            
            uint8_t alpha = PLANE_FRAMES_ALPHA[frameIdx][y * w + x];
            if (alpha > 0) {
                uint16_t fgColor = PLANE_FRAMES_RGB[colorIdx][frameIdx][y * w + x];
                if (alpha == 255) {
                    spr.drawPixel(sx, sy, fgColor);
                } else {
                    // Real-time Alpha Blending for Anti-Aliasing
                    uint16_t bgColor = spr.readPixel(sx, sy);
                    
                    uint8_t r1 = (bgColor >> 11) & 0x1F;
                    uint8_t g1 = (bgColor >> 5) & 0x3F;
                    uint8_t b1 = bgColor & 0x1F;
                    
                    uint8_t r2 = (fgColor >> 11) & 0x1F;
                    uint8_t g2 = (fgColor >> 5) & 0x3F;
                    uint8_t b2 = fgColor & 0x1F;
                    
                    uint8_t r = (r1 * (255 - alpha) + r2 * alpha) / 255;
                    uint8_t g = (g1 * (255 - alpha) + g2 * alpha) / 255;
                    uint8_t b = (b1 * (255 - alpha) + b2 * alpha) / 255;
                    
                    spr.drawPixel(sx, sy, (r << 11) | (g << 5) | b);
                }
            }
        }
    }

    // Selection ring (outside wingtips)
    if (selected) {
        spr.drawCircle(cx, cy, 9, COL_SELECTED);
    }
}

// ══════════════════════════════════════════════════════════════
//  renderDrawSelectionBox — Info box for selected aircraft
//  3 lines at bottom of screen: callsign, alt, speed
// ══════════════════════════════════════════════════════════════
void renderDrawSelectionBox() {
    if (g_selectedIdx < 0 || g_selectedIdx >= g_aircraftCount) return;

    const Aircraft &a = g_aircraft[g_selectedIdx];

    // Bottom info box — starts at y=95
    int boxY = 95;
    int boxH = 33;  // 95-127

    // Background
    spr.fillRect(0, boxY, 160, boxH, RGB_FIX(15, 15, 35));
    // Top separator
    spr.drawFastHLine(0, boxY, 160, COL_ACCENT);

    spr.setTextSize(1);
    
    // Line 1 (Y=97): Callsign (Left) | Country (Right)
    spr.setTextColor(COL_ACCENT, RGB_FIX(15, 15, 35));
    spr.setCursor(4, boxY + 2);
    spr.print(a.callsign[0] ? a.callsign : "UNKNOWN");
    
    // Crop country name (max 12 chars fit)
    char cBuf[13];
    strncpy(cBuf, a.country, 12);
    cBuf[12] = '\0';
    spr.setTextColor(COL_JET, RGB_FIX(15, 15, 35)); // Same color as plane
    spr.setCursor(80, boxY + 2);
    spr.print(cBuf);

    // Line 2 (Y=107): Altitude (Left) | Speed (Right)
    spr.setTextColor(COL_WHITE, RGB_FIX(15, 15, 35));
    int altFt = (int)(a.altitude * 3.281f);
    char altBuf[20];
    snprintf(altBuf, sizeof(altBuf), "ALT:%d", altFt);
    spr.setCursor(4, boxY + 12);
    spr.print(altBuf);
    spr.setCursor(spr.getCursorX() + 1, boxY + 12);
    spr.print("ft");

    int spdKmh = (int)(a.velocity * 3.6f);
    char spdBuf[20];
    if (a.on_ground) {
        snprintf(spdBuf, sizeof(spdBuf), "GROUND");
        spr.setCursor(80, boxY + 12);
        spr.print(spdBuf);
    } else {
        snprintf(spdBuf, sizeof(spdBuf), "SPD:%d", spdKmh);
        spr.setCursor(80, boxY + 12);
        spr.print(spdBuf);
        spr.setCursor(spr.getCursorX() + 2, boxY + 12);
        spr.print("kmh");
    }

    // Line 3 (Y=117): Heading (Left) | ICAO Code (Right)
    spr.setTextColor(COL_GRAY, RGB_FIX(15, 15, 35));
    char hdgBuf[20];
    snprintf(hdgBuf, sizeof(hdgBuf), "HDG:%d", (int)a.heading);
    spr.setCursor(4, boxY + 22);
    spr.print(hdgBuf);

    char icaoBuf[20];
    snprintf(icaoBuf, sizeof(icaoBuf), "ICAO:%s", a.icao24);
    spr.setCursor(80, boxY + 22);
    spr.print(icaoBuf);
}

// ══════════════════════════════════════════════════════════════
//  renderDrawAll — Main screen draw (map + aircraft per frame)
//  Aircraft are interpolated, giving a smooth movement feel
// ══════════════════════════════════════════════════════════════
void renderDrawAll() {
    mapDrawBackground();
    mapDrawGrid();

    if (g_aircraftMutex != NULL) {
        xSemaphoreTake(g_aircraftMutex, portMAX_DELAY);
    }

    for (int i = 0; i < g_aircraftCount; i++) {
        Aircraft &a = g_aircraft[i];
        interpolateAircraft(a);
        latLonToScreen(a.interp_lat, a.interp_lon, a.screen_x, a.screen_y);
        a.on_screen = (a.screen_x >= 0 && a.screen_x < SCR_W &&
                       a.screen_y >= 0 && a.screen_y < SCR_H);
        
        if (i != g_selectedIdx) {
            renderDrawAircraft(a, false);
        }
    }

    if (g_selectedIdx >= 0 && g_selectedIdx < g_aircraftCount) {
        renderDrawAircraft(g_aircraft[g_selectedIdx], true);
    }

    renderDrawStatusBar();
    renderDrawSelectionBox();
    
    s_prevAircraftCount = g_aircraftCount;
    s_prevSelected = g_selectedIdx;

    if (g_aircraftMutex != NULL) {
        xSemaphoreGive(g_aircraftMutex);
    }

    checkScreenshot(spr);
    spr.pushSprite(0, 0);
}

// ══════════════════════════════════════════════════════════════
//  renderDrawConnecting — "Connecting..." screen
// ══════════════════════════════════════════════════════════════
void renderDrawConnecting() {
    spr.fillSprite(COL_BG);
    spr.setTextSize(1);
    spr.setTextColor(COL_ACCENT, COL_BG);
    const char* msg = "Connecting WiFi...";
    int w = strlen(msg) * 6;
    spr.setCursor((160 - w) / 2, 50);
    spr.print(msg);

    spr.setTextColor(COL_GRAY, COL_BG);
    int ssidW = strlen(WIFI_SSID) * 6;
    int ssidX = (160 - ssidW) / 2;
    spr.setCursor(ssidX > 0 ? ssidX : 0, 66);
    spr.print(WIFI_SSID);

    // Animated dots
    static uint8_t dots = 0;
    dots = (dots + 1) % 4;
    spr.setTextColor(COL_WHITE, COL_BG);
    spr.setCursor(75, 82);
    for (int i = 0; i < 3; i++) {
        spr.print(i < dots ? "." : " ");
    }

    checkScreenshot(spr);
    spr.pushSprite(0, 0);
}

// ══════════════════════════════════════════════════════════════
//  renderDrawOffline — Offline mode
// ══════════════════════════════════════════════════════════════
void renderDrawOffline() {
    spr.fillSprite(COL_BG);
    renderDrawStatusBar();

    spr.setTextSize(2);
    spr.setTextColor(COL_DANGER, COL_BG);
    const char* offTxt = "OFFLINE";
    int offW = strlen(offTxt) * 12;
    spr.setCursor((160 - offW) / 2, 25);
    spr.print(offTxt);

    spr.setTextSize(1);
    spr.setTextColor(COL_GRAY, COL_BG);
    const char* msg1 = "OpenSky unreachable";
    int msg1W = strlen(msg1) * 6;
    spr.setCursor((160 - msg1W) / 2, 50);
    spr.print(msg1);

    const char* msg2 = "Waiting for connection...";
    int msg2W = strlen(msg2) * 6;
    spr.setCursor((160 - msg2W) / 2, 62);
    spr.print(msg2);

    if (g_lastUpdate_ms > 0) {
        uint32_t ago = (millis() - g_lastUpdate_ms) / 1000;
        char buf[32];
        snprintf(buf, sizeof(buf), "Last data: %lu s ago", (unsigned long)ago);
        int bw = strlen(buf) * 6;
        spr.setTextColor(COL_DARK_GRAY, COL_BG);
        spr.setCursor((160 - bw) / 2, 78);
        spr.print(buf);
    } else {
        const char* nv = "No prior data";
        int nvW = strlen(nv) * 6;
        spr.setTextColor(COL_DARK_GRAY, COL_BG);
        spr.setCursor((160 - nvW) / 2, 78);
        spr.print(nv);
    }

    const char* retry = "Auto-retrying";
    int retryW = strlen(retry) * 6;
    spr.setTextColor(COL_DARK_GRAY, COL_BG);
    spr.setCursor((160 - retryW) / 2, 105);
    spr.print(retry);

    checkScreenshot(spr);
    spr.pushSprite(0, 0);
}

// ══════════════════════════════════════════════════════════════
//  renderOLEDInfo — same as renderOLEDSummary now
//  OLED shows general state even if aircraft is selected (TFT has details)
// ══════════════════════════════════════════════════════════════
void renderOLEDInfo() {
    renderOLEDSummary();
}

// ══════════════════════════════════════════════════════════════
//  renderOLEDSummary — OLED general status (always on)
//  Line 1: Time (HH:MM:SS) + connection badge
//  Line 2: Total aircraft count
//  Line 3: Zoom level
//  Line 4: Center coordinates
//  Line 5: Last update time
// ══════════════════════════════════════════════════════════════
void renderOLEDSummary() {
    if (!s_oledReady) return;

    oled.setPowerSave(0);
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tr);

    // Line 1 — Real time (NTP) + connection badge
    char timeBuf[24];
    netGetTime(timeBuf, sizeof(timeBuf));
    oled.drawStr(0, 9, timeBuf);

    // Connection badge — top right
    if (netIsConnected()) oled.drawBox(118, 2, 6, 6);
    else                  oled.drawFrame(118, 2, 6, 6);

    // Line 2 — Total aircraft
    char countBuf[24];
    snprintf(countBuf, sizeof(countBuf), "Aircraft: %u", g_aircraftCount);
    oled.drawStr(0, 22, countBuf);

    // Line 3 — Zoom
    char zoomBuf[24];
    snprintf(zoomBuf, sizeof(zoomBuf), "Zoom: %d", g_view.zoom);
    oled.drawStr(0, 35, zoomBuf);

    // Line 4 — Center coordinate
    char centerBuf[24];
    snprintf(centerBuf, sizeof(centerBuf), "%.2f,%.2f", g_view.center_lat, g_view.center_lon);
    oled.drawStr(0, 48, centerBuf);

    // Line 5 — Last update
    char updBuf[24];
    if (g_lastUpdate_ms > 0) {
        uint32_t ago = (millis() - g_lastUpdate_ms) / 1000;
        snprintf(updBuf, sizeof(updBuf), "Update: %lu s", (unsigned long)ago);
    } else {
        strcpy(updBuf, "Update: none");
    }
    oled.drawStr(0, 61, updBuf);

    oled.sendBuffer();
}

void renderOLEDOff() {
    if (s_oledReady) oled.setPowerSave(1);
}

void renderMarkOLEDReady() {
    s_oledReady = true;
}
