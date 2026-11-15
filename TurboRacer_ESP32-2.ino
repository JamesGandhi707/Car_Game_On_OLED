/*
 * ============================================================================
 *  TURBO RACER v2 (with FLY MODE)  -  ESP32 + SSD1306 128x64 OLED
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
 *     START  button -> GPIO 23   <- put this one in the CENTRE. It is also the FLY button
 *     RIGHT  button -> GPIO 19
 *
 *  CONTROLS
 *     LEFT / RIGHT : change lane
 *     START        : start game / restart / FLY (during the race)
 *
 *  FLY MODE
 *     - The FLY meter (bottom right of the screen) must be FULL (label blinks).
 *     - Press START: wings deploy, the car lifts off, the world speeds up
 *       (x1.8) and speed lines streak past. You fly over enemy cars safely.
 *     - Flight lasts 5 seconds, then the car lands (short blinking safety time)
 *       and the meter needs about 12 seconds to recharge.
 *
 *  2-BUTTON MODE (USE_START_BUTTON 0): LEFT/RIGHT start & restart the game,
 *  and pressing LEFT + RIGHT together triggers FLY.
 * ============================================================================
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>


// ============================================================================
//  USER CONFIGURATION
// ============================================================================
#define USE_START_BUTTON 1        // 1 = three buttons, 0 = two buttons

// ---- GPIO pins
#define PIN_SDA        21
#define PIN_SCL        22
#define PIN_BTN_LEFT   18
#define PIN_BTN_RIGHT  19
#define PIN_BTN_START  23         // centre button = START + FLY

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

// ============================================================================
//  GLOBAL OBJECTS
// ============================================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET,
                         I2C_CLOCK_HZ, I2C_CLOCK_HZ);
Preferences prefs;

// ---- Custom types: defined BEFORE any function (Arduino auto-prototypes) ----
enum GameState : uint8_t { ST_TITLE, ST_COUNTDOWN, ST_PLAYING, ST_CRASH, ST_GAMEOVER };

struct Enemy {
  bool    active;
  uint8_t lane;
  uint8_t type;   // 0 = sedan, 1 = van
  float   y;
};

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
#if USE_START_BUTTON
Button btnStart = { PIN_BTN_START, false, 0, false };
#endif

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
#if USE_START_BUTTON
  btnStart.pressedEvent = false;
#endif
}

// True when the player wants to start / restart the game.
bool startPressed() {
#if USE_START_BUTTON
  bool s = takePress(btnStart);
  takePress(btnLeft);
  takePress(btnRight);
  return s;
#else
  bool l = takePress(btnLeft);
  bool r = takePress(btnRight);
  return l || r;
#endif
}

// True when the FLY button was pressed (START, or LEFT+RIGHT together in 2-button mode).
bool flyRequested() {
#if USE_START_BUTTON
  return takePress(btnStart);
#else
  if (btnLeft.stable && btnRight.stable) {
    btnLeft.pressedEvent = false;
    btnRight.pressedEvent = false;
    return true;
  }
  return false;
#endif
}

// ---- Game state -------------------------------------------------------------
GameState state       = ST_TITLE;
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
float    titleScroll  = 0;
float    spawnDist    = 0;
float    nextGap      = 70;
float    scoreFrac    = 0;
uint8_t  prevFree     = 0x07;
uint32_t score        = 0;
uint32_t hiScore      = 0;
bool     newHigh      = false;
bool     inverted     = false;
uint32_t resetHoldStart = 0;

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
  prefs.end();
}

void saveHighScore() {
  prefs.begin("turboracer", false);
  prefs.putUInt("hi", hiScore);
  prefs.end();
}

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
  float step = LATERAL_SPEED * dt;
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

void updatePlaying(float dt, uint32_t now) {
  // --- FLY button ---
  if (flyRequested() && !flying && charge >= 1.0f) {
    flying  = true;
    invulnT = 0.0f;
  }

  steerInput();
  movePlayer(dt);
  updateFlight(dt);

  // --- speed (base grows with time, fly adds a boost) ---
  playTime += dt;
  speed = BASE_SPEED + playTime * ACCEL;
  if (speed > MAX_SPEED) speed = MAX_SPEED;
  float boost = 1.0f + (FLY_BOOST - 1.0f) * wingDeploy;
  curSpeed = speed * boost;
  float adv = curSpeed * dt;
  scrollF += adv;
  updateStreaks(dt);

  // distance score (automatically higher while flying because adv is boosted)
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
  bool ghost = flying || (invulnT > 0.0f);          // cannot crash while flying / just landed
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

void update(float dt, uint32_t now) {
  uint32_t t = now - stateStart;

  switch (state) {
    case ST_TITLE: {
      titleScroll += 70.0f * dt;
      if (startPressed()) { beginRun(now); break; }
#if USE_START_BUTTON
      if (btnLeft.stable && btnRight.stable) {      // hold L+R 3 s = erase high score
        if (resetHoldStart == 0) resetHoldStart = now;
        else if (now - resetHoldStart >= HI_RESET_HOLD_MS) {
          hiScore = 0;
          saveHighScore();
          resetHoldStart = 0;
        }
      } else {
        resetHoldStart = 0;
      }
#else
      if (t >= AUTO_START_MS) beginRun(now);
#endif
      break;
    }

    case ST_COUNTDOWN:
      flyRequested();                               // ignore fly presses before GO
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
      bool pressed = startPressed();
      if (t > GAMEOVER_LOCK_MS && pressed) beginRun(now);
#if !USE_START_BUTTON
      else if (t >= AUTO_START_MS) beginRun(now);
#endif
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

  drawCar(SPR_PLAYER, px, py);
}

void drawHud(uint32_t now) {
  uint32_t best = (score > hiScore) ? score : hiScore;

  display.drawFastVLine(HUD_X - 4, 0, SCREEN_HEIGHT, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(HUD_X, 0);   display.print("SCORE");
  display.setCursor(HUD_X, 9);   display.print(score);
  display.setCursor(HUD_X, 19);  display.print("BEST");
  display.setCursor(HUD_X, 28);  display.print(best);
  display.setCursor(HUD_X, 38);  display.print("LV ");
  display.print((unsigned)currentLevel());

  // speed bar (shows the fly boost too)
  display.drawRect(HUD_X, 47, 40, 5, SSD1306_WHITE);
  float frac = (curSpeed - BASE_SPEED) / (MAX_SPEED * FLY_BOOST - BASE_SPEED);
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  display.fillRect(HUD_X + 2, 48, 1 + (int)(frac * 35.0f), 3, SSD1306_WHITE);

  // FLY meter: label blinks (inverted) when ready
  bool ready = (!flying && charge >= 1.0f);
  if (ready && ((now / 300) & 1)) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  else                            display.setTextColor(SSD1306_WHITE);
  display.setCursor(HUD_X, 54);
  display.print("FLY");
  display.setTextColor(SSD1306_WHITE);

  display.drawRect(HUD_X + 21, 54, 19, 8, SSD1306_WHITE);
  bool warn = flying && charge < 0.2f && ((now / 90) & 1);
  int w = (int)(charge * 17.0f + 0.5f);
  if (w > 0 && !warn) display.fillRect(HUD_X + 22, 56, w, 4, SSD1306_WHITE);
}

void renderScene(bool showPlayer, uint32_t now) {
  drawRoad((int)scrollF);
  drawStreaks();                    // behind the cars
  drawEnemies();
  if (showPlayer) drawPlayer(now);
  drawHud(now);
}

void renderTitle(uint32_t now) {
  char buf[24];

  drawCar(SPR_PLAYER, 8, 4);
  drawCar(SPR_VAN, 109, 4);

  display.setTextColor(SSD1306_WHITE);
  drawTextCentered("TURBO", 64, 1, 2);
  drawTextCentered("RACER", 64, 19, 2);

  display.drawFastHLine(0, 36, SCREEN_WIDTH, SSD1306_WHITE);
  display.drawFastHLine(0, 44, SCREEN_WIDTH, SSD1306_WHITE);
  for (int x = -((int)titleScroll % 16); x < SCREEN_WIDTH; x += 16) {
    display.fillRect(x, 39, 8, 2, SSD1306_WHITE);
  }

  snprintf(buf, sizeof(buf), "HI SCORE %lu", (unsigned long)hiScore);
  drawTextCentered(buf, 64, 47, 1);

  if ((now / 450) & 1) {
#if USE_START_BUTTON
    drawTextCentered("PRESS START", 64, 55, 1);
#else
    drawTextCentered("PRESS ANY BUTTON", 64, 55, 1);
#endif
  }
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
#if USE_START_BUTTON
  display.setCursor(ROAD_X + 3, 50);
  display.print("START=FLY");
#else
  display.setCursor(ROAD_X + 9, 50);
  display.print("L+R=FLY");
#endif
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
#if USE_START_BUTTON
    drawTextCentered("PRESS START", 64, 55, 1);
#else
    drawTextCentered("PRESS ANY BUTTON", 64, 55, 1);
#endif
  }
}

void render(uint32_t now) {
  display.clearDisplay();
  switch (state) {
    case ST_TITLE:     renderTitle(now);        break;
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
#if USE_START_BUTTON
  pinMode(PIN_BTN_START, INPUT_PULLUP);
#endif

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
  enterState(ST_TITLE, lastFrame);
}

void loop() {
  uint32_t now = millis();

  buttonUpdate(btnLeft,  now);
  buttonUpdate(btnRight, now);
#if USE_START_BUTTON
  buttonUpdate(btnStart, now);
#endif

  if (now - lastFrame >= FRAME_MS) {
    float dt = (now - lastFrame) / 1000.0f;
    if (dt > 0.06f) dt = 0.06f;
    lastFrame = now;

    update(dt, now);
    render(now);
  }
}
