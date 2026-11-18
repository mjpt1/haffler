#pragma once
// Game menu + 4 tap/tilt games: Flappy Pet, Dino Run, Stack, Fruit Catch.
// Uses the full-screen `canvas` sprite, the shared `in` (Input) and petReact()/petFeed() from pet_eyes.ino.
#include <Preferences.h>

void petReact(bool good);  // defined in pet_eyes.ino
void petFeed();
void lifeAddCoins(int n);  // life.h
void lifeAddXp(int n);

enum GameId { G_FLAPPY, G_DINO, G_STACK, G_CATCH, G_COUNT };
static const char *GAME_NAMES[G_COUNT] = {"Flappy Pet", "Dino Run", "Stack", "Fruit Catch"};
static const uint32_t GAME_COL[G_COUNT] = {0xFFC24B, 0x48E6FF, 0xB98CFF, 0xFF6B6B};

static const int SW = 320, SH = 240;
static const uint32_t UI_BG = 0x06090D, UI_PANEL = 0x111A22, UI_TEXT = 0xEAF6FF, UI_DIM = 0x7D8C99;

Preferences prefs;
int bestScore[G_COUNT] = {0};

static void loadBest() {
  prefs.begin("pet", false);
  bestScore[G_FLAPPY] = prefs.getInt("flappy", 0);  // keep the old Flappy record
  for (int i = 1; i < G_COUNT; i++) {
    char k[8];
    snprintf(k, sizeof(k), "g%d", i);
    bestScore[i] = prefs.getInt(k, 0);
  }
}

static bool submitScore(int g, int s) {
  lifeAddCoins(min(50, s / 2 + 1));  // every finished game pays a little
  lifeAddXp(5);
  if (s <= bestScore[g]) return false;
  bestScore[g] = s;
  char k[8];
  if (g == G_FLAPPY) snprintf(k, sizeof(k), "flappy");
  else snprintf(k, sizeof(k), "g%d", g);
  prefs.putInt(k, s);
  return true;
}

// ---------- UI helpers ----------
static void drawCloseBtn() {
  canvas.fillSmoothCircle(SW - 20, 20, 13, 0x1C2630);
  canvas.drawWideLine(SW - 25, 15, SW - 15, 25, 1.4f, UI_TEXT);
  canvas.drawWideLine(SW - 15, 15, SW - 25, 25, 1.4f, UI_TEXT);
}
static bool closeTapped() { return in.pressed && in.x > SW - 48 && in.y < 48; }

static void textC(const char *s, int x, int y, const lgfx::IFont *f, uint32_t c) {
  canvas.setFont(f);
  canvas.setTextColor(c);
  canvas.setTextDatum(middle_center);
  canvas.drawString(s, x, y);
}

static void vGradient(uint32_t top, uint32_t bottom) {
  for (int y = 0; y < SH; y++) canvas.drawFastHLine(0, y, SW, mix(top, bottom, y / (float)SH));
}

static uint32_t hsv(float h, float s, float v) {
  h = fmodf(h, 360);
  float c = v * s, x = c * (1 - fabsf(fmodf(h / 60, 2) - 1)), m = v - c, r, g, b;
  if (h < 60) { r = c; g = x; b = 0; }
  else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; }
  else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; }
  else { r = c; g = 0; b = x; }
  return ((uint32_t)((r + m) * 255) << 16) | ((uint32_t)((g + m) * 255) << 8) | (uint32_t)((b + m) * 255);
}

static void hudScore(int s, uint32_t col) {
  char b[12];
  snprintf(b, sizeof(b), "%d", s);
  textC(b, SW / 2, 20, &fonts::FreeSansBold12pt7b, col);
}

// The pet as a little glowing blob with one eye (used in games).
static void drawBlob(float x, float y, float r, float lookY = 0) {
  canvas.fillSmoothCircle(x, y, r + 3, 0x561410);
  canvas.fillSmoothCircle(x, y, r, 0xFF4A3A);
  canvas.fillSmoothCircle(x + r * 0.33f, y - r * 0.25f + lookY, r * 0.33f, 0x1A0605);
  canvas.fillSmoothCircle(x + r * 0.42f, y - r * 0.33f + lookY, r * 0.12f, 0xFFFFFF);
}

