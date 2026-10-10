// ============================================================
//  ESP32 FLIGHT TRACKER V1.1 — OpenSky Network Aircraft Tracking
//  ESP32-S3 N16R8 · 160x128 TFT + 128x64 OLED
//
//  Standalone Port. Fetches real aircraft data from OpenSky API.
//
//  Controls:
//    Joystick <> : Pan map (East/West)
//    Joystick ^v : Pan map (North/South)
//    Joystick SW : Reset viewport to default center
//    BTN_A : Zoom in (+)
//    BTN_B : Zoom out (-)
//    BTN_C : Select nearest aircraft / Deselect
//
//  Requirement: ArduinoJson library (Benoit Blanchon v7.x)
// ============================================================

#include "flight_internal.h"
#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

// ══════════════════════════════════════════════════════════════
//  GLOBAL DEFINITIONS
// ══════════════════════════════════════════════════════════════
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
uint16_t* g_psramMap = NULL;
uint16_t* g_psramMapWorld = NULL;
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

Aircraft  g_aircraft[MAX_AIRCRAFT];
int       g_aircraftCount = 0;
Viewport  g_view;
bool      g_soundEnabled = true;
int       g_selectedIdx = -1;
int       g_joyCenterX = 0;
int       g_joyCenterY = 0;
std::atomic<bool> g_dataValid{false};
uint32_t  g_lastUpdate_ms = 0;
std::atomic<bool> g_polling{false}; // UPDATING indicator during poll
SemaphoreHandle_t g_aircraftMutex = NULL;
TaskHandle_t g_fetchTaskHandle = NULL;

// ─── Poll timer ───
static uint32_t s_lastPoll = 0;
static bool     s_ready = false;

// ─── Render timer ───
static uint32_t s_lastRender = 0;

void netFetchTask(void *pvParameters) {
    while (1) {
        if (millis() - s_lastPoll > POLL_INTERVAL_MS && netIsConnected()) {
            g_polling = true;
            netFetchAircraft();
            g_polling = false;
            s_lastPoll = millis();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS); // Check every 100ms
    }
}

// Only publish a map pointer after the whole, correctly sized file is loaded.
static uint16_t* loadMap(const char* path, size_t expectedBytes) {
    File f = SD.open(path, FILE_READ);
    if (!f || f.size() != expectedBytes) {
        Serial.printf("[SETUP] %s missing/invalid (expected %u bytes, got %u)\n",
                      path, (unsigned)expectedBytes, (unsigned)(f ? f.size() : 0));
        return nullptr;
    }
    uint8_t* dst = (uint8_t*)heap_caps_malloc(expectedBytes,
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!dst) {
        Serial.printf("[SETUP] No PSRAM for %s\n", path);
        return nullptr;
    }
    size_t offset = 0;
    uint32_t started = millis();
    while (offset < expectedBytes) {
        size_t chunk = expectedBytes - offset;
        if (chunk > 65536) chunk = 65536;
        size_t got = f.read(dst + offset, chunk);
        if (!got) break;
        offset += got;
        vTaskDelay(1);
    }
    if (offset != expectedBytes) {
        Serial.printf("[SETUP] %s short read: %u/%u\n", path,
                      (unsigned)offset, (unsigned)expectedBytes);
        heap_caps_free(dst);
        return nullptr;
    }
    Serial.printf("[SETUP] %s loaded: %u bytes (%.2f s)\n", path,
                  (unsigned)offset, (millis() - started) / 1000.0);
    return (uint16_t*)dst;
}

