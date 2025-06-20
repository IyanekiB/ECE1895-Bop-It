# ECE1895-Bop-It
# Cyberpunk “Katana” BOP-IT!

A handheld, cyberpunk-style reaction game built into a 3D-printed katana. The hilt houses an ATmega328P, OLED menu, speaker and controls; the blade contains NeoPixel LEDs and an accelerometer. Players choose Easy/Medium/Hard, then must respond to three voiced prompts—`Swing It!`, `Sheath It!` and `Power Boost!`—within a shrinking timer. Correct swings, sheath-backs and button/encoder presses score points; every 10 successes the blade’s color advances and the game speeds up. Reach 99 points for a celebratory LED flourish, or on failure see **GAME OVER** plus your current and all-time best score (stored in EEPROM).

---
## 👥 Team Members
- **Modeling:** Nicholas Cessna  
- **Integration:** Nurfadil Rafiuddin  
- **Circuitry:** Vish Thirukallam  
- **Software:** Iyan Nekib  

---

## 🔹 Key Features
- **Mode Select** (Easy / Medium / Hard) via OLED menu  
- **Three Unique Actions**  
  - **Swing It!** (blade-tip accelerometer)  
  - **Sheath It!** (spring-return switch in hilt)  
  - **Power Boost!** (button or rotary encoder + NeoPixel blade)  
- **Audio Cues** for each command  
- **Dynamic Difficulty:**  
  - Timer decreases every 10 points  
  - Blade color advances every 10 points  
- **Visual Feedback:**  
  - Green flashes on success  
  - Red flashes on failure  
- **High-Score Persistence** in EEPROM  
- **Game Over Display** on OLED

---

## 📦 Hardware
- **MCU:** ATmega328P (3.3 V)  
- **Power:** 18650 Li-Ion + slide switch + 3.3 V regulator + TP4056 charger  
- **Display:** OLED or RGB backlit 16×2 LCD  
- **Audio:** Micro-speaker + simple driver (WAV via SD or PWM)  
- **Sensors & Controls:**  
  - BHI160B accelerometer (blade tip)  
  - Spring-return sheath switch (hilt)  
  - Boost button or PEC11R rotary encoder (grip)  
- **LEDs:** Adafruit NeoPixel strip along blade  
- **Storage:** EEPROM (high score) + optional micro-SD for audio files  

---

## ⚙️ Software
- **Platform:** AVR/GCC on ATmega328P  
- **Workflow:**  
  1. Power on & select mode  
  2. Press **Start** → game loop begins  
  3. Random command → play audio cue → start timer  
  4. Detect correct input before timeout → update score/level  
  5. Provide LED & tone feedback → repeat or Game Over  
- **Libraries:**  
  - `Adafruit_NeoPixel`  
  - `SSD1306` (or LCD driver)  
  - `EEPROM`  
  - `SPI` / `SD` (optional for audio)  

---
