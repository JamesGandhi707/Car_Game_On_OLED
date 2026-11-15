# 🚗 FARO SPORTS – ESP32 OLED RACING GAME

### A Mini Retro Racing Game with FLY MODE, Special Powers and Police Helicopter!

FARO SPORTS is a retro-style car racing game built using an ESP32 WROOM microcontroller and a 0.96-inch SSD1306 OLED display.

Choose from four different cars, avoid enemy traffic, activate FLY MODE, use special car powers, launch rockets and escape the police helicopter!

---

## 🎮 PROJECT FEATURES

- Animated FARO SPORTS splash screen
- Four selectable racing cars
- Three-lane racing gameplay
- Animated player and enemy cars
- FLY MODE with animated wings
- Four unique car special powers
- Rocket shooting and enemy destruction
- 3D-style slow-motion explosion effects
- Police helicopter chase system
- Wanted star system (maximum 5 stars)
- Player health system
- Police shooting and bullet detection
- Increasing game speed and difficulty
- Score and high-score system
- High score saved in ESP32 memory
- Car selection saved in ESP32 memory
- Game-over and restart system

---

## 🛠️ COMPONENTS REQUIRED

| Component | Quantity |
|---|---:|
| ESP32 WROOM / DevKit | 1 |
| 0.96-inch SSD1306 OLED 128×64 | 1 |
| Push buttons | 4 |
| Breadboard | 1 |
| Jumper wires | As required |
| USB cable | 1 |

---

## 🔌 CIRCUIT CONNECTIONS

### OLED DISPLAY CONNECTIONS

| OLED Pin | ESP32 Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### FOUR PUSH BUTTON CONNECTIONS

| Button | ESP32 GPIO | Function |
|---|---:|---|
| LEFT | GPIO 18 | Move car left |
| RIGHT | GPIO 19 | Move car right |
| START / FLY | GPIO 23 | Start game / Activate FLY |
| POWER | GPIO 4 | Activate special car power |

### BUTTON WIRING

Connect one terminal of each push button to its assigned ESP32 GPIO pin.

Connect the other terminal of each button to GND.

The program uses internal pull-up resistors through `INPUT_PULLUP`, so external pull-up resistors are not required.

**Important:** All buttons and the OLED must share a common GND.

---

## 🕹️ GAME CONTROLS

| Button | Action |
|---|---|
| LEFT | Move car to left lane |
| RIGHT | Move car to right lane |
| START | Start the selected car |
| START during gameplay | Activate FLY MODE |
| POWER | Activate selected car's special ability |

### EXTRA CONTROL

Hold LEFT + RIGHT together for 3 seconds to reset the saved high score.

---

## 🏎️ FOUR SELECTABLE CARS AND SPECIAL POWERS

### 1. TURBO – NITRO ⚡

- Special ability: Speed boost
- Speed multiplier: 1.45×
- Duration: 3 seconds
- Recharge time: 10 seconds
- Provides a temporary speed advantage.

### 2. VIPER – GHOST 👻

- Special ability: Invisibility
- Duration: 5 seconds
- Recharge time: 14 seconds
- Makes the player car invisible.
- Police helicopter loses track of the player.
- Police helicopter temporarily stops chasing and shooting.

### 3. BRUTE – ROCKET 🚀

- Special ability: Rocket launcher
- Initial ammunition: 5 rockets
- Fires rockets straight ahead.
- Rocket cooldown: 0.30 seconds
- One rocket reloads every 8 seconds.
- Maximum 3 active rockets.
- Hitting an enemy car destroys it and creates an explosion.
- Each successful hit gives 30 points and one wanted star.

### 4. COMET – SLOWMO ⏳

- Special ability: Slow motion
- Slow-motion multiplier: 0.50×
- Duration: 4 seconds
- Recharge time: 13 seconds
- Slows down the entire game world.

---

## ✈️ FLY MODE

FLY MODE is available for all four cars.

- Activated using the START button during gameplay.
- Flight duration: 5 seconds.
- Recharge time: approximately 12 seconds.
- World speed multiplier: up to 1.8×.
- Animated wing deployment and retraction.
- Player car rises during flight.
- Flying protects against enemy traffic collisions.
- Passing enemy cars during flight earns double points.
- Landing provides 0.8 seconds of temporary protection.

---

## 🚔 POLICE HELICOPTER AND WANTED SYSTEM

The game features a police helicopter that follows the player when the wanted level becomes active.

### WANTED STAR SYSTEM

| Game Condition | Wanted Stars |
|---|---:|
| Speed Level 4 | 1 Star |
| Speed Level 7 | 2 Stars |
| Speed Level 10 | 3 Stars |
| Destroy an enemy with a rocket | +1 Star |