// ══════════════════════════════════════════════════════════════
//  setup — Hardware init + WiFi + initial data
// ══════════════════════════════════════════════════════════════
void setup() {
    g_aircraftMutex = xSemaphoreCreateMutex();
    
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("==================================");
    Serial.println("  ESP32 FLIGHT TRACKER starting...");
    Serial.println("==================================");
    Serial.flush();
    Serial.printf("[DEBUG] Reset reason: %d | heap: %u | PSRAM: %u\n",
                  (int)esp_reset_reason(), ESP.getFreeHeap(), ESP.getFreePsram());
    if (!g_aircraftMutex) {
        Serial.println("[SETUP] FATAL: aircraft mutex allocation failed");
        return;
    }

    // ─── Pin modes ───
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP);
    pinMode(BTN_D, INPUT_PULLUP);
    pinMode(JOY_SW, INPUT_PULLUP);
    osInitBuzzer();
    pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
    
    // ─── I2C (OLED) ───
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
    delay(20);

    // ─── Start WiFi NOW (non-blocking) — during SD card loading
    //    It will connect in the background. WiFi uses radio, SD uses SPI
    //    → No conflicts, they run in parallel!
    Serial.println("[SETUP] WiFi starting in background...");
    netBeginWiFi();

    // SD Card Init & Loading into PSRAM (Retries up to 3 times)
    bool sdOk = false;
    for (int i = 0; i < 3; i++) {
        if (SD.begin(SD_CS, SPI, 40000000)) {
            sdOk = true;
            break;
        }
        delay(200);
    }

    if (sdOk) {
        Serial.println("[SETUP] SD Card OK");

        g_psramMap = loadMap("/regional.bin", (size_t)REG_MAP_W * REG_MAP_H * sizeof(uint16_t));
        g_psramMapWorld = loadMap("/world.bin", (size_t)WORLD_MAP_W * WORLD_MAP_H * sizeof(uint16_t));
        SD.end(); 
        pinMode(SD_CS, OUTPUT);
        digitalWrite(SD_CS, HIGH);
    } else {
        Serial.println("[SETUP] SD Card FAILED (no maps)");
    }
    
    Serial.println("[DEBUG] before tft.init()...");
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
    
    // Create Sprite (160x128 = 40KB RAM)
    Serial.println("[DEBUG] Creating sprite...");
    void* ptr = spr.createSprite(160, 128);
    if (!ptr) {
        Serial.println("[SETUP] FATAL: Insufficient RAM for sprite");
        return;
    }
    spr.setSwapBytes(true); 
    
    Serial.println("[SETUP] TFT OK");
    Serial.flush();

    initDevTools(tft, false); // Initialize DevTools (SD is already initialized)
    setScreenshotMode(SCR_BGR_NOSWAP); // RGB_FIX uses BGR_NOSWAP

    // ─── Color palette ───
    mapSetupColors();

    // ─── Read viewport + sound from NVS ───
    ctrlLoadView();
    Serial.printf("[SETUP] Viewport: lat=%.2f lon=%.2f zoom=%d\n",
                  g_view.center_lat, g_view.center_lon, g_view.zoom);

    // ─── Joystick calibration ───
    delay(50);
    g_joyCenterX = analogRead(JOY_X);
    delay(10);
    g_joyCenterY = analogRead(JOY_Y);

    // ─── Show map IMMEDIATELY if available (no planes), else connecting screen ───
    if (g_psramMap || g_psramMapWorld) {
        renderDrawAll();  // Map + grid, no aircraft
        Serial.println("[SETUP] Map displayed (waiting for aircraft)");
    } else {
        renderDrawConnecting();
    }

    // ─── WiFi — wait for it (started in background earlier) ───
    bool wifiOk = netWaitWiFi();
    Serial.printf("[SETUP] WiFi result: %s\n", wifiOk ? "CONNECTED" : "FAILED");

    // ─── Init OLED ───
    renderSetup();

    // ─── First poll ───
    if (netIsConnected()) {
        Serial.println("[SETUP] First OpenSky poll starting...");
        if (netFetchAircraft()) {
            Serial.println("[SETUP] First poll SUCCESSFUL");
        } else {
            Serial.println("[SETUP] First poll FAILED");
        }
    } else {
        Serial.println("[SETUP] No WiFi, poll skipped");
    }

    // ─── First screen ───
    if (g_dataValid) {
        renderDrawAll();  // Map + aircraft
    } else if (!g_psramMap && !g_psramMapWorld) {
        renderDrawOffline();  // Offline screen if no map
    }
    Serial.printf("[SETUP] COMPLETE (%.1f s) - loop starting\n", millis() / 1000.0);
    Serial.println("==================================");

    s_lastPoll = millis();
    s_lastRender = millis();

    // Start background fetch task on Core 0
    BaseType_t taskResult = xTaskCreatePinnedToCore(
        netFetchTask,
        "NetFetchTask",
        16384, // WiFiClientSecure requires large stack!
        NULL,
        1,    // Low priority
        &g_fetchTaskHandle,
        0     // Core 0 (Network operations)
    );
    if (taskResult != pdPASS) {
        Serial.println("[SETUP] ERROR: NetFetchTask creation failed; no automatic polls");
    }
    s_ready = true;
}

// ══════════════════════════════════════════════════════════════
//  loop — Main loop
//  1. WiFi maintain
//  2. Joystick + Button inputs
//  3. Render (timed — 50ms for smooth joystick)
//  4. Update OLED
// ══════════════════════════════════════════════════════════════
void loop() {
    if (!s_ready) {
        delay(100);
        return;
    }
    uint32_t now = millis();

    // ─── 1. WiFi maintain ───
    netMaintain();

    // ─── 2. Joystick + Button inputs ───
    ctrlHandleJoystick();
    ctrlHandleButtons();

    // ─── 3. TFT render (If map is loaded) ───
    if (now - s_lastRender >= RENDER_INTERVAL_MS) {
        // A missing SD/map must not freeze the screen or hide valid aircraft.
        if (g_dataValid || g_psramMap || g_psramMapWorld) renderDrawAll();
        else renderDrawOffline();
        s_lastRender = now;
    }

    // ─── 4. OLED update (every 800ms) ───
    static uint32_t lastOLED = 0;
    if (now - lastOLED > 800) {
        lastOLED = now;
        renderOLEDSummary();
    }

    // Frame limiting
    delay(10);
}
