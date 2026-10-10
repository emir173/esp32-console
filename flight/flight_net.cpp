// ============================================================
//  ESP32 FLIGHT TRACKER V1.1 — Network Client (WiFi + OpenSky HTTPS)
//
//  Responsibilities:
//    - Connect / Reconnect to WiFi
//    - Send HTTPS request to OpenSky Network API
//    - Parse JSON and populate g_aircraft[] array
//
//  OpenSky API:
//    GET https://opensky-network.org/api/states/all?lamin=..&lomin=..&lamax=..&lomax=..
//    Response: {"time":..., "states":[[icao,callsign,country,...,lon,lat,...], ...]}
//
//  We use WiFiClientSecure for HTTPS.
//  SSL certificate validation is disabled (setInsecure)
//  — acceptable since the device has no RTC to verify dates and data is public.
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <time.h>
#include "flight_internal.h"

// volatile: WiFi event callback runs in a different task, shared with main thread
static std::atomic<bool> s_wifiConnected{false};
static std::atomic<bool> s_wifiEventDisc{false};
static std::atomic<bool> s_wifiEventConnected{false};
static bool s_ntpRequested = false;
static bool s_ntpSynced = false;
static uint32_t s_lastReconnectAttempt = 0;

// ─── Static PSRAM buffers — allocate once, reuse ───
// Doing malloc/free on every poll causes heap fragmentation → crashes
static char *s_netBuf = nullptr;     // 500KB HTTP response buffer
static bool s_buffersAllocated = false;
// s_cleanBuf REMOVED — chunked encoding bypassed via HTTP/1.0

static bool ensureBuffers() {
    if (s_buffersAllocated) return true;
    s_netBuf = (char*)heap_caps_malloc(500000, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_netBuf) {
        Serial.println("[NET] PSRAM buffer allocation FAILED!");
        return false;
    }
    s_buffersAllocated = true;
    Serial.println("[NET] PSRAM buffer allocated (500KB)");
    return true;
}

// WiFi event callback — MUST BE AS SHORT AS POSSIBLE! Just set flag, return immediately.
// If triggered during SD SPI read, long processing could corrupt SPI FIFO.
static void wifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            s_wifiEventDisc = true;
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            s_wifiEventConnected = true;
            break;
        default:
            break;
    }
}

// Start NTP non-blocking — configTime syncs in background
void netBeginNTP() {
    if (s_ntpRequested) return;
    configTime(NTP_TZ_OFFSET * 3600, NTP_TZ_DST * 3600, NTP_SERVER1, NTP_SERVER2);
    s_ntpRequested = true;
    Serial.println("[NET] NTP sync started (background)");
}

// Process event flags — called in loop or during wait
static void netProcessWifiFlags() {
    if (s_wifiEventConnected.exchange(false)) {
        s_wifiConnected = true;
    }
    if (s_wifiEventDisc.exchange(false)) {
        s_wifiConnected = false;
    }
}

