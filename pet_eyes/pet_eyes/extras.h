#pragma once
// Tools (dice, magic ball, level, seismograph, pomodoro, breathing, stopwatch)
// and Fun (paint, piano, aquarium, fire, matrix, fireworks).
// Each screen returns false when closed. Needs apps.h helpers + imu/in/sound/cloud.

static float accDev() {
  return in.imuValid ? fabsf(sqrtf(imu.ax * imu.ax + imu.ay * imu.ay + imu.az * imu.az) - 1) : 0;
}

// ---------- sub-menu ----------
// returns -2 = close, -1 = stay, >= 0 = picked row
static int subMenu(const char *title, const char *const *names, const uint32_t *cols, int n, uint32_t accent) {
  if (appClose()) return -2;
  appFrame(title, accent);
  int pick = -1;
  for (int i = 0; i < n; i++) {
    int c = 1 - (i % 2), r = i / 2;
    int x = 12 + c * 152, y = 46 + r * 38, w = 144, h = 34;
    bool hot = in.down && in.x > x && in.x < x + w && in.y > y && in.y < y + h;
    canvas.fillSmoothRoundRect(x, y, w, h, 10, hot ? mix(A_CARD, cols[i], 0.35f) : A_CARD);
    canvas.fillSmoothCircle(x + w - 16, y + h / 2, 6, cols[i]);
    faDraw(names[i], x + w - 28, y + 8, faSmall, A_TEXT, 2);
    if (in.released && in.x > x && in.x < x + w && in.y > y && in.y < y + h) pick = i;
  }
  return pick;
}

// ============================================================ TOOLS
enum ToolId { T_DICE, T_BALL, T_LEVEL, T_SEISMO, T_POMO, T_BREATH, T_WATCH, T_PC, T_MOTION, T_COUNT };
static const char *TOOL_NAMES[T_COUNT] = {"تاس", "توپ جادویی", "تراز", "لرزه‌نگار", "پومودورو", "تمرین تنفس", "کرنومتر",
                                          "مانیتور کامپیوتر", "حرکت در اتاق"};
static const uint32_t TOOL_COL[T_COUNT] = {0xFF6B6B, 0xB98CFF, 0x5BE37D, 0x48E6FF, 0xFF8A3D, 0x7FD8FF, 0xFFD45C,
                                           0x3DDC84, 0xFF72B6};

// PC monitor (data from tools/pc_monitor.py over USB)
static bool pcTool() {
  if (appClose()) return false;
  appFrame("مانیتور کامپیوتر", TOOL_COL[T_PC]);
  if (!pcOnline()) {
    faDraw("برنامه‌ی pc_monitor.py را روی کامپیوتر اجرا کنید", 160, 100, faSmall, A_DIM, 1);
    faDraw("python tools/pc_monitor.py", 160, 130, faSmall, A_TEXT, 1);
    return true;
  }
  const char *L[3] = {"پردازنده", "حافظه", "دما"};
  float V[3] = {pcCpu, pcRam, pcTemp};
  for (int i = 0; i < 3; i++) {
    if (i == 2 && pcTemp <= 0) continue;
    int y = 52 + i * 34;
    faDraw(L[i], 306, y, faSmall, A_TEXT, 2);
    float f = constrain(V[i] / 100.0f, 0.0f, 1.0f);
    uint32_t c = f > 0.8f ? 0xFF4A3A : (f > 0.5f ? 0xFFC24B : 0x5BE37D);
    canvas.fillSmoothRoundRect(70, y + 4, 160, 12, 6, 0x1C2630);
    canvas.fillSmoothRoundRect(70, y + 4, 160 * f, 12, 6, c);
    faDraw(faDigits(String((int)V[i])).c_str(), 20, y, faSmall, c, 0);
  }
  for (int i = 1; i < 64; i++) {
    int a = cpuHist[(cpuHead + i - 1) % 64], b = cpuHist[(cpuHead + i) % 64];
    canvas.drawLine(32 + (i - 1) * 4, 232 - a * 0.6f, 32 + i * 4, 232 - b * 0.6f, 0x3DDC84);
  }
  return true;
}