// "Game over" card. Returns true when the player taps to play again.
static bool gameOverCard(int g, int score, bool newBest) {
  int w = 220, h = 140, x = (SW - w) / 2, y = (SH - h) / 2;
  canvas.fillSmoothRoundRect(x - 2, y - 2, w + 4, h + 4, 18, GAME_COL[g]);
  canvas.fillSmoothRoundRect(x, y, w, h, 16, UI_PANEL);
  textC("Game Over", SW / 2, y + 26, &fonts::FreeSansBold12pt7b, UI_TEXT);
  char b[32];
  snprintf(b, sizeof(b), "%d", score);
  textC(b, SW / 2, y + 66, &fonts::FreeSansBold18pt7b, GAME_COL[g]);
  snprintf(b, sizeof(b), newBest ? "New best!" : "Best  %d", bestScore[g]);
  textC(b, SW / 2, y + 100, &fonts::FreeSans9pt7b, newBest ? GAME_COL[g] : UI_DIM);
  textC("tap to play again", SW / 2, y + 124, &fonts::FreeSans9pt7b, UI_DIM);
  return in.pressed && !closeTapped();
}

// ============================================================
// Flappy Pet (tap to flap)
// ============================================================
namespace flappy {
const int PW = 46, GAP = 84, BX = 80;
const float GRAV = 950, FLAP = -280, SPEED = 115;
struct Pipe { float x; int gapY; bool scored; } p[3];
float y, v;
int score;
bool over, newBest;

void start() {
  y = SH / 2; v = 0; score = 0; over = false; newBest = false;
  for (int i = 0; i < 3; i++) p[i] = {SW + 60.0f + i * 160, (int)random(60, SH - 60), false};
}

bool frame(float dt) {
  if (closeTapped()) { petReact(score >= 5); return false; }
  if (!over) {
    if (in.pressed) { v = FLAP; soundPlay(SND_FLAP); }
    v += GRAV * dt;
    y += v * dt;
    for (auto &q : p) {
      q.x -= SPEED * dt;
      if (q.x < -PW) {
        float maxX = 0;
        for (auto &o : p) maxX = max(maxX, o.x);
        q = {maxX + 160, (int)random(60, SH - 60), false};
      }
      if (!q.scored && q.x + PW < BX) { q.scored = true; score++; soundPlay(SND_POINT); }
      bool inX = BX + 11 > q.x && BX - 11 < q.x + PW;
      bool inGap = y - 11 > q.gapY - GAP / 2 && y + 11 < q.gapY + GAP / 2;
      if (inX && !inGap) over = true;
    }
    if (y > SH - 8 || y < -20) over = true;
    if (over) { newBest = submitScore(G_FLAPPY, score); petReact(false); }
  }

  vGradient(0x0B1630, 0x24426B);
  for (auto &q : p) {
    int top = q.gapY - GAP / 2, bot = q.gapY + GAP / 2;
    canvas.fillSmoothRoundRect(q.x, -10, PW, top + 10, 8, 0x2FBF71);
    canvas.fillSmoothRoundRect(q.x, bot, PW, SH - bot + 10, 8, 0x2FBF71);
    canvas.fillRect(q.x + 6, 0, 6, top - 6, 0x63E59B);
    canvas.fillRect(q.x + 6, bot + 6, 6, SH - bot - 6, 0x63E59B);
  }
  canvas.fillRect(0, SH - 8, SW, 8, 0x1A2B44);
  drawBlob(BX, y, 12, constrain(v / 100.0f, -2.0f, 3.0f));
  hudScore(score, UI_TEXT);
  if (over && gameOverCard(G_FLAPPY, score, newBest)) start();
  drawCloseBtn();
  return true;
}
}  // namespace flappy

