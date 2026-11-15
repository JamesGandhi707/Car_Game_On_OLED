/*
 * ============================================================================
 *  FARO SPORTS  -  ESP32 + SSD1306 128x64 OLED
 *  (splash + car menu + FLY + car powers + rockets with 3D slow-motion kill-cam)
 * ============================================================================
 *  Board    : ESP32 DevKit (Arduino-ESP32 core 2.x or 3.x)
 *  Display  : SSD1306 0.96" 128x64, I2C (address 0x3C, or 0x3D on some modules)
 *  Libraries: Adafruit SSD1306, Adafruit GFX, Adafruit BusIO,
 *             Wire + Preferences (included with the ESP32 core)
 *
 *  WIRING (buttons go between the GPIO and GND, internal pull-ups are used)
 *     OLED  VCC -> 3V3        OLED  GND -> GND
 *     OLED  SDA -> GPIO 21    OLED  SCL -> GPIO 22
 *     LEFT   button -> GPIO 18
 *     START  button -> GPIO 23   (centre button)  = START game + FLY
 *     RIGHT  button -> GPIO 19
 *     POWER  button -> GPIO 4    (4th button)     = your car's special power
 *     (No 4th button? set USE_POWER_BUTTON 0: press LEFT + RIGHT together)
 *
 *  GAME FLOW
 *     1. Animated "FARO SPORTS" splash / loading screen
 *     2. SELECT CAR menu : LEFT / RIGHT = choose car, START = go
 *     3. 3-2-1-GO countdown, then the race with the selected car
 *     4. GAME OVER       : START = retry same car, LEFT/RIGHT = car menu
 *
 *  FLY (same for every car)      : START button, 5 s of flight, ~12 s recharge
 *  CAR POWER (different per car) : POWER button
 *     TURBO  NITRO   : 3 s speed boost
 *     VIPER  GHOST   : 5 s invisible - the car drives THROUGH other cars.
 *                      When the power ends you get a short blinking safety
 *                      time, then the meter reloads.
 *     BRUTE  ROCKET  : 5 rockets fired straight ahead (1 reloads every 8 s).
 *                      A hit destroys the car: the game drops into SLOW MOTION
 *                      and the wreck tumbles and breaks apart in 3D
 *                      (wireframe car, fireball, flying debris, shock-wave).
 *                      Every kill earns a star (max 5, they fade over time).
 *     COMET  SLOWMO  : 4 s of slow motion for the whole world
 *
 *  Collision with a traffic car = crash, unless flying / GHOST is active.
 * ============================================================================
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

// ============================================================================
//  USER CONFIGURATION
// ============================================================================
#define USE_POWER_BUTTON 1        // 1 = 4th button for the car power, 0 = LEFT+RIGHT together

// ---- GPIO pins
#define PIN_SDA        21
#define PIN_SCL        22
#define PIN_BTN_LEFT   18
#define PIN_BTN_RIGHT  19
#define PIN_BTN_START  23         // centre button = START + FLY
#define PIN_BTN_POWER  4          // 4th button = car power

// ---- Display
constexpr uint8_t  SCREEN_WIDTH  = 128;
constexpr uint8_t  SCREEN_HEIGHT = 64;
constexpr int8_t   OLED_RESET    = -1;
constexpr uint8_t  OLED_ADDR     = 0x3C;
constexpr uint32_t I2C_CLOCK_HZ  = 400000UL;

// ---- Timing
constexpr uint32_t FRAME_MS          = 33;
constexpr uint32_t DEBOUNCE_MS       = 25;
constexpr uint32_t COUNTDOWN_MS      = 2800;
constexpr uint32_t CRASH_MS          = 1000;
constexpr uint32_t GAMEOVER_LOCK_MS  = 900;
constexpr uint32_t AUTO_START_MS     = 6000;     // 2-button mode only
constexpr uint32_t HI_RESET_HOLD_MS  = 3000;
constexpr uint32_t SPLASH_MS         = 3800;     // FARO SPORTS loading screen
constexpr uint32_t MENU_CONFIRM_MS   = 600;      // little rev animation after START

// ---- Road / sprite geometry
constexpr int LANES       = 3;
constexpr int LANE_W      = 20;
constexpr int ROAD_X      = 10;
constexpr int ROAD_W      = LANES * LANE_W;      // 60 px
constexpr int CAR_W       = 11;
constexpr int CAR_H       = 16;
constexpr int PLAYER_Y    = 46;
constexpr int HUD_X       = 86;
constexpr int MAX_ENEMIES = 8;

// ---- Gameplay tuning (px per second)
constexpr float BASE_SPEED    = 62.0f;
constexpr float MAX_SPEED     = 175.0f;
constexpr float ACCEL         = 1.6f;
constexpr float LATERAL_SPEED = 170.0f;

// ---- FLY MODE tuning
constexpr float FLY_DURATION_S     = 5.0f;   // seconds of flight on a full meter
constexpr float RECHARGE_S         = 12.0f;  // seconds to refill an empty meter
constexpr float FLY_BOOST          = 1.8f;   // world speed multiplier while flying
constexpr float WING_DEPLOY_RATE   = 2.8f;   // wing open speed  (0 -> 1 in ~0.35 s)
constexpr float WING_RETRACT_RATE  = 3.5f;   // wing close speed
constexpr float LAND_GRACE_S       = 0.8f;   // safe (blinking) time after landing
constexpr int   WING_LEN           = 9;      // wing span per side in pixels
constexpr int   LIFT_PX            = 5;      // how far the car rises while flying
constexpr int   STREAKS            = 12;     // number of speed lines

// ---- Car powers / weapons / police
constexpr int   MAX_ROCKETS        = 3;
constexpr int   MAX_BOOMS          = 5;
constexpr int   ROCKET_AMMO        = 5;
constexpr float ROCKET_SPEED       = 160.0f; // px/s (upwards)
constexpr float ROCKET_COOLDOWN    = 0.30f;  // s between two shots
constexpr float AMMO_RELOAD_S      = 8.0f;   // s to get 1 rocket back
constexpr float NITRO_BOOST        = 1.45f;
constexpr float SLOW_FACTOR        = 0.50f;
constexpr float KILLCAM_S          = 1.6f;   // real seconds of slow motion after a rocket kill
constexpr float KILLCAM_SCALE      = 0.20f;  // world speed during the kill-cam
constexpr float BOOM_LIFE          = 0.60f;  // explosion length in WORLD seconds
constexpr float WANTED_DECAY_S     = 14.0f;  // s for one kill-star to fade

// ============================================================================
//  SPRITES  (11 x 16 px, 2 bytes per row, MSB first)
// ============================================================================
const uint8_t SPR_PLAYER[] PROGMEM = {
  0x0E,0x00, 0x1F,0x00, 0xDF,0x60, 0xDF,0x60,
  0xCE,0x60, 0x1F,0x00, 0x11,0x00, 0x11,0x00,
  0x1F,0x00, 0x1F,0x00, 0xDF,0x60, 0xDF,0x60,
  0xDF,0x60, 0x1F,0x00, 0x3F,0x80, 0x3F,0x80
};

const uint8_t SPR_SEDAN[] PROGMEM = {
  0x3F,0x80, 0x5F,0x40, 0xDF,0x60, 0xD1,0x60,
  0xD1,0x60, 0x1F,0x00, 0x11,0x00, 0x11,0x00,
  0x11,0x00, 0x1F,0x00, 0xDF,0x60, 0xD1,0x60,
  0xD1,0x60, 0x5F,0x40, 0x7F,0xC0, 0x3F,0x80
};

const uint8_t SPR_VAN[] PROGMEM = {
  0x3F,0x80, 0x7F,0xC0, 0xD1,0x60, 0xD1,0x60,
  0x7F,0xC0, 0x7F,0xC0, 0x7F,0xC0, 0x5F,0x40,
  0x5F,0x40, 0x5F,0x40, 0x7F,0xC0, 0xDF,0x60,
  0xDF,0x60, 0xDF,0x60, 0x7F,0xC0, 0x3F,0x80
};

const uint8_t SPR_VIPER[] PROGMEM = {
  0x0A,0x00, 0x0E,0x00, 0x1F,0x00, 0xDF,0x60,
  0xD1,0x60, 0x11,0x00, 0x15,0x00, 0x11,0x00,
  0x1F,0x00, 0x1F,0x00, 0xDF,0x60, 0xDF,0x60,
  0xDF,0x60, 0x7F,0xC0, 0x7F,0xC0, 0x3F,0x80
};

const uint8_t SPR_BRUTE[] PROGMEM = {
  0x3F,0x80, 0x7F,0xC0, 0xDF,0x60, 0xD1,0x60,
  0xD1,0x60, 0xDF,0x60, 0x1F,0x00, 0x1F,0x00,
  0x15,0x00, 0x1F,0x00, 0xDF,0x60, 0xDF,0x60,
  0xDF,0x60, 0x7F,0xC0, 0x7F,0xC0, 0x3F,0x80
};

const uint8_t SPR_COMET[] PROGMEM = {
  0x04,0x00, 0x04,0x00, 0x0E,0x00, 0x0E,0x00,
  0x0A,0x00, 0x0A,0x00, 0x0E,0x00, 0x1F,0x00,
  0x1F,0x00, 0x3F,0x80, 0xDF,0x60, 0xDF,0x60,
  0x9F,0x20, 0xDF,0x60, 0xBF,0xA0, 0x1B,0x00
};

const uint8_t SPR_STAR[] PROGMEM = { 0x20, 0xF8, 0x70, 0x50, 0x88 };   // 5x5 wanted star

// ============================================================================
//  PLAYER CARS  (name, sprite, grip, special power)
// ============================================================================
enum PowerType : uint8_t { PW_NITRO, PW_GHOST, PW_ROCKET, PW_SLOW };

struct CarDef {
  const char    *name;
  const uint8_t *spr;
  float          lateral;       // sideways speed (px/s)
  uint8_t        gripStars;     // 1..5 (menu)
  PowerType      power;
  float          powerDur;      // seconds the power stays on
  float          powerRecharge; // seconds to refill the meter
  const char    *powerName;     // menu
  const char    *powerDesc;     // menu (max 12 chars)
  const char    *hudTag;        // HUD label (3 chars)
};

constexpr int NUM_CARS = 4;
const CarDef CARS[NUM_CARS] = {
  { "TURBO", SPR_PLAYER, 170.0f, 3, PW_NITRO,  3.0f, 10.0f, "NITRO",  "BOOST 3S",     "NOS" },
  { "VIPER", SPR_VIPER,  205.0f, 5, PW_GHOST,  5.0f, 14.0f, "GHOST",  "THRU CARS 5S", "GHO" },
  { "BRUTE", SPR_BRUTE,  145.0f, 2, PW_ROCKET, 0.0f,  0.0f, "ROCKET", "5 ROCKETS",    "RKT" },
  { "COMET", SPR_COMET,  165.0f, 3, PW_SLOW,   4.0f, 13.0f, "SLOWMO", "SLOW TIME 4S", "SLO" },
};

// ============================================================================
//  GLOBAL OBJECTS
// ============================================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET,
                         I2C_CLOCK_HZ, I2C_CLOCK_HZ);
Preferences prefs;

// ---- Custom types: defined BEFORE any function (Arduino auto-prototypes) ----
enum GameState : uint8_t { ST_SPLASH, ST_MENU, ST_COUNTDOWN, ST_PLAYING, ST_CRASH, ST_GAMEOVER };

struct Enemy {
  bool    active;
  uint8_t lane;
  uint8_t type;   // 0 = sedan, 1 = van
  float   y;
};

struct Rocket { bool active; float x; float y; };          // player rocket
struct Boom   { bool active; float x; float y; float t; uint8_t seed; };  // 3D explosion

struct Streak {   // one speed line
  float   x;
  float   y;
  uint8_t len;
};

struct Button {
  uint8_t  pin;
  bool     stable;        // true = currently pressed (debounced)
  uint32_t lastChange;
  bool     pressedEvent;  // latched "was pressed" flag
};

// ---- Buttons ----------------------------------------------------------------
Button btnLeft  = { PIN_BTN_LEFT,  false, 0, false };
Button btnRight = { PIN_BTN_RIGHT, false, 0, false };
Button btnStart = { PIN_BTN_START, false, 0, false };
Button btnPower = { PIN_BTN_POWER, false, 0, false };

// First edge is accepted immediately, then changes are ignored for DEBOUNCE_MS.
void buttonUpdate(Button &b, uint32_t now) {
  bool raw = (digitalRead(b.pin) == LOW);          // INPUT_PULLUP: LOW = pressed
  if (raw != b.stable && (now - b.lastChange) >= DEBOUNCE_MS) {
    b.stable = raw;
    b.lastChange = now;
    if (raw) b.pressedEvent = true;
  }
}

bool takePress(Button &b) {
  bool e = b.pressedEvent;
  b.pressedEvent = false;
  return e;
}

void flushButtons() {
  btnLeft.pressedEvent = false;
  btnRight.pressedEvent = false;
  btnStart.pressedEvent = false;
  btnPower.pressedEvent = false;
}

// True when the player wants to start / restart the game.
bool startPressed() {
  bool s = takePress(btnStart);
  takePress(btnLeft);
  takePress(btnRight);
  takePress(btnPower);
  return s;
}

// True when the FLY button (START) was pressed.
bool flyRequested() {
  return takePress(btnStart);
}

// True when the car-power button was pressed (4th button, or LEFT+RIGHT together).
bool powerRequested() {
#if USE_POWER_BUTTON
  return takePress(btnPower);
#else
  if (btnLeft.stable && btnRight.stable) {
    btnLeft.pressedEvent  = false;
    btnRight.pressedEvent = false;
    return true;
  }
  return false;
#endif
}

// ---- Game state -------------------------------------------------------------
GameState state       = ST_SPLASH;
uint32_t  stateStart  = 0;
uint32_t  lastFrame   = 0;

Enemy    enemies[MAX_ENEMIES];
Streak   streaks[STREAKS];
float    playerX      = 0;
int      targetLane   = 1;
float    speed        = BASE_SPEED;   // base speed (grows with time)
float    curSpeed     = BASE_SPEED;   // real speed including fly boost
float    playTime     = 0;
float    scrollF      = 0;
float    spawnDist    = 0;
float    nextGap      = 70;
float    scoreFrac    = 0;
uint8_t  prevFree     = 0x07;
uint32_t score        = 0;
uint32_t hiScore      = 0;
bool     newHigh      = false;
bool     inverted     = false;
uint32_t resetHoldStart = 0;

// car selection / menu
int      selCar         = 0;          // index into CARS[]
float    carLateral     = 170.0f;     // grip of the selected car (set by applyCar)
float    menuScroll     = 0;          // road animation inside the preview box
float    menuSlide      = 0;          // slide-in offset when the car changes
bool     menuConfirming = false;
uint32_t menuConfirmAt  = 0;

// weapons
Rocket   rockets[MAX_ROCKETS];
Boom     booms[MAX_BOOMS];
bool     powerOn      = false;        // car power currently active
float    powerCharge  = 1.0f;         // 0..1 meter (non-rocket cars)
int      ammo         = ROCKET_AMMO;
float    ammoTimer    = 0.0f;
float    rocketCd     = 0.0f;
int      killStars    = 0;            // stars earned by destroying cars
float    starTimer    = 0.0f;
int      kills        = 0;            // cars destroyed this run
float    killCamT     = 0.0f;         // >0 = slow-motion kill-cam running (real seconds)

// fly mode
bool     flying       = false;
float    charge       = 1.0f;         // 0..1 fly energy
float    wingDeploy   = 0.0f;         // 0 = folded, 1 = fully open (animation value)
float    invulnT      = 0.0f;         // post-landing safe time (seconds)

// ============================================================================
//  HELPERS
// ============================================================================
inline int laneX(int lane) {
  return ROAD_X + lane * LANE_W + (LANE_W - CAR_W) / 2;
}

void loadHighScore() {
  prefs.begin("turboracer", true);
  hiScore = prefs.getUInt("hi", 0);
  selCar  = prefs.getUChar("car", 0);
  prefs.end();
  if (selCar < 0 || selCar >= NUM_CARS) selCar = 0;
}

void saveHighScore() {
  prefs.begin("turboracer", false);
  prefs.putUInt("hi", hiScore);
  prefs.end();
}

void saveCar() {
  prefs.begin("turboracer", false);
  prefs.putUChar("car", (uint8_t)selCar);
  prefs.end();
}

void applyCar() {
  carLateral = CARS[selCar].lateral;
}

inline bool hidden()  { return powerOn && CARS[selCar].power == PW_GHOST; }
inline bool nitroOn() { return powerOn && CARS[selCar].power == PW_NITRO; }
inline bool slowOn()  { return powerOn && CARS[selCar].power == PW_SLOW; }

void drawCar(const uint8_t *spr, int x, int y) {
  display.fillRect(x, y, CAR_W, CAR_H, SSD1306_BLACK);
  display.drawBitmap(x, y, spr, CAR_W, CAR_H, SSD1306_WHITE);
}

void drawTextCentered(const char *s, int cx, int y, uint8_t size) {
  int w = (int)strlen(s) * 6 * size;
  display.setTextSize(size);
  display.setCursor(cx - w / 2, y);
  display.print(s);
}

uint8_t currentLevel() {
  return 1 + (uint8_t)((speed - BASE_SPEED) / 12.0f);
}

// ============================================================================
//  GAME LOGIC
// ============================================================================
void initStreaks() {
  for (int i = 0; i < STREAKS; i++) {
    streaks[i].x   = (float)random(0, HUD_X - 6);
    streaks[i].y   = (float)random(-8, SCREEN_HEIGHT);
    streaks[i].len = (uint8_t)random(6, 15);
  }
}

void resetGame() {
  applyCar();
  for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
  targetLane = 1;
  playerX    = laneX(1);
  speed      = BASE_SPEED;
  curSpeed   = BASE_SPEED;
  playTime   = 0;
  spawnDist  = 0;
  nextGap    = 80;
  scoreFrac  = 0;
  prevFree   = 0x07;
  score      = 0;
  newHigh    = false;

  flying     = false;
  charge     = 1.0f;         // fly is ready at the start of every run
  wingDeploy = 0.0f;
  invulnT    = 0.0f;

  for (int i = 0; i < MAX_ROCKETS; i++) rockets[i].active = false;
  for (int i = 0; i < MAX_BOOMS;   i++) booms[i].active   = false;
  powerOn     = false;
  powerCharge = 1.0f;
  ammo        = ROCKET_AMMO;
  ammoTimer   = 0.0f;
  rocketCd    = 0.0f;
  killStars   = 0;
  starTimer   = 0.0f;
  kills       = 0;
  killCamT    = 0.0f;
  initStreaks();
}

void enterState(GameState s, uint32_t now, bool flush = true) {
  state = s;
  stateStart = now;
  if (flush) flushButtons();
}

void beginRun(uint32_t now) {
  resetGame();
  enterState(ST_COUNTDOWN, now);
}

void steerInput() {
  bool l = takePress(btnLeft);
  bool r = takePress(btnRight);
  if (l && targetLane > 0) targetLane--;
  if (r && targetLane < LANES - 1) targetLane++;
}

void movePlayer(float dt) {
  float tx = (float)laneX(targetLane);
  float step = carLateral * dt;
  if (playerX < tx)      { playerX += step; if (playerX > tx) playerX = tx; }
  else if (playerX > tx) { playerX -= step; if (playerX < tx) playerX = tx; }
}

// Fly energy, wing animation and landing grace.
void updateFlight(float dt) {
  if (flying) {
    charge -= dt / FLY_DURATION_S;
    if (charge <= 0.0f) {                  // out of power -> land
      charge = 0.0f;
      flying = false;
      invulnT = LAND_GRACE_S;
    }
    wingDeploy += WING_DEPLOY_RATE * dt;
    if (wingDeploy > 1.0f) wingDeploy = 1.0f;
  } else {
    charge += dt / RECHARGE_S;
    if (charge > 1.0f) charge = 1.0f;
    wingDeploy -= WING_RETRACT_RATE * dt;
    if (wingDeploy < 0.0f) wingDeploy = 0.0f;
    if (invulnT > 0.0f) {
      invulnT -= dt;
      if (invulnT < 0.0f) invulnT = 0.0f;
    }
  }
}

void updateStreaks(float dt) {
  if (wingDeploy <= 0.02f) return;
  float v = curSpeed * 2.4f;                       // lines move faster than the road
  for (int i = 0; i < STREAKS; i++) {
    streaks[i].y += v * dt;
    if (streaks[i].y > SCREEN_HEIGHT) {
      streaks[i].len = (uint8_t)random(6, 15);
      streaks[i].y   = -(float)streaks[i].len;
      streaks[i].x   = (float)random(0, HUD_X - 6);
    }
  }
}

void addEnemy(uint8_t lane, float y) {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) {
      enemies[i].active = true;
      enemies[i].lane   = lane;
      enemies[i].type   = (uint8_t)random(2);
      enemies[i].y      = y;
      return;
    }
  }
}

bool rowReachable(uint8_t prevFreeMask, uint8_t newFreeMask) {
  for (int a = 0; a < LANES; a++) {
    if (!(prevFreeMask & (1 << a))) continue;
    for (int b = 0; b < LANES; b++) {
      if ((newFreeMask & (1 << b)) && abs(a - b) <= 1) return true;
    }
  }
  return false;
}

void spawnRow(float startY) {
  uint8_t level = currentLevel();
  int doubleChance = 6 + level * 4;
  if (doubleChance > 40) doubleChance = 40;

  uint8_t block;
  if (random(100) < doubleChance) {
    uint8_t freeLane = (uint8_t)random(LANES);
    block = 0x07 & ~(1 << freeLane);
  } else {
    block = 1 << random(LANES);
  }
  uint8_t freeMask = 0x07 & ~block;
  if (!rowReachable(prevFree, freeMask)) {
    block = 1 << random(LANES);
    freeMask = 0x07 & ~block;
  }
  prevFree = freeMask;

  for (uint8_t lane = 0; lane < LANES; lane++) {
    if (block & (1 << lane)) addEnemy(lane, startY);
  }
}

float computeGap() {
  float minGap = speed * 0.34f;
  if (minGap < CAR_H + 20) minGap = CAR_H + 20;
  return minGap + (float)random(0, 30);
}

bool hitsPlayer(const Enemy &e) {
  int px = (int)playerX + 1, py = PLAYER_Y + 2;
  int ex = laneX(e.lane) + 1, ey = (int)e.y + 2;
  const int w = CAR_W - 2, h = CAR_H - 4;
  return (px < ex + w) && (px + w > ex) && (py < ey + h) && (py + h > ey);
}

void startCrash(uint32_t now) {
  enterState(ST_CRASH, now);
  display.invertDisplay(true);
  inverted = true;
}

// ---- Stars (earned by destroying cars) -------------------------------------
int wantedStars() {
  return killStars;
}

// ---- Car powers --------------------------------------------------------------
void activatePower() {
  const CarDef &c = CARS[selCar];
  if (c.power == PW_ROCKET) {
    if (ammo <= 0 || rocketCd > 0.0f) return;
    for (int i = 0; i < MAX_ROCKETS; i++) {
      if (rockets[i].active) continue;
      int lift = (int)(LIFT_PX * wingDeploy + 0.5f);
      rockets[i].active = true;
      rockets[i].x = playerX + CAR_W / 2 - 1;
      rockets[i].y = (float)(PLAYER_Y - lift - 7);
      ammo--;
      rocketCd = ROCKET_COOLDOWN;
      return;
    }
  } else if (!powerOn && powerCharge >= 1.0f) {
    powerOn = true;
  }
}

void updatePower(float dt) {
  const CarDef &c = CARS[selCar];
  if (c.power == PW_ROCKET) {
    if (rocketCd > 0.0f) rocketCd -= dt;
    if (ammo < ROCKET_AMMO) {
      ammoTimer += dt;
      if (ammoTimer >= AMMO_RELOAD_S) { ammo++; ammoTimer = 0.0f; }
    } else {
      ammoTimer = 0.0f;
    }
    return;
  }
  if (powerOn) {
    powerCharge -= dt / c.powerDur;
    if (powerCharge <= 0.0f) {
      powerCharge = 0.0f;
      powerOn = false;
      if (c.power == PW_GHOST) invulnT = LAND_GRACE_S;   // safe (blinking) time if still inside a car
    }
  } else {
    powerCharge += dt / c.powerRecharge;
    if (powerCharge > 1.0f) powerCharge = 1.0f;
  }
}

// ---- Rockets and explosions ----------------------------------------------------
void addBoom(float x, float y) {
  for (int i = 0; i < MAX_BOOMS; i++) {
    if (!booms[i].active) {
      booms[i].active = true;
      booms[i].x = x;
      booms[i].y = y;
      booms[i].t = 0.0f;
      booms[i].seed = (uint8_t)random(256);
      return;
    }
  }
}

// adv = how far the road moved this frame (enemies already moved by adv)
void updateRockets(float dt, float adv) {
  for (int i = 0; i < MAX_ROCKETS; i++) {
    if (!rockets[i].active) continue;
    float oldY = rockets[i].y;
    rockets[i].y -= ROCKET_SPEED * dt;
    if (rockets[i].y < -8.0f) { rockets[i].active = false; continue; }

    float rTop = rockets[i].y;
    float rBot = oldY + 7.0f;
    for (int k = 0; k < MAX_ENEMIES; k++) {
      if (!enemies[k].active) continue;
      float ex = (float)(laneX(enemies[k].lane) + 1);
      if (rockets[i].x + 3.0f < ex || rockets[i].x > ex + (CAR_W - 2)) continue;

      float e0 = enemies[k].y - adv;                         // previous position
      float e1 = enemies[k].y;
      float eTop = ((e0 < e1) ? e0 : e1) + 2.0f;
      float eBot = ((e0 > e1) ? e0 : e1) + CAR_H - 2.0f;
      if (rTop < eBot && rBot > eTop) {                      // HIT -> destroy the car
        addBoom(laneX(enemies[k].lane) + CAR_W / 2.0f, enemies[k].y + CAR_H / 2.0f);
        enemies[k].active = false;
        rockets[i].active = false;
        score += 30;
        if (killStars < 5) killStars++;                      // +1 star
        starTimer = WANTED_DECAY_S;
        kills++;
        killCamT = KILLCAM_S;                                // SLOW MOTION kill-cam
        break;
      }
    }
  }
}

void updateBooms(float dt, float adv) {
  for (int i = 0; i < MAX_BOOMS; i++) {
    if (!booms[i].active) continue;
    booms[i].t += dt;
    booms[i].y += adv;                                       // wreck drifts down with the road
    if (booms[i].t > BOOM_LIFE) booms[i].active = false;
  }
}

void updatePlaying(float rdt, uint32_t now) {
  // --- kill-cam: the whole world slows down after a rocket kill, then eases back ---
  float kscale = 1.0f;
  if (killCamT > 0.0f) {
    killCamT -= rdt;
    if (killCamT < 0.0f) killCamT = 0.0f;
    float k = killCamT / KILLCAM_S;                          // 1 -> 0
    kscale = (k > 0.3f) ? KILLCAM_SCALE
                        : KILLCAM_SCALE + (1.0f - KILLCAM_SCALE) * (1.0f - k / 0.3f);
  }
  float dt = rdt * kscale;                                   // simulation time step

  // --- buttons: FLY (START) and car POWER ---
  if (flyRequested() && !flying && charge >= 1.0f) {
    flying  = true;
    invulnT = 0.0f;
  }
  if (powerRequested()) activatePower();

  steerInput();
  movePlayer(dt);
  updateFlight(dt);
  updatePower(dt);

  // --- speed (base grows with time; fly / nitro boost; slow-mo power slows the world) ---
  playTime += dt;
  speed = BASE_SPEED + playTime * ACCEL;
  if (speed > MAX_SPEED) speed = MAX_SPEED;
  float boost = 1.0f + (FLY_BOOST - 1.0f) * wingDeploy;
  if (nitroOn() && NITRO_BOOST > boost) boost = NITRO_BOOST;
  float tscale = slowOn() ? SLOW_FACTOR : 1.0f;
  curSpeed = speed * boost * tscale;
  float adv = curSpeed * dt;
  scrollF += adv;
  updateStreaks(dt);

  scoreFrac += adv * 0.02f;
  if (scoreFrac >= 1.0f) {
    uint32_t inc = (uint32_t)scoreFrac;
    score += inc;
    scoreFrac -= inc;
  }

  // spawn enemy rows
  spawnDist += adv;
  if (spawnDist >= nextGap) {
    float over = spawnDist - nextGap;
    spawnRow(-CAR_H + over);
    spawnDist = over;
    nextGap = computeGap();
  }

  // move enemies, score passed cars, detect collisions
  // (no crash while flying, just after landing, or while GHOST is active)
  bool ghost = flying || (invulnT > 0.0f) || hidden();
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) continue;
    enemies[i].y += adv;
    if (enemies[i].y > SCREEN_HEIGHT) {
      enemies[i].active = false;
      score += flying ? 20 : 10;                    // double points for cars passed in flight
    } else if (!ghost && hitsPlayer(enemies[i])) {
      startCrash(now);
      return;
    }
  }

  // rockets, explosions
  updateRockets(dt, adv);
  updateBooms(dt, adv);

  // kill stars fade over time
  if (killStars > 0) {
    starTimer -= dt;
    if (starTimer <= 0.0f) { killStars--; starTimer = WANTED_DECAY_S; }
  }
}

void finishRun(uint32_t now) {
  newHigh = false;
  if (score > hiScore) {
    hiScore = score;
    newHigh = true;
    saveHighScore();
  }
  enterState(ST_GAMEOVER, now);
}

void openMenu(uint32_t now) {
  menuConfirming = false;
  menuSlide      = 0;
  enterState(ST_MENU, now);
}

void changeCar(int dir) {
  selCar = (selCar + dir + NUM_CARS) % NUM_CARS;
  menuSlide = (float)dir * 30.0f;          // new car slides in from that side
}

void updateMenu(float dt, uint32_t now) {
  menuScroll += (menuConfirming ? 260.0f : 45.0f) * dt;
  menuSlide  -= menuSlide * ((dt * 14.0f > 1.0f) ? 1.0f : dt * 14.0f);

  if (menuConfirming) {                          // rev animation, then race
    flushButtons();
    if (now - menuConfirmAt >= MENU_CONFIRM_MS) {
      saveCar();
      beginRun(now);
    }
    return;
  }

  bool l = takePress(btnLeft);
  bool r = takePress(btnRight);

  if (takePress(btnStart)) { menuConfirming = true; menuConfirmAt = now; return; }
  if (btnLeft.stable && btnRight.stable) {       // hold L+R 3 s = erase high score
    if (resetHoldStart == 0) resetHoldStart = now;
    else if (now - resetHoldStart >= HI_RESET_HOLD_MS) {
      hiScore = 0;
      saveHighScore();
      resetHoldStart = 0;
    }
  } else {
    resetHoldStart = 0;
  }

  if (l) changeCar(-1);
  if (r) changeCar(+1);
}

void update(float dt, uint32_t now) {
  uint32_t t = now - stateStart;

  switch (state) {
    case ST_SPLASH:
      if (t >= SPLASH_MS || (t > 900 && startPressed())) enterState(ST_MENU, now);
      break;

    case ST_MENU:
      updateMenu(dt, now);
      break;

    case ST_COUNTDOWN:
      flyRequested();                               // ignore fly / power presses before GO
      powerRequested();
      steerInput();
      movePlayer(dt);
      scrollF += BASE_SPEED * dt;
      if (t >= COUNTDOWN_MS) enterState(ST_PLAYING, now, false);
      break;

    case ST_PLAYING:
      updatePlaying(dt, now);
      break;

    case ST_CRASH:
      flushButtons();
      if (inverted && t > 90) { display.invertDisplay(false); inverted = false; }
      if (t >= CRASH_MS) finishRun(now);
      break;

    case ST_GAMEOVER: {
      bool l = takePress(btnLeft);
      bool r = takePress(btnRight);
      bool retry = takePress(btnStart);
      if (t > GAMEOVER_LOCK_MS) {
        if (retry)       beginRun(now);                // same car again
        else if (l || r) openMenu(now);                // choose another car
      }
      break;
    }
  }
}

// ============================================================================
//  RENDERING
// ============================================================================
void drawRoad(int s) {
  display.fillRect(ROAD_X - 2,      0, 2, SCREEN_HEIGHT, SSD1306_WHITE);   // left kerb
  display.fillRect(ROAD_X + ROAD_W, 0, 2, SCREEN_HEIGHT, SSD1306_WHITE);   // right kerb

  for (int y = (s & 7) - 8; y < SCREEN_HEIGHT; y += 8) {                   // rumble strips
    display.fillRect(ROAD_X - 5,          y, 3, 4, SSD1306_WHITE);
    display.fillRect(ROAD_X + ROAD_W + 2, y, 3, 4, SSD1306_WHITE);
  }

  for (int l = 1; l < LANES; l++) {                                        // lane dashes
    int x = ROAD_X + l * LANE_W - 1;
    for (int y = (s & 15) - 16; y < SCREEN_HEIGHT; y += 16) {
      display.fillRect(x, y, 2, 8, SSD1306_WHITE);
    }
  }

  for (int y = (s % 23) - 23; y < SCREEN_HEIGHT; y += 23) {                // scenery
    display.fillRect(1, y + 6, 3, 3, SSD1306_WHITE);
  }
  for (int y = (s % 31) - 31; y < SCREEN_HEIGHT; y += 31) {
    display.fillRect(78, y + 10, 3, 3, SSD1306_WHITE);
    display.drawPixel(79, y + 9, SSD1306_WHITE);
  }
}

void drawStreaks() {
  int n = (int)(STREAKS * wingDeploy + 0.5f);
  for (int i = 0; i < n && i < STREAKS; i++) {
    display.drawFastVLine((int)streaks[i].x, (int)streaks[i].y, streaks[i].len, SSD1306_WHITE);
  }
}

void drawEnemies() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) continue;
    drawCar(enemies[i].type ? SPR_VAN : SPR_SEDAN, laneX(enemies[i].lane), (int)enemies[i].y);
  }
}

// Car drawn "see-through" (dithered + shimmering) while invisible.
void drawCarGhost(const uint8_t *spr, int x, int y, uint32_t now) {
  uint8_t ph = (now / 80) & 1;
  for (int r = 0; r < CAR_H; r++) {
    uint16_t row = ((uint16_t)pgm_read_byte(&spr[r * 2]) << 8) | pgm_read_byte(&spr[r * 2 + 1]);
    for (int c = 0; c < CAR_W; c++) {
      if ((row & (0x8000 >> c)) && (((r + c) & 1) == ph)) display.drawPixel(x + c, y + r, SSD1306_WHITE);
    }
  }
}

void drawFlames(int px, int py) {
  int h = (int)random(4, 9);
  display.fillTriangle(px + 2, py + CAR_H, px + 4, py + CAR_H, px + 3, py + CAR_H + h, SSD1306_WHITE);
  display.fillTriangle(px + 6, py + CAR_H, px + 8, py + CAR_H, px + 7, py + CAR_H + h, SSD1306_WHITE);
}

// Player car with wings, shadow and flame when flying.
void drawPlayer(uint32_t now) {
  int px   = (int)(playerX + 0.5f);
  int lift = (int)(LIFT_PX * wingDeploy + 0.5f);
  int py   = PLAYER_Y - lift;

  // blink while in post-landing safe time
  if (!flying && invulnT > 0.0f && ((now / 70) & 1)) return;

  if (wingDeploy > 0.05f) {
    // ground shadow (dotted)
    for (int x = px + 1; x < px + CAR_W - 1; x += 2)
      display.drawPixel(x, PLAYER_Y + CAR_H, SSD1306_WHITE);
    for (int x = px + 2; x < px + CAR_W - 2; x += 2)
      display.drawPixel(x, PLAYER_Y + CAR_H + 1, SSD1306_WHITE);

    // twin exhaust flames (random length = flicker)
    if (flying && wingDeploy > 0.5f) {
      int h = (int)random(3, 8);
      display.fillTriangle(px + 2, py + CAR_H, px + 4, py + CAR_H, px + 3, py + CAR_H + h, SSD1306_WHITE);
      display.fillTriangle(px + 6, py + CAR_H, px + 8, py + CAR_H, px + 7, py + CAR_H + h, SSD1306_WHITE);
    }

    // wings: swept-back triangles that grow outward as they deploy.
    // In the last second of flight they blink as a landing warning.
    bool wingsVisible = !(flying && charge < 0.2f && ((now / 90) & 1));
    int len = (int)(WING_LEN * wingDeploy + 0.5f);
    if (wingsVisible && len > 0) {
      display.fillTriangle(px - 1, py + 5, px - 1, py + 12, px - 1 - len, py + 13, SSD1306_WHITE);
      display.fillTriangle(px + CAR_W, py + 5, px + CAR_W, py + 12, px + CAR_W + len, py + 13, SSD1306_WHITE);
    }
  }

  if (nitroOn() && !(flying && wingDeploy > 0.5f)) drawFlames(px, py);   // nitro exhaust

  if (hidden()) drawCarGhost(CARS[selCar].spr, px, py, now);
  else          drawCar(CARS[selCar].spr, px, py);
}

// ---- rockets and explosions ----------------------------------------------------
void drawRockets() {
  for (int i = 0; i < MAX_ROCKETS; i++) {
    if (!rockets[i].active) continue;
    int x = (int)rockets[i].x, y = (int)rockets[i].y;
    display.fillRect(x - 1, y - 1, 5, 9, SSD1306_BLACK);
    display.drawPixel(x + 1, y, SSD1306_WHITE);                    // nose
    display.fillRect(x, y + 1, 3, 4, SSD1306_WHITE);               // body
    display.drawPixel(x - 1, y + 4, SSD1306_WHITE);                // fins
    display.drawPixel(x + 3, y + 4, SSD1306_WHITE);
    display.drawFastVLine(x + 1, y + 5, 2 + (int)random(0, 3), SSD1306_WHITE);   // flame
  }
}

// ---- 3D explosion (slow-motion kill-cam) ---------------------------------------
// Tiny 3D projection: x/y on the road, z = height above the road.
static void proj3(float x, float y, float z, float cx, float cy, int &sx, int &sy) {
  float s = 1.0f + z * 0.03f;                                // closer to the "camera" when high
  sx = (int)(cx + x * s + 0.5f);
  sy = (int)(cy + y * s * 0.8f - z * 0.9f + 0.5f);
}

static const uint8_t BOX_EDGES[12][2] = {
  {0,1},{2,3},{4,5},{6,7},      // along x
  {0,2},{1,3},{4,6},{5,7},      // along y
  {0,4},{1,5},{2,6},{3,7}       // along z
};

void drawBoom3D(const Boom &b, uint32_t now) {
  const float t = b.t;
  float u = t / BOOM_LIFE;
  if (u > 1.0f) u = 1.0f;
  const float cx = b.x, cy = b.y;
  const int icx = (int)cx, icy = (int)cy;

  // 1) ground shock-wave: flattened (elliptical) rings
  for (int ring = 0; ring < 2; ring++) {
    if (ring && u > 0.8f) break;
    float R = (4.0f + u * 28.0f) * (ring ? 0.6f : 1.0f);
    for (int k = 0; k < 24; k++) {
      float a = k * 0.2618f;
      display.drawPixel(icx + (int)(cosf(a) * R), icy + 5 + (int)(sinf(a) * R * 0.38f), SSD1306_WHITE);
    }
  }

  // 2) impact flash
  if (u < 0.08f) display.fillCircle(icx, icy - 3, 13, SSD1306_WHITE);

  // 3) shaded fireball (lit from the top-left, rises while it grows)
  if (u < 0.55f) {
    int r  = 2 + (int)(6.0f * sinf(u * 5.712f));
    int fy = icy - 3 - (int)(u * 10.0f);
    display.fillCircle(icx, fy, r, SSD1306_WHITE);
    if (r >= 4) {
      display.fillCircle(icx + r / 3, fy + r / 3, (r * 2) / 3, SSD1306_BLACK);
      display.fillCircle(icx + r / 3, fy + r / 3, r / 3, SSD1306_WHITE);
    }
  }

  // 4) the wrecked car as a tumbling 3D wireframe box that is thrown up and shrinks away
  if (u < 0.75f) {
    float sc = (u < 0.5f) ? 1.0f : 1.0f - (u - 0.5f) / 0.25f;
    float z0 = 3.0f + 42.0f * t - 80.0f * t * t;             // thrown up, falls back
    if (z0 < 2.0f) z0 = 2.0f;
    float ax = 7.0f * t, az = 4.0f * t + b.seed * 0.02f;
    float ca = cosf(ax), sa = sinf(ax), cz = cosf(az), sz = sinf(az);
    int px[8], py[8];
    for (int i = 0; i < 8; i++) {
      float x = ((i & 1) ? 5.5f : -5.5f) * sc;
      float y = ((i & 2) ? 8.0f : -8.0f) * sc;
      float z = ((i & 4) ? 3.0f : -3.0f) * sc;
      float y1 = y * ca - z * sa;                            // pitch
      float z1 = y * sa + z * ca;
      float x2 = x * cz - y1 * sz;                           // yaw
      float y2 = x * sz + y1 * cz;
      proj3(x2, y2, z0 + z1, cx, cy, px[i], py[i]);
    }
    for (int e = 0; e < 12; e++) {
      int a = BOX_EDGES[e][0], c = BOX_EDGES[e][1];
      display.drawLine(px[a], py[a] + 1, px[c], py[c] + 1, SSD1306_BLACK);   // dark edge for contrast
      display.drawLine(px[a], py[a],     px[c], py[c],     SSD1306_WHITE);
    }
  }

  // 5) flying debris: spinning planks with ground shadows
  for (int i = 0; i < 8; i++) {
    int   h  = (b.seed * 13 + i * 53) & 255;
    float a  = h * 0.0245f;
    float v  = 22.0f + (h % 40);
    float vz = 24.0f + ((h * 7) & 31);
    float x  = cosf(a) * v * t;
    float y  = sinf(a) * v * t * 0.8f;
    float z  = 2.0f + vz * t - 80.0f * t * t;
    bool landed = z < 0.0f;
    if (landed) z = 0.0f;
    int sx, sy, gx, gy;
    proj3(x, y, z, cx, cy, sx, sy);
    proj3(x, y, 0.0f, cx, cy, gx, gy);
    if (!landed) display.drawPixel(gx, gy, SSD1306_WHITE);                // shadow
    if (u > 0.85f && ((now / 60) & 1)) continue;                          // fade out
    float phi = t * (10.0f + (h & 7)) + i;
    int dx = (int)(cosf(phi) * 2.0f), dy = (int)(sinf(phi) * 2.0f);
    display.drawLine(sx - dx, sy - dy, sx + dx, sy + dy, SSD1306_WHITE);
  }

  // 6) smoke rings rising from the wreck
  if (u > 0.3f && u < 0.95f) {
    for (int k = 0; k < 3; k++) {
      display.drawCircle(icx + (k - 1) * 4, icy - 4 - (int)(u * 14.0f) - k * 2, 2 + k, SSD1306_WHITE);
    }
  }
}

void drawBooms(uint32_t now) {
  for (int i = 0; i < MAX_BOOMS; i++) {
    if (booms[i].active) drawBoom3D(booms[i], now);
  }
}

void drawHud(uint32_t now) {
  display.drawFastVLine(HUD_X - 4, 0, SCREEN_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(HUD_X, 0);   display.print("SCORE");
  display.setCursor(HUD_X, 8);   display.print(score);

  // stars earned by destroying cars
  int st = wantedStars();
  for (int i = 0; i < 5; i++) {
    int x = HUD_X + i * 8;
    if (i < st) display.drawBitmap(x, 17, SPR_STAR, 5, 5, SSD1306_WHITE);
    else        display.drawPixel(x + 2, 19, SSD1306_WHITE);
  }

  display.setCursor(HUD_X, 26);
  display.print("LV ");
  display.print((unsigned)currentLevel());
  display.setCursor(HUD_X, 34);
  display.print("KILL ");
  display.print(kills);

  // FLY meter (common to every car): label blinks (inverted) when ready
  bool ready = (!flying && charge >= 1.0f);
  if (ready && ((now / 300) & 1)) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  else                            display.setTextColor(SSD1306_WHITE);
  display.setCursor(HUD_X, 43);
  display.print("FLY");
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(HUD_X + 21, 43, 19, 8, SSD1306_WHITE);
  bool warn = flying && charge < 0.2f && ((now / 90) & 1);
  int w = (int)(charge * 17.0f + 0.5f);
  if (w > 0 && !warn) display.fillRect(HUD_X + 22, 45, w, 4, SSD1306_WHITE);

  // CAR POWER meter (or rocket ammo)
  const CarDef &c = CARS[selCar];
  bool pReady = (c.power != PW_ROCKET) && !powerOn && powerCharge >= 1.0f;
  if (pReady && ((now / 300) & 1)) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  else                             display.setTextColor(SSD1306_WHITE);
  display.setCursor(HUD_X, 53);
  display.print(c.hudTag);
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(HUD_X + 21, 53, 19, 8, SSD1306_WHITE);
  if (c.power == PW_ROCKET) {
    for (int i = 0; i < ammo && i < ROCKET_AMMO; i++)
      display.fillRect(HUD_X + 23 + i * 3, 55, 2, 4, SSD1306_WHITE);
  } else {
    int pw = (int)(powerCharge * 17.0f + 0.5f);
    if (pw > 0) display.fillRect(HUD_X + 22, 55, pw, 4, SSD1306_WHITE);
  }
}

void renderScene(bool showPlayer, uint32_t now) {
  drawRoad((int)scrollF);
  drawStreaks();                    // behind the cars
  drawEnemies();
  drawRockets();
  drawBooms(now);
  if (showPlayer) drawPlayer(now);
  drawHud(now);
}

// Small side-view car used by the splash animation (facing right).
void drawSideCar(int x, int y, uint32_t now) {
  // speed lines behind the car
  for (int i = 0; i < 3; i++) {
    int len = 6 + (int)(((now / 45) + i * 5) % 9);
    display.drawFastHLine(x - 3 - len, y + 4 + i * 3, len, SSD1306_WHITE);
  }
  display.fillRoundRect(x, y + 3, 20, 6, 2, SSD1306_WHITE);     // body
  display.fillRoundRect(x + 5, y, 10, 5, 1, SSD1306_WHITE);     // cabin
  display.fillRect(x + 6,  y + 1, 3, 2, SSD1306_BLACK);         // windows
  display.fillRect(x + 11, y + 1, 3, 2, SSD1306_BLACK);
  display.drawPixel(x + 20, y + 5, SSD1306_WHITE);              // headlight beam
  display.drawPixel(x + 22, y + 4, SSD1306_WHITE);
  display.drawPixel(x + 22, y + 6, SSD1306_WHITE);
  for (int w = 0; w < 2; w++) {                                 // wheels
    int cx = x + 5 + w * 10;
    display.fillCircle(cx, y + 9, 3, SSD1306_BLACK);
    display.fillCircle(cx, y + 9, 2, SSD1306_WHITE);
    display.drawPixel(cx, y + 9, (now / 60) & 1 ? SSD1306_BLACK : SSD1306_WHITE);
  }
}

// FARO SPORTS splash: logo slides in, a glint sweeps over it, a car "loads" along the road.
void renderSplash(uint32_t now) {
  uint32_t t = now - stateStart;
  display.setTextColor(SSD1306_WHITE);

  // "FARO" slides in from the left, "SPORTS" from the right (ease-out)
  float p1 = t / 700.0f;                         if (p1 > 1) p1 = 1;
  float p2 = ((int32_t)t - 300) / 700.0f;        if (p2 < 0) p2 = 0; if (p2 > 1) p2 = 1;
  float e1 = 1.0f - (1.0f - p1) * (1.0f - p1) * (1.0f - p1);
  float e2 = 1.0f - (1.0f - p2) * (1.0f - p2) * (1.0f - p2);
  int x1 = -80 + (int)((28 + 80) * e1);
  int x2 = 130 + (int)((28 - 130) * e2);

  display.setTextSize(3);
  display.setCursor(x1, 2);
  display.print("FARO");
  display.setTextSize(2);
  display.setCursor(x2, 28);
  display.print("SPORTS");

  // underline grows from the centre
  if (t > 1000) {
    int w = (int)((t - 1000) / 5);
    if (w > 100) w = 100;
    display.drawFastHLine(64 - w / 2, 46, w, SSD1306_WHITE);
  }

  // glint sweeping over the logo
  if (t > 1300) {
    int sx = 22 + (int)(((t - 1300) % 1100) / 9);
    if (sx < 100) display.fillRect(sx, 2, 5, 42, SSD1306_INVERSE);
  }

  // loading: car drives left -> right, the road behind it fills up
  float p = ((int32_t)t - 200) / (float)(SPLASH_MS - 500);
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  int carX = -24 + (int)(p * (128 + 24));
  display.drawFastHLine(0, 62, 128, SSD1306_WHITE);
  int fill = carX + 10;
  if (fill > 128) fill = 128;
  if (fill > 0) display.fillRect(0, 61, fill, 3, SSD1306_WHITE);
  drawSideCar(carX, 49, now);
}

// 22x32 (2x) sprite, clipped horizontally so it stays inside the preview box.
void drawSprite2x(const uint8_t *spr, int x, int y, int clipL, int clipR) {
  for (int r = 0; r < CAR_H; r++) {
    uint16_t row = ((uint16_t)pgm_read_byte(&spr[r * 2]) << 8) | pgm_read_byte(&spr[r * 2 + 1]);
    for (int c = 0; c < CAR_W; c++) {
      if (!(row & (0x8000 >> c))) continue;
      int px = x + c * 2;
      if (px < clipL || px + 1 > clipR) continue;
      display.fillRect(px, y + r * 2, 2, 2, SSD1306_WHITE);
    }
  }
}

void drawStars(int x, int y, uint8_t n) {
  for (int i = 0; i < 5; i++) {
    if (i < n) display.fillRect(x + i * 7, y, 6, 5, SSD1306_WHITE);
    else       display.drawRect(x + i * 7, y, 6, 5, SSD1306_WHITE);
  }
}

void renderMenu(uint32_t now) {
  const CarDef &c = CARS[selCar];
  char buf[24];

  // ---- title bar ----
  display.fillRect(0, 0, SCREEN_WIDTH, 11, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(3, 2);
  display.print("SELECT CAR");
  snprintf(buf, sizeof(buf), "HI %lu", (unsigned long)hiScore);
  display.setCursor(SCREEN_WIDTH - (int)strlen(buf) * 6 - 3, 2);
  display.print(buf);
  display.setTextColor(SSD1306_WHITE);

  // ---- preview box with moving road markings ----
  display.drawRoundRect(2, 13, 46, 38, 3, SSD1306_WHITE);
  int s = (int)menuScroll;
  for (int k = 0; k < 2; k++) {
    int dx = k ? 40 : 7;
    for (int y = 15 + (s % 12) - 12; y < 49; y += 12) {
      int ys = y < 15 ? 15 : y;
      int ye = (y + 6) > 49 ? 49 : (y + 6);
      if (ye > ys) display.fillRect(dx, ys, 2, ye - ys, SSD1306_WHITE);
    }
  }
  int shake = menuConfirming ? (((now / 40) & 1) ? 1 : -1) : 0;
  int carX = 14 + (int)menuSlide + shake;
  display.fillRect(carX < 4 ? 4 : carX, 16, 22, 32, SSD1306_BLACK);
  drawSprite2x(c.spr, carX, 16, 4, 46);

  // ---- name + stats ----
  display.setTextSize(2);
  display.setCursor(54, 15);
  display.print(c.name);
  display.setTextSize(1);
  display.setCursor(54, 31);  display.print("GRIP");  drawStars(84, 32, c.gripStars);
  display.setCursor(54, 40);  display.print("PWR ");  display.print(c.powerName);
  display.setCursor(54, 47);  display.print(c.powerDesc);

  // ---- bottom bar: < o o o o >   [ START ] ----
  display.fillTriangle(3, 58, 8, 55, 8, 61, SSD1306_WHITE);
  display.fillTriangle(56, 58, 51, 55, 51, 61, SSD1306_WHITE);
  for (int i = 0; i < NUM_CARS; i++) {
    int cx = 16 + i * 9;
    if (i == selCar) display.fillCircle(cx, 58, 2, SSD1306_WHITE);
    else             display.drawCircle(cx, 58, 2, SSD1306_WHITE);
  }

  bool filled = menuConfirming ? (((now / 60) & 1) == 0) : true;
  if (filled) {
    display.fillRoundRect(62, 54, 64, 10, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.drawRoundRect(62, 54, 64, 10, 3, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
  }
  drawTextCentered("START", 94, 55, 1);
  display.setTextColor(SSD1306_WHITE);
}

void renderCountdown(uint32_t now) {
  renderScene(true, now);
  uint32_t step = (now - stateStart) / 700;
  const char *label;
  switch (step) {
    case 0:  label = "3";   break;
    case 1:  label = "2";   break;
    case 2:  label = "1";   break;
    default: label = "GO!"; break;
  }
  int w = (int)strlen(label) * 18;
  int x = (ROAD_X + ROAD_W / 2) - w / 2;
  display.fillRect(x - 4, 12, w + 8, 32, SSD1306_BLACK);
  display.drawRect(x - 4, 12, w + 8, 32, SSD1306_WHITE);
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, 16);
  display.print(label);

  // control hint
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setCursor(ROAD_X + 3, 49);
  display.print("START=FLY");
  char hint[16];
#if USE_POWER_BUTTON
  snprintf(hint, sizeof(hint), "PWR=%s", CARS[selCar].powerName);
#else
  snprintf(hint, sizeof(hint), "L+R=%s", CARS[selCar].powerName);
#endif
  display.setCursor(ROAD_X, 57);
  display.print(hint);
  display.setTextColor(SSD1306_WHITE);
}

void renderCrash(uint32_t now) {
  uint32_t t = now - stateStart;
  renderScene(t < 220, now);

  int cx = (int)playerX + CAR_W / 2;
  int cy = PLAYER_Y + CAR_H / 2;
  int r  = (int)(t / 25);

  if (r < 10) {
    display.fillCircle(cx, cy, r + 3, SSD1306_WHITE);
  } else {
    display.drawCircle(cx, cy, r, SSD1306_WHITE);
    display.drawCircle(cx, cy, r - 8, SSD1306_WHITE);
  }
  for (int i = 0; i < 12; i++) {
    float a = i * 0.5236f + t * 0.003f;
    float d = r + 2 + (i & 1) * 5;
    display.drawPixel(cx + (int)(cosf(a) * d), cy + (int)(sinf(a) * d), SSD1306_WHITE);
  }
  for (int i = 0; i < 4; i++) {
    display.fillRect(cx + (int)random(-r, r + 1), cy + (int)random(-r, r + 1), 2, 2, SSD1306_WHITE);
  }
}

void renderGameOver(uint32_t now) {
  char buf[24];
  uint32_t t = now - stateStart;

  display.setTextColor(SSD1306_WHITE);
  drawTextCentered("GAME OVER", 64, 3, 2);
  display.drawFastHLine(10, 21, 108, SSD1306_WHITE);

  snprintf(buf, sizeof(buf), "SCORE %lu", (unsigned long)score);
  drawTextCentered(buf, 64, 26, 1);
  snprintf(buf, sizeof(buf), "BEST  %lu", (unsigned long)hiScore);
  drawTextCentered(buf, 64, 36, 1);

  if (newHigh && ((now / 300) & 1)) {
    drawTextCentered("* NEW HIGH SCORE *", 64, 46, 1);
  }

  if (t > GAMEOVER_LOCK_MS && ((now / 450) & 1)) {
    drawTextCentered("START=RETRY <>=CARS", 64, 55, 1);
  }
}

void render(uint32_t now) {
  display.clearDisplay();
  switch (state) {
    case ST_SPLASH:    renderSplash(now);       break;
    case ST_MENU:      renderMenu(now);         break;
    case ST_COUNTDOWN: renderCountdown(now);    break;
    case ST_PLAYING:   renderScene(true, now);  break;
    case ST_CRASH:     renderCrash(now);        break;
    case ST_GAMEOVER:  renderGameOver(now);     break;
  }
  display.display();
}

// ============================================================================
//  ARDUINO ENTRY POINTS
// ============================================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_BTN_LEFT,  INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_CLOCK_HZ);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR, true, false)) {
    Serial.println("SSD1306 not found - check wiring and I2C address (0x3C / 0x3D)");
    while (true) delay(1000);
  }
  display.setTextWrap(false);
  display.clearDisplay();
  display.display();

  randomSeed(esp_random());
  loadHighScore();
  resetGame();

  lastFrame = millis();
  enterState(ST_SPLASH, lastFrame);
}

void loop() {
  uint32_t now = millis();

  buttonUpdate(btnLeft,  now);
  buttonUpdate(btnRight, now);
  buttonUpdate(btnStart, now);
  buttonUpdate(btnPower, now);

  if (now - lastFrame >= FRAME_MS) {
    float dt = (now - lastFrame) / 1000.0f;
    if (dt > 0.06f) dt = 0.06f;
    lastFrame = now;

    update(dt, now);
    render(now);
  }
}
