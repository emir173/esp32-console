const i18nData = {
  "tr": {
    "nav": {
      "arch": "Mimari",
      "games": "Oyunlar",
      "roadmap": "Yol Haritası",
      "github": "GitHub →"
    },
    "hero": {
      "title": "ESP32 <span class=\"accent\">el konsolu</span>",
      "desc": "Çift çekirdekli işletim sistemi üzerinde 15 oyun çalıştıran, çift ekranlı ESP32 el konsolu.",
      "btn_arch": "Mimariyi İncele",
      "btn_code": "Kaynak Kod",
      "f1": "Özel RTOS Scheduler",
      "f2": "Triple Buffer Rendering",
      "f3": "15 Yerleşik Oyun",
      "f4": "Çift Çekirdek Mimari",
      "f5": "OTA Bootloader"
    },
    "spec": {
      "cpu_lbl": "İşlemci",
      "mem_lbl": "Bellek",
      "scr_lbl": "Ekranlar",
      "lang_lbl": "Dil",
      "sd_lbl": "Depolama",
      "audio_lbl": "Ses",
      "audio_val": "Pasif Buzzer"
    },
    "schem": {
      "eyebrow": "DEVRE ŞEMASI",
      "title": "Pin bağlantıları.",
      "readout_lbl": "// seçili hat",
      "readout_val": "Üstteki şemada bir düğüme dokun veya gezin. <span class=\"pin-detail\">Her düğüm gerçek GPIO atamasını ve paylaşılan hat varsa hangi modüllerle paylaşıldığını gösterir.</span>"
    },
    "arch": {
      "eyebrow": "ÇİFT ÇEKİRDEK MİMARİSİ",
      "title": "Core 0 hesaplar. Core 1 çizer.<br/>Hiçbiri birbirini bekletmez.",
      "desc": "Tüm yük iki çekirdeğe dengeli şekilde paylaştırıldı. Oyunun zorlu matematiğini ve fiziklerini arka planda Core 0 hesaplarken, Core 1 sadece ekrana kusursuz ve akıcı görüntüler çizmeye odaklanır.",
      "core0_lbl": "CORE 0 — HESAPLAMA",
      "core0_t1": "↳ raycasting · fizik · AI",
      "core0_t2": "↳ OLED çizimi, düşük öncelik",
      "core1_lbl": "CORE 1 — RENDER",
      "core1_t1": "↳ pushImage · HUD delta-draw",
      "core1_pri1": "tek başına",
      "core1_t2": "↳ triple buffer swap, mutex korumalı",
      "code1": "// fb_render: TaskEngine'in yazdığı buffer",
      "code2": "// fb_display: TaskDisplay'in okuduğu buffer",
      "code3": "// fb_ready: el değiştirmeye hazır olan buffer",
      "code4": "/* üçüncü boş buffer'ı seç */",
      "code5": "// Core 0: Oyun mantığı ve fizik hesaplamaları",
      "code6": "// Core 1: Sadece donanımsal grafik çizimi"
    },
    "story": {
      "eyebrow": "HİKAYE",
      "title": "Neden yaptım?",
      "desc": "Dış bir grafik işlemcisine veya geleneksel bir işletim sistemine bağlı kalmadan bir ESP32'nin sınırlarını ne kadar zorlayabileceğimi görmek istedim.",
      "challenges_eyebrow": "MÜHENDİSLİK ZORLUKLARI",
      "challenges_title": "En büyük engeller.",
      "c1_t": "Shared SPI Bus (Paylaşımlı SPI)",
      "c1_d": "İki çekirdekten aynı anda tek bir SPI veriyoluna erişimi, kilitlenme veya ekran bozulması olmadan yönetmek.",
      "c2_t": "Triple Buffer Senkronizasyonu",
      "c2_d": "İki farklı CPU çekirdeği arasında mutex ve semaforlar kullanarak yırtılmasız (tear-free) bir render hattı kurmak.",
      "c3_t": "Hafıza Optimizasyonu",
      "c3_d": "15 oyunu ve arayüzü sınırlı SRAM içine sığdırmak, PSRAM'i performans kaybı yaşamadan verimli kullanmak.",
      "c4_t": "60 FPS Raycasting",
      "c4_d": "Doku kaplanmış duvarları akıcı bir deneyim için yeterince hızlı çizebilen özel bir 3D motor yazmak."
    },
    "games": {
      "eyebrow": "OYUN KÜTÜPHANESİ",
      "title": "15 oyun"
    },
    "apps": {
      "eyebrow": "UYGULAMALAR",
      "title": "3 uygulama"
    },
    "os": {
      "eyebrow": "İŞLETİM SİSTEMİ",
      "title": "E-OS Launcher.",
      "sub": "Tüm oyunlara ev sahipliği yapan akıcı ve yenilikçi ana menü.",
      "pause_t": "Donanımsal Duraklatma",
      "pause_d": "Hangi oyunda olursan ol, joystick butonuna basınca ilgili RTOS task'ı anında askıya alınır.",
      "ota_t": "OTA Bootloader",
      "ota_d": "SD karttan binary okuyup flash'a yazar. RTC magic value ile reboot sonrası oyun seçimi hatırlanır.",
      "ui_t": "Double-Buffered Menü",
      "ui_d": "Dönen carousel menü, ekran yırtılması olmadan akar — telefon hissiyatına yakın geçişler.",
      "sound_t": "Donanımsal Ses Kontrolü",
      "sound_d": "Settings menüsü üzerinden sistem genelindeki sesi kapatın, kısın (LOW) veya tam seste oynayın (HIGH).",
      "fps_t": "Canlı FPS Göstergesi",
      "fps_d": "Geliştiriciler için tüm oyunların köşesinde gerçek zamanlı kare hızını (FPS) anlık olarak görebilme imkanı.",
      "cpu_t": "Asenkron Çift Çekirdek",
      "cpu_d": "Core 0 sadece arka plan görevlerini ve oyun mantığını işlerken, Core 1 tamamen grafik çizimine adanmıştır."
    },
    "roadmap": {
      "eyebrow": "YOL HARİTASI",
      "title": "Sırada ne var.",
      "v1_tag": "v1.0 — tamam",
      "v1_desc": "<strong>15 oyunluk kütüphane</strong> — Mode 7, Wire-Frame 3D, Galactic Strike ve Platformer çekirdek oyunlara katıldı; ardından Tetris, Dungeon ve Tower Defense eklendi. Ortak <span class=\"mono\" style=\"color:var(--paper)\">hardware_config.h</span> ile pin tanımları tekilleştirildi.",
      "v2_tag": "v2.0 — planlanan",
      "v2_desc": "<strong>LiPo batarya entegrasyonu</strong> — korumalı TP4056 şarj devresi, özel PCB tasarımı ve 3D yazıcı ile kasa üretimi.",
      "v21_tag": "v2.1 — planlanan",
      "v21_desc": "<strong>I²S ses çıkışı</strong> — MAX98357A amplifikatör ile buzzer'ın yerini gerçek PCM ses alacak.",
      "v3_tag": "v3.0 — fikir aşamasında",
      "v3_desc": "<strong>ESP-NOW multiplayer</strong> — ESP32-S3'ün dahili radyosu üzerinden iki cihaz arası düşük gecikmeli bağlantı."
    },
    "build": {
      "eyebrow": "DERLEME",
      "title": "Arduino IDE veya PlatformIO.",
      "sub": "Gerekli kütüphaneler ve pin ayarları aşağıda — tüm detaylar repo içinde.",
      "code1": "// Gerekli kütüphaneler",
      "code2": "// Ekran sürücüsü — User_Setup.h ile yapılandırılır",
      "code3": "// OLED sürücüsü (HW I2C modunda)",
      "code4": "// NVS — skor / ayar kalıcılığı",
      "code5": "// Tüm oyunlar ortak pin dosyasını include eder"
    },
    "footer": {
      "desc": "ESP32 El Konsolu.",
      "github": "GitHub",
      "hardware": "Donanım / PCB",
      "firmware": "Yazılım (Firmware)",
      "license": "Lisans",
      "contact": "İletişim",
      "version": "v2.1.0",
      "last_updated": "Son Güncelleme: Temmuz 2026"
    }
  },
  "en": {
    "nav": {
      "arch": "Architecture",
      "games": "Games",
      "roadmap": "Roadmap",
      "github": "GitHub →"
    },
    "hero": {
      "title": "ESP32 <span class=\"accent\">handheld console</span>",
      "desc": "Dual-screen ESP32 handheld console running 15 games on a dual-core operating system.",
      "btn_arch": "View Architecture",
      "btn_code": "Source Code",
      "f1": "Custom RTOS Scheduler",
      "f2": "Triple Buffer Rendering",
      "f3": "15 Native Games",
      "f4": "Dual-Core Architecture",
      "f5": "OTA Bootloader"
    },
    "spec": {
      "cpu_lbl": "Processor",
      "mem_lbl": "Memory",
      "scr_lbl": "Displays",
      "lang_lbl": "Language",
      "sd_lbl": "Storage",
      "audio_lbl": "Audio",
      "audio_val": "Passive Buzzer"
    },
    "schem": {
      "eyebrow": "CIRCUIT SCHEMATIC",
      "title": "Pinout connections.",
      "readout_lbl": "// selected line",
      "readout_val": "Hover or tap on a node in the schematic above. <span class=\"pin-detail\">Each node shows the actual GPIO mapping and shared modules if applicable.</span>"
    },
    "arch": {
      "eyebrow": "DUAL-CORE ARCHITECTURE",
      "title": "Core 0 computes. Core 1 renders.<br/>Neither waits for the other.",
      "desc": "The workload is perfectly balanced across two cores. While Core 0 handles the heavy game logic and physics in the background, Core 1 focuses entirely on drawing smooth graphics to the screen.",
      "core0_lbl": "CORE 0 — COMPUTE",
      "core0_t1": "↳ raycasting · physics · AI",
      "core0_t2": "↳ OLED rendering, low priority",
      "core1_lbl": "CORE 1 — RENDER",
      "core1_t1": "↳ pushImage · HUD delta-draw",
      "core1_pri1": "standalone",
      "core1_t2": "↳ triple buffer swap, mutex protected",
      "code1": "// fb_render: buffer written by TaskEngine",
      "code2": "// fb_display: buffer read by TaskDisplay",
      "code3": "// fb_ready: buffer ready to be swapped",
      "code4": "/* select the 3rd empty buffer */",
      "code5": "// Core 0: Game logic & physics",
      "code6": "// Core 1: Dedicated hardware rendering"
    },
    "story": {
      "eyebrow": "THE STORY",
      "title": "Why I built this.",
      "desc": "I wanted to see how far an ESP32 could go without relying on external graphics hardware or a traditional operating system.",
      "challenges_eyebrow": "ENGINEERING CHALLENGES",
      "challenges_title": "Biggest hurdles.",
      "c1_t": "Shared SPI Bus",
      "c1_d": "Managing concurrent access from both cores to a single SPI bus without causing deadlocks or display corruption.",
      "c2_t": "Triple Buffer Synchronization",
      "c2_d": "Implementing a tear-free render pipeline using mutexes and semaphores across two different CPU cores.",
      "c3_t": "Memory Optimization",
      "c3_d": "Fitting 15 games and a custom GUI into limited SRAM, utilizing PSRAM effectively without taking a performance hit.",
      "c4_t": "Raycasting at 60 FPS",
      "c4_d": "Writing a custom fixed-point math 3D engine that can render textured walls fast enough for a smooth experience."
    },
    "games": {
      "eyebrow": "GAME LIBRARY",
      "title": "15 games"
    },
    "apps": {
      "eyebrow": "APPS",
      "title": "3 apps"
    },
    "os": {
      "eyebrow": "OPERATING SYSTEM",
      "title": "E-OS Launcher.",
      "sub": "Fluid, rotating (carousel) main menu interface hosting all games.",
      "pause_t": "Hardware Pause",
      "pause_d": "Instantly suspend the active RTOS task by pressing the joystick button in any game.",
      "ota_t": "OTA Bootloader",
      "ota_d": "Reads binaries from SD card and writes to flash. Reboot state is remembered via RTC magic values.",
      "ui_t": "Double-Buffered UI",
      "ui_d": "Rotating carousel menu runs completely tear-free, offering smartphone-like smooth transitions.",
      "sound_t": "Hardware Sound Control",
      "sound_d": "Mute, lower (LOW), or play games at full volume (HIGH) globally via the Settings menu.",
      "fps_t": "Live FPS Overlay",
      "fps_d": "Real-time frame rate (FPS) overlay available for developers on all games.",
      "cpu_t": "Asynchronous Dual-Core",
      "cpu_d": "Core 0 strictly handles background tasks and game logic, while Core 1 is dedicated to drawing graphics."
    },
    "roadmap": {
      "eyebrow": "ROADMAP",
      "title": "What's next.",
      "v1_tag": "v1.0 — done",
      "v1_desc": "<strong>15 games library</strong> — Mode 7, Wire-Frame 3D, Galactic Strike and Platformer joined the core games; then Tetris, Dungeon and Tower Defense were added. Pin definitions unified with <span class=\"mono\" style=\"color:var(--paper)\">hardware_config.h</span>.",
      "v2_tag": "v2.0 — planned",
      "v2_desc": "<strong>LiPo battery integration</strong> — protected TP4056 charging circuit, custom PCB design, and a 3D-printed case.",
      "v21_tag": "v2.1 — planned",
      "v21_desc": "<strong>I²S audio output</strong> — MAX98357A amplifier replacing the buzzer with real PCM audio.",
      "v3_tag": "v3.0 — idea",
      "v3_desc": "<strong>ESP-NOW multiplayer</strong> — low-latency peer-to-peer connection over ESP32-S3's internal radio."
    },
    "build": {
      "eyebrow": "BUILD",
      "title": "Arduino IDE or PlatformIO.",
      "sub": "Required libraries and pin configs below — all details in the repo.",
      "code1": "// Required libraries",
      "code2": "// Display driver — configured via User_Setup.h",
      "code3": "// OLED driver (in HW I2C mode)",
      "code4": "// NVS — persistent scores & settings",
      "code5": "// All games include the shared pin config"
    },
    "footer": {
      "desc": "ESP32 Handheld Console.",
      "github": "GitHub",
      "hardware": "Hardware / PCB",
      "firmware": "Firmware",
      "license": "License",
      "contact": "Contact",
      "version": "v2.1.0",
      "last_updated": "Last Updated: July 2026"
    }
  }
};
