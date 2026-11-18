// Pet eyes for Haffler CodeRun V3 (ESP32-S3 + ST7789 240x320).
// Grumpy pet with oval anti-aliased eyes, moods, motion (ICM-42688), touch (GT911),
// calendar themes (DS3231 RTC) and mini games.
// Arduino IDE: board "ESP32S3 Dev Module", Flash Size 4MB, USB CDC On Boot: Enabled (USB2 port),
// library "LovyanGFX" from Library Manager.
//
// Gestures on the eyes screen:
//   stroke = happy, hold = love, tap = boop, 3 quick taps = swearing,
//   swipe down = feed, swipe up = games, tilt = look, shake = dizzy, shake again = vomit,
//   face down = sleep.

#include <functional>
#include "display.h"
#include "imu.h"
#include "touch.h"
#include "rtc.h"
#include "sound.h"
#include "cloud.h"
#include "link.h"

// ---------- Tuning ----------
static const uint8_t ROTATION   = 3;     // landscape 320x240
uint8_t brightness = 140;              // runtime, set from the panel
bool grumpy = true;                   // false = calm base face (panel setting)
static const float BASE_ANGER   = 0.55f; // how deep the resting frown is
int themeOverride = -1;                // -1 = from calendar, or a ThemeId number (panel setting)

static const float TILT_SIGN_X = 1.0f;   // tilt goes the wrong way? flip sign
static const float TILT_SIGN_Y = 1.0f;
static const float TILT_MIN    = 0.15f;  // g, dead zone
static const float JOLT_G      = 0.35f;  // sudden move -> surprised
static const float SHAKE_LIMIT = 2.5f;   // shake energy -> dizzy (again soon -> vomit)

static const bool TOUCH_SWAP_XY = true;  // measured on the board
static const bool TOUCH_FLIP_X  = true;
static const bool TOUCH_FLIP_Y  = false;

// ---------- Look ----------
struct Palette { uint32_t eye, glow1, glow2; };
static const Palette PAL_RAGE = {0xFF2414, 0x3A0804, 0x7A140C};
static const Palette PAL_LOVE = {0xFF72B6, 0x2E0E1C, 0x5C1D3A};
static const Palette PAL_SICK = {0x9BE15D, 0x15260A, 0x2E4A14};
static const Palette PAL_CALM = {0x48E6FF, 0x0A262E, 0x124A58};
static const uint32_t COL_BG    = 0x000000;
static const uint32_t COL_SHINE = 0xF2FEFF;

static const int EYE_RX  = 38;  // oval radii
static const int EYE_RY  = 52;
static const int EYE_GAP = 52;
static const int GAZE_X  = 34;
static const int GAZE_Y  = 18;
static const int BAND_H  = 190;  // only this band is redrawn on the eyes screen

LGFX lcd;
LGFX_Sprite canvas(&lcd);
int bandY = 0;

enum Mood { NORMAL, HAPPY, CURIOUS, SLEEPY, SURPRISED, DIZZY, ANGRY, SAD, LOVE, EAT, SWEAR, VOMIT };

struct EyeState {
  float x = 0, y = 0, tx = 0, ty = 0;
  float moodK = 0;  // 0..1 blend of current mood
  Mood mood = NORMAL;
  uint32_t moodStart = 0;
} eye;

// personality
float energy = 1.0f;  // 0 tired .. 1 rested
float joy = 0.5f;     // 0 sad .. 1 happy
float hunger = 0.3f;  // 0 full .. 1 starving
bool asleep = false;
float sleepK = 0;
uint32_t lastInteractMs = 0, faceDownSince = 0, lastDizzyMs = 0;

// timers
uint32_t nextGazeMs = 0, nextBlinkMs = 0, blinkStartMs = 0, nextMoodMs = 0, moodEndMs = 0;
bool blinking = false, doubleBlink = false;

// sensors
bool imuOk = false, touchOk = false;
ImuData imu;
float shakeEnergy = 0, tiltX = 0, tiltY = 0;

// calendar
DateTime clockNow;
uint32_t clockReadMs = 0;
int themeId = 0;
uint32_t themeShownMs = 0;

struct TouchTrack {
  bool down = false, petting = false;
  int x = 0, y = 0;
  uint32_t t0 = 0;
  float travel = 0;
} tp;
uint32_t tapTimes[3] = {0, 0, 0};

// eating
int foodType = 0, lastBite = 0;

bool secondPass = false;  // true while a full-screen page draws its second strip (see renderFull)

// ---------- helpers ----------
static float ease(float cur, float target, float k, float dt) {
  return cur + (target - cur) * (1.0f - powf(1.0f - k, dt * 60.0f));
}

static uint32_t mix(uint32_t a, uint32_t b, float t) {
  t = constrain(t, 0.0f, 1.0f);
  int r = ((a >> 16) & 0xFF) + (((int)((b >> 16) & 0xFF) - (int)((a >> 16) & 0xFF)) * t);
  int g = ((a >> 8) & 0xFF) + (((int)((b >> 8) & 0xFF) - (int)((a >> 8) & 0xFF)) * t);
  int c = (a & 0xFF) + (((int)(b & 0xFF) - (int)(a & 0xFF)) * t);
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)c;
}

static Palette mixPal(const Palette &a, const Palette &b, float t) {
  return {mix(a.eye, b.eye, t), mix(a.glow1, b.glow1, t), mix(a.glow2, b.glow2, t)};
}

#include "theme.h"
#include "fa_text.h"

static const Theme &theme() { return THEMES[themeId]; }