// WiFi motion (CSI)
static float motionHist[80];
static int motionHead = 0;
static bool motionTool() {
  if (appClose()) return false;
  appFrame("حرکت در اتاق", TOOL_COL[T_MOTION]);
  if (!csiOk) {
    faDraw("این قابلیت به وای‌فای وصل نیاز دارد", 160, 110, faSmall, A_DIM, 1);
    return true;
  }
  if (!secondPass) {
    motionHist[motionHead] = csiScore;
    motionHead = (motionHead + 1) % 80;
  }
  float mx = max(1.0f, csiBase * 4);
  for (int i = 0; i < 80; i++) {
    float v = motionHist[(motionHead + i) % 80] / mx;
    int h = constrain((int)(v * 120), 1, 120);
    canvas.fillRect(10 + i * 3.75f, 220 - h, 3, h, v > 0.6f ? 0xFF72B6 : 0x2A3644);
  }
  bool m = motionNow();
  faDraw(m ? "کسی در حال حرکت است!" : "آرام است", 160, 52, faBig, m ? 0xFF72B6 : A_DIM, 1);
  return true;
}

// dice
static int dice[2] = {3, 5};
static uint32_t diceRollMs = 0;
static void drawDie(int cx, int cy, int v) {
  canvas.fillSmoothRoundRect(cx - 40, cy - 40, 80, 80, 16, 0xF4F7FA);
  static const int8_t P[7][6][2] = {{}, {{0, 0}}, {{-1, -1}, {1, 1}}, {{-1, -1}, {0, 0}, {1, 1}},
                                   {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}}, {{-1, -1}, {1, -1}, {0, 0}, {-1, 1}, {1, 1}},
                                   {{-1, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {1, 1}}};
  for (int i = 0; i < v; i++) canvas.fillSmoothCircle(cx + P[v][i][0] * 22, cy + P[v][i][1] * 22, 7, v == 1 ? 0xE8283A : 0x1A1F28);
}
static bool diceTool() {
  if (appClose()) return false;
  appFrame("تاس", TOOL_COL[T_DICE]);
  if ((in.pressed && in.y > 44) || accDev() > 0.8f) { if (millis() - diceRollMs > 700) { diceRollMs = millis(); soundPlay(SND_DIZZY); } }
  bool rolling = millis() - diceRollMs < 600;
  if (rolling) { dice[0] = random(1, 7); dice[1] = random(1, 7); }
  float wob = rolling ? sinf(millis() / 30.0f) * 6 : 0;
  drawDie(100 + wob, 130, dice[0]);
  drawDie(220 - wob, 130, dice[1]);
  String s = rolling ? "…" : "جمع: " + faDigits(String(dice[0] + dice[1]));
  faDraw(s.c_str(), 160, 186, faBig, A_TEXT, 1);
  faDraw("ضربه بزنید یا تکان بدهید", 160, 220, faSmall, A_DIM, 1);
  return true;
}

// magic ball
static const char *BALL[] = {"قطعاً بله", "بدون شک", "احتمالاً", "بعداً دوباره بپرس", "الان نمی‌شه گفت",
                             "روش حساب نکن", "نه!", "شک دارم", "آره، برو جلو", "نشونه‌ها می‌گن آره", "عمراً", "تمرکز کن و دوباره بپرس"};
static int ballIdx = -1;
static uint32_t ballMs = 0;
static bool ballTool() {
  if (appClose()) return false;
  appFrame("توپ جادویی", TOOL_COL[T_BALL]);
  if (((in.pressed && in.y > 44) || accDev() > 0.8f) && millis() - ballMs > 900) {
    ballMs = millis();
    ballIdx = random(sizeof(BALL) / sizeof(BALL[0]));
    soundPlay(SND_SURPRISE);
  }
  canvas.fillSmoothCircle(160, 132, 80, 0x101014);
  canvas.fillSmoothCircle(140, 108, 18, 0x2A2A34);
  float a = constrain((millis() - ballMs) / 800.0f, 0.0f, 1.0f);
  canvas.fillSmoothCircle(160, 136, 46, mix(0x101014, 0x1E3A8A, a));
  if (ballIdx >= 0) {
    String lines[3];
    int n = faWrap(BALL[ballIdx], faSmall, 80, lines, 3);
    for (int i = 0; i < n; i++) faDraw(lines[i].c_str(), 160, 124 - n * 9 + i * 20, faSmall, mix(0x1E3A8A, 0xFFFFFF, a), 1);
  } else {
    textC("8", 160, 136, &fonts::FreeSansBold24pt7b, 0xFFFFFF);
  }
  faDraw("سؤالت را بپرس و تکان بده", 160, 220, faSmall, A_DIM, 1);
  return true;
}

