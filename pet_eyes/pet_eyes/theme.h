#pragma once
// Calendar themes (seasons + holidays) and a small particle system.
// Needs: Palette, mix(), canvas, COL_BG (from pet_eyes.ino).

enum ThemeId {
  TH_AUTUMN, TH_WINTER, TH_SPRING, TH_SUMMER,
  TH_HALLOWEEN, TH_YALDA, TH_CHRISTMAS, TH_NOWRUZ, TH_COUNT
};

enum PKind : uint8_t { P_NONE, P_LEAF, P_SNOW, P_PETAL, P_SPARK, P_BAT, P_SEED, P_GLYPH, P_CRUMB, P_DROP, P_FLOWER };

struct Theme {
  const char *name;
  Palette pal;
  PKind particle;
  uint8_t count;     // ambient particles on screen (0 = only on tap)
  uint32_t c1, c2;   // particle colors
  bool catPupil;     // Halloween slit pupils
  bool santaHat;     // Santa hat (winter + Christmas)
  bool pomegranate;  // Yalda: eyes look like open pomegranates
  bool haftSin;      // Nowruz: haft-sin table at the bottom
  bool tapFlowers;   // Spring: flowers rain down when you tap
};

// Base look is grumpy red; holidays change the eye color.
static const Palette PAL_GRUMPY = {0xFF4A3A, 0x2A0806, 0x561410};

static const Theme THEMES[TH_COUNT] = {
  {"Autumn",          PAL_GRUMPY,                     P_LEAF,   10, 0xE8742A, 0xB8441C, false, false, false, false, false},
  {"Winter",          PAL_GRUMPY,                     P_SNOW,   28, 0xFFFFFF, 0xBFDFFF, false, true,  false, false, false},
  {"Spring",          PAL_GRUMPY,                     P_FLOWER,  0, 0xFFB7D5, 0xFFF3A0, false, false, false, false, true },
  {"Summer",          PAL_GRUMPY,                     P_SPARK,  12, 0xFFD45C, 0xFF9E3D, false, false, false, false, false},
  {"Happy Halloween", {0xFF8A1F, 0x24103A, 0x5A2A0E}, P_BAT,     5, 0x2A1240, 0x7A3DB8, true,  false, false, false, false},
  {"Shab-e Yalda",    {0xC8102E, 0x2E0610, 0x5C0C1E}, P_SEED,   10, 0xE0102F, 0xFF5A70, false, false, true,  false, false},
  {"Merry Christmas", {0xE8F6FF, 0x0F2A1C, 0x1F5A3A}, P_SNOW,   30, 0xFFFFFF, 0xD8ECFF, false, true,  false, false, false},
  {"Nowruz Mobarak",  {0x5BE37D, 0x0B2A14, 0x165A2A}, P_PETAL,   8, 0xFFB7D5, 0xFFF3A0, false, false, false, true,  true },
};

static ThemeId pickTheme(int m, int d) {
  if ((m == 10 && d >= 20) || (m == 11 && d <= 1)) return TH_HALLOWEEN;
  if (m == 12 && d >= 19 && d <= 22) return TH_YALDA;
  if ((m == 12 && d >= 23) || (m == 1 && d <= 1)) return TH_CHRISTMAS;
  if ((m == 3 && d >= 16) || (m == 4 && d <= 2)) return TH_NOWRUZ;
  if (m >= 3 && m <= 5) return TH_SPRING;
  if (m >= 6 && m <= 8) return TH_SUMMER;
  if (m >= 9 && m <= 11) return TH_AUTUMN;
  return TH_WINTER;
}

// ---------- particles ----------
struct Particle {
  float x, y, vx, vy, age, life, phase, size;
  uint32_t col;
  PKind kind;
  char ch;
  bool ambient;
};
static const int NP = 64;
Particle parts[NP];

static Particle *spawnParticle(PKind k, bool ambient) {
  for (auto &p : parts)
    if (p.kind == P_NONE) {
      p = {};
      p.kind = k;
      p.ambient = ambient;
      p.phase = random(1000) / 159.0f;
      return &p;
    }
  return nullptr;
}