static void setMood(Mood m, uint32_t durMs, uint32_t now) {
  if (m != eye.mood) {
    eye.mood = m;
    eye.moodK = 0;
    eye.moodStart = now;
    if (m == EAT) { foodType = random(2); lastBite = 0; }
    switch (m) {  // a sound for each mood
      case HAPPY: soundPlay(SND_HAPPY); break;
      case LOVE: soundPlay(SND_LOVE); break;
      case ANGRY: soundPlay(SND_GROWL); break;
      case SWEAR: soundPlay(SND_BLEEP); break;
      case VOMIT: soundPlay(SND_VOMIT); break;
      case SURPRISED: soundPlay(SND_SURPRISE); break;
      case DIZZY: soundPlay(SND_DIZZY); break;
      default: break;
    }
  }
  moodEndMs = now + durMs;
}

static void interact(uint32_t now, bool startle) {
  lastInteractMs = now;
  if (asleep) {
    asleep = false;
    setMood(startle ? SURPRISED : NORMAL, startle ? 1200 : 10, now);
    nextBlinkMs = now + 300;
  }
}

static bool isNight() {
  int h = clockNow.hour, a = cloud.nightFrom, b = cloud.nightTo;
  return a > b ? (h >= a || h < b) : (h >= a && h < b);
}

// Sub-pixel center: moves smoothly by fractions of a pixel (no 1px jumps).
static void fillAAEllipse(float cx, float cy, float rx, float ry, uint32_t col, uint32_t under) {
  if (rx < 1 || ry < 1) return;
  int y0 = (int)floorf(cy - ry), y1 = (int)ceilf(cy + ry);
  for (int y = y0; y <= y1; y++) {
    float best = 0, cov = 0;
    for (int s = 0; s < 2; s++) {  // 2 samples per row for soft top/bottom edges
      float d = (y + 0.25f + 0.5f * s - cy) / ry;
      float t = 1.0f - d * d;
      if (t > 0) { best = max(best, rx * sqrtf(t)); cov += 0.5f; }
    }
    if (cov == 0) continue;
    float xl = cx - best, xr = cx + best;
    int il = (int)ceilf(xl), ir = (int)floorf(xr);
    if (ir > il) canvas.drawFastHLine(il, y, ir - il, cov >= 1.0f ? col : mix(under, col, cov));
    canvas.drawPixel(il - 1, y, mix(under, col, (il - xl) * cov));
    canvas.drawPixel(ir, y, mix(under, col, (xr - ir) * cov));
  }
}

// Erase everything above the line a->b (down from yTop), with an anti-aliased edge.
// Fast: one vertical line + one blended pixel per column.
static void cutAbove(float xa, float ya, float xb, float yb, float yTop) {
  if (xa > xb) { float t = xa; xa = xb; xb = t; t = ya; ya = yb; yb = t; }
  int x0 = max(0, (int)xa), x1 = min((int)canvas.width() - 1, (int)xb);
  int top = (int)yTop;
  for (int x = x0; x <= x1; x++) {
    float yl = ya + (yb - ya) * ((x + 0.5f - xa) / (xb - xa));
    int yi = (int)floorf(yl);
    if (yi > top) canvas.drawFastVLine(x, top, yi - top, COL_BG);
    if (yi < 0 || yi >= canvas.height()) continue;
    uint16_t c = canvas.readPixel(x, yi);
    uint32_t c888 = (((uint32_t)((c >> 11) & 0x1F) * 255 / 31) << 16) |
                    (((uint32_t)((c >> 5) & 0x3F) * 255 / 63) << 8) | ((uint32_t)(c & 0x1F) * 255 / 31);
    canvas.drawPixel(x, yi, mix(c888, COL_BG, 1 - (yl - yi)));
  }
}

static bool softMood(Mood m) {
  return m == HAPPY || m == LOVE || m == EAT || m == SAD || m == SURPRISED || m == SLEEPY || m == VOMIT ||
         m == DIZZY;
}

static void drawEye(float cx, float cy, float scale, float openK, bool leftEye, const Palette &p) {
  float rx = EYE_RX * scale;
  float ry = max(2.5f, EYE_RY * scale * openK);
  fillAAEllipse(cx, cy, rx + 10, ry + 10, p.glow1, COL_BG);
  fillAAEllipse(cx, cy, rx + 4, ry + 4, p.glow2, p.glow1);
  fillAAEllipse(cx, cy, rx, ry, p.eye, p.glow2);

  Mood m = eye.mood;
  float k = eye.moodK;
  bool happyShape = (m == HAPPY || m == LOVE || m == EAT) && k > 0.01f;

  // frown amount: grumpy at rest, deeper when angry, gone when happy/sad/etc.
  float brow = grumpy ? BASE_ANGER * (1 - sleepK) : 0;
  if (softMood(m)) brow *= (1 - k);
  if (m == ANGRY || m == SWEAR) brow += (1.15f - brow) * k;

  // Halloween cat pupil
  if (theme().catPupil && openK > 0.3f && !happyShape) {
    float px = cx + eye.x * 0.25f, py = cy + eye.y * 0.2f;
    fillAAEllipse(px, py, 4.5f * scale, ry * 0.62f, 0x1A0800, p.eye);
  }

  if (theme().pomegranate && !happyShape) drawPomegranate(cx, cy, rx, ry, openK);

  bool shine = openK > 0.45f && sleepK < 0.4f && !happyShape && brow < 0.8f && m != VOMIT;
  if (shine) {
    float hx = cx - rx * 0.38f + eye.x * 0.15f;
    float hy = cy - ry * 0.42f + eye.y * 0.15f + ry * 0.25f * brow;
    canvas.fillSmoothCircle(hx, hy, 7 * scale, COL_SHINE);
    canvas.fillSmoothCircle(hx + 12 * scale, hy + 11 * scale, 3 * scale, mix(p.eye, COL_SHINE, 0.6f));
  }

  float top = cy - ry - 14;
  float xo = leftEye ? cx - rx - 14 : cx + rx + 14;  // outer side
  float xi = leftEye ? cx + rx + 14 : cx - rx - 14;  // inner side (nose)

  if (happyShape) {  // "^ ^"
    float cutR = rx * 0.95f + 14;
    float off = ry + cutR - 2 * ry * 0.55f * k;
    canvas.fillSmoothCircle(cx, cy + off + 6, cutR, COL_BG);
  }
  if (brow > 0.02f) {  // frown toward the nose
    cutAbove(xo, top, xi, top + ry * brow, top);
  }
  if (m == SAD && k > 0.01f) {
    cutAbove(xi, top, xo, top + ry * 0.9f * k, top);
  }
  if (m == SLEEPY && k > 0.01f) {
    canvas.fillRect(cx - rx - 14, top, 2 * rx + 28, 14 + ry * 0.9f * k, COL_BG);
  }
  if (theme().santaHat && leftEye && sleepK < 0.9f) drawSantaHat(cx, cy, rx, max(ry, EYE_RY * scale * 0.6f));
}