// level
static bool levelTool() {
  if (appClose()) return false;
  appFrame("تراز", TOOL_COL[T_LEVEL]);
  float ax = in.imuValid ? imu.ax : 0, ay = in.imuValid ? imu.ay : 0;
  float bx = 160 - constrain(ax, -1.0f, 1.0f) * 110, by = 136 - constrain(ay, -1.0f, 1.0f) * 70;
  bool flat = fabsf(ax) < 0.02f && fabsf(ay) < 0.02f;
  canvas.drawCircle(160, 136, 30, 0x2A3644);
  canvas.drawCircle(160, 136, 70, 0x2A3644);
  canvas.drawFastHLine(30, 136, 260, 0x2A3644);
  canvas.drawFastVLine(160, 56, 160, 0x2A3644);
  canvas.fillSmoothCircle(bx, by, 18, flat ? 0x5BE37D : 0xC8F06E);
  canvas.fillSmoothCircle(bx - 6, by - 6, 5, 0xFFFFFF);
  char b[32];
  snprintf(b, sizeof(b), "%.1f  /  %.1f", asinf(constrain(ax, -1.0f, 1.0f)) * RAD_TO_DEG, asinf(constrain(ay, -1.0f, 1.0f)) * RAD_TO_DEG);
  textC(b, 160, 222, &fonts::FreeSans12pt7b, flat ? 0x5BE37D : A_TEXT);
  return true;
}

// seismograph
static int8_t seis[320];
static int seisHead = 0;
static float seisPeak = 0;
static bool seismoTool() {
  if (appClose()) return false;
  appFrame("لرزه‌نگار", TOOL_COL[T_SEISMO]);
  float d = in.imuValid ? (sqrtf(imu.ax * imu.ax + imu.ay * imu.ay + imu.az * imu.az) - 1) : 0;
  if (!secondPass) {
    seis[seisHead] = constrain((int)(d * 400), -90, 90);
    seisHead = (seisHead + 1) % 320;
  }
  seisPeak = max(seisPeak * 0.995f, fabsf(d));
  for (int y = 60; y < 230; y += 34) canvas.drawFastHLine(0, y, 320, 0x16202A);
  int prev = 0;
  for (int i = 0; i < 320; i++) {
    int v = seis[(seisHead + i) % 320];
    if (i) canvas.drawLine(i - 1, 145 - prev, i, 145 - v, abs(v) > 40 ? 0xFF4A3A : 0x48E6FF);
    prev = v;
  }
  char b[24];
  snprintf(b, sizeof(b), "peak %.3f g", seisPeak);
  textC(b, 160, 226, &fonts::FreeSans9pt7b, A_DIM);
  return true;
}

// pomodoro
static bool pomoRun = false, pomoBreak = false;
static uint32_t pomoEndMs = 0, pomoLeftMs = 0;
static int pomoDone = 0;
static bool pomoTool() {
  if (appClose()) return false;
  appFrame(pomoBreak ? "پومودورو · استراحت" : "پومودورو · تمرکز", TOOL_COL[T_POMO]);
  uint32_t total = (pomoBreak ? 5 : max(1, cloud.pomodoroMin)) * 60000UL;
  if (!pomoRun && !pomoLeftMs) pomoLeftMs = total;
  if (in.released && in.y > 44) {
    if (pomoRun) { pomoLeftMs = pomoEndMs - millis(); pomoRun = false; soundPlay(SND_POMO_PAUSE); }
    else { pomoEndMs = millis() + pomoLeftMs; pomoRun = true; soundPlay(SND_POMO_START); }
  }
  uint32_t left = pomoRun ? (millis() < pomoEndMs ? pomoEndMs - millis() : 0) : pomoLeftMs;
  if (pomoRun && left == 0) {
    soundPlay(pomoBreak ? SND_BREAK_DONE : SND_FOCUS_DONE);
    if (!pomoBreak) { pomoDone++; lifeAddCoins(5); lifeAddXp(10); }
    pomoBreak = !pomoBreak;
    pomoRun = false;
    pomoLeftMs = 0;
  }
  float frac = 1 - left / (float)total;
  uint32_t c = pomoBreak ? 0x5BE37D : TOOL_COL[T_POMO];
  canvas.fillArc(160, 132, 78, 68, 0, 360, 0x1C2630);
  if (frac > 0.002f) canvas.fillArc(160, 132, 78, 68, 270, 270 + 360 * frac, c);
  char b[8];
  snprintf(b, sizeof(b), "%02lu:%02lu", (unsigned long)(left / 60000), (unsigned long)(left / 1000 % 60));
  canvas.setFont(&fonts::Font7);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(A_TEXT);
  canvas.drawString(b, 160, 132);
  String s = (pomoRun ? "ضربه = مکث" : "ضربه = شروع") + String("  ·  انجام‌شده: ") + faDigits(String(pomoDone));
  faDraw(s.c_str(), 160, 218, faSmall, A_DIM, 1);
  return true;
}

