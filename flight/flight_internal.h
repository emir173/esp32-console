#pragma once
// ============================================================
//  ESP32 FLIGHT TRACKER V1.1 — Shared State (Internal Header)
//
//  All .cpp files include this header.
//  Global variables are defined in the .ino file,
//  only extern declarations and data structures are here.
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <TFT_eSPI.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include "flight_config.h"
#include "hardware_config.h"
#include "../GameBase.h"
#include "../dev_tools.h"

// ─── RGB_FIX (BGR correction for the TFT) ───
extern TFT_eSPI tft;
extern TFT_eSprite spr;
extern uint16_t* g_psramMap;          // regional (2048x1024, High Res)
extern uint16_t* g_psramMapWorld;     // world (512x256, Low Res)
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C oled;

inline uint16_t RGB_FIX(uint8_t r, uint8_t g, uint8_t b) {
    return tft.color565(b, g, r);  // BGR swap
}

// ══════════════════════════════════════════════════════════════
//  DATA STRUCTURES
// ══════════════════════════════════════════════════════════════

// Data for a single aircraft — Extracted from OpenSky states array
// OpenSky states[i] format:
//   [0] icao24, [1] callsign, [2] origin_country,
//   [5] longitude, [6] latitude, [7] baro_altitude,
//   [8] on_ground, [9] velocity, [10] true_track, [13] geo_altitude
struct Aircraft {
    char     callsign[10];   // e.g. "THY3BE" (trimmed)
    char     icao24[8];      // Transponder address
    char     country[16];    // Country of origin
    float    lon;            // Longitude (-180..180)
    float    lat;            // Latitude (-90..90)
    float    altitude;       // Altitude (meters)
    float    velocity;       // Velocity (m/s)
    float    heading;        // Heading (0-359°, north=0)
    bool     on_ground;      // Is it on the ground?
    int      screen_x;       // X pixel on screen (calculated during render)
    int      screen_y;       // Y pixel on screen
    bool     on_screen;      // Is it currently visible on screen?
    // ─── Interpolation Data ───
    float    interp_lat;     // Interpolated latitude
    float    interp_lon;     // Interpolated longitude
    uint32_t last_update_ms; // Last OpenSky data timestamp (millis)
};

// Map viewport state — navigated by user via joystick
struct Viewport {
    float center_lat;        // Center latitude
    float center_lon;        // Center longitude
    int   zoom;              // Zoom level (1-10)
};

// ══════════════════════════════════════════════════════════════
//  GLOBAL STATE (Defined in .ino, extern here)
// ══════════════════════════════════════════════════════════════

extern SemaphoreHandle_t g_aircraftMutex; // Mutex for dual-core safe access
extern Aircraft  g_aircraft[];     // Aircraft array (MAX_AIRCRAFT)
extern int       g_aircraftCount;  // Number of active aircraft
extern Viewport  g_view;           // Current viewport
extern bool      g_soundEnabled;   // Is sound enabled (from NVS)
extern int       g_selectedIdx;    // Selected aircraft index (-1 = none)
extern int       g_joyCenterX;     // Joystick calibration X
extern int       g_joyCenterY;     // Joystick calibration Y
extern bool      g_dataValid;      // Has data been received from OpenSky?
extern uint32_t  g_lastUpdate_ms;  // Last successful poll time
extern bool      g_polling;        // Is currently polling? (UPDATING indicator)

// ─── Color Palette (Assigned in flight_map.cpp setupColors()) ───
extern uint16_t COL_BG;
extern uint16_t COL_OCEAN;
extern uint16_t COL_LAND;
extern uint16_t COL_BORDER;
extern uint16_t COL_ACCENT;
extern uint16_t COL_WHITE;
extern uint16_t COL_GRAY;
extern uint16_t COL_DARK_GRAY;
extern uint16_t COL_BLACK;
extern uint16_t COL_JET;
extern uint16_t COL_PROP;
extern uint16_t COL_GROUND;
extern uint16_t COL_SELECTED;
extern uint16_t COL_DANGER;

// ══════════════════════════════════════════════════════════════
//  MODULE APIs
// ══════════════════════════════════════════════════════════════

// flight_net.cpp
bool netConnectWiFi();
void netBeginWiFi();                   // Non-blocking WiFi start (parallel SD loading)
bool netWaitWiFi();                    // Wait for WiFi to connect (blocking)
void netBeginNTP();                    // Non-blocking NTP start
bool netIsConnected();
void netMaintain();
bool netFetchAircraft();              // Fetch and parse aircraft from OpenSky
void netGetTime(char *buf, int len);  // Real time "HH:MM:SS"
bool netIsNTPSynced();                // Is NTP synced?

// flight_map.cpp
void mapSetupColors();
void mapDrawBackground();             // Ocean + continents (simple)
void mapDrawGrid();                   // Lat/Lon grid
void latLonToScreen(float lat, float lon, int &sx, int &sy);  // Projection
bool screenToLatLon(int sx, int sy, float &lat, float &lon);  // Inverse
void mapGetViewBounds(float &latMin, float &latMax, float &lonMin, float &lonMax);

// flight_render.cpp
void renderSetup();
void renderDrawAll();                 // TFT: map + aircraft + UI
void renderDrawAircraft(const Aircraft &a, bool selected);
void renderDrawStatusBar();
void renderDrawSelectionBox();        // Highlight for selected aircraft
void renderDrawConnecting();
void renderDrawOffline();
void renderOLEDInfo();                // OLED: selected aircraft details
void renderOLEDSummary();             // OLED: total aircraft count
void renderOLEDOff();
void renderMarkOLEDReady();

// flight_control.cpp
void ctrlLoadView();                  // Read viewport + sound from NVS
void ctrlSaveView();                  // Save viewport to NVS
void ctrlHandleJoystick();            // Handle map panning via joystick
void ctrlHandleButtons();             // Process BTN_A/B/C
void ctrlSelectNearest();             // Select nearest aircraft on screen

// ─── Helper: trim callsign (remove whitespaces) ───
inline void trimCallsign(char *dst, const char *src, int len) {
    int j = 0;
    for (int i = 0; src[i] && j < len - 1; i++) {
        if (src[i] != ' ') dst[j++] = src[i];
    }
    dst[j] = '\0';
}