// Food floating between the eyes while eating
static void drawFood(float t, float cx, float cy) {
  float tx = cx, ty = cy + 40;
  float sx = canvas.width() + 20;  // slides in from the right
  float x = t < 0.5f ? sx + (tx - sx) * (1 - powf(1 - t / 0.5f, 3)) : tx;
  int bites = constrain((int)((t - 0.5f) / 0.45f), 0, 4);
  if (bites > lastBite) {
    lastBite = bites;
    burstCrumbs(x + 8, ty - 4, foodType == 0 ? 0xFF3B3B : 0xC98B4A);
  }
  if (bites >= 4) return;
  if (foodType == 0) {  // apple
    canvas.fillSmoothCircle(x, ty, 14, 0xE8283A);
    canvas.fillSmoothCircle(x - 5, ty - 5, 4, 0xFF8A95);
    canvas.drawWideLine(x, ty - 13, x + 2, ty - 20, 1.3f, 0x6B3B1A);
    canvas.fillSmoothCircle(x + 6, ty - 18, 4, 0x3DDC84);
  } else {  // cookie
    canvas.fillSmoothCircle(x, ty, 14, 0xD39A55);
    canvas.fillSmoothCircle(x - 5, ty - 3, 2.2f, 0x4A2A12);
    canvas.fillSmoothCircle(x + 4, ty + 4, 2.2f, 0x4A2A12);
    canvas.fillSmoothCircle(x + 5, ty - 6, 1.8f, 0x4A2A12);
  }
  for (int i = 0; i < bites; i++) canvas.fillSmoothCircle(x + 12 - i * 3, ty - 10 + i * 7, 7, COL_BG);
}

bool lifeSick();  // life.h
static Mood pickRandomMood(uint32_t now) {
  float idle = (now - lastInteractMs) / 1000.0f;
  float wHappy = joy * 40;
  float wCurious = 25;
  float wSleepy = (1 - energy) * 40 + (idle > 25 ? 30 : 0) + (isNight() ? 30 : 0);
  float wSad = joy < 0.4f ? (0.4f - joy) * 120 : 0;
  if (lifeSick()) { wSad += 60; wSleepy += 40; wHappy *= 0.3f; }
  float wAngry = grumpy ? 15 + hunger * 40 : 0;  // hangry
  float wSwear = grumpy ? 4 + hunger * 10 : 0;
  float total = wHappy + wCurious + wSleepy + wSad + wAngry + wSwear;
  float r = random(1000) / 1000.0f * total;
  if ((r -= wHappy) < 0) return HAPPY;
  if ((r -= wCurious) < 0) return CURIOUS;
  if ((r -= wSleepy) < 0) return SLEEPY;
  if ((r -= wSad) < 0) return SAD;
  if ((r -= wAngry) < 0) return ANGRY;
  return SWEAR;
}

static void mapTouch(int rx, int ry, int &x, int &y) {
  float fx = rx / (float)gtMaxX, fy = ry / (float)gtMaxY;
  if (TOUCH_SWAP_XY) { float t = fx; fx = fy; fy = t; }
  if (TOUCH_FLIP_X) fx = 1 - fx;
  if (TOUCH_FLIP_Y) fy = 1 - fy;
  x = fx * lcd.width();
  y = fy * lcd.height();
}

// ---------- shared input (touch + tilt) ----------
struct Input {
  bool down = false, pressed = false, released = false, imuValid = false;
  int x = 0, y = 0, sx = 0, sy = 0;
  uint32_t downMs = 0;
  float tiltX = 0, tiltY = 0;
} in;
uint32_t touchCount = 0;

static void updateInput(uint32_t now, float dt) {
  in.pressed = in.released = false;
  if (touchOk && touchPoll()) {
    if (gtDown) {
      int x, y;
      mapTouch(gtX, gtY, x, y);
      if (!in.down) {
        in.pressed = true;
        in.sx = x; in.sy = y; in.downMs = now;
        touchCount++;
      }
      in.down = true;
      in.x = x; in.y = y;
    } else if (in.down) {
      in.down = false;
      in.released = true;
    }
  }
  in.imuValid = false;
  if (imuOk && imuRead(imu)) {
    float mag = sqrtf(imu.ax * imu.ax + imu.ay * imu.ay + imu.az * imu.az);
    if (mag > 0.2f) {  // ignore rare zero reads
      in.imuValid = true;
      in.tiltX = ease(in.tiltX, constrain(imu.ax * TILT_SIGN_X, -1.0f, 1.0f), 0.25f, dt);
      in.tiltY = ease(in.tiltY, constrain(imu.ay * TILT_SIGN_Y, -1.0f, 1.0f), 0.25f, dt);
    }
  }
}

#include "games.h"

void petReact(bool good) {
  uint32_t now = millis();
  lastInteractMs = now;
  if (good) { setMood(HAPPY, 3000, now); joy = min(1.0f, joy + 0.15f); }
  else { setMood(ANGRY, 2500, now); joy = max(0.0f, joy - 0.05f); }
}

void petFeed() {
  uint32_t now = millis();
  lastInteractMs = now;
  setMood(EAT, 3200, now);
}

