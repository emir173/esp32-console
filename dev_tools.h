#pragma once
// ============================================================
//  E-OS — Gelistirici Araclari (Dev Tools)
//  FPS HUD + USB screenshot / continuous GIF-video capture
//
//  Kullanim:
//    1. Oyun .ino dosyasinin basina:  #include "../dev_tools.h"
//    2. setup() icinde USB baslat:    initDevTools(tft);
//       (SPI.begin sonrasi, tft.init ONCE)
//    3. loop() sonunda HUD ciz:       drawDevHUD(canvas);
//    4. Screenshot: pushSprite ONCESI checkScreenshot(canvas);
//    5. USB: 's' = screenshot, 'g' = continuous capture, 'x' = stop, 'k' = FULL.
//       Screenshot submission does not wait. GIF submission waits up to 2 seconds
//       for a free buffer; a transfer failure stops capture, not the game.
//       Capture has a 20 ms minimum interval (at most 50 FPS); actual FPS depends
//       on USB throughput and game rendering. Games continue to use real millis().
//       SATIR BAZLI DELTA: sadece degisen satirlar gonderilir.
//       Kalite korunur, bant genisligi ihtiyaci duser.
//       USB modu: HWCDC (JTAG) stabil ~254 KB/s. USBCDC (USB-OTG/TinyUSB) modunda
//       A header and its pixels are written as one packet; short writes abort
//       the session. The USB driver's bounded TX timeout keeps failure finite.
//
//  Renk modlari (oyuna gore set edin -- Python otomatik algilar):
//    setScreenshotMode(SCR_BGR_SWAP);   // RGB() makrosu (wireframe3d, galacticstrike, platformer)
//    setScreenshotMode(SCR_RGB_SWAP);   // TFT_* sabitleri (snake, arkanoid, flappy, pacman, spaceInvaders, mode7)
//    setScreenshotMode(SCR_BGR_NOSWAP); // RGB_FIX() + direkt fb (doom)
//  Header'da renk modu + cozunurluk + delta modu gonderilir:
//    FRAME:BGR_SWAP:160x128:DELTA\n + [uint16 nRows][nRows x (uint16 row + 320B)]
//    FRAME:BGR_SWAP:160x128:FULL\n  + 40960 byte  (keyframe)
//    SHOT:BGR_SWAP:160x128:FULL\n   + 40960 byte  (screenshot)
//  Python script'leri (capture.py, capture_gif.py, record_video.py)
//  suffix'i + WxH'i + delta modunu otomatik okuyup cozer.
//  Eski firmware (FRAME:BGR_SWAP\n, mod field yok) -> Python FULL varsayar (geri uyum).
//
//  SD screenshot writes are removed; capture does not use the shared SPI bus.
// ============================================================

// Pin guard — oyun kendi pinlerini tanimladiysa tekrar include etme
#ifndef TFT_CS
  #include "hardware_config.h"
#endif

#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_heap_caps.h>

// --- FreeRTOS (Async USB Frame Dumper) ---
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <atomic>          // std::atomic<bool> _forceKey (Core1 -> Core0 keyframe talebi)
#include <string.h>        // memcmp / memcpy (delta satir karsilastirma)



// ============ Screenshot Renk Modlari ============
#define SCR_BGR_SWAP    0  // RGB() makrosu + sprite (byte swap + BGR565)
#define SCR_RGB_SWAP    1  // TFT_* sabitleri + sprite (byte swap + RGB565)
#define SCR_BGR_NOSWAP  2  // RGB_FIX() + direkt fb (swap yok + BGR565)

inline int _scr_color_mode = SCR_BGR_SWAP;

inline void setScreenshotMode(int mode) {
    _scr_color_mode = mode;
}

// Header suffix -- renk modunu USB header'a gomer.
// Python tarafinda otomatik renk algilama icin:
//   "FRAME:BGR_SWAP\n"  ->  Python swap=True,  bgr=True  (RGB() makrosu)
//   "FRAME:RGB_SWAP\n"  ->  Python swap=True,  bgr=False (TFT_* sabitleri)
//   "FRAME:BGR_NOSWAP\n"->  Python swap=False, bgr=True  (RGB_FIX/direkt fb)
inline const char* _colorSuffix() {
    switch (_scr_color_mode) {
        case SCR_RGB_SWAP:   return ":RGB_SWAP";
        case SCR_BGR_NOSWAP: return ":BGR_NOSWAP";
        case SCR_BGR_SWAP:
        default:             return ":BGR_SWAP";
    }
}

