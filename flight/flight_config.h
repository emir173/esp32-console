#pragma once
// ============================================================
//  ESP32 FLIGHT TRACKER V1.1 — Configuration File
//
//  Uses the OpenSky Network API — free, no API key required for basic use.
//  Fetches real aircraft data: callsign, lat/lon, altitude, velocity, heading.
//
//  LIBRARY REQUIREMENTS:
//    ArduinoJson (Benoit Blanchon) v7.x
//    TFT_eSPI
//    U8g2
//
//  OPENSKY API RATE LIMIT:
//    - Anonymous: 100 requests/day
//    - Registered (Free): 4000 requests/day
//    Register here: https://opensky-network.org/index.php?option=com_users&view=registration
//
//  HOW TO RUN:
//    1. Fill in your WiFi credentials below.
//    2. Fill in your OpenSky username/password if you registered.
//    3. Copy the map files (regional.bin, world.bin) to your SD Card.
//    4. Compile and upload to your ESP32-S3!
// ============================================================

// ─── WiFi Configuration ───
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

#define WIFI_TIMEOUT_MS 15000
#define WIFI_RETRY_MS   5000

// ─── OpenSky Network API ───
// Free endpoint — no API key required
#define OPENSKY_HOST     "opensky-network.org"
#define OPENSKY_PORT     443
#define OPENSKY_PATH     "/api/states/all"
// We use HTTPS (port 443) — SSL is required
// OpenSky Account (4000 credits/day) — Basic Auth
// clientId = username, clientSecret = password
#define OPENSKY_USER     "YOUR_OPENSKY_USERNAME" // (leave empty if not registered)
#define OPENSKY_PASS     "YOUR_OPENSKY_PASSWORD"

// ─── Polling Interval ───
// How often to fetch data from OpenSky (ms)
// Registered (4000 credits/day): 120 sec = 720/day (very safe)
// OpenSky also has hourly limits (~400/hour), 120 sec = 30/hour is safe
#define POLL_INTERVAL_MS 120000    // 2 minutes — safe limit
// Map rendering interval (ms) — keeps joystick panning smooth
// 50ms is aggressive → 100ms (10 FPS) is enough since flight data changes slowly
#define RENDER_INTERVAL_MS 100

// ─── Default Map Region ───
// Turkey + Europe region (Centered around Istanbul)
// OpenSky bbox parameters: lamin, lomin, lamax, lomax (lat/lon)
// Turkey: lat 36-42, lon 26-45
#define DEFAULT_CENTER_LAT  39.5f   // Default center latitude
#define DEFAULT_CENTER_LON  32.0f   // Default center longitude
#define DEFAULT_ZOOM        6       // 1=world, 10=city (6 is good for country level)

// ─── Data Region (BBox to fetch from API) ───
// FIXED region — independent of viewport, covers Turkey + surroundings
// Even if user zooms in/out, we fetch the same data but only render what's visible
// This prevents aircraft count from drastically changing on zoom
#define DATA_BBOX_LAT_MIN  33.0f   // South Turkey + Eastern Med
#define DATA_BBOX_LAT_MAX  44.0f   // North Turkey + Black Sea
#define DATA_BBOX_LON_MIN  23.0f   // West Turkey + Aegean
#define DATA_BBOX_LON_MAX  47.0f   // East Turkey + Caucasus
#define MAX_AIRCRAFT       200     // 200 aircraft (internal RAM is sufficient)

// ─── Screen Dimensions ───
#define SCR_W 160
#define SCR_H 128

// ─── Hardware Pins ───
// (Comes from hardware_config.h)
// TFT_CS, SD_CS, JOY_X, JOY_Y, JOY_SW, BTN_A..D, BUZZER, I2C_SDA/SCL

// ─── NVS Keys ───
// Last map position/zoom is saved — so device starts where you left off
#define NVS_NAMESPACE     "flight"
#define NVS_KEY_LAT       "lat"
#define NVS_KEY_LON       "lon"
#define NVS_KEY_ZOOM      "zoom"
#define NVS_KEY_SOUND     "snd"

// ─── NTP (Real Time Clock) ───
// Syncs time from NTP server upon WiFi connection
// ESP32 doesn't have an RTC battery, so it relies on WiFi
#define NTP_SERVER1     "pool.ntp.org"
#define NTP_SERVER2     "time.google.com"
#define NTP_TZ_OFFSET   3       // Turkey UTC+3 (hours)
#define NTP_TZ_DST      0       // Daylight saving time (0 = no)

// ─── Version ───
#define FLIGHT_VERSION "v1.1"