static int countAmbient() {
  int n = 0;
  for (auto &p : parts) if (p.kind != P_NONE && p.ambient) n++;
  return n;
}

static void spawnAmbient(const Theme &th, int w, int h) {
  if (countAmbient() >= th.count || random(100) > 12) return;
  Particle *p = spawnParticle(th.particle, true);
  if (!p) return;
  p->col = random(2) ? th.c1 : th.c2;
  p->x = random(w);
  p->y = -6;
  p->life = 12;
  switch (th.particle) {
    case P_LEAF:  p->vy = random(18, 34); p->vx = random(-8, 9); p->size = random(3, 5); break;
    case P_SNOW:  p->vy = random(14, 32); p->vx = random(-6, 7); p->size = random(10, 26) / 10.0f; break;
    case P_PETAL: p->vy = random(14, 26); p->vx = random(4, 16); p->size = random(2, 4); break;
    case P_SEED:  p->vy = random(26, 46); p->vx = random(-4, 5); p->size = random(20, 30) / 10.0f; break;
    case P_SPARK: p->y = random(h); p->vy = -random(6, 14); p->life = random(15, 30) / 10.0f; p->size = random(10, 22) / 10.0f; break;
    case P_BAT: {
      bool left = random(2);
      p->x = left ? -12 : w + 12;
      p->y = random(10, h / 2);
      p->vx = (left ? 1 : -1) * random(35, 65);
      p->vy = 0;
      p->size = random(4, 7);
      p->life = 15;
      break;
    }
    default: break;
  }
}

static void updateParticles(float dt, int w, int h) {
  for (auto &p : parts) {
    if (p.kind == P_NONE) continue;
    p.age += dt;
    if (p.kind == P_CRUMB || p.kind == P_DROP) p.vy += 420 * dt;  // gravity
    if (p.kind == P_LEAF || p.kind == P_PETAL || p.kind == P_FLOWER) p.x += sinf(p.age * 2.2f + p.phase) * 22 * dt;
    if (p.kind == P_SNOW) p.x += sinf(p.age * 1.3f + p.phase) * 8 * dt;
    if (p.kind == P_BAT) p.y += sinf(p.age * 3 + p.phase) * 18 * dt;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    if (p.age > p.life || p.y > h + 10 || p.y < -30 || p.x < -30 || p.x > w + 30) p.kind = P_NONE;
  }
}

static void drawParticles(bool ambient) {
  for (auto &p : parts) {
    if (p.kind == P_NONE || p.ambient != ambient) continue;
    float fade = constrain((p.life - p.age) / 0.5f, 0.0f, 1.0f);
    if (p.kind == P_SPARK) fade *= sinf(constrain(p.age / p.life, 0.0f, 1.0f) * PI);
    uint32_t c = mix(COL_BG, p.col, fade);
    switch (p.kind) {
      case P_SNOW:
      case P_SPARK:
        canvas.fillSmoothCircle(p.x, p.y, p.size, c);
        break;
      case P_LEAF: {
        float a = sinf(p.age * 2.2f + p.phase) * 0.8f;
        canvas.fillSmoothCircle(p.x - cosf(a) * 2, p.y - sinf(a) * 2, p.size * 0.8f, c);
        canvas.fillSmoothCircle(p.x + cosf(a) * 2, p.y + sinf(a) * 2, p.size * 0.8f, c);
        break;
      }
      case P_PETAL:
        canvas.fillSmoothCircle(p.x, p.y, p.size, c);
        canvas.fillSmoothCircle(p.x + p.size, p.y + 1, p.size * 0.7f, mix(COL_BG, 0xFFFFFF, fade * 0.8f));
        break;
      case P_SEED:
        canvas.fillSmoothCircle(p.x, p.y, p.size, c);
        canvas.fillSmoothCircle(p.x - 0.8f, p.y - 0.8f, p.size * 0.35f, mix(COL_BG, 0xFFC0C8, fade));
        break;
      case P_BAT: {
        float flap = sinf(p.age * 18) * p.size * 0.8f;
        float s = p.size;
        canvas.fillTriangle(p.x, p.y, p.x - s * 2.2f, p.y - flap, p.x - s * 1.2f, p.y + s * 0.6f, c);
        canvas.fillTriangle(p.x, p.y, p.x + s * 2.2f, p.y - flap, p.x + s * 1.2f, p.y + s * 0.6f, c);
        canvas.fillSmoothCircle(p.x, p.y, s * 0.6f, mix(COL_BG, p.col == 0x2A1240 ? 0x7A3DB8 : p.col, fade));
        break;
      }
      case P_GLYPH: {
        char s[2] = {p.ch, 0};
        canvas.setFont(&fonts::FreeSansBold12pt7b);
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(c);
        canvas.drawString(s, p.x, p.y);
        break;
      }
      case P_FLOWER: {
        float rot = p.age * 2 + p.phase;
        for (int k = 0; k < 5; k++) {
          float a = rot + k * 2 * PI / 5;
          canvas.fillSmoothCircle(p.x + cosf(a) * p.size, p.y + sinf(a) * p.size, p.size * 0.75f, c);
        }
        canvas.fillSmoothCircle(p.x, p.y, p.size * 0.55f, mix(COL_BG, 0xFFD45C, fade));
        break;
      }
      case P_CRUMB:
      case P_DROP:
        canvas.fillSmoothCircle(p.x, p.y, p.size, c);
        break;
      default: break;
    }
  }
}

