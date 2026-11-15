# 🚗 TURBO RACER v2 – ESP32 OLED Racing Game

A mini retro-style racing game built using an ESP32 WROOM and a 0.96-inch SSD1306 OLED display.

The game features three-lane racing, enemy vehicles, increasing speed, collision detection, score tracking, and an exciting FLY MODE with animated wings!

## 🎮 Project Features

- 128×64 monochrome OLED graphics
- Three-lane racing gameplay
- Player car movement
- Random enemy car generation
- Collision detection
- Increasing game speed and difficulty
- FLY MODE with animated wings
- Flight energy and recharge system
- Score and high-score tracking
- Game-over and restart system
- High score stored in ESP32 Preferences

## 🛠️ Components Required

| Component | Quantity |
|---|---:|
| ESP32 WROOM | 1 |
| 0.96-inch SSD1306 OLED (128×64) | 1 |
| Push buttons | 3 |
| Jumper wires | As required |
| Breadboard | 1 |
| USB cable | 1 |

## 🔌 Circuit Connections

### OLED Display

| OLED Pin | ESP32 Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Push Buttons

| Button | ESP32 GPIO |
|---|---|
| LEFT | GPIO 18 |
| RIGHT | GPIO 19 |
| START / FLY | GPIO 23 |

Connect the other terminal of every button to GND.

The project uses internal ESP32 pull-up resistors through `INPUT_PULLUP`.

## 💻 Software Requirements

- Arduino IDE
- ESP32 Arduino Board Package
- Adafruit SSD1306 Library
- Adafruit GFX Library
- Adafruit BusIO Library

The Wire and Preferences libraries are included with the ESP32 Arduino core.

## ⚙️ Installation

1. Download or clone this repository.
2. Open `TurboRacer_ESP32-4.ino` in Arduino IDE.
3. Install the required libraries.
4. Select ESP32 Dev Module.
5. Select your ESP32 COM port.
6. Connect the OLED and buttons according to the wiring table.
7. Upload the code.
8. Enjoy the game!

## 🕹️ How to Play

- Press START to begin.
- Press LEFT to move into the left lane.
- Press RIGHT to move into the right lane.
- Avoid enemy vehicles.
- Press START during gameplay to activate FLY MODE.
- Use flight mode to pass over enemy vehicles safely.
- Collect points and beat your high score.

### FLY MODE

- Flight duration: 5 seconds.
- Energy recharge: approximately 12 seconds.
- Flying speed boost: up to 1.8×.
- Passing enemy vehicles while flying earns 20 points.

## 🧠 Technologies Used

- Embedded C++
- Arduino IDE
- ESP32 microcontroller
- I2C communication
- SSD1306 OLED graphics
- Adafruit GFX
- ESP32 Preferences storage

## 📸 Project Preview

Add your project photos and gameplay video here.

## 🚀 Future Improvements

- Dedicated FLY button
- Sound effects
- More enemy vehicle designs
- Additional game modes
- Pause and resume functionality

## 👨‍💻 Author

Created as an ESP32 embedded systems and DIY gaming project.

If you like this project, give the repository a ⭐!

## 📜 License

MIT License
