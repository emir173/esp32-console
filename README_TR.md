<div align="right">
  <b>Türkçe</b> | <a href="README.md">English</a>
</div>

<div align="center">
  <img src="docs/images/Front.jpg" alt="E-OS Console" width="350" style="border-radius: 12px;"/>
  <br/><br/>
  <h1>E-OS — ESP32-S3 El Konsolu</h1>
  <p>ESP32-S3 tabanlı, çift ekranlı (TFT+OLED) ve FreeRTOS mimarisi üzerinde çalışan el yapımı oyun konsolu projesi.</p>
  
  <p>
    <img src="https://img.shields.io/badge/MCU-ESP32--S3-blue?style=for-the-badge&logo=espressif" alt="ESP32-S3"/>
    <img src="https://img.shields.io/badge/OS-FreeRTOS-yellow?style=for-the-badge" alt="FreeRTOS"/>
    <img src="https://img.shields.io/badge/RAM-8MB_PSRAM-purple?style=for-the-badge" alt="PSRAM"/>
    <img src="https://img.shields.io/badge/Flash-16MB-green?style=for-the-badge" alt="Flash"/>
  </p>
  
  <h3>
    <a href="https://emir173.github.io/esp32-console/">🌐 Proje Web Sitesi</a>
  </h3>
</div>

---

## Proje Hakkında
Bu proje, ESP32-S3 mikrodenetleyicisi kullanılarak sıfırdan geliştirilmiş bir el konsoludur. Herhangi bir hazır arayüz veya emülatör kullanılmadan, işletim sistemi (E-OS) ve oyun motorları donanıma özel olarak C++ ile kodlanmıştır.

### Donanım Mimarisi
- **İşlemci:** 240 MHz hızında çalışan Dual-Core ESP32-S3.
- **Çift Ekran:** 
  - *Ana Ekran:* 160x128 Renkli TFT (SPI). Ana oyun akışı ve UI.
  - *İkinci Ekran:* 128x64 OLED (I2C). Cihazın üst kısmında durum bilgileri ve skorlar için kullanılır.
- **Bellek:** 16MB Flash + 8MB PSRAM OPI. Geniş bellek kapasitesi sayesinde akıcı bir deneyim sunar.
- **Ses ve Kontrol:** 8-bit buzzer ve analog joystick (deadzone filtreli).
- **Depolama:** Oyun verileri için Micro SD Kart entegrasyonu.

---

## Yazılım Mimarisi (E-OS)
- **FreeRTOS:** İşlemcinin bir çekirdeği (Core 0) oyun mantığını işlerken, diğer çekirdeği (Core 1) ekran çizimi (rendering) işlemlerini yürütür.
- **Carousel UI:** Oyunlar arası geçişler için dönerek açılan, animasyonlu bir arayüz tasarımı mevcuttur.
- **Donanımsal Duraklatma (Pause):** RTOS görev yönetimi sayesinde oyunlar donanımsal olarak anında duraklatılabilir.

---

## 15 Adet Özel Oyun
Tüm oyunlar cihazın çözünürlüğüne ve donanım limitlerine göre optimize edilmiştir.

<table>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/DOOM/D3_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>DOOM:</b> Doom tarzı raycasting motoru.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Mode7/M2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>MODE 7 RACING:</b> Klasik yarış mekanikleri. Checkpointleri toplayarak rakiplerini geride bırak.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Wireframe3D/W2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>WIRE-FRAME 3D:</b> Üç boyutlu uzay savaşları.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/GalacticStrike/G2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>GALACTIC STRIKE:</b> Düşman filolarına karşı hayatta kal, güçlendirmeleri topla ve bölüm sonu canavarlarını yen.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Platformer/P2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>PLATFORMER:</b> Engelleri aş, tuzaklardan kaç ve bölümleri geç.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/SpaceInvaders/S2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>SPACE INVADERS:</b> Dalga dalga gelen uzaylılara karşı hayatta kal.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Arkanoid/A2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>ARKANOID:</b> Giderek hızlanan topla tüm tuğlaları kır.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Pacman/P2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>PAC-MAN:</b> Hayaletlerden kaç, labirentteki tüm noktaları topla.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Flappy/F3_Gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>FLAPPY BIRD:</b> Boru engellerinin arasından dikkatlice uç.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Snake/S2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>SNAKE:</b> Kuyruğunu uzat, duvarlara ve kendine çarpma.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Tetris/T2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>TETRIS:</b> Blokları yerleştir, satırları temizle; seviye yükseldikçe hızlan.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Dungeon/D5_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>DUNGEON:</b> Boss savaşları, büyü sistemi, tüccar ve farklı biyomlarla top-down zindan keşfi.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/TowerDefense/T3_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>TOWER DEFENSE:</b> Dalga tabanlı strateji: kule yerleştir, yükselt ve gelen dalgaları çağırarak yönet.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Game2048/2048_2_Gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>2048:</b> Aynı sayıları birleştirerek 2048 karosuna ulaşmaya çalışın.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Rhythm/R2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>RHYTHM:</b> Müziğin ritmine ayak uydurun ve neon notaları yakalayın.</td>
  </tr>