// Effects used by moods
static void burstCrumbs(float x, float y, uint32_t col) {
  for (int i = 0; i < 7; i++) {
    Particle *p = spawnParticle(P_CRUMB, false);
    if (!p) return;
    p->x = x; p->y = y;
    p->vx = random(-70, 71); p->vy = -random(40, 130);
    p->size = random(10, 22) / 10.0f; p->life = 1.0f;
    p->col = col;
  }
}

static void spawnGlyph(float x, float y) {
  static const char G[] = "#@$%&!*?";
  static const uint32_t C[] = {0xFFD23F, 0xFF3B2F, 0xFFFFFF, 0xFF8A1F};
  Particle *p = spawnParticle(P_GLYPH, false);
  if (!p) return;
  p->x = x; p->y = y;
  p->vx = random(-70, 71); p->vy = -random(50, 110);
  p->life = 0.9f;
  p->ch = G[random(sizeof(G) - 1)];
  p->col = C[random(4)];
}

static void spawnDrop(float x, float y) {
  Particle *p = spawnParticle(P_DROP, false);
  if (!p) return;
  p->x = x + random(-6, 7); p->y = y;
  p->vx = random(-45, 46); p->vy = random(20, 90);
  p->size = random(18, 36) / 10.0f; p->life = 1.4f;
  p->col = random(2) ? 0x8FD14F : 0x6FAE2E;
}

// Spring: a shower of flowers when the screen is tapped
static void burstFlowers(const Theme &th, int w) {
  static const uint32_t FC[5] = {0xFFB7D5, 0xFFFFFF, 0xFF8FB8, 0xFFF3A0, 0xC8A8FF};
  for (int i = 0; i < 14; i++) {
    Particle *p = spawnParticle(th.particle, true);
    if (!p) return;
    p->x = random(w);
    p->y = -random(4, 60);
    p->vy = random(30, 60);
    p->vx = random(-10, 11);
    p->size = random(3, 6);
    p->life = 9;
    p->col = th.particle == P_FLOWER ? FC[random(5)] : (random(2) ? th.c1 : th.c2);
  }
}