// breathing 4-4-6
static uint32_t breathStart = 0;
static bool breathTool() {
  if (appClose()) return false;
  if (!breathStart) breathStart = millis();
  vGradient(0x06101A, 0x0B1E2E);
  float t = fmodf((millis() - breathStart) / 1000.0f, 14);
  float k;
  const char *label;
  if (t < 4) { k = t / 4; label = "دم"; }
  else if (t < 8) { k = 1; label = "نگه دار"; }
  else { k = 1 - (t - 8) / 6; label = "بازدم"; }
  k = k * k * (3 - 2 * k);
  float r = 30 + 60 * k;
  fillAAEllipse(160, 112, r + 14, r + 14, 0x0E2A3E, 0x06101A);
  fillAAEllipse(160, 112, r, r, 0x7FD8FF, 0x0E2A3E);
  faDraw(label, 160, 208, faBig, A_TEXT, 1);
  canvas.fillSmoothCircle(20, 20, 13, 0x1C2630);
  canvas.drawWideLine(15, 15, 25, 25, 1.4f, A_DIM);
  canvas.drawWideLine(25, 15, 15, 25, 1.4f, A_DIM);
  return true;
}

// stopwatch
static bool swRun = false;
static uint32_t swStart = 0, swAcc = 0;
static bool watchTool() {
  if (appClose()) return false;
  appFrame("کرنومتر", TOOL_COL[T_WATCH]);
  if (in.released && in.y > 44) {
    if (millis() - in.downMs > 700) { swRun = false; swAcc = 0; }  // long press = reset
    else if (swRun) { swAcc += millis() - swStart; swRun = false; }
    else { swStart = millis(); swRun = true; }
    soundPlay(SND_CLICK);
  }
  uint32_t ms = swAcc + (swRun ? millis() - swStart : 0);
  char b[16];
  snprintf(b, sizeof(b), "%02lu:%02lu.%lu", (unsigned long)(ms / 60000), (unsigned long)(ms / 1000 % 60), (unsigned long)(ms / 100 % 10));
  canvas.setFont(&fonts::Font7);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(swRun ? 0xFFD45C : A_TEXT);
  canvas.drawString(b, 160, 128);
  faDraw("ضربه = شروع/توقف   ·   نگه داشتن = صفر", 160, 210, faSmall, A_DIM, 1);
  return true;
}

static bool toolRun(int t) {
  switch (t) {
    case T_DICE: return diceTool();
    case T_BALL: return ballTool();
    case T_LEVEL: return levelTool();
    case T_SEISMO: return seismoTool();
    case T_POMO: return pomoTool();
    case T_BREATH: return breathTool();
    case T_WATCH: return watchTool();
    case T_PC: return pcTool();
    case T_MOTION: return motionTool();
  }
  return false;
}

// ============================================================ FUN
enum FunId { F_PAINT, F_PIANO, F_AQUA, F_FIRE, F_MATRIX, F_FIREWORKS, F_COUNT };
static const char *FUN_NAMES[F_COUNT] = {"نقاشی", "پیانو", "آکواریوم", "آتش", "ماتریکس", "آتش‌بازی"};
static const uint32_t FUN_COL[F_COUNT] = {0xFF72B6, 0xF4F7FA, 0x48A6FF, 0xFF8A3D, 0x3DDC84, 0xFFD45C};