// ══════════════════════════════════════════════════════════════
//  netBeginWiFi — Start WiFi (non-blocking, returns immediately)
//  Connects in background during SD card loading
// ══════════════════════════════════════════════════════════════
void netBeginWiFi() {
    if (s_wifiConnected && WiFi.status() == WL_CONNECTED) return;

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.disconnect(false, false);
    delay(50);
    WiFi.setScanMethod(WIFI_FAST_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    WiFi.setHostname("eos-flight");
    WiFi.onEvent(wifiEvent);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("[NET] WiFi.begin() (background)");
}

// ══════════════════════════════════════════════════════════════
//  netWaitWiFi — Wait until WiFi connects (blocking, with timeout)
//  Polls event flags — callback is thread-safe
// ══════════════════════════════════════════════════════════════
bool netWaitWiFi() {
    if (s_wifiConnected && WiFi.status() == WL_CONNECTED) return true;

    uint32_t t0 = millis();
    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED) {
        // Check event flags (from callback)
        netProcessWifiFlags();
        if (s_wifiConnected) {
            Serial.print("[NET] WiFi connected. IP: ");
            Serial.print(WiFi.localIP());
            Serial.print(" | Ch: ");
            Serial.print(WiFi.channel());
            Serial.print(" | RSSI: ");
            Serial.print(WiFi.RSSI());
            Serial.print(" dBm | Wait: ");
            Serial.print((millis() - t0) / 1000.0, 2);
            Serial.println(" s");
            // Start NTP non-blocking — configTime ~200ms won't block much
            netBeginNTP();
            return true;
        }
        if (millis() - startMs > WIFI_TIMEOUT_MS) {
            s_wifiConnected = false;
            Serial.print("[NET] TIMEOUT! ");
            Serial.print((millis() - startMs) / 1000.0, 2);
            Serial.println(" s");
            return false;
        }
        delay(50);
    }
    s_wifiConnected = true;
    Serial.print("[NET] WiFi connected. IP: ");
    Serial.print(WiFi.localIP());
    Serial.print(" | RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.print(" dBm | Wait: ");
    Serial.print((millis() - t0) / 1000.0, 2);
    Serial.println(" s");
    netBeginNTP();
    return true;
}

// ══════════════════════════════════════════════════════════════
//  netConnectWiFi — Connect to WiFi (blocking, legacy wrapper)
//  = netBeginWiFi() + netWaitWiFi()
// ══════════════════════════════════════════════════════════════
bool netConnectWiFi() {
    netBeginWiFi();
    return netWaitWiFi();
}

// ══════════════════════════════════════════════════════════════
//  netGetTime — Return real time in "HH:MM:SS" format
//  Returns "00:00:00" if NTP is not synced
// ══════════════════════════════════════════════════════════════
void netGetTime(char *buf, int len) {
    time_t now = time(nullptr);
    if (now < 1700000000) {  // Before 2023 = not synced
        s_ntpSynced = false;
        snprintf(buf, len, "--:--:--");
        return;
    }
    s_ntpSynced = true;
    struct tm local;
    localtime_r(&now, &local);
    snprintf(buf, len, "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
}

bool netIsNTPSynced() {
    return time(nullptr) >= 1700000000;
}

bool netIsConnected() {
    return s_wifiConnected && WiFi.status() == WL_CONNECTED;
}

// ══════════════════════════════════════════════════════════════
//  netMaintain — Called in loop, retry if connection dropped
//  Also processes event flags
// ══════════════════════════════════════════════════════════════
void netMaintain() {
    // Process event flags (from callback)
    netProcessWifiFlags();

    if (WiFi.status() == WL_CONNECTED) {
        if (!s_wifiConnected) s_wifiConnected = true;
        netBeginNTP();
        return;
    }
    s_wifiConnected = false;
    
    uint32_t now = millis();
    if (now - s_lastReconnectAttempt < WIFI_RETRY_MS) return;
    s_lastReconnectAttempt = now;
    
    WiFi.disconnect(false, false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ══════════════════════════════════════════════════════════════
//  netFetchAircraft — Fetch + parse aircraft from OpenSky
//
//  Flow:
//    1. Calculate bbox from current viewport (visible region + margin)
//    2. Send HTTPS GET request: /api/states/all?lamin=..&lomin=..&lamax=..&lomax=..
//    3. Parse JSON — convert each aircraft in states array to Aircraft struct
//    4. Populate g_aircraft[] array, update g_aircraftCount
//
//  Return: true = success, false = error
// ══════════════════════════════════════════════════════════════
// OpenSky token and rate-limit state belongs to the fetch task.
static String s_accessToken;
static uint32_t s_tokenIssuedMs = 0, s_tokenLifetimeMs = 0;
static uint32_t s_retryStartedMs = 0, s_retryDelayMs = 0;
static const char* RATE_HEADERS[] = {"X-Rate-Limit-Retry-After-Seconds", "Retry-After", "Transfer-Encoding"};

class PsramJsonAllocator : public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    void deallocate(void* ptr) override { heap_caps_free(ptr); }
    void* reallocate(void* ptr, size_t size) override {
        return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
};
static PsramJsonAllocator s_jsonAllocator;

static void recordRateLimit(HTTPClient& http) {
    String retry = http.header(RATE_HEADERS[0]);
    if (retry.isEmpty()) retry = http.header(RATE_HEADERS[1]);
    uint32_t seconds = strtoul(retry.c_str(), nullptr, 10);
    if (!seconds) seconds = 300;
    if (seconds > 86400) seconds = 86400;
    s_retryStartedMs = millis();
    s_retryDelayMs = seconds * 1000;
    Serial.printf("[NET] HTTP 429: wait %u s before retry\n", (unsigned)seconds);
}

static String formEncode(const char* value) {
    static const char hexDigits[] = "0123456789ABCDEF";
    String encoded;
    while (*value) {
        uint8_t c = (uint8_t)*value++;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += (char)c;
        } else {
            encoded += '%'; encoded += hexDigits[c >> 4]; encoded += hexDigits[c & 15];
        }
    }
    return encoded;
}

static bool ensureToken() {
    if (!s_accessToken.isEmpty() && millis() - s_tokenIssuedMs < s_tokenLifetimeMs) return true;
    s_accessToken = "";
    WiFiClientSecure client;
    client.setInsecure(); // Preserve the existing TLS policy.
    HTTPClient http;
    http.setTimeout(15000);
    http.setConnectTimeout(10000);
    if (!http.begin(client, OPENSKY_TOKEN_URL)) return false;
    http.collectHeaders(RATE_HEADERS, 3);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String body = "grant_type=client_credentials&client_id=" + formEncode(OPENSKY_CLIENT_ID)
                + "&client_secret=" + formEncode(OPENSKY_CLIENT_SECRET);
    int code = http.POST(body);
    if (code != HTTP_CODE_OK) {
        if (code == 429) recordRateLimit(http);
        Serial.printf("[NET] OAuth token request failed: %d\n", code);
        http.end();
        return false;
    }
    // HTTPClient handles chunking for the small token response.
    String response = http.getString();
    http.end();
    JsonDocument doc(&s_jsonAllocator);
    DeserializationError err = deserializeJson(doc, response);
    const char* token = doc["access_token"] | "";
    uint32_t seconds = doc["expires_in"] | 0U;
    if (err || !token[0] || seconds <= 30) {
        Serial.println("[NET] Invalid OAuth token response");
        return false;
    }
    if (seconds > 86400) seconds = 86400;
    s_accessToken = token;
    s_tokenIssuedMs = millis();
    s_tokenLifetimeMs = (seconds - 30) * 1000;
    Serial.println("[NET] OAuth token ready"); // Never log credentials/token.
    return true;
}

// A pause in network delivery is not the end of the response.
static bool readResponse(HTTPClient& http, size_t& totalRead) {
    constexpr size_t BUF_SIZE = 500000;
    int contentLen = http.getSize();
    if (contentLen >= (int)BUF_SIZE || contentLen == 0 ||
        !http.header("Transfer-Encoding").isEmpty()) {
        Serial.println("[NET] Unsupported/oversized HTTP body");
        return false;
    }
    NetworkClient* stream = http.getStreamPtr();
    uint32_t started = millis(), lastData = started;
    totalRead = 0;
    while (millis() - started < 30000) {
        if (contentLen >= 0 && totalRead == (size_t)contentLen) break;
        size_t available = stream->available();
        if (!available) {
            if (!stream->connected()) break;
            if (millis() - lastData >= 15000) break;
            delay(1);
            continue;
        }
        if (totalRead == BUF_SIZE - 1) {
            Serial.println("[NET] HTTP body exceeds buffer");
            return false;
        }
        size_t remaining = BUF_SIZE - 1 - totalRead;
        if (contentLen >= 0 && remaining > (size_t)contentLen - totalRead)
            remaining = (size_t)contentLen - totalRead;
        if (available > remaining) available = remaining;
        int got = stream->read((uint8_t*)s_netBuf + totalRead, available);
        if (got > 0) { totalRead += got; lastData = millis(); }
        else delay(1);
    }
    s_netBuf[totalRead] = '\0';
    bool complete = contentLen >= 0 ? totalRead == (size_t)contentLen
                                   : !stream->connected() && !stream->available();
    if (!complete || !totalRead) {
        Serial.printf("[NET] Incomplete HTTP body: %u/%d bytes\n", (unsigned)totalRead, contentLen);
        return false;
    }
    return true;
}

static bool parseAircraft(size_t bodySize) {
    JsonDocument filter;
    // Preserve positional indices in each state vector; array filters use [0]
    // as a template for every element, so a per-index field mask is unsafe.
    filter["states"] = true;
    JsonDocument doc(&s_jsonAllocator);
    DeserializationError err = deserializeJson(doc, s_netBuf, bodySize,
                                               DeserializationOption::Filter(filter));
    if (err) {
        Serial.printf("[NET] JSON rejected: %s\n", err.c_str());
        return false;
    }
    JsonVariant states = doc["states"];
    if (states.isUnbound() || (!states.isNull() && !states.is<JsonArray>())) {
        Serial.println("[NET] Missing/invalid states array");
        return false;
    }
    static Aircraft temp[MAX_AIRCRAFT];
    int count = 0;
    for (JsonVariant row : states.as<JsonArray>()) {
        if (!row.is<JsonArray>() || row.size() < 11) {
            Serial.println("[NET] Invalid state vector");
            return false;
        }
        // A missing position is different from a valid zero coordinate.
        if (row[5].isNull() || row[6].isNull()) continue;
        if (!row[5].is<float>() || !row[6].is<float>()) return false;
        float lon = row[5].as<float>(), lat = row[6].as<float>();
        if (!isfinite(lon) || !isfinite(lat) || lon < -180 || lon > 180 || lat < -90 || lat > 90) continue;
        const char* icao = row[0] | "";
        if (!icao[0]) continue;
        if (count >= MAX_AIRCRAFT) continue;
        Aircraft& a = temp[count++];
        a = Aircraft{};
        snprintf(a.icao24, sizeof(a.icao24), "%s", icao);
        trimCallsign(a.callsign, row[1] | "", sizeof(a.callsign));
        if (!a.callsign[0]) strcpy(a.callsign, "----");
        snprintf(a.country, sizeof(a.country), "%s", row[2] | "");
        a.lon = a.interp_lon = lon;
        a.lat = a.interp_lat = lat;
        a.altitude = row[7] | 0.0f;
        a.on_ground = row[8] | false;
        a.velocity = row[9] | 0.0f;
        a.heading = row[10] | 0.0f;
        a.last_update_ms = millis();
    }
    if (!g_aircraftMutex || xSemaphoreTake(g_aircraftMutex, portMAX_DELAY) != pdTRUE) return false;
    char selectedIcao[8] = {};
    if (g_selectedIdx >= 0 && g_selectedIdx < g_aircraftCount)
        strcpy(selectedIcao, g_aircraft[g_selectedIdx].icao24);
    memcpy(g_aircraft, temp, count * sizeof(Aircraft));
    g_aircraftCount = count;
    g_selectedIdx = -1;
    for (int i = 0; i < count; i++) {
        if (selectedIcao[0] && strcmp(g_aircraft[i].icao24, selectedIcao) == 0) {
            g_selectedIdx = i;
            break;
        }
    }
    g_lastUpdate_ms = millis();
    g_dataValid = true;
    xSemaphoreGive(g_aircraftMutex);
    Serial.printf("[NET] %d aircraft received | free PSRAM: %u\n", count, ESP.getFreePsram());
    return true;
}

bool netFetchAircraft() {
    if (!netIsConnected()) return false;
    if (s_retryDelayMs && millis() - s_retryStartedMs < s_retryDelayMs) return false;
    s_retryDelayMs = 0;
    bool useAuth = OPENSKY_CLIENT_ID[0] && OPENSKY_CLIENT_SECRET[0];
    if ((OPENSKY_CLIENT_ID[0] != 0) != (OPENSKY_CLIENT_SECRET[0] != 0)) {
        Serial.println("[NET] Configure both OpenSky client ID and secret");
        return false;
    }
    if (!ensureBuffers()) return false;
    char path[256];
    snprintf(path, sizeof(path), "%s?lamin=%.4f&lomin=%.4f&lamax=%.4f&lomax=%.4f",
             OPENSKY_PATH, DATA_BBOX_LAT_MIN, DATA_BBOX_LON_MIN, DATA_BBOX_LAT_MAX, DATA_BBOX_LON_MAX);
    // Refresh once on 401, with a bounded retry.
    for (int attempt = 0; attempt < 2; attempt++) {
        if (useAuth && !ensureToken()) return false;
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setTimeout(15000);
        http.setConnectTimeout(10000);
        if (!http.begin(client, OPENSKY_HOST, OPENSKY_PORT, path, true)) return false;
        http.useHTTP10(true);
        http.collectHeaders(RATE_HEADERS, 3);
        if (useAuth) http.addHeader("Authorization", "Bearer " + s_accessToken);
        int code = http.GET();
        Serial.printf("[NET] OpenSky HTTP: %d\n", code);
        if (code == 401 && useAuth) {
            s_accessToken = "";
            http.end();
            continue;
        }
        if (code != HTTP_CODE_OK) {
            if (code == 429) recordRateLimit(http);
            http.end();
            return false;
        }
        size_t bodySize = 0;
        bool complete = readResponse(http, bodySize);
        http.end();
        if (!complete) return false;
        return parseAircraft(bodySize);
    }
    return false;
}