</table>


## 3 Uygulama

<table>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Flight/F2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>FLIGHT TRACKER:</b> Yakın zamanlı uçuş takip sistemi. OpenSky Network API kullanarak uçakları izleyin.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Tools/T1.png" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>TOOLS:</b> Kronometre ve metronom içeren temel bir araç uygulaması.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Draw/D2_gif.gif" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>DRAW (ETCH-A-SKETCH):</b> Klasik Etch-a-Sketch tarzı çizim uygulaması. Modları değiştirerek renkli çizimler yapın.</td>
  </tr>
</table>

## İşletim Sistemi (E-OS Launcher)
Tüm oyunlara ev sahipliği yapan akıcı, dönen (carousel) ana menü arayüzü olan **E-OS Launcher**, konsolun çekirdeğini oluşturur.

- **Donanımsal Duraklatma:** Hangi oyunda olursan ol, joystick butonuna basınca ilgili RTOS task'ı anında askıya alınır.
- **Double-Buffered Menü:** Dönen carousel menü, ekran yırtılması olmadan akar — telefon hissiyatına yakın geçişler sunar.
- **OTA Bootloader:** SD karttan binary okuyup flash'a yazar. RTC magic value ile reboot sonrası oyun seçimi hatırlanır.
- **Donanımsal Ses Kontrolü:** Settings menüsü üzerinden sistem genelindeki sesi kapatın, kısın (LOW) veya tam seste oynayın (HIGH).
- **Canlı FPS Göstergesi:** Geliştiriciler için tüm oyunların köşesinde gerçek zamanlı kare hızını (FPS) anlık olarak görebilme imkanı.
- **Asenkron Çift Çekirdek:** Core 0 sadece arka plan görevlerini ve oyun mantığını işlerken, Core 1 tamamen grafik çizimine (render) adanmıştır.

---

## Screenshot Sistemi (USB Üzerinden)
Ekran görüntüleri ve GIF'ler **USB seri bağlantı** üzerinden alınır. Oyun 60 FPS akmaya devam ederken framebuffer bilgisayara aktarılır.
Bu veriyi bilgisayarınızda PNG veya GIF olarak işleyip kaydetmek için ana dizindeki `tools/` klasörü içerisinde bulunan Python araçları (`capture.py`, `capture_gif.py` vb.) kullanılmaktadır.

---

## Nasıl Derlenir?

### Gerekli Kütüphaneler
- `TFT_eSPI` — TFT ekran sürücüsü (ST7735)
- `U8g2` — OLED ekran sürücüsü (SH1106)
- `SD` — SD kart erişimi
- `Preferences` — NVS yüksek skor kaydı

### Kurulum Adımları
1. **TFT_eSPI kurulumu:** `User_Setup.h` dosyasını TFT_eSPI kütüphane klasörüne kopyalayın:
   ```
   Windows: C:\Users\<kullanıcı>\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
   ```
2. **Partitions:** Her oyun klasöründe bulunan `partitions.csv` dosyasını kullanın.
3. **Board ayarları (Arduino IDE):**
   - Board: **ESP32S3 Dev Module**
   - Flash Size: **16MB (128Mb)**
   - PSRAM: **OPI 8MB**
   - Partition Scheme: **Custom** (partitions.csv)
4. **Derleme:** Her oyun kendi klasöründe ayrı bir `.ino` dosyası olarak derlenir. `launcher.ino` ana OS'tir.

### Pin Bağlantıları

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
  <i>E-OS Konsol Projesi</i>
</div>
