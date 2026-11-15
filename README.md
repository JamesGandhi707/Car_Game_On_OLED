# FARO SPORTS – ESP32 OLED Car Racing Game

An arcade-style car racing game built using an **ESP32 DevKit** and a **0.96-inch SSD1306 OLED display (128×64)**. Dodge incoming traffic, use special car abilities, fire rockets, and try to beat your high score!

## Features

- Animated FARO SPORTS splash screen
- Four selectable cars, each with a unique special ability
- Three-lane racing gameplay
- Increasing speed and difficulty
- FLY mode with animated wings
- Rocket firing and 3D-style slow-motion destruction effects
- Score and high-score system
- High score and selected car saved in ESP32 memory
- Crash and Game Over screens

## Hardware Required

- ESP32 DevKit board
- 0.96-inch SSD1306 OLED display (128×64, I2C)
- 4 push buttons
- Jumper wires
- Breadboard (optional)

## Circuit Connections

### OLED Display

| OLED Pin | ESP32 Pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Push Buttons

Connect each push button between its GPIO pin and GND.

| Button | ESP32 GPIO | Function |
|---|---:|---|
| LEFT | GPIO 18 | Move left |
| RIGHT | GPIO 19 | Move right |
| START | GPIO 23 | Start game / FLY |
| POWER | GPIO 4 | Activate car's special power |

The buttons use the ESP32's internal pull-up resistors, so external resistors are not required.

**Using only 3 buttons?** Set `USE_POWER_BUTTON` to `0` in the code. LEFT + RIGHT together will activate the car's special power.

## Game Controls

| Button | Action |
|---|---|
| LEFT | Change to the left lane |
| RIGHT | Change to the right lane |
| START | Start or retry the game |
| START during racing | Activate FLY mode |
| POWER | Activate the selected car's special ability |

## Selectable Cars and Special Powers

### 1. TURBO – NITRO
- Activates a speed boost.
- Boost multiplier: 1.45×
- Duration: 3 seconds
- Recharge time: 10 seconds

### 2. VIPER – GHOST
- Makes the car temporarily invisible.
- Allows the car to pass through enemy traffic.
- Duration: 5 seconds
- Recharge time: 14 seconds

### 3. BRUTE – ROCKET
- Starts with 5 rockets.
- Fires rockets straight ahead.
- One rocket reloads every 8 seconds.
- Hitting an enemy destroys it.
- A successful hit triggers an explosion and slow-motion kill-cam effect.
- Each destroyed enemy earns a star.

### 4. COMET – SLOWMO
- Slows down the entire game world.
- Speed multiplier: 0.50×
- Duration: 4 seconds
- Recharge time: 13 seconds

## FLY Mode

FLY mode is available for all four cars.

- Activate it using the START button while racing.
- Flight duration: 5 seconds.
- Recharge time: approximately 12 seconds.
- Increases world speed while flying.
- Avoids collisions with enemy vehicles during flight.
- Passing enemy cars while flying earns double points.
- Provides a short safety period after landing.

## Scoring System

| Action | Points |
|---|---:|
| Pass an enemy car | +10 |
| Pass an enemy car while flying | +20 |
| Destroy an enemy using a rocket | +30 |

The game becomes faster as the race continues, increasing the challenge.

## Game Flow

1. The FARO SPORTS animated splash screen appears.
2. The car selection menu opens.
3. Use LEFT and RIGHT to select a car.
4. Press START to begin.
5. A 3-2-1-GO countdown appears.
6. Dodge traffic and use your special abilities.
7. Avoid collisions to keep racing.
8. After a crash, the Game Over screen displays your score and best score.
9. Press START to retry or LEFT/RIGHT to return to car selection.

## Required Arduino Libraries

Install these libraries using the Arduino IDE Library Manager:

- Adafruit SSD1306
- Adafruit GFX Library
- Adafruit BusIO

The following libraries are included with the ESP32 Arduino core:

- Wire
- Preferences

## How to Upload

1. Install the Arduino IDE.
2. Install the ESP32 board package.
3. Install the required Adafruit libraries.
4. Connect the ESP32 to your computer using USB.
5. Open `FARO CAR GAME_V1.ino`.
6. Select your ESP32 board and COM port.
7. Upload the program.
8. Connect the OLED display and buttons according to the wiring table.
9. Power on the ESP32 and enjoy the game!

## Technical Details

| Specification | Value |
|---|---|
| Microcontroller | ESP32 DevKit |
| Display | SSD1306 OLED |
| Resolution | 128 × 64 |
| Communication | I2C |
| OLED Address | 0x3C (some modules use 0x3D) |
| Game Frame Interval | 33 ms |
| Number of Cars | 4 |
| Number of Traffic Lanes | 3 |
| Persistent Storage | ESP32 Preferences |

## Project

**FARO SPORTS – ESP32 OLED Car Racing Game**

Built with Arduino C++ for ESP32.

Enjoy racing! 🚗
