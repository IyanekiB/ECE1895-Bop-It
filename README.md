# ECE1895 Bop It - Cyberpunk Katana Reaction Game

A handheld, cyberpunk-inspired reaction game built into a custom 3D-printed katana for ECE 1895. This embedded systems project combines real-time sensor processing, dynamic audio-visual feedback, and precision mechanical design into an engaging handheld gaming device.

## Overview

The Cyberpunk Katana houses a complete embedded gaming system within its hilt and blade. Players navigate an OLED menu to select difficulty modes, then respond to three distinct voiced commands within progressively shrinking time windows. The blade illuminates with custom NeoPixel animations while an accelerometer detects precise sword movements.

## Features

### Game Mechanics
- **Four Difficulty Modes**: Easy, Medium, Hard, and Quick mode with separate high score tracking
- **Three Unique Actions**:
  - **Swing It!** - Detected via MPU6050 accelerometer in blade tip (3g threshold)
  - **Sheath It!** - Spring-return switch activation in hilt
  - **Power Boost!** - Rotary encoder interaction in grip
- **Dynamic Difficulty Scaling**: Timer interval reduces with each point scored
- **Progressive Visual Feedback**: Blade color advances every 10 points through custom gradient effects
- **Persistent High Scores**: EEPROM storage with separate tracking for Quick Mode and standard modes

### Hardware Integration
- **Audio System**: Voiced command prompts via Adafruit FX Soundboard and micro-speaker
- **Visual Effects**: 30-60 NeoPixel LED strip embedded in blade with custom animations
- **User Interface**: 128x64 OLED display for menu navigation and game status
- **Power Management**: 18650 Li-Ion battery with TP4056 charging circuit and 3.3V regulation
- **Robust Enclosure**: Custom 3D-printed katana with internal wire routing and snap-in components

## Technical Specifications

### Core Hardware
- **Microcontroller**: ATmega328P (3.3V, Arduino UNO bootloader compatible)
- **Power System**: 18650 Li-Ion battery, TP4056 charger module, 3.3V LDO regulator, slide switch
- **Display**: SSD1306 128x64 OLED (I2C)
- **Audio**: Adafruit FX Soundboard with micro-speaker (active-low triggers)
- **Motion Sensing**: MPU6050 6-DOF IMU (I2C)
- **LED System**: Adafruit NeoPixel strip (30-60 pixels, <500mA total current)
- **User Controls**: Rotary encoder with push button, spring-return sheath switch, menu navigation buttons

### Software Architecture
- **Platform**: AVR-GCC via Arduino IDE
- **Key Libraries**: 
  - `Adafruit_NeoPixel` - LED blade control
  - `Adafruit_SSD1306` - OLED display driver
  - `MPU6050` - Accelerometer interface (I2C)
  - `EEPROM` - Persistent score storage
- **Memory Optimization**: 
  - Flash usage: 98% → 72% optimized
  - SRAM usage: 72% → 23% optimized
  - String literals stored in Flash using `F()` macro

### Mechanical Design
- **Enclosure**: Multi-part 3D-printed katana (handle, blade, mounting brackets)
- **Internal Routing**: Dedicated wire channels and component mounting points
- **Serviceability**: Snap-in OLED window, removable battery access panel
- **Durability**: Vibration-resistant design with robust component mounting

## Team Members

- **Nicholas Cessna** - Enclosure Design & 3D Modeling
- **Nurfadil Rafiuddin** - Hardware Design & System Integration  
- **Vish Thirukallam** - Schematic Design & PCB Layout
- **Iyan Nekib** - Software Architecture & Firmware Development

## Getting Started

### Prerequisites
- Arduino IDE with ATmega328P support
- 3D printer capable of printing PLA/PETG parts
- Basic soldering equipment
- Micro-usb for audio files

### Hardware Assembly

1. **Print Enclosure Components**
   - Use files in `Enclosure_Design_Files/` directory
   - Test-fit display, buttons, encoder, and blade strip before final assembly

2. **PCB Assembly**
   - Follow schematic in `Altium_Files/` directory
   - Install PCB and mainboard in hilt according to wiring diagram
   - Connect OLED, FX Soundboard, NeoPixels, switches, and battery per pin mapping

