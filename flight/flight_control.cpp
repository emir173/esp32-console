// ============================================================
//  ESP32 FLIGHT TRACKER V1.1 — Control (Joystick + Buttons)
//
//  Controls:
//    Joystick ^v<> : Map panning
//    BTN_A : Zoom in (+)
//    BTN_B : Zoom out (-)
//    BTN_C : Select nearest aircraft / Deselect
//    BTN_D : (Not used in standalone port)
//    Joystick SW : Reset viewport (Default Center)
//
//  Viewport state is saved in NVS — restores on next boot
// ============================================================

#include "flight_internal.h"

static uint32_t s_lastPanMove = 0;
static bool     s_swPressed = false;

// ══════════════════════════════════════════════════════════════
//  ctrlLoadView — Read viewport + sound from NVS
// ══════════════════════════════════════════════════════════════
void ctrlLoadView() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    g_view.center_lat = prefs.getFloat(NVS_KEY_LAT, DEFAULT_CENTER_LAT);
    g_view.center_lon = prefs.getFloat(NVS_KEY_LON, DEFAULT_CENTER_LON);
    g_view.zoom       = prefs.getInt(NVS_KEY_ZOOM, DEFAULT_ZOOM);
    prefs.end();

    // Read global OS settings (Inherited from Launcher, default true)
    prefs.begin("os", true);
    g_soundEnabled = prefs.getBool("sound_en", true);
    prefs.end();

    // Boundary checks
    if (g_view.zoom < 1) g_view.zoom = 1;
    if (g_view.zoom > 10) g_view.zoom = 10;
    if (g_view.center_lat < -85) g_view.center_lat = -85;
    if (g_view.center_lat > 85) g_view.center_lat = 85;
    if (g_view.center_lon < -180) g_view.center_lon = -180;
    if (g_view.center_lon > 180) g_view.center_lon = 180;
}

// ══════════════════════════════════════════════════════════════
//  ctrlSaveView — Save viewport to NVS
// ══════════════════════════════════════════════════════════════
void ctrlSaveView() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putFloat(NVS_KEY_LAT, g_view.center_lat);
    prefs.putFloat(NVS_KEY_LON, g_view.center_lon);
    prefs.putInt(NVS_KEY_ZOOM, g_view.zoom);
    // Sound setting (sound_en) is not overwritten here
    prefs.end();
}

// ══════════════════════════════════════════════════════════════
//  ctrlHandleJoystick — Map Panning
//  Joystick X/Y → shift lat/lon center
//  Debounce: 100ms (smooth but not too fast)
//
//  Pan speed is scaled with zoom — smaller steps at higher zoom
// ══════════════════════════════════════════════════════════════
void ctrlHandleJoystick() {
    uint32_t now = millis();
    if (now - s_lastPanMove < 100) return;  // debounce

    int jx = analogRead(JOY_X) - g_joyCenterX;
    int jy = analogRead(JOY_Y) - g_joyCenterY;

    bool moved = false;
    // Pan step — based on zoom level
    // zoom 1: 30°/step, zoom 10: 0.05°/step
    float panStep = 30.0f / powf(2.0f, g_view.zoom - 1);
    if (panStep < 0.01f) panStep = 0.01f;

    // X axis → longitude (right=east positive, left=west negative)
    if (abs(jx) > 500) {
        g_view.center_lon += (jx > 0 ? panStep : -panStep);
        // Wrap around -180..180
        if (g_view.center_lon > 180) g_view.center_lon -= 360;
        if (g_view.center_lon < -180) g_view.center_lon += 360;
        moved = true;
    }

    // Y axis → latitude (up=north positive, down=south negative)
    // TFT Y grows downwards → analogRead up has lower value
    // But after subtracting center: up → jy negative
    if (abs(jy) > 500) {
        g_view.center_lat += (jy < 0 ? panStep : -panStep);
        if (g_view.center_lat > 85) g_view.center_lat = 85;
        if (g_view.center_lat < -85) g_view.center_lat = -85;
        moved = true;
    }

    if (moved) {
        s_lastPanMove = now;
        if (g_soundEnabled) tone(BUZZER, 392, 20);
    }

    // Joystick SW (press) — reset viewport
    if (digitalRead(JOY_SW) == LOW && !s_swPressed) {
        s_swPressed = true;
        g_view.center_lat = DEFAULT_CENTER_LAT;
        g_view.center_lon = DEFAULT_CENTER_LON;
        g_view.zoom = DEFAULT_ZOOM;
        g_selectedIdx = -1;
        if (g_soundEnabled) tone(BUZZER, 587, 40);
        ctrlSaveView();
    }
    if (digitalRead(JOY_SW) == HIGH) s_swPressed = false;
}

// ══════════════════════════════════════════════════════════════
//  ctrlHandleButtons — Button Inputs
// ══════════════════════════════════════════════════════════════
void ctrlHandleButtons() {
    // BTN_A: Zoom in
    if (!digitalRead(BTN_A)) {
        delay(50);
        if (!digitalRead(BTN_A)) {
            if (g_view.zoom < 10) {
                g_view.zoom++;
                if (g_soundEnabled) tone(BUZZER, 659, 30);
            }
            ctrlSaveView();
            while (!digitalRead(BTN_A)) delay(30);
        }
    }

    // BTN_B: Zoom out
    if (!digitalRead(BTN_B)) {
        delay(50);
        if (!digitalRead(BTN_B)) {
            if (g_view.zoom > 1) {
                g_view.zoom--;
                if (g_soundEnabled) tone(BUZZER, 392, 30);
            }
            ctrlSaveView();
            while (!digitalRead(BTN_B)) delay(30);
        }
    }

    // BTN_C: Select nearest aircraft / Deselect
    if (!digitalRead(BTN_C)) {
        delay(50);
        if (!digitalRead(BTN_C)) {
            if (g_selectedIdx >= 0) {
                // Already selected → deselect
                g_selectedIdx = -1;
                if (g_soundEnabled) tone(BUZZER, 400, 30);
            } else {
                // Select nearest
                ctrlSelectNearest();
                if (g_selectedIdx >= 0 && g_soundEnabled) {
                    tone(BUZZER, 784, 40);
                }
            }
            while (!digitalRead(BTN_C)) delay(30);
        }
    }

    // BTN_D: Return to OS Launcher
    if (!digitalRead(BTN_D)) {
        delay(50);
        if (!digitalRead(BTN_D)) {
            renderOLEDOff();
            ctrlSaveView();
            osReturnToOS(tft, g_soundEnabled);
        }
    }
}

// ══════════════════════════════════════════════════════════════
//  ctrlSelectNearest — Select aircraft closest to screen center
//  Mutex protected — prevents conflict with fetch (Core 0)
// ══════════════════════════════════════════════════════════════
void ctrlSelectNearest() {
    int centerX = SCR_W / 2;
    int centerY = SCR_H / 2;
    int bestIdx = -1;
    int bestDist = 99999;

    if (g_aircraftMutex != NULL) {
        xSemaphoreTake(g_aircraftMutex, portMAX_DELAY);
    }

    for (int i = 0; i < g_aircraftCount; i++) {
        if (!g_aircraft[i].on_screen) continue;
        int dx = g_aircraft[i].screen_x - centerX;
        int dy = g_aircraft[i].screen_y - centerY;
        int dist = dx * dx + dy * dy;
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }

    if (g_aircraftMutex != NULL) {
        xSemaphoreGive(g_aircraftMutex);
    }

    g_selectedIdx = bestIdx;
}