// Yalda: seeds (arils) inside the eye + a small crown on top, like an open pomegranate
static void drawPomegranate(float cx, float cy, float rx, float ry, float openK) {
  if (openK < 0.35f) return;
  const float sp = 11;
  for (float y = -ry + 8; y < ry - 6; y += sp * 0.85f) {
    float half = rx * sqrtf(max(0.0f, 1 - (y * y) / (ry * ry))) - 7;
    float off = ((int)((y + ry) / (sp * 0.85f)) % 2) * sp * 0.5f;
    for (float x = -half + off; x <= half; x += sp) {
      canvas.fillSmoothCircle(cx + x, cy + y, 4.2f, 0xFF4D6A);
      canvas.fillSmoothCircle(cx + x - 1.2f, cy + y - 1.4f, 1.3f, 0xFFC7D2);
    }
  }
  float top = cy - ry * openK - 2;  // calyx crown
  for (int k = -2; k <= 2; k++)
    canvas.fillTriangle(cx + k * 6 - 4, top + 3, cx + k * 6 + 4, top + 3, cx + k * 6, top - 8 - (k == 0 ? 3 : 0), 0x7A0F1E);
}

// Nowruz: haft-sin table along the bottom of the band
static void drawHaftSin(int w, int h) {
  int y = h - 16;
  canvas.fillSmoothRoundRect(8, y - 2, w - 16, 16, 6, 0x8E1B2E);       // termeh cloth
  canvas.drawFastHLine(12, y + 1, w - 24, 0xD9A441);
  canvas.drawFastHLine(12, y + 10, w - 24, 0xD9A441);
  float sway = sinf(millis() / 600.0f) * 1.5f;
  int x = 24;
  const int step = (w - 48) / 7;
  // 1 sabzeh (sprouts)
  canvas.fillSmoothRoundRect(x - 9, y - 6, 18, 8, 3, 0x8A5A3A);
  for (int i = -3; i <= 3; i++) canvas.drawWideLine(x + i * 2.5f, y - 6, x + i * 2.5f + sway, y - 18 - abs(i), 1.1f, 0x5BE37D);
  x += step;  // 2 sib (apple)
  canvas.fillSmoothCircle(x, y - 6, 7, 0xE8283A);
  canvas.fillSmoothCircle(x + 3, y - 14, 3, 0x3DDC84);
  x += step;  // 3 sir (garlic)
  canvas.fillSmoothCircle(x, y - 5, 6, 0xF2EDE4);
  canvas.fillTriangle(x - 3, y - 9, x + 3, y - 9, x, y - 17, 0xF2EDE4);
  x += step;  // 4 samanu
  fillAAEllipse(x, y - 4, 9, 4, 0xC9D6E3, COL_BG);
  fillAAEllipse(x, y - 6, 7, 2.5f, 0x7A4A22, 0xC9D6E3);
  x += step;  // 5 serkeh (vinegar)
  canvas.fillSmoothRoundRect(x - 4, y - 16, 8, 16, 3, 0x9E6B2A);
  canvas.fillRect(x - 2, y - 20, 4, 5, 0x6B4A22);
  x += step;  // 6 senjed
  for (int i = 0; i < 5; i++) canvas.fillSmoothCircle(x - 5 + (i % 3) * 5, y - 3 - (i / 3) * 4, 2.5f, 0xB5651D);
  x += step;  // 7 sekkeh (coins)
  canvas.fillSmoothCircle(x - 3, y - 4, 4.5f, 0xFFC93C);
  canvas.fillSmoothCircle(x + 4, y - 6, 4.5f, 0xFFD45C);
  x += step;  // goldfish bowl
  canvas.fillSmoothCircle(x, y - 9, 9, 0x2A6FB0);
  float fx = sinf(millis() / 700.0f) * 3;
  canvas.fillSmoothCircle(x + fx, y - 9, 2.5f, 0xFF8A1F);
}

// Santa hat on top of one eye
static void drawSantaHat(float cx, float cy, float rx, float ry) {
  float base = cy - ry - 6;
  canvas.fillTriangle(cx - rx * 0.85f, base, cx + rx * 0.85f, base, cx + rx * 1.25f, base - 34, 0xD7263D);
  canvas.fillSmoothRoundRect(cx - rx, base - 4, rx * 2, 10, 5, 0xF4F7FA);
  canvas.fillSmoothCircle(cx + rx * 1.25f, base - 34, 6, 0xF4F7FA);
}