// ============ FPS Sayaci ============
inline uint32_t _fps_lastMs = 0;
inline uint16_t _fps_count = 0;
inline uint16_t _fps_value = 0;

inline void updateFPS() {
    _fps_count++;
    uint32_t now = millis();
    if (now - _fps_lastMs >= 1000) {
        _fps_value = (uint32_t)_fps_count * 1000 / (now - _fps_lastMs);
        _fps_count = 0;
        _fps_lastMs = now;
    }
}

// ============ Dev HUD ============
inline void drawDevHUD(TFT_eSprite &spr) {
    updateFPS();
    spr.setTextSize(1);
    spr.setTextColor(TFT_WHITE);
    spr.setCursor(2, 2);
    spr.printf("FPS:%d", _fps_value);
}

// ============ Screenshot ============
inline void devToolsTick() {
    // No-op
}

inline void initDevTools(TFT_eSPI &tft, bool init_sd = true);

// NOT (2026-07-08): Eski SD-screenshot yolu (_writeBmpPixels / _writeBmpHeader /
// _resetSPIAndSD) ve initDevTools'taki SD kalıntısı (SD.begin + /screenshots mkdir +
// dosya tarama) KALDIRILDI. Ekran görüntüleri yalnızca USB üzerinden alınıyor
// (aşağıdaki async FrameDumper). SD'ye BMP yazma bu kartta TFT_eSPI ile ortak SPI
// veriyolunda kilitleniyordu; bu yüzden USB'ye taşınmıştı. initDevTools artık SD'ye
// hiç dokunmaz; yalnızca SD_CS'i deselect eder (paylaşılan bus'ta CS floating kalmasın).


// ============================================================
//  ASYNC USB FRAME DUMPER — Non-blocking Framebuffer Sizdirma
//
//  Sorun: Serial.write(40KB) TX FIFO dolana kadar main loop'u
//         ~30ms blokluyor -> oyunda stuttering.
//
//  Cozum: Bloklayan USB yazma + delta encode isini Core 0'da calisan
//         ayri bir FreeRTOS task'a tasiyoruz. Main loop (Core 1) sadece
//         hizli bir tam kare kopyasi (memcpy 40960B) + non-blocking queue
//         send yapar. Delta karsilastirma Core 0'da yapilir.
//
//  Mimarisi (Producer-Consumer + Ping-Pong):
//    [Main loop Core 1]  memcpy(full frame) -> free buffer -> filled queue (0 block)
//    [Dump task  Core 0] filled queue -> DELTA/FULL encode -> Serial.write -> free buffer
//
//  Akis kontrolu: screenshot free-buffer beklemez; GIF en fazla 2 saniye bekler.
//  Aktarim hatasinda kayit durur. Yeni 'g' veya 's' yeni bir FULL oturumu acar.
//
//  SATIR BAZLI DELTA FRAME SIKISTIRMASI (Line-based Delta Compression):
//  ----------------------------------------------------------------
//  USB-Serial-JTAG (HWCDC) + Windows usbser.sys donanimsal tavani
//  ~254 KB/s (~6 FPS @ 160x128 tam kare 40960B). 30 FPS icin bant
//  genisligi ZORLANMAZ; bunun yerine gonderilen veri azaltilir:
//  sadece onceki kareye gore DEGISIM GOSTEREN satirlar gonderilir.
//
//    Tam kare (keyframe): 40960 byte               -> ~6 FPS
//    Delta (degisen satirlar): 2 + N*(2 + 320) byte
//      N=0   (statik ekran)  -> 2 byte             -> 30+ FPS
//      N=32  (%25 degisim)   -> 10306 byte         -> ~25 FPS @ 254 KB/s
//      N=128 (tum ekran)     -> 41218 byte         -> delta FULL'den buyuk -> FULL
//
//  Kalite KORUNUR (downscale YOK): her piksel 160x128 tam cozunurlukte,
//  sadece degismeyen satirlar tekrar gonderilmez. Doom gibi tam ekran
//  scroll oyunlarda delta orani yuksek -> FPS duser ama detay kaybi YOK.
//  Snake/Pacman/Arkanoid gibi oyunlarda cogu karede ekranin buyuk kismi
//  sabit -> delta orani dusuk -> 30 FPS kolayca.
//
//  Keyframe mekanizmasi (delta zinciri senkronizasyonu):
//    - Ilk kare (_prevValid=false)            -> FULL
//    - Python 'k' komutu (requestKeyframe)    -> sonraki kare FULL
//    - Her KEYFRAME_INTERVAL (60) gif karede  -> FULL (periyodik resync)
//    - Delta boyutu FULL'i asarsa             -> otomatik FULL
//    - SHOT (screenshot)                      -> hep FULL (tek kare, delta anlamsiz)
//  Drop edilen kareler delta zincirini BOZMAZ: _prevFrame sadece gonderilen
//  karelerle guncellenir; Python cache'i de sadece gelen karelerle guncellenir.
//
//  Wire format (header'da 4. field = mod):
//    FRAME:<COLOR>:160x128:FULL\n   + 40960 byte                   (keyframe)
//    FRAME:<COLOR>:160x128:DELTA\n  + [u16 nRows][nRows x (u16 row + 320B)]
//    SHOT:<COLOR>:160x128:FULL\n    + 40960 byte                   (screenshot)
//  Python find_next_header() 4. field'i parse eder; eski FW (field yok) -> FULL.
// ============================================================
class FrameDumper {
public:
    // Kayit (capture) cozunurlugu = TAM 160x128 (downscale YOK).
    // Delta sikistirma ile bant genisligi ihtiyaci dusurulur, kalite korunur.
    static constexpr int    CAP_W = 160, CAP_H = 128;
    static constexpr size_t FB_SIZE   = CAP_W * CAP_H * 2;   // 40960 byte
    static constexpr size_t ROW_BYTES = CAP_W * 2;           // 320 byte/satir
    static constexpr size_t HEADER_BYTES = 40;              // space before pixels
    static constexpr int    KEYFRAME_INTERVAL = 60;          // her 60 gif karede FULL