#include "apps.h"

enum AppMode { MODE_PET, MODE_MENU, MODE_GAME, MODE_LAUNCHER, MODE_APP, MODE_MESSAGE, MODE_ALARM };
AppMode mode = MODE_PET;
int curGame = 0, curApp = 0;

// One 16-bit frame buffer (320 x 190) allocated once at boot and never freed, so memory
// fragmentation from WiFi/TLS can't push us into 8-bit color. The eyes use it as-is;
// full-screen pages are drawn in two strips (rows 0-189, then 190-239) into the same buffer.
static uint8_t *fbuf = nullptr;
static const int STRIP = BAND_H;

#define USE_STRIPS 0  // diagnostic switch: 0 = old sprite method

static void makeCanvas(int h) {
#if USE_STRIPS
  if (!fbuf) fbuf = (uint8_t *)heap_caps_malloc(320 * STRIP * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  canvas.clearClipRect();
  if (h == BAND_H) canvas.setBuffer(fbuf, 320, BAND_H, lgfx::rgb565_2Byte);
#else
  canvas.deleteSprite();
  canvas.setColorDepth(16);
  if (!canvas.createSprite(lcd.width(), h)) {
    canvas.setColorDepth(8);
    canvas.createSprite(lcd.width(), h);
  }
#endif
}

// Paint keeps its drawing on the display itself, so it bypasses the strip buffer.
static bool appDrawsDirect() { return curApp == APP_FUN && subSel == F_PAINT; }

// Draw a full-screen page in two strips. `draw` runs once per strip; input events and
// time steps only count on the first strip (secondPass = true on the second).
static bool renderFull(const std::function<bool()> &draw) {
#if !USE_STRIPS
  bool k = draw();
  canvas.pushSprite(0, 0);
  return k;
#endif
  Input saved = in;
  bool keep = true;
  for (int pass = 0; pass < 2; pass++) {
    int y0 = pass ? STRIP : 0, h = pass ? 240 - STRIP : STRIP;
    canvas.setBuffer(fbuf - (size_t)y0 * 320 * 2, 320, 240, lgfx::rgb565_2Byte);
    canvas.setClipRect(0, y0, 320, h);
    secondPass = pass == 1;
    if (secondPass) in.pressed = in.released = false;
    bool k = draw();
    if (!secondPass) keep = k;
    lcd.pushImage(0, y0, 320, h, (lgfx::swap565_t *)fbuf);
    if (!keep) break;
  }
  in = saved;
  secondPass = false;
  canvas.clearClipRect();
  return keep;
}
static void enterMenu() { makeCanvas(lcd.height()); mode = MODE_MENU; }
static void enterLauncher() { makeCanvas(lcd.height()); mode = MODE_LAUNCHER; soundPlay(SND_CLICK); }
static void enterApp(int id) {
  if (mode == MODE_PET) makeCanvas(lcd.height());
  if (id == APP_GAMES) { mode = MODE_MENU; return; }
  curApp = id;
  appOpen(id);
  mode = MODE_APP;
}
static void enterOverlay(int m) {  // int: Arduino hoists prototypes above the enum
  if (mode == MODE_PET) makeCanvas(lcd.height());
  overlayStart = millis();
  mode = (AppMode)m;
}
static void enterPet() {
  makeCanvas(BAND_H);
  lcd.fillScreen(COL_BG);
  for (auto &p : parts) p.kind = P_NONE;
  mode = MODE_PET;
}

static void updateClock(uint32_t now) {
  if (clockReadMs && now - clockReadMs < 30000) return;
  clockReadMs = now;
  time_t sys = time(nullptr);
  if (sys > 1700000000) {  // internet time is available: use it and keep the RTC in step
    struct tm t;
    localtime_r(&sys, &t);
    clockNow.year = t.tm_year + 1900; clockNow.month = t.tm_mon + 1; clockNow.day = t.tm_mday;
    clockNow.hour = t.tm_hour; clockNow.minute = t.tm_min; clockNow.second = t.tm_sec;
    static uint32_t rtcWrittenMs = 0;
    if (!rtcWrittenMs || now - rtcWrittenMs > 3600000UL) { rtcWrite(clockNow); rtcWrittenMs = now; }
  } else {
    rtcRead(clockNow);
  }
  int t = themeOverride >= 0 ? themeOverride : pickTheme(clockNow.month, clockNow.day);
  if (t != themeId || !themeShownMs) {
    themeId = t;
    themeShownMs = now;
    Serial.printf("THEME: %s (%04d-%02d-%02d %02d:%02d)\n", theme().name, clockNow.year, clockNow.month,
                  clockNow.day, clockNow.hour, clockNow.minute);
  }
}

// ---------- setup ----------
void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // never block when no PC is reading the USB serial
  randomSeed(esp_random());

  lcd.init();
  lcd.setRotation(ROTATION);
  lcd.setBrightness(brightness);
  lcd.fillScreen(COL_BG);

  imuOk = imuBegin();
  touchOk = touchBegin();
  rtcBegin(clockNow);
  soundBegin();
  cloudBegin();
  espnowBegin();

  bandY = (lcd.height() - BAND_H) / 2;
  makeCanvas(BAND_H);
  loadBest();
  lifeLoad();

  uint32_t now = millis();
  updateClock(now);
  lastInteractMs = now;
  nextGazeMs = now + 1200;
  nextBlinkMs = now + random(2200, 6000);
  nextMoodMs = now + random(12000, 22000);
}