// ============================================================
// Dino Run (tap to jump over cacti and bats)
// ============================================================
namespace dino {
const int GROUND = SH - 40, PX = 64, R = 13;
struct Obs { float x; int w, h; bool bat; } ob[3];
float y, v, speed, dist, groundScroll;
int score;
bool over, newBest;
uint8_t starX[30], starY[30];

void placeObs(Obs &o, float x) {
  o.x = x;
  o.bat = score > 12 && random(4) == 0;
  o.w = o.bat ? 26 : random(14, 24);
  o.h = o.bat ? 14 : random(24, 44);
}

void start() {
  y = GROUND - R; v = 0; speed = 170; dist = 0; score = 0; groundScroll = 0;
  over = false; newBest = false;
  for (int i = 0; i < 3; i++) placeObs(ob[i], SW + 40 + i * 210 + random(0, 120));
  for (int i = 0; i < 30; i++) { starX[i] = random(256); starY[i] = random(GROUND - 20); }
}

bool frame(float dt) {
  if (closeTapped()) { petReact(score >= 20); return false; }
  if (!over) {
    bool onGround = y >= GROUND - R - 0.5f;
    if (in.pressed && onGround) { v = -430; soundPlay(SND_FLAP); }
    v += 1350 * dt;
    y = min((float)(GROUND - R), y + v * dt);
    if (y >= GROUND - R) v = 0;
    speed += 5 * dt;
    dist += speed * dt;
    groundScroll = fmodf(groundScroll + speed * dt, 24);
    score = dist / 25;
    for (auto &o : ob) {
      o.x -= speed * dt;
      if (o.x < -40) {
        float maxX = 0;
        for (auto &q : ob) maxX = max(maxX, q.x);
        placeObs(o, max((float)SW + 20, maxX + random(180, 320)));
      }
      float top = o.bat ? GROUND - 34 : GROUND - o.h;
      float cx = constrain((float)PX, o.x, o.x + o.w), cy = constrain(y, top, top + o.h);
      if ((PX - cx) * (PX - cx) + (y - cy) * (y - cy) < (R - 2) * (R - 2)) over = true;
    }
    if (over) { newBest = submitScore(G_DINO, score); petReact(false); }
  }

  vGradient(0x070B1E, 0x1B2550);
  for (int i = 0; i < 30; i++) {
    int sx = ((int)(starX[i] * 1.25f - dist * 0.05f) % SW + SW) % SW;
    canvas.drawPixel(sx, starY[i], i % 3 ? 0x8090C0 : 0xFFFFFF);
  }
  canvas.fillSmoothCircle(270, 44, 16, 0xF4F1DE);  // moon
  canvas.fillSmoothCircle(278, 38, 14, 0x0A0F26);
  canvas.fillRect(0, GROUND, SW, SH - GROUND, 0x111833);
  canvas.drawFastHLine(0, GROUND, SW, 0x3A4A7A);
  for (int x = -(int)groundScroll; x < SW; x += 24) canvas.drawFastHLine(x, GROUND + 8, 8, 0x26305A);
  for (auto &o : ob) {
    if (o.bat) {
      float top = GROUND - 34, flap = sinf(millis() / 60.0f) * 6;
      float bx = o.x + o.w / 2.0f, by = top + 7;
      canvas.fillTriangle(bx, by, bx - 14, by - flap, bx - 6, by + 5, 0x7A3DB8);
      canvas.fillTriangle(bx, by, bx + 14, by - flap, bx + 6, by + 5, 0x7A3DB8);
      canvas.fillSmoothCircle(bx, by, 5, 0x4A2380);
    } else {
      canvas.fillSmoothRoundRect(o.x, GROUND - o.h, o.w, o.h + 4, 6, 0x2FBF71);
      canvas.fillSmoothRoundRect(o.x - 7, GROUND - o.h * 0.65f, 8, o.h * 0.3f, 4, 0x2FBF71);
      canvas.fillSmoothRoundRect(o.x + o.w - 1, GROUND - o.h * 0.8f, 8, o.h * 0.3f, 4, 0x2FBF71);
      canvas.fillRect(o.x + 3, GROUND - o.h + 4, 3, o.h - 6, 0x63E59B);
    }
  }
  float squish = y >= GROUND - R - 0.5f ? sinf(millis() / 50.0f) * 0.6f : 0;
  drawBlob(PX, y + squish, R, v > 0 ? 2 : -1);
  hudScore(score, UI_TEXT);
  if (over && gameOverCard(G_DINO, score, newBest)) start();
  drawCloseBtn();
  return true;
}
}  // namespace dino

