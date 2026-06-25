<div align="right">
  <a href="README.md">🇹🇷 Türkçe</a> | <b>🇬🇧 English</b>
</div>

<div align="center">
  <img src="docs/images/hero.jpg" alt="E-OS Console" width="600" style="border-radius: 12px;"/>
  <br/><br/>
  <h1>E-OS — ESP32-S3 Handheld Console</h1>
  <p>An ESP32-S3 based, dual-screen (TFT+OLED) handmade gaming console project running on a FreeRTOS architecture.</p>
  
  <p>
    <img src="https://img.shields.io/badge/MCU-ESP32--S3-blue?style=for-the-badge&logo=espressif" alt="ESP32-S3"/>
    <img src="https://img.shields.io/badge/OS-FreeRTOS-yellow?style=for-the-badge" alt="FreeRTOS"/>
    <img src="https://img.shields.io/badge/RAM-8MB_PSRAM-purple?style=for-the-badge" alt="PSRAM"/>
    <img src="https://img.shields.io/badge/Flash-16MB-green?style=for-the-badge" alt="Flash"/>
  </p>
  
  <h3>
    <a href="https://emir173.github.io/esp32-console/">🌐 Project Website</a>
  </h3>
</div>

---

## 🚀 About the Project
This project is a handheld console developed from scratch using the ESP32-S3 microcontroller. Without relying on any pre-built UI frameworks or emulators, the operating system (E-OS) and the game engines were custom coded in C++ to run specifically on this hardware.

### ⚙️ Hardware Architecture
- **Processor:** Dual-Core ESP32-S3 running at 240 MHz.
- **Dual Display:** 
  - *Main Screen:* 160x128 Color TFT (SPI). Handles the main game rendering and UI.
  - *Secondary Screen:* 128x64 OLED (I2C). Located at the top of the device; displays system status and high scores.
- **Memory:** 16MB Flash + 8MB PSRAM OPI. Massive memory bandwidth ensures a stutter-free experience.
- **Audio & Controls:** 8-bit buzzer and an analog joystick (with hardware deadzone filtering).
- **Storage:** Micro SD Card integration for game assets.

---

## 🧠 Software Architecture (E-OS)
- **FreeRTOS:** One processor core (Core 0) handles the game logic, while the other core (Core 1) is entirely dedicated to screen rendering.
- **Carousel UI:** Features an animated, rotating carousel menu design for navigating between games.
- **Hardware Pause:** Thanks to RTOS task management, games can be paused instantly at the hardware level.

---

## 🕹️ 11 Custom Games
All games are heavily optimized for the device's resolution and hardware limits.

<table>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Doom/SCR_042.bmp" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>DOOM (3D Raycasting):</b> 3D environments and enemy AI (FreeRTOS triple-buffer + PSRAM textures).</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Wire%203D/1.bmp" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>Wire3D:</b> Wireframe space shooter.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/SpaceInvaders/SCR_024.bmp" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>Space Invaders:</b> Classic alien shooting mechanics with OLED integration.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Galacticstrike/SCR_036.bmp" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>Galactic Strike:</b> Spaceship themed combat game.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Mode%207/SCR_031.bmp" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>Mode7:</b> SNES style pseudo-3D racing engine.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Platformer/SCR_030.bmp" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>Platformer:</b> Side-scrolling platform adventure.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Arkanoid/SCR_012.bmp" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>Arkanoid:</b> Joystick-controlled brick breaker.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Pacman/SCR_005.bmp" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>Pac-Man:</b> Classic maze game with custom ghost AI.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Flappy/SCR_008.bmp" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>Flappy Bird:</b> Timing-based skill game.</td>
  </tr>
  <tr>
    <td width="200" align="center"><img src="docs/screenshots/Snake/SCR_004.bmp" width="180" style="border-radius: 6px;"></td>
    <td valign="middle"><b>Snake:</b> Classic snake game.</td>
  </tr>
</table>

*Note: The system also includes the animated **E-OS Launcher** interface which acts as the main OS.*

---

## 📸 Screenshot System (Temporarily Disabled)
The in-game screenshot feature (writing BMPs to the SD Card) is permanently disabled in `dev_tools.h` due to an **SPI bus conflict (freezing between TFT and SD card)**. The related logging functions are still present in the code; interested developers can reactivate them to experiment with an asynchronous SPI bus or PSRAM based solution in the future.

---

## ✨ Highlighted Features
- **Security & Memory:** Addressed memory vulnerabilities across the code and increased stack limits.
- **OTA Protection:** Added Magic Byte protection to prevent corrupted updates from bricking the device.
- **Standardization:** Centralized hardware pin definitions. Created the `GameBase.h` wrapper for common API usage.

---

## 🤝 Good First Issues (Contributing)
Open tasks for developers looking to contribute to the project:
1. **[Refactor] Launcher `delay()` Cleanup:** Replace the blocking `delay()` loops used for button debouncing in the main menu with an asynchronous `millis()` based structure.
2. **[Refactor] `GameBase.h` Integration:** Port the remaining older games into the unified `GameBase.h` architecture.
3. **[Refactor] Magic Numbers:** Convert raw HEX color codes (e.g. `0xF800`) in older game files to readable macros.

---

## 💻 How to Compile

### Required Libraries
- `TFT_eSPI` — TFT display driver (ST7735)
- `U8g2` — OLED display driver (SH1106)
- `SD` — SD card access
- `Preferences` — NVS high-score saving

### Setup Steps
1. **TFT_eSPI Setup:** Copy the `User_Setup.h` file into your TFT_eSPI library folder:
   ```
   Windows: C:\Users\<user>\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
   ```
2. **Partitions:** Use the `partitions.csv` file provided in each game folder.
3. **Board Settings (Arduino IDE):**
   - Board: **ESP32S3 Dev Module**
   - Flash Size: **16MB (128Mb)**
   - PSRAM: **OPI 8MB**
   - Partition Scheme: **Custom** (partitions.csv)
4. **Compilation:** Each game is compiled as a separate `.ino` file in its own folder. `launcher.ino` is the main OS.

### Pin Configuration

| Pin | Function |
|-----|----------|
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
| 3 | Button A |
| 21 | Button B |
| 4 | Button C |
| 6 | Button D |
| 5 | Buzzer |

---
<div align="center">
  <i>E-OS Console Project</i>
</div>