// ---------- link between the cloud task and the pet ----------
static void runCommand(const CloudCmd &c, uint32_t now) {
  String cmd = c.cmd, arg = c.arg;
  Serial.printf("CMD: %s %s\n", c.cmd, c.arg);
  bool onPet = mode == MODE_PET;
  if (cmd == "feed") { if (!onPet) enterPet(); setMood(EAT, 3200, now); hunger = 0; joy = min(1.0f, joy + 0.2f); }
  else if (cmd == "pet") { if (!onPet) enterPet(); interact(now, false); setMood(HAPPY, 3000, now); }
  else if (cmd == "love") { if (!onPet) enterPet(); interact(now, false); setMood(LOVE, 3000, now); }
  else if (cmd == "angry") { if (!onPet) enterPet(); interact(now, false); setMood(ANGRY, 3500, now); }
  else if (cmd == "swear") { if (!onPet) enterPet(); interact(now, false); setMood(SWEAR, 3000, now); }
  else if (cmd == "vomit") { if (!onPet) enterPet(); interact(now, false); setMood(VOMIT, 3200, now); }
  else if (cmd == "dizzy") { if (!onPet) enterPet(); interact(now, false); setMood(DIZZY, 2600, now); }
  else if (cmd == "surprise") { if (!onPet) enterPet(); interact(now, true); setMood(SURPRISED, 1500, now); }
  else if (cmd == "sleep") { if (!onPet) enterPet(); asleep = true; eye.mood = NORMAL; eye.moodK = 0; }
  else if (cmd == "wake") { if (!onPet) enterPet(); interact(now, true); }
  else if (cmd == "sound") soundPlay(soundByName(arg));
  else if (cmd == "mute") soundOn = arg != "1";
  else if (cmd == "theme") { themeOverride = arg.toInt(); clockReadMs = 0; }
  else if (cmd == "brightness") { brightness = constrain(arg.toInt(), 10, 255); lcd.setBrightness(brightness); }
  else if (cmd == "reboot") ESP.restart();
  else if (cmd == "hafez") enterApp(APP_HAFEZ);
  else if (cmd == "ota") { xSemaphoreTake(cloudMux, portMAX_DELAY); cloud.fwId = arg.toInt(); cloud.otaProgress = 0; xSemaphoreGive(cloudMux); enterApp(APP_INFO); }
  else if (cmd == "screen") {
    if (arg == "pet") { if (!onPet) enterPet(); }
    else if (arg == "clock") enterApp(APP_CLOCK);
    else if (arg == "todo") enterApp(APP_TODO);
    else if (arg == "calendar") enterApp(APP_CALENDAR);
    else if (arg == "weather") enterApp(APP_WEATHER);
    else if (arg == "prices") enterApp(APP_PRICES);
    else if (arg == "games") enterApp(APP_GAMES);
  }
}

static void cloudBridge(uint32_t now) {
  // commands from the panel
  CloudCmd c;
  while (xQueueReceive(cmdQueue, &c, 0)) runCommand(c, now);

  static uint32_t lastPush = 0;
  static int seenRev = -2;
  if (now - lastPush < 500) return;
  lastPush = now;

  // extra senses
  static bool csiStarted = false;
  // WiFi motion sensing (CSI) is off: this board's WiFi library delivers too few samples to be reliable
  (void)csiStarted;
  static uint32_t lastHello = 0;
  if (now - lastHello > 5000) { lastHello = now; espnowHello(cloud.petName, eye.mood, lifeLevel()); }
  if (friendNew && mode == MODE_PET) {
    friendNew = false;
    overlayText = String("یک پت دیگر پیدا کردم: ") + friendName + " !";
    soundPlay(SND_LOVE);
    enterOverlay(MODE_MESSAGE);
    lifeAddXp(5);
  }

  // pet life: "missed you" at start, daily bonus, last-seen time, level-up message
  struct tm lt;
  nowTm(lt);
  uint32_t epoch = (uint32_t)mktime(&lt);
  static bool greeted = false;
  if (!greeted && mode == MODE_PET && now > 3000) {
    greeted = true;
    long gap = life.lastSeen ? (long)epoch - (long)life.lastSeen : 0;
    if (gap > 2L * 86400) {
      overlayText = "دلم برات تنگ شده بود! کجا بودی؟";
      soundPlay(SND_LOVE);
      enterOverlay(MODE_MESSAGE);
      joy = 0.2f;
    } else if (gap > 6L * 3600) {
      setMood(LOVE, 3000, now);
    }
  }
  static uint32_t lastLifeSave = 0;
  if (now - lastLifeSave > 60000) {
    lastLifeSave = now;
    life.lastSeen = epoch;
    if (lt.tm_yday != life.lastDay) { life.lastDay = lt.tm_yday; life.coins += 10; }  // daily bonus
    lifeSave();
  }
  if (levelUpPending && mode == MODE_PET) {
    levelUpPending = false;
    overlayText = "تبریک! پت شما به سطح " + faDigits(String(lifeLevel())) + " رسید و سکه جایزه گرفت";
    soundPlay(SND_MELODY);
    enterOverlay(MODE_MESSAGE);
  }

  xSemaphoreTake(cloudMux, portMAX_DELAY);
  // our state for the next sync
  petState.mood = eye.mood; petState.joy = joy; petState.hunger = hunger; petState.energy = energy;
  petState.asleep = asleep; petState.temp = temperatureRead();
  // settings from the panel
  if (cloud.rev != seenRev && cloud.lastOk) {
    seenRev = cloud.rev;
    grumpy = cloud.grumpy;
    soundOn = cloud.sound;
    buzzerOn = cloud.buzzer;
    soundVolume = cloud.volume;
    if (brightness != cloud.brightness) { brightness = cloud.brightness; lcd.setBrightness(brightness); }
    if (themeOverride != cloud.theme) { themeOverride = cloud.theme; clockReadMs = 0; }
    if (cloud.tz.length() && cloud.tz != net.tz) {  // time zone changed in the panel
      net.tz = cloud.tz;
      setenv("TZ", net.tz.c_str(), 1);
      tzset();
      netSave();
      clockReadMs = 0;
      Serial.printf("TZ: %s\n", net.tz.c_str());
    }
  }
  // new message (kept in the inbox while a game or alarm is on screen)
  String msg;
  bool busy = mode == MODE_GAME || mode == MODE_ALARM || mode == MODE_MESSAGE;
  if (cloud.msgCount && !busy) {
    msg = cloud.msgs[0];
    for (int i = 1; i < cloud.msgCount; i++) cloud.msgs[i - 1] = cloud.msgs[i];
    cloud.msgCount--;
  }
  // alarms: check once per minute
  struct tm t;
  bool haveTime = nowTm(t);
  static int lastMinute = -1;
  String almLabel, almKind;
  if (t.tm_min != lastMinute) {
    lastMinute = t.tm_min;
    char hhmm[6];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", t.tm_hour, t.tm_min);
    int satDay = (t.tm_wday + 1) % 7;  // bit 0 = Saturday
    for (int i = 0; i < cloud.alarmCount; i++) {
      AlarmItem &a = cloud.alarms[i];
      if (a.hhmm == hhmm && (a.days & (1 << satDay))) { almLabel = a.label; almKind = a.kind.length() ? a.kind : "alarm"; }
    }
    // water reminder every N minutes (daytime only)
    if (!almKind.length() && cloud.waterEvery > 0 && !isNight() && haveTime) {
      int mins = t.tm_hour * 60 + t.tm_min;
      if (mins % cloud.waterEvery == 0) almKind = "water";
    }
  }
  xSemaphoreGive(cloudMux);

  if (almKind.length() && mode != MODE_ALARM) {
    alarmLabel = almLabel;
    alarmKind = almKind;
    enterOverlay(MODE_ALARM);
  } else if (msg.length()) {
    overlayText = msg;
    soundPlay(SND_DING);
    enterOverlay(MODE_MESSAGE);
  }
}