    bool begin() {
        if (_running.load()) return true;
        _freeQ   = xQueueCreate(2, sizeof(uint8_t*));
        _filledQ = xQueueCreate(2, sizeof(DumpJob));
        if (!_freeQ || !_filledQ) { _releaseInitResources(); return false; }

        for (int i = 0; i < 2; i++) {
            // Once internal SRAM (yazma hizli), yetersizse PSRAM
            _buf[i] = (uint8_t*)heap_caps_malloc(FB_SIZE + HEADER_BYTES,
                          MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
            if (!_buf[i]) {
                _buf[i] = (uint8_t*)heap_caps_malloc(FB_SIZE + HEADER_BYTES,
                              MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
            }
            if (!_buf[i]) { _releaseInitResources(); return false; }
            uint8_t* p = _buf[i] + HEADER_BYTES;
            xQueueSend(_freeQ, &p, portMAX_DELAY);
        }
        // Delta encoder buffer'lari (PSRAM oncelikli — sadece Core 0 erisir,
        // hiz kritik degil; yetersizse internal SRAM fallback, _buf pattern'i).
        // Ikisi de fail ederse _prevFrame=null -> _run hep FULL gonderir (guvenli).
        _prevFrame = (uint8_t*)heap_caps_malloc(FB_SIZE,
                        MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
        if (!_prevFrame)
            _prevFrame = (uint8_t*)heap_caps_malloc(FB_SIZE,
                            MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        _deltaBuf  = (uint8_t*)heap_caps_malloc(FB_SIZE + 512,
                        MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
        if (!_deltaBuf)
            _deltaBuf = (uint8_t*)heap_caps_malloc(FB_SIZE + 512,
                            MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        _prevValid     = false;
        _gifFrameCount = 0;
        _workerEpoch = 1;
        _captureEpoch.store(1);
        _forceKey.store(false);
        _failedEpoch.store(0);
        _dropped = 0;
        // Core 0, dusuk oncelik (1) -> WiFi/BT ve oyun (Core 1) etkilenmez
        if (xTaskCreatePinnedToCore(_taskEntry, "fbDump", 4096, this, 1, &_task, 0) != pdPASS) {
            _releaseInitResources();
            return false;
        }
        _running.store(true);
        return true;
    }

    // Main loop'tan cagrilir.
    //   blocking=false (default): timeout 0 -> HIC bloklamaz. USB yetisemezse
    //     kare drop edilir (oyun devam eder). Screenshot (tek kare) icin ideal.
    //   blocking=true : waits at most 2 seconds; false signals a capture failure.
    // true = kare kuyruklandi (gidecek)
    // false = invalid source, unavailable worker, transfer failure or timeout
    bool submit(const uint8_t* src, bool isGif, bool blocking = false) {
        if (!src || !_running.load() || failed()) return false;
        const uint32_t epoch = _captureEpoch.load();
        uint8_t* dst;
        TickType_t wait = blocking ? pdMS_TO_TICKS(2000) : 0;
        if (xQueueReceive(_freeQ, &dst, wait) != pdPASS) {
            _dropped++;
            return false;  // Akis kontrolu: bu kare atilir (non-blocking)
        }
        if (failed() || epoch != _captureEpoch.load()) {
            xQueueSend(_freeQ, &dst, 0);
            return false;
        }
        // TEK main-loop maliyeti: tam kare kopyasi (40960B memcpy, ~0.5ms PSRAM'den).
        // Delta karsilastirma Core 0'da yapilir (oyun Core 1'i bloklamaz).
        memcpy(dst, src, FB_SIZE);
        DumpJob job = { dst, isGif, epoch };
        // filledQ'yu da blocking modda bekle (teori: free varsa filled de vardir)
        if (xQueueSend(_filledQ, &job, wait) != pdPASS) {
            // (Teorik olmamali) buffer'i geri koy
            xQueueSend(_freeQ, &dst, 0);
            _dropped++;
            return false;
        }
        return true;
    }

    // Python 'k' komutu -> sonraki kareyi keyframe (FULL) yap.
    // Core 1'den cagrilir, Core 0 (_run) consume eder -> atomic.
    void requestKeyframe() { _forceKey.store(true); }

    inline uint32_t dropped() const { return _dropped; }
    inline bool     running() const { return _running.load(); }
    inline bool     failed() const  { return _failedEpoch.load() == _captureEpoch.load(); }
    // Only the worker changes its frame counter/cache. Old queued jobs are skipped.
    inline void resetGifCount() {
        _captureEpoch.fetch_add(1);
        if (_captureEpoch.load() == 0) _captureEpoch.fetch_add(1);
    }

private:
    struct DumpJob { uint8_t* buf; bool isGif; uint32_t epoch; };

    void _releaseInitResources() {
        for (int i = 0; i < 2; i++) {
            if (_buf[i]) heap_caps_free(_buf[i]);
            _buf[i] = nullptr;
        }
        if (_prevFrame) heap_caps_free(_prevFrame);
        if (_deltaBuf) heap_caps_free(_deltaBuf);
        _prevFrame = nullptr;
        _deltaBuf = nullptr;
        if (_freeQ) vQueueDelete(_freeQ);
        if (_filledQ) vQueueDelete(_filledQ);
        _freeQ = nullptr;
        _filledQ = nullptr;
        _task = nullptr;
        _running.store(false);
    }

    // One Serial.write holds the USB driver's TX lock for header AND pixels.
    // Other tasks' debug messages cannot interleave with a successful packet.
    // A short write is a failed session, never a new delta reference.
    bool _writeAll(const uint8_t* data, size_t size) {
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
        if (!Serial) return false;
#endif
        return Serial.write(data, size) == size;
    }

    static void _taskEntry(void* arg) { static_cast<FrameDumper*>(arg)->_run(); }

    // Core 0'da calisir — burada bloklamak serbest, oyun Core 1'de akar.
    // Her kareyi onceki kareyle karsilastirir; degisen satirlari paketler
    // (DELTA) veya tam kare gonderir (FULL/keyframe).
    void _run() {
        DumpJob job;
        uint8_t changedRows[CAP_H];   // degisen satir numaralari (stack, 128B)
        for (;;) {
            if (xQueueReceive(_filledQ, &job, portMAX_DELAY) != pdPASS) continue;
            if (job.buf == nullptr) break;  // stop sinyali
            if (job.epoch != _captureEpoch.load() || failed()) {
                xQueueSend(_freeQ, &job.buf, portMAX_DELAY);
                continue;
            }
            if (job.epoch != _workerEpoch) {
                _workerEpoch = job.epoch;
                _prevValid = false;
                _gifFrameCount = 0;
            }

            const bool isGif = job.isGif;

            // --- Keyframe karari ---
            bool wantKey = false;
            if (!isGif) {
                wantKey = true;                              // SHOT hep FULL
            } else {
                if (!_prevValid) wantKey = true;             // ilk kare
                if (_forceKey.exchange(false)) wantKey = true;   // Python 'k' talebi
                if (_gifFrameCount > 0 &&
                    (_gifFrameCount % KEYFRAME_INTERVAL) == 0)      // periyodik resync
                    wantKey = true;
            }

            // --- Delta karsilastirma (sadece wantKey degilse) ---
            int nChanged = 0;
            bool fullFrame;
            if (!wantKey && _prevFrame && _deltaBuf) {
                for (int y = 0; y < CAP_H; y++) {
                    const uint8_t* prev = _prevFrame + (size_t)y * ROW_BYTES;
                    const uint8_t* curr = job.buf   + (size_t)y * ROW_BYTES;
                    if (memcmp(prev, curr, ROW_BYTES) != 0) {
                        changedRows[nChanged++] = (uint8_t)y;
                    }
                }
                // Delta boyutu FULL'i asarsa -> FULL'e don (overhead'i engelle)
                size_t deltaSize = 2 + (size_t)nChanged * (2 + ROW_BYTES);
                fullFrame = (deltaSize >= FB_SIZE);
            } else {
                fullFrame = true;   // wantKey veya _prevFrame alloc fail -> FULL
            }

            // --- Header: FRAME/SHOT : COLOR : WxH : FULL/DELTA ---
            char hdr[40];
            strcpy(hdr, isGif ? "FRAME" : "SHOT");
            strcat(hdr, _colorSuffix());
            char suffix[16];
            sprintf(suffix, ":%dx%d:%s", CAP_W, CAP_H, fullFrame ? "FULL" : "DELTA");
            strcat(hdr, suffix);
            strcat(hdr, "\n");
            const size_t headerLen = strlen(hdr);
            uint8_t* packet;
            size_t payloadSize;

            // --- Payload ---
            if (fullFrame) {
                // Tam kare: FB_SIZE byte
                packet = job.buf - headerLen;
                payloadSize = FB_SIZE;
            } else {
                size_t pos = HEADER_BYTES;
                _deltaBuf[pos++] = (uint8_t)(nChanged & 0xFF);
                _deltaBuf[pos++] = (uint8_t)((nChanged >> 8) & 0xFF);
                for (int i = 0; i < nChanged; i++) {
                    int y = changedRows[i];
                    _deltaBuf[pos++] = (uint8_t)(y & 0xFF);
                    _deltaBuf[pos++] = (uint8_t)((y >> 8) & 0xFF);
                    memcpy(_deltaBuf + pos, job.buf + (size_t)y * ROW_BYTES, ROW_BYTES);
                    pos += ROW_BYTES;
                }
                payloadSize = pos - HEADER_BYTES;
                packet = _deltaBuf + HEADER_BYTES - headerLen;
            }
            memcpy(packet, hdr, headerLen);
            bool sent = _writeAll(packet, headerLen + payloadSize);

            if (sent) {
                if (_prevFrame) { memcpy(_prevFrame, job.buf, FB_SIZE); _prevValid = true; }
                if (isGif) _gifFrameCount++;
            } else {
                _prevValid = false;
                _failedEpoch.store(job.epoch);
            }

            // Buffer'i havuza iade et
            uint8_t* p = job.buf;
            xQueueSend(_freeQ, &p, portMAX_DELAY);
        }
        vTaskDelete(nullptr);
    }

    QueueHandle_t      _freeQ   = nullptr;
    QueueHandle_t      _filledQ = nullptr;
    TaskHandle_t       _task    = nullptr;
    uint8_t*           _buf[2]        = { nullptr, nullptr };
    uint8_t*           _prevFrame     = nullptr;   // delta karsilastirma (PSRAM, FB_SIZE)
    uint8_t*           _deltaBuf      = nullptr;   // delta payload olusturma (PSRAM)
    std::atomic<bool>  _running { false };
    std::atomic<uint32_t> _failedEpoch { 0 };
    std::atomic<uint32_t> _captureEpoch { 1 };
    uint32_t          _workerEpoch = 1;
    bool               _prevValid     = false;     // _prevFrame gecerli mi (ilk kare -> keyframe)
    uint32_t           _gifFrameCount = 0;         // periyodik keyframe sayaci (sadece gif)
    std::atomic<bool>  _forceKey { false };        // Python 'k' keyframe talebi (Core1->Core0)
    uint32_t           _dropped       = 0;
};

// Global tekil ornek — initDevTools() icinde baslatilir.
// Inline state is shared across translation units.
inline FrameDumper FrameDump;

inline void startFrameDumper() {
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT || ARDUINO_USB_MODE
    if (!FrameDump.running()) Serial.setTxBufferSize(4096);
#endif
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(250);
#endif
    if (!FrameDump.begin()) log_e("Frame dumper initialization failed");
}


// ============ Screenshot (USB Dump) — ASYNC ============
inline bool _gif_recording = false;

// Artik ASYNC: main loop'u bloklamaz. Core 0'daki FrameDump task'i gonderir.
// true = kare kuyruklandi, false = USB yetismedi (drop, oyun devam eder)
//   is_gif=true  (video kaydi): bounded wait -> oyun USB hizina yavaslayabilir
//   is_gif=false (screenshot) : non-blocking -> drop toleransli, oyun durmaz
inline bool _dumpFrameUSB(uint16_t *buf, int w, int h, bool is_gif) {
    if (!buf || w != FrameDumper::CAP_W || h != FrameDumper::CAP_H) return false;
    return FrameDump.submit((uint8_t*)buf, is_gif, /*blocking=*/is_gif);
}

// Compatibility helper: recording uses the same real clock as the games.
inline uint32_t getDevMillis() {
    return millis();
}

inline void _handleSerialCommands(uint16_t *buf, int w, int h) {
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c == 's') {
            if (FrameDump.failed()) FrameDump.resetGifCount();
            _dumpFrameUSB(buf, w, h, false);
        } else if (c == 'g') {
            // Sürekli kayıt başlat (idempotent: zaten kayıtta ise tekrar başlatma)
            if (!_gif_recording && buf && w == FrameDumper::CAP_W &&
                h == FrameDumper::CAP_H && FrameDump.running()) {
                _gif_recording = true;
                FrameDump.resetGifCount(); // keyframe interval sayacı sıfırla
            }
        } else if (c == 'x') {
            // Kayıt durdur
            _gif_recording = false;
        } else if (c == 'k') {
            FrameDump.requestKeyframe(); // Python keyframe talebi -> sonraki kare FULL
        }
    }

    // A stalled/failed transfer stops recording. The game loop stays usable.
    if (_gif_recording) {
        uint32_t _frameStart = millis();
        if (!_dumpFrameUSB(buf, w, h, true)) {
            _gif_recording = false;
            return;
        }
        // 50 FPS throttle: USB hizliysa (kucuk delta) kalan sureyi bekle
        uint32_t _frameElapsed = millis() - _frameStart;
        if (_frameElapsed < 20) {
            delay(20 - _frameElapsed);
        }
    }
}

// USB command polling — pushSprite ONCESI cagir
inline bool checkScreenshot(TFT_eSprite &spr) {
    uint16_t* buf = spr.getColorDepth() == 16 ? (uint16_t*)spr.getPointer() : nullptr;
    _handleSerialCommands(buf, spr.width(), spr.height());
    return false; 
}

// USB command polling — 16-bit framebuffer icin (doom)
inline bool checkScreenshotFB(uint16_t *buf, int w, int h) {
    _handleSerialCommands(buf, w, h);
    return false; 
}

inline void initDevTools(TFT_eSPI &tft, bool init_sd) {
    (void)tft; (void)init_sd; // imza korundu (13 oyun çağırıyor); SD artık kullanılmıyor
    Serial.println("[DEV] initDevTools basliyor...");

    Serial.begin(115200);

    // Paylaşılan SPI bus'ta SD_CS floating kalmasın diye deselect (HIGH) tutulur.
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    startFrameDumper();
    Serial.println("[DEV] initDevTools tamamlandi.");
    Serial.flush();
}