// paint: the sprite itself is the drawing (not cleared every frame)
static bool paintFresh = true;
static uint32_t paintCol = 0xFF4A3A;
static int paintX = -1, paintY = -1;
static const uint32_t PAINT_COLS[7] = {0xFF4A3A, 0xFFD45C, 0x5BE37D, 0x48E6FF, 0xB98CFF, 0xFFFFFF, 0x000000};
static void paintToolbar() {
  lcd.fillRect(0, 0, 320, 34, 0x10161E);
  for (int i = 0; i < 7; i++) {
    lcd.fillSmoothCircle(60 + i * 34, 17, 11, PAINT_COLS[i]);
    if (PAINT_COLS[i] == paintCol) lcd.drawCircle(60 + i * 34, 17, 14, 0xFFFFFF);
  }
  lcd.setFont(&fonts::FreeSansBold9pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(A_DIM, 0x10161E);
  lcd.drawString("CLR", 298, 17);
  lcd.fillSmoothCircle(20, 17, 11, 0x1C2630);
  lcd.drawWideLine(16, 13, 24, 21, 1.3f, A_TEXT);
  lcd.drawWideLine(24, 13, 16, 21, 1.3f, A_TEXT);
}
static bool paintFun() {  // draws straight on the display, so the picture stays
  if (paintFresh) { lcd.fillScreen(0x000000); paintToolbar(); paintFresh = false; }
  if (in.pressed && in.y < 34) {
    if (in.x < 40) { paintFresh = true; return false; }
    if (in.x > 275) { lcd.fillRect(0, 34, 320, 206, 0x000000); return true; }
    int i = (in.x - 43) / 34;
    if (i >= 0 && i < 7) { paintCol = PAINT_COLS[i]; paintToolbar(); }
    return true;
  }
  if (in.down && in.y > 36) {
    if (paintX >= 0) lcd.drawWideLine(paintX, paintY, in.x, in.y, paintCol == 0 ? 8.0f : 2.5f, paintCol);
    else lcd.fillSmoothCircle(in.x, in.y, 2.5f, paintCol);
    paintX = in.x;
    paintY = in.y;
  } else {
    paintX = -1;
  }
  return true;
}

// piano: 8 white keys (C5..C6) + black keys
static bool pianoFun() {
  if (appClose()) { soundHold(0); return false; }
  appFrame("پیانو", FUN_COL[F_PIANO]);
  static const uint16_t WF[8] = {523, 587, 659, 698, 784, 880, 988, 1047};
  static const uint16_t BF[7] = {554, 622, 0, 740, 831, 932, 0};
  int kw = 40, top = 60, h = 170;
  int playing = 0;
  bool blackHit = false;
  for (int i = 0; i < 7; i++)
    if (BF[i] && in.down && in.y < top + 100 && in.y > top && in.x > (i + 1) * kw - 12 && in.x < (i + 1) * kw + 12) {
      playing = BF[i];
      blackHit = true;
    }
  for (int i = 0; i < 8; i++) {
    bool on = !blackHit && in.down && in.y > top && in.x >= i * kw && in.x < (i + 1) * kw;
    if (on) playing = WF[i];
    canvas.fillSmoothRoundRect(i * kw + 1, top, kw - 2, h, 6, on ? 0xFFD45C : 0xF4F7FA);
  }
  for (int i = 0; i < 7; i++) {
    if (!BF[i]) continue;
    bool on = blackHit && playing == BF[i];
    canvas.fillSmoothRoundRect((i + 1) * kw - 12, top, 24, 100, 5, on ? 0xFF8A3D : 0x1A1F28);
  }
  soundHold(playing);
  return true;
}

// aquarium
struct Fish { float x, y, vx, ph; uint32_t c; float s; };
static Fish fish[7];
static bool fishInit = false;
static bool aquaFun() {
  if (appClose()) return false;
  if (!fishInit) {
    fishInit = true;
    for (auto &f : fish) f = {(float)random(320), (float)random(50, 200), (random(2) ? 1.0f : -1.0f) * random(15, 45),
                              random(100) / 10.0f, (uint32_t)(random(2) ? 0xFF8A3D : (random(2) ? 0xFFD45C : 0x48E6FF)), random(7, 13) / 1.0f};
  }
  vGradient(0x062A4A, 0x021022);
  for (int i = 0; i < 5; i++) {  // light rays
    float x = 40 + i * 70 + sinf(millis() / 2000.0f + i) * 10;
    canvas.drawLine(x, 0, x - 40, 240, 0x0A3A60);
  }
  for (int i = 0; i < 9; i++) {  // weeds
    float sway = sinf(millis() / 700.0f + i) * 6;
    canvas.drawWideLine(20 + i * 36, 240, 20 + i * 36 + sway, 200 - (i % 3) * 14, 2.5f, 0x1F7A4A);
  }
  for (auto &f : fish) {
    f.x += f.vx * (secondPass ? 0.0f : 0.033f);
    if (f.x < -30) f.x = 350; if (f.x > 350) f.x = -30;
    float y = f.y + sinf(millis() / 600.0f + f.ph) * 6;
    int d = f.vx > 0 ? 1 : -1;
    fillAAEllipse(f.x, y, f.s * 1.6f, f.s, f.c, 0x062A4A);
    canvas.fillTriangle(f.x - d * f.s * 1.4f, y, f.x - d * f.s * 2.6f, y - f.s, f.x - d * f.s * 2.6f, y + f.s, f.c);
    canvas.fillSmoothCircle(f.x + d * f.s * 0.8f, y - f.s * 0.2f, 2, 0x000000);
  }
  for (int i = 0; i < 6; i++) {  // bubbles
    float yy = 240 - fmodf(millis() / 25.0f + i * 47, 260);
    canvas.drawCircle(60 + i * 45 + sinf(yy / 20) * 4, yy, 3, 0x7FD8FF);
  }
  return true;
}

// fire: 80x48 heat buffer scaled 4x5
static uint8_t heat[48][80];
static bool fireFun() {
  if (appClose()) return false;
  if (!secondPass) for (int x = 0; x < 80; x++) heat[47][x] = random(160, 255);
  if (!secondPass) for (int y = 0; y < 47; y++)
    for (int x = 0; x < 80; x++) {
      int s = heat[y + 1][(x + 79) % 80] + heat[y + 1][x] + heat[y + 1][(x + 1) % 80] + heat[min(47, y + 2)][x];
      heat[y][x] = max(0, s / 4 - (int)random(0, 3));
    }
  for (int y = 0; y < 48; y++)
    for (int x = 0; x < 80; x++) {
      uint8_t h = heat[y][x];
      uint32_t c = h < 85 ? mix(0x000000, 0xB01000, h / 85.0f) : (h < 170 ? mix(0xB01000, 0xFF8A00, (h - 85) / 85.0f) : mix(0xFF8A00, 0xFFF4C0, (h - 170) / 85.0f));
      canvas.fillRect(x * 4, y * 5, 4, 5, c);
    }
  return true;
}

// matrix rain
static int16_t mDrop[40];
static bool matrixFun() {
  if (appClose()) return false;
  static bool init = false;
  if (!init) { init = true; for (auto &d : mDrop) d = random(-30, 0); }
  canvas.fillScreen(0x000000);
  canvas.setFont(&fonts::Font0);
  canvas.setTextDatum(top_left);
  for (int c = 0; c < 40; c++) {
    for (int k = 0; k < 14; k++) {
      int row = mDrop[c] - k;
      if (row < 0 || row > 29) continue;
      canvas.setTextColor(k == 0 ? 0xD8FFD8 : mix(0x000000, 0x3DDC84, 1 - k / 14.0f));
      char ch[2] = {(char)(33 + (c * 7 + row * 13 + mDrop[c]) % 90), 0};
      canvas.drawString(ch, c * 8, row * 8);
    }
    if (!secondPass && random(3) == 0) mDrop[c]++;
    if (mDrop[c] > 44) mDrop[c] = random(-20, 0);
  }
  return true;
}

// fireworks
struct Spark { float x, y, vx, vy, life; uint32_t c; };
static Spark sparks[90];
static uint32_t nextBurst = 0;
static bool fireworksFun() {
  if (appClose()) return false;
  if (!secondPass && (millis() > nextBurst || (in.pressed && in.y > 44))) {
    nextBurst = millis() + random(500, 1300);
    float bx = in.pressed ? in.x : random(40, 280), by = in.pressed ? in.y : random(40, 140);
    uint32_t c = hsv(random(360), 0.7f, 1);
    int made = 0;
    for (auto &s : sparks)
      if (s.life <= 0 && made < 30) {
        float a = made * 2 * PI / 30, v = random(40, 110);
        s = {bx, by, cosf(a) * v, sinf(a) * v, 1.4f, c};
        made++;
      }
    soundPlay(SND_BOOP);
  }
  canvas.fillScreen(0x02030A);
  for (auto &s : sparks) {
    if (s.life <= 0) continue;
    float st = secondPass ? 0.0f : 0.033f;
    s.life -= st;
    s.vy += 60 * st;
    s.x += s.vx * st;
    s.y += s.vy * st;
    canvas.fillSmoothCircle(s.x, s.y, 2, mix(0x02030A, s.c, min(1.0f, s.life)));
  }
  return true;
}

static bool funRun(int f) {
  switch (f) {
    case F_PAINT: return paintFun();
    case F_PIANO: return pianoFun();
    case F_AQUA: return aquaFun();
    case F_FIRE: return fireFun();
    case F_MATRIX: return matrixFun();
    case F_FIREWORKS: return fireworksFun();
  }
  return false;
}