// ---------- loop ----------
static void petFrame(uint32_t now, float dt);

void loop() {
  static uint32_t last = millis();
  uint32_t now = millis();
  float dt = min((now - last) / 1000.0f, 0.1f);
  last = now;
  updateInput(now, dt);
  updateClock(now);
  pcPoll();
  soundUpdate();
  cloudBridge(now);

  if (mode == MODE_PET) {
    petFrame(now, dt);
  } else if (mode == MODE_MENU) {
    int r = -1;
    renderFull([&] { int v = menuFrame(); if (!secondPass) r = v; return true; });
    if (r == -2) enterPet();
    else if (r >= 0) { curGame = r; startGame(r); mode = MODE_GAME; }
  } else if (mode == MODE_GAME) {
    bool keep = renderFull([&] { return gameFrame(curGame, secondPass ? 0 : dt); });
    if (!keep) enterPet();
  } else if (mode == MODE_LAUNCHER) {
    int r = -1;
    renderFull([&] { int v = launcherFrame(); if (!secondPass) r = v; return true; });
    if (r == -2) enterPet();
    else if (r >= 0) enterApp(r);
  } else if (mode == MODE_APP) {
    bool keep = appDrawsDirect() ? appRun(curApp) : renderFull([&] { return appRun(curApp); });
    if (!keep) enterPet();
  } else if (mode == MODE_MESSAGE || mode == MODE_ALARM) {
    bool keep = renderFull([&] { return mode == MODE_MESSAGE ? messageOverlay() : alarmOverlay(); });
    if (!keep) {
      bool wasAlarm = mode == MODE_ALARM;
      soundPlay(SND_CLICK);
      enterPet();
      setMood(wasAlarm ? ANGRY : HAPPY, 2000, now);
    }
  }

  static uint32_t lastLog = 0, frames = 0;
  frames++;
  if (now - lastLog > 2000) {
    Serial.printf("fps %.1f | depth %d heap %u | touches %u | mode %d mood %d energy %.2f joy %.2f hunger %.2f coins %d lv %d %s%s\n",
                  frames * 1000.0f / (now - lastLog), canvas.getColorDepth(), ESP.getFreeHeap(), touchCount, mode,
                  eye.mood, energy, joy, hunger, life.coins, lifeLevel(), asleep ? "ASLEEP " : "", life.sick ? "SICK" : "");
    Serial.printf("  pc %s cpu %.0f ram %.0f | csi score %.2f base %.2f pkts %u motion %d\n", pcOnline() ? "ON" : "off",
                  pcCpu, pcRam, (float)csiScore, (float)csiBase, (unsigned)csiPackets, motionNow());
    frames = 0;
    lastLog = now;
  }
}

