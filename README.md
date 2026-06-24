<div align="center">
  <img src="docs/images/cihaz 2.jpg" alt="E-OS V2.1 Console" width="600" style="border-radius: 12px;"/>
  <br/><br/>
  <h1>E-OS V2.1 — Handheld Console</h1>
  <p><b>Sınırları Zorlayan Güç.</b></p>
  <p>ESP32-S3 mimarisi üzerinde sıfırdan yazılmış E-OS işletim sistemi. Çift ekran, 11 oyun ve muazzam bir akıcılık.</p>
  
  <p>
    <img src="https://img.shields.io/badge/MCU-ESP32--S3-blue?style=for-the-badge&logo=espressif" alt="ESP32-S3"/>
    <img src="https://img.shields.io/badge/OS-FreeRTOS-yellow?style=for-the-badge" alt="FreeRTOS"/>
    <img src="https://img.shields.io/badge/RAM-8MB_PSRAM-purple?style=for-the-badge" alt="PSRAM"/>
    <img src="https://img.shields.io/badge/Flash-16MB-green?style=for-the-badge" alt="Flash"/>
  </p>
  
  <h3>
    <a href="https://emir173.github.io/esp32-console/">🌐 Projenin Canlı Sunum Sitesini İnceleyin</a>
  </h3>
</div>

---

## 🚀 Proje Hakkında
Sıradan bir "DIY" projesine bakmıyorsunuz. **E-OS V2.1**, mikrodenetleyici limitlerinin sonuna kadar zorlandığı, işletim sisteminden (OS) oyun motorlarına kadar her şeyin **sıfırdan** yazıldığı ve donanımla tam entegre edildiği bir mühendislik eseridir. İçinde hiçbir hazır arayüz veya emülatör bulunmaz. Her bir piksel, cihaza özel kodlanmıştır.

### ⚙️ Donanım Mimarisi
Cihazın kalbinde 240 MHz hızında çalışan **Dual-Core ESP32-S3** bulunuyor. 
- **Çift Ekran (Dual Display):** 
  - *Ana Ekran:* 160x128 Renkli TFT (SPI). Tüm aksiyon ve ana UI burada akar.
  - *İkinci Ekran:* 128x64 OLED (I2C). Cihazın tepesinde bulunur; flash bellek bilgisini, logoları, oyunlardaki en yüksek skoru ve taktiksel istatistikleri anlık olarak yansıtır.
- **Bellek:** 16MB Flash + 8MB PSRAM OPI. Bu devasa bant genişliği sayesinde oyunlar arası geçişlerde "Frame Drop" (takılma) veya ekran yırtılması (screen-tear) yaşanmaz.
- **Ses ve Kontrol:** Yazılımsal frekans filtreli **8-bit akustik buzzer** ve donanımsal deadzone (titreme önleyici) korumalı Analog Joystick.
- **Depolama:** Oyun verileri için Micro SD Kart entegrasyonu.

---

## 🧠 Yazılım ve E-OS İşletim Sistemi
Hazır kütüphanelerin aksine E-OS, doğrudan donanımla konuşur.
* **FreeRTOS Entegrasyonu:** İşlemcinin birinci çekirdeği (Core 0) oyun mantığını ve raycasting matematiğini hesaplarken, ikinci çekirdeği (Core 1) tamamen ekranların pürüzsüz çizimine (rendering) adanmıştır.
* **Double-Buffered UI:** Cihazın ana menüsü tam bir akıllı telefon hissiyatı verir. Dönerek açılan animasyonlu carousel menü tasarımı sayesinde oyunlar arasında hızlıca dolaşabilirsiniz.
* **Donanımsal Duraklatma (Pause):** Hangi oyunda olursanız olun, donanımsal "Pause" butonuna bastığınızda RTOS görevleri (task) dondurulur ve oyun anında duraklatılır.

---

## 🕹️ Özel Kodlanmış 11 Oyun
Oyunlar basit birer port değildir; bu cihazın çözünürlüğü ve işlemcisi için yeniden inşa edilmiştir.

<div align="center">
  <img src="docs/images/doom1.jpg" width="45%" alt="Doom">
  <img src="docs/images/space.jpg" width="45%" alt="Space Invaders">
</div>

1. **DOOM (3D Raycasting):** Mikrodenetleyicilerde görmesi nadir olan, gerçek zamanlı 3D ortamlar, silah mekanikleri ve düşman yapay zekası **100+ FPS** hızında TFT ekrana akar. FreeRTOS triple-buffer + PSRAM textures.
2. **Wire3D (Space Shooter):** Wireframe 3D uzay savaşı. 114 FPS.
3. **Space Invaders:** Dalga dalga gelen uzaylılara karşı pürüzsüz mekanikler ve OLED ekran entegrasyonu.
4. **Galactic Strike:** Uzay gemisi ile düşman filolarına karşı savaş.
5. **Mode7 (Yarış):** SNES tarzı Mode 7 pseudo-3D yarış motoru.
6. **Platformer:** Yan kaydırmalı (side-scrolling) platform macerası.
7. **Arkanoid (Breakout):** Joystick hassasiyetinin ön planda olduğu, seviyeleri giderek zorlaşan tuğla kırma efsanesi.
8. **Pac-Man:** Özel yapay zeka ile kodlanmış hayaletler ve klasik labirent heyecanı.
9. **Flappy Bird:** Milisaniyelik tepkiler isteyen bağımlılık yapıcı donanım testi. 148 FPS.
10. **Snake:** Akıcı mekanikleriyle retro yılan oyunu.
11. **Launcher (E-OS):** Dönerek açılan animasyonlu carousel menü — PSP/PS3 tarzı UI.