// ============================================================
// Stack (tap to drop the sliding block)
// ============================================================
namespace stack {
const int BH = 16, MAXB = 48, FLOOR = SH - 24;
struct Blk { float x, w; } b[MAXB];
int n, base, score;
float curX, curW, dir, speed, cam;
bool over, newBest;
bool falling;
float fallX, fallW, fallY, fallVy;
uint32_t fallCol;

uint32_t blockCol(int i) { return hsv(200 + i * 11, 0.6f, 0.95f); }
float blockY(int i) { return FLOOR - (i + 1) * BH + cam; }

void start() {
  n = 1; base = 0; score = 0;
  b[0] = {SW / 2.0f - 60, 120};
  curW = 120; curX = 0; dir = 1; speed = 130; cam = 0;
  over = false; newBest = false; falling = false;
}

bool frame(float dt) {
  if (closeTapped()) { petReact(score >= 10); return false; }
  int top = n + base;  // absolute index of the moving block
  if (!over) {
    curX += dir * speed * dt;
    if (curX < 0) { curX = 0; dir = 1; }
    if (curX + curW > SW) { curX = SW - curW; dir = -1; }
    if (in.pressed) {
      Blk &prev = b[n - 1];
      float l = max(curX, prev.x), r = min(curX + curW, prev.x + prev.w);
      float w = r - l;
      falling = true;
      fallY = blockY(top);
      fallVy = 0;
      fallCol = blockCol(top);
      if (w <= 0) {
        fallX = curX; fallW = curW;
        over = true;
        newBest = submitScore(G_STACK, score);
        petReact(score >= 10);
      } else {
        if (fabsf(w - curW) < 3) { w = curW; l = prev.x; falling = false; }  // perfect drop
        else if (curX < prev.x) { fallX = curX; fallW = prev.x - curX; }
        else { fallX = r; fallW = curX + curW - r; }
        if (n == MAXB) { memmove(b, b + 1, sizeof(Blk) * (MAXB - 1)); n--; base++; }
        b[n++] = {l, w};
        score++;
        soundPlay(SND_POINT);
        curW = w;
        dir = (score % 2) ? -1 : 1;
        curX = dir > 0 ? 0 : SW - curW;
        speed = min(260.0f, speed + 6);
      }
    }
  }
  cam = cam + (max(0.0f, (float)(n + base - 8) * BH) - cam) * min(1.0f, dt * 6);
  if (falling) {
    fallVy += 900 * dt;
    fallY += fallVy * dt;
    if (fallY > SH + 20) falling = false;
  }

  vGradient(mix(0x0A0618, blockCol(n + base), 0.15f), 0x05030C);
  for (int i = 0; i < n; i++) {
    float y = blockY(i + base);
    if (y > SH || y < -BH) continue;
    canvas.fillRect(b[i].x, y, b[i].w, BH, blockCol(i + base));
    canvas.drawFastHLine(b[i].x, y, b[i].w, mix(blockCol(i + base), 0xFFFFFF, 0.4f));
  }
  if (!over) {
    float y = blockY(n + base);
    canvas.fillRect(curX, y, curW, BH, blockCol(n + base));
    canvas.drawFastHLine(curX, y, curW, mix(blockCol(n + base), 0xFFFFFF, 0.4f));
  }
  if (falling) canvas.fillRect(fallX, fallY, fallW, BH, fallCol);
  hudScore(score, UI_TEXT);
  if (over && !falling && gameOverCard(G_STACK, score, newBest)) start();
  drawCloseBtn();
  return true;
}
}  // namespace stack