static void petFrame(uint32_t now, float dt) {
  bool lookOverride = false;

  // --- motion ---
  bool tilted = false;
  if (in.imuValid) {
    float mag = sqrtf(imu.ax * imu.ax + imu.ay * imu.ay + imu.az * imu.az);
    float dev = fabsf(mag - 1.0f);
    shakeEnergy = shakeEnergy * powf(0.92f, dt * 60.0f) + dev;

    bool busy = eye.mood == DIZZY || eye.mood == VOMIT;
    if (shakeEnergy > SHAKE_LIMIT && !busy) {
      interact(now, true);
      if (now - lastDizzyMs < 7000) {
        setMood(VOMIT, 3200, now);  // shaken twice -> sick
        static uint32_t lastVomitMs = 0;
        if (hunger > 0.8f || (lastVomitMs && now - lastVomitMs < 60000)) { life.sick = true; lifeSave(); }
        lastVomitMs = now;
        joy = max(0.0f, joy - 0.15f);
      } else {
        setMood(DIZZY, 2600, now);
        joy = max(0.0f, joy - 0.05f);
      }
      lastDizzyMs = now;
    } else if (dev > JOLT_G && !busy && eye.mood != SURPRISED && eye.mood != EAT) {
      interact(now, true);
      setMood(SURPRISED, 1200, now);
    }

    tiltX = in.tiltX;
    tiltY = in.tiltY;
    tilted = fabsf(tiltX) > TILT_MIN || fabsf(tiltY) > TILT_MIN;
    if (tilted) interact(now, false);

    if (imu.az > 0.75f) {  // face down (screen to the table) -> sleep
      if (!faceDownSince) faceDownSince = now;
    } else {
      faceDownSince = 0;
    }
  }

  // --- touch ---
  if (in.down) {
    int x = in.x, y = in.y;
    if (!tp.down) {
      tp = {true, false, x, y, now, 0};
      interact(now, false);
    } else {
      tp.travel += hypotf(x - tp.x, y - tp.y);
      tp.x = x;
      tp.y = y;
    }
    lastInteractMs = now;
    eye.tx = constrain((x - lcd.width() / 2) / (lcd.width() / 2.0f), -1.0f, 1.0f) * GAZE_X * 1.2f;
    eye.ty = constrain((y - lcd.height() / 2) / (lcd.height() / 2.0f), -1.0f, 1.0f) * GAZE_Y * 1.4f;
    lookOverride = true;
    nextGazeMs = now + 1500;

    bool vertical = abs(in.y - in.sy) > abs(in.x - in.sx);
    if (tp.travel > 60 && !vertical) tp.petting = true;
    if (tp.petting) {
      setMood(HAPPY, 1500, now);
      joy = min(1.0f, joy + dt * 0.12f);
    } else if (now - tp.t0 > 900 && tp.travel < 25) {
      setMood(LOVE, 1500, now);  // long press = hug
      joy = min(1.0f, joy + dt * 0.08f);
    }
  } else if (tp.down) {
    uint32_t dur = now - tp.t0;
    int dx = in.x - in.sx, dy = in.y - in.sy;
    tp.down = false;
    if (-dy > 80 && abs(dx) < 80) {  // swipe up -> apps
      enterLauncher();
      return;
    }
    if (dy > 80 && abs(dx) < 80) {  // swipe down -> food
      setMood(EAT, 3200, now);
      hunger = 0;
      joy = min(1.0f, joy + 0.2f);
    } else if (!tp.petting && tp.travel < 25 && dur < 350) {
      tapTimes[0] = tapTimes[1];
      tapTimes[1] = tapTimes[2];
      tapTimes[2] = now;
      if (theme().tapFlowers) burstFlowers(theme(), canvas.width());
      if (tapTimes[0] && now - tapTimes[0] < 1500) {
        setMood(SWEAR, 2800, now);  // poked too much
        joy = max(0.0f, joy - 0.1f);
        tapTimes[0] = tapTimes[1] = tapTimes[2] = 0;
      } else if (eye.mood != SWEAR && eye.mood != ANGRY) {
        setMood(SURPRISED, 600, now);  // boop
      }
    }
  }

  // --- personality ---
  float idle = (now - lastInteractMs) / 1000.0f;
  joy = constrain(ease(joy, 0.5f, 0.002f, dt), 0.0f, 1.0f);
  hunger = min(1.0f, hunger + dt / 900.0f);  // hungry again after ~15 min
  if (!asleep) {
    energy = max(0.0f, energy - dt / ((isNight() || life.sick) ? 300.0f : 600.0f));
    static float starving = 0;  // starving too long -> sick
    starving = hunger >= 1.0f ? starving + dt : 0;
    if (starving > 600 && !life.sick) { life.sick = true; lifeSave(); }
    bool faceDown = faceDownSince && now - faceDownSince > 1500;
    float sleepAfter = isNight() ? 20 : 60;
    if (idle > sleepAfter || (energy < 0.2f && idle > 15) || faceDown) {
      asleep = true;
      eye.mood = NORMAL;
      eye.moodK = 0;
    }
  } else {
    energy = min(1.0f, energy + dt / 120.0f);
    if (energy >= 1.0f && !faceDownSince && !isNight() && random(1000) < 2) interact(now, false);
  }
  sleepK = ease(sleepK, asleep ? 1.0f : 0.0f, 0.06f, dt);

  // --- extra senses: busy PC -> sweat, someone moving in the room -> wake / look ---
  if (pcOnline() && pcCpu > 80 && !asleep && random(1000) < dt * 1500) {
    Particle *p = spawnParticle(P_DROP, false);
    if (p) {
      p->x = canvas.width() / 2 - 64 - 30 + random(-6, 7) + eye.x;
      p->y = canvas.height() / 2 - 58 + eye.y;
      p->vx = random(-15, 16); p->vy = 10; p->size = 3; p->life = 1.2f; p->col = 0x7FD8FF;
    }
  }
  if (motionNow()) {
    static uint32_t lastMotionReact = 0;
    if (now - lastMotionReact > 20000) {
      lastMotionReact = now;
      if (asleep && !isNight()) interact(now, false);
      else if (!asleep && eye.mood == NORMAL && idle > 8) setMood(CURIOUS, 2000, now);
    }
  }

  // --- gaze ---
  if (eye.mood == DIZZY) {
    float t = now / 1000.0f * 9.0f;
    eye.tx = cosf(t) * GAZE_X * 0.7f;
    eye.ty = sinf(t) * GAZE_Y * 0.9f;
  } else if (eye.mood == EAT) {
    eye.tx = 0;
    eye.ty = GAZE_Y * 0.5f;  // look at the food
  } else if (lookOverride) {
    // set by touch
  } else if (tilted) {
    eye.tx = constrain(tiltX * 1.6f, -1.0f, 1.0f) * GAZE_X * 1.3f;
    eye.ty = constrain(tiltY * 1.6f, -1.0f, 1.0f) * GAZE_Y * 1.3f;
    nextGazeMs = now + 800;
  } else if (asleep) {
    eye.tx = 0;
    eye.ty = GAZE_Y * 0.6f;
  } else if (now >= nextGazeMs) {
    if (random(100) < 35) { eye.tx = 0; eye.ty = 0; }
    else {
      eye.tx = random(-GAZE_X, GAZE_X + 1);
      eye.ty = random(-GAZE_Y, GAZE_Y + 1);
    }
    nextGazeMs = now + random(900, 3600);
  }
  float gk = eye.mood == DIZZY ? 0.4f : 0.12f;
  eye.x = ease(eye.x, eye.tx, gk, dt);
  eye.y = ease(eye.y, eye.ty, gk, dt);

  // --- blink ---
  if (!blinking && !asleep && now >= nextBlinkMs && eye.mood != SURPRISED) {
    blinking = true;
    blinkStartMs = now;
    doubleBlink = random(100) < 18;
  }
  float openK = 1.0f;
  if (blinking) {
    uint32_t t = now - blinkStartMs;
    if (t < 70) openK = 1.0f - t / 70.0f;
    else if (t < 200) openK = (t - 70) / 130.0f;
    else {
      blinking = false;
      if (doubleBlink) { doubleBlink = false; nextBlinkMs = now + 120; }
      else nextBlinkMs = now + random(2200, 6000);
    }
    openK = openK * openK * (3 - 2 * openK);
  }
  openK *= 1.0f - sleepK * 0.95f;

  // --- moods ---
  if (!asleep && eye.mood == NORMAL && now >= nextMoodMs) {
    setMood(pickRandomMood(now), random(2500, 4500), now);
  }
  if (eye.mood != NORMAL && now >= moodEndMs) {
    eye.moodK = ease(eye.moodK, 0, 0.15f, dt);
    if (eye.moodK < 0.02f) {
      eye.mood = NORMAL;
      eye.moodK = 0;
      nextMoodMs = now + random(10000, 22000);
    }
  } else if (eye.mood != NORMAL) {
    eye.moodK = ease(eye.moodK, 1, 0.12f, dt);
  }
  float moodT = (now - eye.moodStart) / 1000.0f;
  if (eye.mood == EAT && moodT > 0.5f && moodT < 2.3f) openK *= 1 - 0.3f * fabsf(sinf(moodT * 7));
  if (eye.mood == VOMIT) openK *= 1 - 0.55f * eye.moodK;

  // --- layout ---
  float breath = asleep ? sinf(now / 1000.0f * 0.9f) * 0.05f : sinf(now / 1000.0f * 1.6f) * 0.025f;
  float cx = canvas.width() / 2.0f + eye.x;
  float cy = canvas.height() / 2.0f + eye.y;
  if (eye.mood == ANGRY || eye.mood == SWEAR) cx += sinf(now / 1000.0f * 40.0f) * 0.9f * eye.moodK;
  if (eye.mood == VOMIT) cx += sinf(now / 1000.0f * 9.0f) * 3 * eye.moodK;
  float dx = EYE_RX + EYE_GAP / 2.0f;

  // --- effects ---
  if (eye.mood == SWEAR && moodT < 2.4f && random(1000) < dt * 12000) spawnGlyph(cx + random(-24, 25), cy - 46);
  if (eye.mood == VOMIT && moodT > 0.5f && moodT < 2.3f && random(1000) < dt * 30000) spawnDrop(cx, cy + 30);
  spawnAmbient(theme(), canvas.width(), canvas.height());
  updateParticles(dt, canvas.width(), canvas.height());

  // --- palette ---
  Palette pal = grumpy ? theme().pal : PAL_CALM;
  if (life.skin >= 0 && themeId < TH_HALLOWEEN) pal = SKINS[life.skin];  // bought eye color (holidays win)
  if (life.sick) pal = mixPal(pal, PAL_SICK, 0.45f);
  if (themeId >= TH_HALLOWEEN) pal = theme().pal;
  if (eye.mood == ANGRY || eye.mood == SWEAR) pal = mixPal(pal, PAL_RAGE, eye.moodK);
  if (eye.mood == LOVE) pal = mixPal(pal, PAL_LOVE, eye.moodK);
  if (eye.mood == VOMIT) pal = mixPal(pal, PAL_SICK, eye.moodK);

  float sL = 1.0f + breath, sR = 1.0f + breath;
  if (eye.mood == CURIOUS) {
    sL += 0.14f * eye.moodK;
    sR -= 0.10f * eye.moodK;
  } else if (eye.mood == SURPRISED) {
    sL += 0.18f * eye.moodK;
    sR += 0.18f * eye.moodK;
  } else if (eye.mood == DIZZY || eye.mood == VOMIT) {
    float w = sinf(now / 1000.0f * 14.0f) * 0.12f * eye.moodK;
    sL -= 0.08f * eye.moodK - w;
    sR -= 0.08f * eye.moodK + w;
  } else if (eye.mood == ANGRY || eye.mood == SWEAR) {
    sL -= 0.06f * eye.moodK;
    sR -= 0.06f * eye.moodK;
  }

  // --- draw ---
  canvas.fillScreen(COL_BG);
  drawParticles(true);  // theme particles behind the eyes
  if (theme().haftSin) drawHaftSin(canvas.width(), canvas.height());
  if (eye.mood == VOMIT) {  // puddle
    float r = min(1.0f, moodT / 2.0f) * 60 * eye.moodK;
    if (r > 2) fillAAEllipse(cx, canvas.height() - 10, r, 6, 0x5E9A2A, COL_BG);
  }
  drawEye(cx - dx, cy, sL, openK, true, pal);
  drawEye(cx + dx, cy, sR, openK, false, pal);
  if (sleepK < 0.95f) drawAccessories(cx, cy, dx, sL, sR, openK);
  if (eye.mood == EAT) drawFood(moodT, cx, cy);
  drawParticles(false);  // glyphs, crumbs, drops in front

  if (now - themeShownMs < 4000) {  // theme greeting
    float a = sinf(min(1.0f, (now - themeShownMs) / 4000.0f) * PI);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(mix(COL_BG, theme().pal.eye, a));
    canvas.drawString(theme().name, canvas.width() / 2, canvas.height() - 2);
  }
  canvas.pushSprite(0, bandY);
}