<div align="center">
  <img src="docs/images/arkanoid1.jpg" width="30%" alt="Arkanoid">
  <img src="docs/images/pacman.jpg" width="30%" alt="Pacman">
  <img src="docs/images/snake.jpg" width="30%" alt="Snake">
</div>

---

## 📸 Screenshot Sistemi (Geçici Olarak Kapalı)
Oyunlarda ekran görüntüsü alma özelliği (SD Karta BMP yazdırma), **SPI veriyolu çakışması (TFT ve SD kart arası donma)** nedeniyle `dev_tools.h` üzerinden kalıcı olarak devre dışı bırakılmıştır. İlgili kayıt fonksiyonları kodlarda hala mevcuttur; gelecekte farklı bir SPI bus veya PSRAM üzerinden asenkron çözmek isteyen geliştiriciler yeniden aktifleştirip test edebilir.

---

## ✨ E-OS V2.1 Öne Çıkan Güncellemeleri
- **Güvenlik & Bellek:** Tüm oyunlardaki `strcpy` zafiyetleri `strncpy` ile kapatıldı. Stack/bellek taşması riskleri engellendi (Doom TaskRadar 20KB'a çıkarıldı).
- **OTA Koruması:** Bozuk güncelleme (.bin) dosyalarının cihazı çökertmesini engelleyen `0xE9` Magic Byte koruması eklendi.
- **Standartlaşma:** Tüm oyunlardaki donanım pinleri (`BUZZER`, `I2C` vs.) merkezileştirildi. Ortak API kullanımı için `GameBase.h` kütüphanesi tasarlandı ve oyunların yarısına entegre edildi.

---

## 🤝 Katkıda Bulunmak İsteyenler İçin (Good First Issues)
Projeyi daha da ileri taşımak isteyen geliştiriciler (contributors) için açık görevler:
1. **[Refactor] Launcher `delay()` Temizliği:** Ana menüdeki (Launcher) buton titreşimini engellemek (debounce) için kullanılan bloke edici (blocking) `delay()` döngülerinin asenkron `millis()` tabanlı yapıya geçirilmesi (Kritik).
2. **[Refactor] `GameBase.h` Entegrasyonu:** Kalan eski oyunların kendi içindeki ses ve ekran kapatma döngülerini bozmadan ortak `GameBase.h` sarmalayıcısına geçirilmesi.
3. **[Refactor] Sihirli Sayılar (Magic Numbers):** Eski oyun kodlarının içindeki `0xF800` gibi ham HEX renk kodlarının `#define COL_RED` gibi okunabilir makrolara dönüştürülmesi.

---

## 💻 Nasıl Derlenir?

### Gerekli Kütüphaneler
- `TFT_eSPI` — TFT ekran sürücüsü (ST7735)
- `U8g2` — OLED ekran sürücüsü (SH1106)
- `SD` — SD kart erişimi (Arduino core dahil)
- `Preferences` — NVS yüksek skor kaydı (Arduino core dahil)

### Kurulum Adımları
1. **TFT_eSPI kurulumu:** `User_Setup.h` dosyasını TFT_eSPI kütüphane klasörüne kopyalayın:
   ```
   Windows: C:\Users\<kullanıcı>\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
   ```
2. **Partitions:** Her oyun klasöründe `partitions.csv` dosyası mevcuttur (16MB Flash, OTA + SPIFFS).
3. **Board ayarları (Arduino IDE):**
   - Board: **ESP32S3 Dev Module**
   - Flash Size: **16MB (128Mb)**
   - PSRAM: **OPI 8MB**
   - Partition Scheme: **Custom** (partitions.csv otomatik kullanılır)
4. **Derleme:** Her oyun kendi klasöründe ayrı bir `.ino` dosyası olarak derlenir. `launcher.ino` ana OS'tir.

### Pin Bağlantıları
Pin tanımları `hardware_config.h` dosyasında merkezi olarak tanımlıdır.

| Pin | İşlev |
|-----|-------|
| 12 | SPI SCK |
| 11 | SPI MOSI |
| 42 | SPI MISO |
| 15 | TFT CS |
| 10 | SD CS |
| 41 | TFT DC |
| 8 | I2C SDA (OLED) |
| 9 | I2C SCL (OLED) |
| 1 | Joystick X |
| 2 | Joystick Y |
| 18 | Joystick SW |
| 3 | Buton A |
| 21 | Buton B |
| 4 | Buton C |
| 6 | Buton D |
| 5 | Buzzer |

---
<div align="center">
  <i>E-OS V2.1 Konsol Projesi | Sıfırdan Kodlandı</i>
</div>
