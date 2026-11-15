# 🚗 FARO SPORTS – ESP32 POWER

### A Mini Retro Racing Game Powered by ESP32

FARO SPORTS is an exciting retro-style racing game developed using an ESP32 WROOM microcontroller and a 0.96-inch SSD1306 OLED display.

Select your favourite car, avoid enemy traffic, activate FLY MODE, use special car powers, escape the police helicopter and beat your high score!

## 🎮 Features

- Animated FARO SPORTS splash screen
- Four selectable racing cars
- Three-lane racing gameplay
- Animated car sprites
- FLY MODE with animated wings
- Four unique special abilities
- Enemy traffic and collision detection
- Police helicopter chase system
- Police shooting and bullet detection
- Five-level wanted star system
- Player health system
- Score and high-score tracking
- Increasing game speed and difficulty
- Game-over and restart system
- High-score and car selection saved in ESP32 memory

## 🛠️ Hardware Requirements

| Component | Quantity |
|---|---:|
| ESP32 WROOM | 1 |
| SSD1306 0.96-inch OLED 128×64 | 1 |
| Push buttons | 4 |
| Breadboard | 1 |
| Jumper wires | As required |
| USB cable | 1 |

## 🔌 Circuit Connections

### OLED Display

| OLED | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Push Buttons

| Button | GPIO | Function |
|---|---:|---|
| LEFT | 18 | Move left |
| RIGHT | 19 | Move right |
| START / FLY | 23 | Start / Flight |
| POWER | 4 | Special ability |

Connect the other terminal of each push button to GND.

The program uses ESP32 internal pull-up resistors (`INPUT_PULLUP`).

## 🕹️ Game Controls

- LEFT: Move the car to the left lane.
- RIGHT: Move the car to the right lane.
- START: Start or restart the game.
- START during gameplay: Activate FLY MODE.
- POWER: Activate the selected car's special ability.

## 🏎️ Selectable Cars and Special Powers

### 1. TURBO – NITRO
- Speed boost: 1.45×
- Duration: 3 seconds
- Recharge: 10 seconds

### 2. VIPER – GHOST
- Become invisible.
- Police lose track of the player.
- Police bullets pass through.
- Duration: 5 seconds
- Recharge: 14 seconds

### 3. BRUTE – ROCKET
- Starts with 5 rockets.
- Fires rockets forward.
- Destroys enemy cars on impact.
- Each successful hit earns 30 points and one wanted star.
- One rocket reloads every 8 seconds.

### 4. COMET – SLOWMO
- Slows down the entire game.
- Slow-motion factor: 0.5×
- Duration: 4 seconds
- Recharge: 13 seconds

## ✈️ FLY MODE

FLY MODE is available for all four cars.

- Activated using START.
- Flight duration: 5 seconds.
- Flight recharge: 12 seconds.
- Speed multiplier: up to 1.8×.
- Animated wing deployment.
- Protects against traffic collisions.
- Landing provides 0.8 seconds of temporary safety.
- Passing enemy cars during flight earns double points.

## 🚔 Police and Wanted System

The police helicopter chases the player when wanted stars are activated.

- Maximum wanted level: 5 stars.
- Speed level 4: 1 wanted star.
- Speed level 7: 2 wanted stars.
- Speed level 10: 3 wanted stars.
- Destroying enemy cars with rockets earns wanted stars.
- Police helicopter follows and shoots at the player.
- Higher wanted levels increase police aggression.
- Twin bullets are fired at 4 or more wanted stars.
- Police bullets reduce player health.
- HP reaching zero ends the game.

## 🎯 How to Play

1. Power on the ESP32.
2. Watch the FARO SPORTS splash screen.
3. Select a car using LEFT and RIGHT.
4. Press START to confirm your car.
5. Wait for the 3-2-1-GO countdown.
6. Avoid enemy vehicles.
7. Activate FLY MODE using START.
8. Activate special powers using POWER.
9. Avoid police bullets and protect your health.
10. Beat your high score!

### Game Over Controls

- START: Retry with the same car.
- LEFT / RIGHT: Return to car selection.

## 💻 Software Requirements

- Arduino IDE
- ESP32 Arduino Board Package (core 2.x or 3.x)
- Adafruit GFX Library
- Adafruit SSD1306 Library
- Adafruit BusIO Library

Wire and Preferences are included with the ESP32 Arduino core.

## ⚙️ Installation and Upload

1. Download this repository.
2. Open `FaroSports_ESP32-powergta5.ino` in Arduino IDE.
3. Install the required libraries.
4. Select ESP32 Dev Module.
5. Select the correct COM port.
6. Connect the OLED and four buttons.
7. Click Verify.
8. Upload the program to your ESP32.
9. Enjoy FARO SPORTS!

## 🧠 Technologies Used

- ESP32 Microcontroller
- Embedded C++
- Arduino IDE
- I2C Communication
- SSD1306 OLED Graphics
- Adafruit GFX
- ESP32 Preferences
- Game logic and collision detection

## 📸 Project Gallery

Add your own project photographs here:

- Complete hardware setup
- OLED gameplay
- Car selection menu
- Wiring connections

## 🚀 Future Improvements

- Additional racing cars
- More special abilities
- Sound effects
- More enemy vehicle designs
- Additional game modes
- Improved animations

## 👨‍💻 Project Name

**FARO SPORTS – ESP32 POWER GTA 5**

A DIY embedded gaming project built with creativity and electronics.

If you like this project, please give the repository a ⭐ on GitHub!

## 📜 License

MIT License
