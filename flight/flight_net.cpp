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
#include <esp_heap_caps.h>
#include <time.h>
#include "flight_internal.h"

// volatile: WiFi event callback runs in a different task, shared with main thread
static volatile bool s_wifiConnected = false;
static volatile bool s_wifiEventDisc = false;      // disconnect flag (from callback)
static volatile bool s_wifiEventConnected = false; // connect flag (from callback)
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
    // 32-bit aligned: memcpy uses word-aligned optimization
    s_netBuf = (char*)heap_caps_malloc(500000, MALLOC_CAP_SPIRAM | MALLOC_CAP_32BIT);
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
    if (s_wifiEventConnected) {
        s_wifiEventConnected = false;
        s_wifiConnected = true;
    }
    if (s_wifiEventDisc) {
        s_wifiEventDisc = false;
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
    WiFi.setSleep(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.disconnect(true, true);
    delay(50);
    WiFi.setScanMethod(WIFI_FAST_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    WiFi.setHostname("eos-flight");
    WiFi.mode(WIFI_STA);
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
    struct tm *t = localtime(&now);
    snprintf(buf, len, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
}

bool netIsNTPSynced() {
    return s_ntpSynced;
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
        return;
    }
    s_wifiConnected = false;
    
    uint32_t now = millis();
    if (now - s_lastReconnectAttempt < WIFI_RETRY_MS) return;
    s_lastReconnectAttempt = now;
    
    WiFi.disconnect(true, false);
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
bool netFetchAircraft() {
    if (!netIsConnected()) {
        Serial.println("[NET] No WiFi, fetch skipped");
        return false;
    }

    // ─── 1. Bbox — FIXED Turkey region (independent of viewport) ───
    // Even if user zooms/pans, fetches same data — consistent aircraft count
    float latMin = DATA_BBOX_LAT_MIN;
    float latMax = DATA_BBOX_LAT_MAX;
    float lonMin = DATA_BBOX_LON_MIN;
    float lonMax = DATA_BBOX_LON_MAX;

    // Build URL — bbox parameters
    char path[256];
    snprintf(path, sizeof(path),
             "%s?lamin=%.4f&lomin=%.4f&lamax=%.4f&lomax=%.4f",
             OPENSKY_PATH, latMin, lonMin, latMax, lonMax);

    Serial.print("[NET] OpenSky request: ");
    Serial.println(path);

    // ─── 2. HTTPS GET — WiFiClientSecure ───
    WiFiClientSecure client;
    client.setInsecure();  // Cert validation off (no RTC)
    client.setTimeout(15000);

    HTTPClient http;
    http.setTimeout(15000);
    http.setConnectTimeout(10000);

    if (!http.begin(client, OPENSKY_HOST, OPENSKY_PORT, path)) {
        Serial.println("[NET] http.begin FAILED");
        return false;
    }

    // USE HTTP/1.0 — NO chunked encoding, Content-Length ALWAYS known!
    // This removes the need for the 60-line chunked cleaner code.
    http.useHTTP10(true);

    // Basic Auth (if provided — for OpenSky accounts)
    if (strlen(OPENSKY_USER) > 0) {
        http.setAuthorization(OPENSKY_USER, OPENSKY_PASS);
    }

    int code = http.GET();
    Serial.print("[NET] HTTP code: ");
    Serial.println(code);

    if (code == 429) {
        Serial.println("[NET] 429 - Rate limit! Waiting 5 minutes...");
        http.end();
        delay(300000);
        return false;
    }

    if (code != HTTP_CODE_OK) {
        Serial.print("[NET] HTTP error: ");
        Serial.println(http.errorToString(code));
        http.end();
        return false;
    }

    // ─── 3. Fetch response into PSRAM buffer ───
    if (!ensureBuffers()) {
        Serial.println("[NET] No buffer, poll skipped");
        http.end();
        return false;
    }

    // With HTTP/1.0 Content-Length is ALWAYS known (no chunked)
    int contentLen = http.getSize();
    Serial.print("[NET] Content-Length: ");
    Serial.print(contentLen);
    Serial.println(" bytes");

    size_t bufSize = 500000;
    char *buf = s_netBuf;

    // Stream reading — via Content-Length OR idle timeout
    // (HTTP/1.0 sometimes omits Content-Length, so idle timeout is needed)
    WiFiClient *stream = http.getStreamPtr();
    size_t totalRead = 0;
    uint32_t totalTimeout = millis() + 10000;
    uint32_t lastDataMs = millis();
    uint32_t idleTimeout = 300;  // 300ms no data = transfer complete
    while (totalRead < bufSize - 1 && millis() < totalTimeout) {
        // If Content-Length is known, exit upon full read
        if (contentLen > 0 && totalRead >= (size_t)contentLen) break;

        size_t avail = stream->available();
        if (avail == 0) {
            // Data arrived but stopped → check idle timeout
            if (totalRead > 0 && millis() - lastDataMs > idleTimeout) {
                break;  // 300ms no new data → done
            }
            delay(1);
            continue;
        }
        lastDataMs = millis();
        size_t toRead = avail;
        if (toRead > bufSize - 1 - totalRead) toRead = bufSize - 1 - totalRead;
        if (contentLen > 0 && totalRead + toRead > (size_t)contentLen) {
            toRead = contentLen - totalRead;
        }
        size_t bytesRead = stream->readBytes((uint8_t*)(buf + totalRead), toRead);
        totalRead += bytesRead;
        if (bytesRead == 0) break;
    }
    buf[totalRead] = '\0';
    http.end();

    Serial.print("[NET] Total read: ");
    Serial.print(totalRead);
    Serial.print(" bytes, buf[0..40]: ");
    if (totalRead > 40) {
        char dbg[41];
        memcpy(dbg, buf, 40);
        dbg[40] = '\0';
        Serial.println(dbg);
    } else {
        Serial.println(buf);
    }

    // ─── Chunked cleaner CODE REMOVED! ───
    // With HTTP/1.0 there is no chunked encoding, JSON arrives directly.

    // ─── 4. Manual JSON parser — No ArduinoJson, much less RAM ───
    // OpenSky format: {"time":...,"states":[["icao","callsign","country",...,lon,lat,...],...]}
    // We manually split and parse each states[i] array for each aircraft.
    // Bypasses JsonDocument's internal RAM usage entirely.
    Serial.println("[NET] Manual parser starting...");

    static Aircraft s_tempAircraft[MAX_AIRCRAFT];
    int count = 0;

    // Find "states":[ section
    char *statesStart = strstr(buf, "\"states\":[");
    if (!statesStart) {
        Serial.println("[NET] 'states' array not found");
        // buf is static — do not free
        return false;
    }
    statesStart += 10;  // After "states":[

    // Process each [[...] array sequentially
    char *p = statesStart;
    while (*p && count < MAX_AIRCRAFT) {
        // Search for next [ — start of new aircraft
        while (*p && *p != '[') p++;
        if (*p != '[') break;
        p++;  // After [

        // Copy entire array (to temp buffer)
        char *arrayEnd = strchr(p, ']');
        if (!arrayEnd) break;
        int arrayLen = arrayEnd - p;
        if (arrayLen > 511) arrayLen = 511;  // safety cap

        char arrBuf[512];
        strncpy(arrBuf, p, arrayLen);
        arrBuf[arrayLen] = '\0';
        
        p = arrayEnd + 1; // Move to next aircraft

        // Parse array — comma separated values
        // [0] icao24, [1] callsign, [2] country, [5] lon, [6] lat, [7] alt, [8] ground, [9] vel, [10] track
        // Manual parse: split by comma, skip null values
        char *tokStart = arrBuf;
        int fieldIdx = 0;
        Aircraft &a = s_tempAircraft[count];

        // Init all string values empty
        a.icao24[0] = '\0';
        a.callsign[0] = '\0';
        a.country[0] = '\0';
        a.lat = 0; a.lon = 0; a.altitude = 0; a.velocity = 0; a.heading = 0;
        a.on_ground = false;

        bool skip = false;
        while (tokStart && *tokStart && fieldIdx <= 10) {
            // Next token start — " or number or null or true/false
            while (*tokStart == ' ' || *tokStart == ',') tokStart++;
            if (*tokStart == '\0') break;

            char *tokEnd;
            if (*tokStart == '"') {
                // String value
                tokStart++;
                tokEnd = strchr(tokStart, '"');
                if (!tokEnd) break;
                int slen = tokEnd - tokStart;
                if (slen < 0) slen = 0;
                switch (fieldIdx) {
                    case 0: { // icao24
                        if (slen > 6) slen = 6;
                        strncpy(a.icao24, tokStart, slen);
                        a.icao24[slen] = '\0';
                        break;
                    }
                    case 1: { // callsign — trim spaces
                        int j = 0;
                        for (int k = 0; k < slen && j < 9; k++) {
                            if (tokStart[k] != ' ') a.callsign[j++] = tokStart[k];
                        }
                        a.callsign[j] = '\0';
                        if (j == 0) strcpy(a.callsign, "----");
                        break;
                    }
                    case 2: { // country
                        if (slen > 15) slen = 15;
                        strncpy(a.country, tokStart, slen);
                        a.country[slen] = '\0';
                        break;
                    }
                }
                tokStart = tokEnd + 1;
            } else {
                // Number, null, true, false
                tokEnd = strchr(tokStart, ',');
                if (!tokEnd) tokEnd = strchr(tokStart, '\0');
                int slen = tokEnd - tokStart;
                char valBuf[32];
                if (slen > 30) slen = 30;
                strncpy(valBuf, tokStart, slen);
                valBuf[slen] = '\0';

                if (strncmp(valBuf, "null", 4) == 0) {
                    // null — skip
                } else if (strncmp(valBuf, "true", 4) == 0) {
                    if (fieldIdx == 8) a.on_ground = true;
                } else if (strncmp(valBuf, "false", 5) == 0) {
                    if (fieldIdx == 8) a.on_ground = false;
                } else {
                    // Number
                    float val = strtof(valBuf, nullptr);
                    switch (fieldIdx) {
                        case 5:  a.lon = val; break;
                        case 6:  a.lat = val; break;
                        case 7:  a.altitude = val; break;
                        case 9:  a.velocity = val; break;
                        case 10: a.heading = val; break;
                    }
                }
                tokStart = tokEnd;
            }
            fieldIdx++;
        }

        // Does it have a position? (lat/lon cannot be null)
        if (a.lat != 0 || a.lon != 0) {
            a.on_screen = false;
            a.last_update_ms = millis();  // Timestamp for interpolation
            count++;
        }

        p = arrayEnd + 1;
        // Skip next comma
        while (*p && *p != ',' && *p != ']') p++;
        if (*p == ',') p++;
    }

    // buf is static — do not free, reuse on next poll

    if (g_aircraftMutex != NULL) {
        if (xSemaphoreTake(g_aircraftMutex, portMAX_DELAY) == pdTRUE) {
            // Save selected aircraft ICAO code
            char selectedIcao[20] = {0};
            if (g_selectedIdx >= 0 && g_selectedIdx < g_aircraftCount) {
                strncpy(selectedIcao, g_aircraft[g_selectedIdx].icao24, 19);
            }

            // Copy new list
            memcpy(g_aircraft, s_tempAircraft, count * sizeof(Aircraft));
            g_aircraftCount = count;
            g_dataValid = true;
            g_lastUpdate_ms = millis();

            // Find old aircraft in new list and update selection
            int newSelectedIdx = -1;
            if (selectedIcao[0] != '\0') {
                for (int i = 0; i < count; i++) {
                    if (strcmp(g_aircraft[i].icao24, selectedIcao) == 0) {
                        newSelectedIdx = i;
                        break;
                    }
                }
            }
            g_selectedIdx = newSelectedIdx;

            xSemaphoreGive(g_aircraftMutex);
        }
    } else {
        // Fallback (if no Mutex)
        char selectedIcao[20] = {0};
        if (g_selectedIdx >= 0 && g_selectedIdx < g_aircraftCount) {
            strncpy(selectedIcao, g_aircraft[g_selectedIdx].icao24, 19);
        }
        memcpy(g_aircraft, s_tempAircraft, count * sizeof(Aircraft));
        g_aircraftCount = count;
        g_dataValid = true;
        g_lastUpdate_ms = millis();
        int newSelectedIdx = -1;
        if (selectedIcao[0] != '\0') {
            for (int i = 0; i < count; i++) {
                if (strcmp(g_aircraft[i].icao24, selectedIcao) == 0) {
                    newSelectedIdx = i;
                    break;
                }
            }
        }
        g_selectedIdx = newSelectedIdx;
    }

    Serial.print("[NET] Manual parser: ");
    Serial.print(count);
    Serial.println(" aircraft received");
    return true;
}
