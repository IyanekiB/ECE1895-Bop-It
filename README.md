# ECE1895-Bop-It
## Cyberpunk “Katana” BOP-IT!

A handheld, cyberpunk-inspired reaction game built into a custom 3D-printed katana. The hilt houses an ATmega328P, OLED menu, speaker, and controls; the blade integrates NeoPixel LEDs and an accelerometer. Players choose from four modes (Easy, Medium, Hard, Quick) and must respond to three voiced prompts—**Swing It!**, **Sheath It!**, and **Power Boost!**—within a shrinking timer. Correct swings, sheath-backs, and boost actions score points; every 10 successes, the blade’s color advances and the game speeds up. Reach 99 points for a celebratory LED flourish, or on failure see **GAME OVER** plus your current and all-time best score (stored in EEPROM).

---

## Team Members
- **Enclosure Design:** Nicholas Cessna  
- **Hardware Design & Integration:** Nurfadil Rafiuddin  
- **Schematic Design:** Vish Thirukallam  
- **Software Design:** Iyan Nekib  

---

## Key Features

- **Mode Select** (Easy / Medium / Hard / Quick) via OLED menu
- **Three Unique Actions**  
  - **Swing It!** (MPU6050 accelerometer in blade tip)  
  - **Sheath It!** (spring-return switch in hilt)  
  - **Power Boost!** (rotary encoder/button in grip)
- **Audio Cues** for each command (Adafruit FX Soundboard)
- **Dynamic Difficulty:**  
  - Timer interval reduces with each point  
  - Blade color advances every 10 points
- **Visual Feedback:**  
  - Custom NeoPixel animations (green, red, blue, yellow)  
  - “Katana” style gradient effects  
- **Persistent High Scores** in EEPROM (Quick Mode and standard modes are tracked separately)
- **Game Over Display** on OLED

---

## Hardware Overview

- **MCU:** ATmega328P (3.3 V)
- **Power:** 18650 Li-Ion + slide switch + 3.3 V LDO + TP4056 charger
- **Display:** 128x64 SSD1306 OLED
- **Audio:** Micro-speaker via Adafruit FX Soundboard (active-low triggers)
- **Sensors & Controls:**  
  - **MPU6050** accelerometer for “Swing It!”  
  - **Spring-return sheath switch** for “Sheath It!”  
  - **Rotary encoder** for “Boost It!”  
- **LEDs:** Adafruit NeoPixel strip (30–60 pixels) embedded in blade
- **Enclosure:**  
  - 3D-printed handle, blade, brackets  
  - Wire routing through internal channels  
  - Snap-in window for OLED & speaker  
  - Removable battery access panel

---

## Software Overview

- **Platform:** AVR/GCC (Arduino IDE) on ATmega328P
- **Main Flow:**
  1. Initialization of all hardware (display, NeoPixel, IMU, FX board, controls)
  2. User selects mode (Easy/Medium/Hard/Quick) via OLED menu
  3. Wait for Start button, then game loop begins
  4. Random command (audio + OLED prompt), start timer
  5. Detect and validate input (IMU/button/switch/encoder) before timeout
  6. Provide LED feedback (success/failure)
  7. Update score, shrink timer, and advance blade color as needed
  8. Repeat or trigger Game Over with high score display

- **Key Libraries:**
  - `Adafruit_NeoPixel` – NeoPixel blade
  - `Adafruit_SSD1306` – OLED display
  - `EEPROM` – persistent score storage
  - `MPU6050` – accelerometer (I2C)
- **Memory & Performance:**
  - Strings stored in Flash using `F()` macro to reduce SRAM usage
  - Unused libraries removed; OLED buffer reduced
  - SRAM usage optimized (98% → 72% Flash; 72% → 23% SRAM)
- **Input Processing:**
  - “Swing It!”: Uses magnitude of acceleration vector, normalized to G’s
  - Threshold (`~3g`) set after Goldilocks trials to maximize true positives and minimize false triggers
- **Enclosure Integration:**
  - Pin mapping and brightness capped to <500 mA for safety
  - PCB, display, and battery mount into a robust 3D-printed shell
  - Internal wire guides, removable panels, and vibration-resistance tested

---

## How to Build & Reproduce

1. **Print enclosure parts** (`Enclosure_Design_Files/`) and test-fit display, buttons, encoder, blade strip.
2. **Assemble PCB** and mainboard, install in hilt and blade as per wiring diagram (`Altium_Files/`).
3. **Wire up** OLED, FX Soundboard, NeoPixels, switches, and battery following the pin mapping.
4. **Flash the latest `.ino`** (from the root) to ATmega328P (Arduino UNO bootloader recommended).
5. **Copy audio files** to the FX Soundboard micro-SD card.
6. **Power on and play!**
    - Select mode and respond to prompts.
    - Scores are saved; Quick Mode high scores are tracked separately.
    - Power off any time via the slide switch.