// ============================================================
// Fruit Catch (tilt or drag to feed the pet, avoid bombs)
// ============================================================
namespace catcher {
const int PY = SH - 26, PR = 20;
struct Item { float x, y, vy; uint8_t type; bool alive; } it[8];
float px, spawnT, chomp;
int score, lives;
bool over, newBest;
static const uint32_t FRUIT_COL[3] = {0xFF3B3B, 0xFFA02E, 0x9B5CFF};

void start() {
  px = SW / 2; spawnT = 0.5f; chomp = 0; score = 0; lives = 3;
  over = false; newBest = false;
  for (auto &i : it) i.alive = false;
}

bool frame(float dt) {
  if (closeTapped()) {
    petReact(score >= 10);
    if (score >= 10) petFeed();
    return false;
  }
  if (!over) {
    if (in.down) px += (in.x - px) * min(1.0f, dt * 12);
    else px += in.tiltX * 520 * dt;
    px = constrain(px, (float)PR, (float)(SW - PR));
    chomp = max(0.0f, chomp - dt * 4);

    spawnT -= dt;
    if (spawnT <= 0) {
      spawnT = max(0.35f, 1.0f - score * 0.02f);
      for (auto &i : it)
        if (!i.alive) {
          i = {(float)random(16, SW - 16), -12, (float)(80 + score * 4 + random(0, 60)),
               (uint8_t)(score > 4 && random(5) == 0 ? 3 : random(3)), true};
          break;
        }
    }
    for (auto &i : it) {
      if (!i.alive) continue;
      i.y += i.vy * dt;
      if (fabsf(i.x - px) < PR + 6 && i.y > PY - PR - 6 && i.y < PY + 4) {
        i.alive = false;
        if (i.type == 3) lives--;
        else { score++; chomp = 1; soundPlay(SND_CHOMP); }
      } else if (i.y > SH + 12) {
        i.alive = false;
        if (i.type != 3) lives--;
      }
    }
    if (lives <= 0) {
      over = true;
      newBest = submitScore(G_CATCH, score);
      petReact(false);
    }
  }

  vGradient(0x1A0B12, 0x2B1420);
  canvas.fillRect(0, SH - 6, SW, 6, 0x3A1C2A);
  for (auto &i : it) {
    if (!i.alive) continue;
    if (i.type == 3) {
      canvas.fillSmoothCircle(i.x, i.y, 9, 0x1C1C24);
      canvas.fillSmoothCircle(i.x - 3, i.y - 3, 2.5f, 0x55556A);
      canvas.drawWideLine(i.x + 5, i.y - 7, i.x + 9, i.y - 13, 1.2f, 0xC9A26B);
      if ((millis() / 120) % 2) canvas.fillSmoothCircle(i.x + 9, i.y - 13, 2.5f, 0xFFD23F);
    } else {
      canvas.fillSmoothCircle(i.x, i.y, 9, FRUIT_COL[i.type]);
      canvas.fillSmoothCircle(i.x - 3, i.y - 3, 2.5f, mix(FRUIT_COL[i.type], 0xFFFFFF, 0.6f));
      canvas.fillSmoothCircle(i.x + 3, i.y - 10, 3, 0x3DDC84);
    }
  }
  // pet with an open mouth
  canvas.fillSmoothCircle(px, PY, PR + 3, 0x561410);
  canvas.fillSmoothCircle(px, PY, PR, 0xFF4A3A);
  canvas.fillSmoothCircle(px - 7, PY - 6, 3.5f, 0x1A0605);
  canvas.fillSmoothCircle(px + 7, PY - 6, 3.5f, 0x1A0605);
  float mouth = 3 + chomp * 5;
  canvas.fillSmoothRoundRect(px - 8, PY + 3, 16, mouth, mouth / 2, 0x1A0605);
  for (int i = 0; i < 3; i++)
    canvas.fillSmoothCircle(24 + i * 14, 20, 4.5f, i < lives ? 0xFF3B2F : 0x2A1414);
  hudScore(score, UI_TEXT);
  if (over && gameOverCard(G_CATCH, score, newBest)) start();
  drawCloseBtn();
  return true;
}
}  // namespace catcher

// ============================================================
// Menu
// ============================================================
// returns: -2 = back to pet, -1 = stay, >=0 = start that game
static int menuFrame() {
  vGradient(0x0B0608, 0x1A0E12);
  textC("Games", SW / 2, 24, &fonts::FreeSansBold12pt7b, UI_TEXT);
  int pick = -1;
  for (int i = 0; i < G_COUNT; i++) {
    int x = 16 + (i % 2) * 148, y = 52 + (i / 2) * 92, w = 140, h = 84;
    bool hot = in.down && in.x > x && in.x < x + w && in.y > y && in.y < y + h;
    canvas.fillSmoothRoundRect(x, y, w, h, 14, hot ? GAME_COL[i] : mix(UI_BG, GAME_COL[i], 0.35f));
    canvas.fillSmoothRoundRect(x + 2, y + 2, w - 4, h - 4, 12, mix(UI_PANEL, GAME_COL[i], hot ? 0.25f : 0.08f));
    canvas.fillSmoothCircle(x + 24, y + 26, 9, GAME_COL[i]);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextColor(UI_TEXT);
    canvas.setTextDatum(middle_left);
    canvas.drawString(GAME_NAMES[i], x + 14, y + 54);
    char b[16];
    snprintf(b, sizeof(b), "best %d", bestScore[i]);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(UI_DIM);
    canvas.drawString(b, x + 14, y + 72);
    if (in.released && in.x > x && in.x < x + w && in.y > y && in.y < y + h) pick = i;
  }
  drawCloseBtn();
  if (closeTapped()) return -2;
  return pick;
}

static void startGame(int g) {
  if (g == G_FLAPPY) flappy::start();
  if (g == G_DINO) dino::start();
  if (g == G_STACK) stack::start();
  if (g == G_CATCH) catcher::start();
}

static bool gameFrame(int g, float dt) {
  switch (g) {
    case G_FLAPPY: return flappy::frame(dt);
    case G_DINO: return dino::frame(dt);
    case G_STACK: return stack::frame(dt);
    case G_CATCH: return catcher::frame(dt);
  }
  return false;
}