3. **Software Installation**
   - Flash the latest `Iterations/iteration_6_katana_it/iteration_6_katana_it.ino` file to ATmega328P (Arduino UNO bootloader recommended)
   - Copy provided audio files to FX Soundboard micro-usb

### Operation

1. **Power On**: Use slide switch to activate system
2. **Mode Selection**: Navigate OLED menu to choose difficulty (Easy/Medium/Hard/Quick)
3. **Gameplay**: 
   - Press Start button to begin
   - Respond to audio prompts within time limit
   - Score points for correct responses
   - Reach 99 points for victory celebration
4. **High Scores**: Automatically saved to EEPROM with separate Quick Mode tracking
5. **Power Off**: Use slide switch when finished

## Game Flow

```
System Initialization
    ↓
OLED Menu (Mode Selection)
    ↓
Wait for Start Button
    ↓
Game Loop:
  → Random Command (Audio + OLED)
  → Start Timer
  → Detect Input (IMU/Button/Switch/Encoder)
  → Validate Response
  → LED Feedback (Success/Failure)
  → Update Score & Difficulty
  → Check Win/Loss Conditions
    ↓
Game Over Screen with High Scores
```

## Technical Achievements

### Performance Optimization
- Accelerometer calibration through "Goldilocks trials" for optimal sensitivity
- Memory-constrained optimization for ATmega328P platform
- Real-time sensor fusion and response validation
- Power-efficient design with <500mA current limiting

### System Integration
- Multi-subsystem coordination (audio, visual, sensor, control)
- Mechanical packaging with electronic integration
- Modular design allowing component replacement and maintenance
- Power management and charging system

## Repository Structure

```
ECE1895-Bop-It/
├── README.md                         # This file
├── Altium_Files/                     # PCB design and schematics
│   ├── Button_Breakout_GERBER.zip    # Contains GERBER and NC Drill files
│   └── ....                          # Rest of the PCB layout and Schematics
├── Components/                       # Contains incremental code for the components used
│   ├── Encoder_RGB                   # Code for encoder switches colors (Uses a color matrix based on encoder ticks)
│   ├── Inputs                        # Code for all three inputs used (segregated within the folder)
│   ├── OLED                          # Code(s) for how the OLED is to react during the game
│   ├── OLED_Accel                    # Code that tests out the Accelerometer and the OLED together
│   └── counter_test                  # Code to check whether the counter for each successive input is stored correctly
├── Enclosure_Design_Files/           # 3D printable parts
│   ├── Blade.stl                     # Blade model
│   ├── Blade_Rail.stl                # LED strip housing (goes within blade)
│   ├── Coupler.stl                   # For making the blade and handle sit
│   ├── Handle.stl                    # Handle model
│   ├── Handle_Cover.stl              # Handle Cover model
│   └── Sheath.stl                    # The Sheath-It Cover that goes on the Handle
├── Iterations/                       # Iterations of each instance of the Bop-It project
│   ├── iteration_1_katana_it         # Contains the prelimanary Iteration w/ basic functionality
│   ├── iteration_2_katana_it         # Contains more in-depth regarding the inputs
│   ├── iteration_3_katana_it         # Ran into memeory issues so restructured code to better fit with memory usage
│   ├── iteration_4_katana_it         # Contains stable code which runs with audio sound board
│   ├── iteration_5_katana_it         # Comtains code that was tweaked to fit a different LED strip
│   └── iteration_6_katana_it         # Most stable version of the project (Currently used)
├── LED_Animations/                   # Conceptual animations for Input/Success/Game Over
│   ├── 30LED_Rainbow                 # Animation for Boost-It!
│   ├── 30LED_Success                 # Animation for every correct input
│   └── 60LED_Success                 # Animation for every correct input (60 LED)
├── sensor_examples/                  # Code examples for limiting memory usage (If needed)
│   ├── SDFatExample.ino              # Reduces the usage of the SD Card by 20%
│   └── accel.cpp                     # Contains code for receiving Accelerometer data
└── Pseudocode for Bop-It.txt         # Inital Pseudocode for the project
```

## Acknowledgments

Special thanks to the ECE 1895 course staff and the University of Pittsburgh Electrical and Computer Engineering Department for project guidance and laboratory resources.