- Maximum wanted level: 5 stars.
- Police helicopter follows the player's horizontal position.
- Police helicopter shoots bullets at the player.
- Higher wanted levels increase police aggression.
- At 4 or more stars, the helicopter fires twin bullets.
- Police bullets reduce player health.
- Wanted stars earned through rocket kills slowly disappear.
- Kill-earned stars fade three times faster while using GHOST mode.

---

## ❤️ HEALTH AND COLLISION SYSTEM

- Starting player health: 100 HP.
- Collision with enemy traffic causes an instant crash.
- Police bullets reduce player health.
- Bullet damage starts at 3 HP.
- Bullet damage increases at higher wanted levels.
- GHOST mode allows police bullets to pass through.
- When HP reaches zero, the game ends.

---

## 🏆 SCORE AND HIGH SCORE SYSTEM

- Passing an enemy car normally earns 10 points.
- Passing an enemy car while flying earns 20 points.
- Destroying an enemy using a rocket earns 30 points.
- Game speed increases gradually.
- High score is saved using ESP32 Preferences.
- Selected car is also saved in ESP32 memory.

### HIGH SCORE RESET

Hold LEFT and RIGHT together for 3 seconds to reset the saved high score.

---

## 🎯 HOW TO PLAY

1. Power on the ESP32.
2. Wait for the animated FARO SPORTS splash screen.
3. Enter the SELECT CAR menu.
4. Use LEFT and RIGHT to select your favourite car.
5. Press START to confirm your car.
6. Wait for the 3-2-1-GO countdown.
7. Avoid enemy traffic using LEFT and RIGHT.
8. Press START to activate FLY MODE.
9. Press POWER to activate your selected car's special ability.
10. Avoid police bullets and protect your health.
11. Earn points and beat your high score!

### GAME OVER CONTROLS

- START: Restart with the same car.
- LEFT / RIGHT: Return to car selection.

---

## 💻 SOFTWARE REQUIREMENTS

### DEVELOPMENT ENVIRONMENT

- Arduino IDE
- ESP32 Arduino Board Package (Core 2.x or 3.x)

### REQUIRED LIBRARIES

Install these libraries through Arduino IDE Library Manager:

1. Adafruit GFX Library
2. Adafruit SSD1306
3. Adafruit BusIO

The following libraries are included with the ESP32 Arduino core:

- Wire
- Preferences

---

## ⚙️ HOW TO INSTALL AND UPLOAD

1. Download or clone this GitHub repository.
2. Open `FaroSports_ESP32-powergta5.ino` in Arduino IDE.
3. Install all required libraries.
4. Select **ESP32 Dev Module** under Tools → Board.
5. Select your ESP32 COM port.
6. Connect the OLED display and four push buttons.
7. Click Verify to compile the program.
8. Click Upload.
9. Restart your ESP32.
10. Enjoy FARO SPORTS!

### OLED TROUBLESHOOTING

The default OLED I2C address is `0x3C`.

Some SSD1306 displays use `0x3D`. If the display does not initialize, check the wiring and I2C address.

---

## 📊 TECHNICAL SPECIFICATIONS

| Specification | Details |
|---|---|
| Microcontroller | ESP32 WROOM |
| Display | SSD1306 OLED |
| Display Resolution | 128 × 64 |
| Communication | I2C |
| I2C Address | 0x3C |
| Number of Lanes | 3 |
| Selectable Cars | 4 |
| Physical Buttons | 4 |
| Frame Interval | 33 ms |
| Starting Health | 100 HP |
| Maximum Wanted Stars | 5 |
| High Score Storage | ESP32 Preferences |
| Programming Language | Embedded C++ |
| Development Environment | Arduino IDE |

---

## 📁 PROJECT STRUCTURE

```text
Car_Game_On_OLED/
│
├── README.md
├── FaroSports_ESP32-powergta5.ino
├── LICENSE
│
└── images/
    ├── project.jpg
    ├── gameplay.jpg
    ├── circuit.jpg
    └── car-selection.jpg
```

---

## 🚀 FUTURE IMPROVEMENTS

- More selectable racing cars
- New enemy vehicle designs
- Additional special powers
- Sound effects
- More challenging police AI
- Additional racing environments
- New game modes
- Improved OLED animations

---

## 👨‍💻 AUTHOR

**James Gandhi**

Project Name: FARO SPORTS – ESP32 OLED Racing Game

Built with creativity using ESP32, Arduino IDE and SSD1306 OLED technology.

If you like this project, please give this repository a ⭐ on GitHub!

---

## 📜 LICENSE

MIT License (see the LICENSE file in this repository).

---

**THANK YOU FOR CHECKING OUT FARO SPORTS!** 🚗💨
