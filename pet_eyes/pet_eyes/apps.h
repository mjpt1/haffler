#pragma once
// App launcher + app screens. All draw on the full-screen `canvas` (320x240).
// Needs (from pet_eyes.ino / headers): canvas, in, mix, fillAAEllipse, faDraw/faWrap/faDigits,
// cloud + cloudMux, toggleQueue, reqHafez, reqPortal, clockNow, soundPlay, games.h helpers.
#include "jalali.h"
void lifeAddCoins(int n);
void lifeAddXp(int n);

enum AppId { APP_CLOCK, APP_CALENDAR, APP_TODO, APP_WEATHER, APP_PRICES, APP_HAFEZ, APP_GAMES, APP_HABITS,
             APP_PET, APP_TOOLS, APP_FUN, APP_INFO, APP_COUNT };
static const char *APP_NAMES[APP_COUNT] = {"ساعت", "تقویم", "کارها", "هوا", "قیمت‌ها", "فال حافظ", "بازی‌ها", "عادت‌ها",
                                           "پت من", "ابزارها", "سرگرمی", "تنظیمات"};
static const uint32_t APP_COL[APP_COUNT] = {0x48E6FF, 0xFF8A3D, 0x5BE37D, 0xFFD45C, 0x3DDC84, 0xB98CFF, 0xFF6B6B, 0xFF72B6,
                                            0xFF4A3A, 0x7FD8FF, 0xFFC24B, 0x9FB3C8};

static const char *THEME_FA[8] = {"پاییز", "زمستان", "بهار", "تابستان", "هالووین", "یلدا", "کریسمس", "نوروز"};

static const uint32_t A_BG1 = 0x0A0E14, A_BG2 = 0x121A24, A_CARD = 0x161F2A, A_TEXT = 0xEAF2FA, A_DIM = 0x7D8C99;

// ---------- time helper: system time (NTP / server) or RTC ----------
static bool nowTm(struct tm &t) {
  time_t s = time(nullptr);
  if (s > 1700000000) { localtime_r(&s, &t); return true; }
  memset(&t, 0, sizeof(t));
  t.tm_year = clockNow.year - 1900; t.tm_mon = clockNow.month - 1; t.tm_mday = clockNow.day;
  t.tm_hour = clockNow.hour; t.tm_min = clockNow.minute; t.tm_sec = clockNow.second;
  mktime(&t);
  return false;
}

static void appFrame(const char *title, uint32_t accent) {
  vGradient(A_BG1, A_BG2);
  canvas.fillRect(0, 0, 320, 40, mix(A_BG1, accent, 0.10f));
  canvas.drawFastHLine(0, 40, 320, mix(A_BG1, accent, 0.5f));
  faDraw(title, 306, 9, faSmall, A_TEXT, 2);
  canvas.fillSmoothCircle(20, 20, 13, 0x1C2630);
  canvas.drawWideLine(15, 15, 25, 25, 1.4f, A_TEXT);
  canvas.drawWideLine(25, 15, 15, 25, 1.4f, A_TEXT);
}
static bool appClose() { return in.pressed && in.x < 48 && in.y < 48; }

// ---------- icons ----------
static void drawIcon(int id, int cx, int cy, uint32_t c) {
  switch (id) {
    case APP_CLOCK:
      canvas.fillSmoothCircle(cx, cy, 15, c); canvas.fillSmoothCircle(cx, cy, 12, A_CARD);
      canvas.drawWideLine(cx, cy, cx, cy - 8, 1.3f, c); canvas.drawWideLine(cx, cy, cx + 6, cy + 3, 1.3f, c); break;
    case APP_CALENDAR:
      canvas.fillSmoothRoundRect(cx - 14, cy - 12, 28, 26, 5, c); canvas.fillRect(cx - 11, cy - 4, 22, 15, A_CARD);
      for (int i = 0; i < 3; i++) canvas.fillRect(cx - 8 + i * 6, cy, 3, 3, c); break;
    case APP_TODO:
      canvas.fillSmoothRoundRect(cx - 14, cy - 14, 28, 28, 7, c);
      canvas.drawWideLine(cx - 7, cy, cx - 2, cy + 6, 2.0f, A_CARD); canvas.drawWideLine(cx - 2, cy + 6, cx + 8, cy - 6, 2.0f, A_CARD); break;
    case APP_WEATHER:
      canvas.fillSmoothCircle(cx - 5, cy - 4, 9, 0xFFD45C);
      canvas.fillSmoothCircle(cx + 3, cy + 4, 8, 0xDDE6EE); canvas.fillSmoothCircle(cx + 11, cy + 6, 6, 0xDDE6EE);
      canvas.fillSmoothRoundRect(cx - 4, cy + 4, 20, 9, 4, 0xDDE6EE); break;
    case APP_PRICES:
      canvas.fillSmoothCircle(cx, cy, 15, c);
      textC("$", cx, cy + 1, &fonts::FreeSansBold12pt7b, A_CARD); break;
    case APP_HAFEZ:
      canvas.fillSmoothRoundRect(cx - 15, cy - 11, 14, 24, 3, c); canvas.fillSmoothRoundRect(cx + 1, cy - 11, 14, 24, 3, c);
      for (int i = 0; i < 3; i++) { canvas.drawFastHLine(cx - 12, cy - 5 + i * 6, 8, A_CARD); canvas.drawFastHLine(cx + 4, cy - 5 + i * 6, 8, A_CARD); } break;
    case APP_GAMES:
      canvas.fillSmoothRoundRect(cx - 17, cy - 9, 34, 20, 9, c);
      canvas.fillRect(cx - 11, cy - 1, 8, 3, A_CARD); canvas.fillRect(cx - 8, cy - 4, 3, 9, A_CARD);
      canvas.fillSmoothCircle(cx + 8, cy - 2, 2.5f, A_CARD); canvas.fillSmoothCircle(cx + 12, cy + 3, 2.5f, A_CARD); break;
    case APP_HABITS:
      for (int i = 0; i < 4; i++) canvas.fillSmoothRoundRect(cx - 14 + i * 7, cy + 12 - (6 + i * 6), 5, 6 + i * 6, 2, c); break;
    case APP_PET:
      fillAAEllipse(cx - 8, cy, 6, 9, c, A_CARD); fillAAEllipse(cx + 8, cy, 6, 9, c, A_CARD); break;
    case APP_TOOLS:
      canvas.drawWideLine(cx - 10, cy + 10, cx + 6, cy - 6, 3.5f, c); canvas.fillSmoothCircle(cx + 8, cy - 8, 8, c);
      canvas.fillSmoothCircle(cx + 11, cy - 11, 4, A_CARD); break;
    case APP_FUN:
      for (int i = 0; i < 5; i++) { float a = -PI / 2 + i * 2 * PI / 5;
        canvas.fillTriangle(cx, cy, cx + cosf(a) * 15, cy + sinf(a) * 15, cx + cosf(a + 0.6f) * 6, cy + sinf(a + 0.6f) * 6, c);
        canvas.fillTriangle(cx, cy, cx + cosf(a) * 15, cy + sinf(a) * 15, cx + cosf(a - 0.6f) * 6, cy + sinf(a - 0.6f) * 6, c); } break;
    case APP_INFO:
      canvas.fillSmoothCircle(cx, cy, 14, c); canvas.fillSmoothCircle(cx, cy, 6, A_CARD);
      for (int i = 0; i < 8; i++) { float a = i * PI / 4; canvas.fillSmoothCircle(cx + cosf(a) * 14, cy + sinf(a) * 14, 3.5f, c); } break;
  }
}

// returns -2 = back to pet, -1 = stay, >= 0 = open that app
static int launcherFrame() {
  vGradient(A_BG1, A_BG2);
  faDraw("برنامه‌ها", 160, 6, faSmall, A_TEXT, 1);
  int pick = -1;
  for (int i = 0; i < APP_COUNT; i++) {
    int col = 3 - (i % 4), row = i / 4;  // right-to-left order
    int x = 6 + col * 78, y = 32 + row * 69, w = 74, h = 64;
    bool hot = in.down && in.x > x && in.x < x + w && in.y > y && in.y < y + h;
    canvas.fillSmoothRoundRect(x, y, w, h, 12, hot ? mix(A_CARD, APP_COL[i], 0.35f) : A_CARD);
    drawIcon(i, x + w / 2, y + 23, APP_COL[i]);
    faDraw(APP_NAMES[i], x + w / 2, y + 42, faSmall, A_TEXT, 1);
    if (in.released && in.x > x && in.x < x + w && in.y > y && in.y < y + h) pick = i;
  }
  canvas.fillSmoothCircle(20, 20, 13, 0x1C2630);
  canvas.drawWideLine(15, 15, 25, 25, 1.4f, A_TEXT);
  canvas.drawWideLine(25, 15, 15, 25, 1.4f, A_TEXT);
  if (appClose()) return -2;
  return pick;
}

// ---------- clock ----------
static bool clockApp() {
  if (appClose()) return false;
  struct tm t;
  nowTm(t);
  vGradient(0x05070B, 0x0E1420);
  // two small eyes that look at the seconds
  float a = t.tm_sec / 60.0f * 2 * PI;
  float lx = sinf(a) * 4, ly = -cosf(a) * 3;
  bool blink = (millis() / 100) % 45 == 0;
  for (int s = -1; s <= 1; s += 2) {
    fillAAEllipse(160 + s * 22 + lx, 30 + ly, 11, blink ? 1.5f : 14, 0xFF4A3A, 0x05070B);
  }
  char b[16];
  snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
  canvas.setFont(&fonts::Font8);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(A_TEXT);
  canvas.drawString(b, 160, 108);
  // seconds bar
  canvas.fillSmoothRoundRect(60, 150, 200, 4, 2, 0x1C2630);
  canvas.fillSmoothRoundRect(60, 150, 200 * (t.tm_sec + 1) / 60, 4, 2, 0xFF4A3A);
  JDate j = toJalali(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  String fa = String(FA_WEEKDAYS[t.tm_wday]) + " " + faDigits(String(j.d)) + " " + FA_MONTHS[j.m - 1] + " " + faDigits(String(j.y));
  faDraw(fa.c_str(), 160, 166, faBig, A_TEXT, 1);
  snprintf(b, sizeof(b), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  const char *hol = faHoliday(j);
  if (!hol) hol = canadaHoliday(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  if (hol) faDraw(hol, 160, 198, faSmall, 0xFF6B6B, 1);
  textC(b, 160, hol ? 228 : 214, &fonts::FreeSans9pt7b, A_DIM);
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  bool wx = cloud.wxValid;
  float temp = cloud.wxTemp;
  xSemaphoreGive(cloudMux);
  if (wx) {
    snprintf(b, sizeof(b), "%d°", (int)roundf(temp));
    faDraw(faDigits(b).c_str(), 306, 10, faSmall, A_DIM, 2);
  }
  canvas.fillSmoothCircle(20, 20, 13, 0x1C2630);
  canvas.drawWideLine(15, 15, 25, 25, 1.4f, A_DIM);
  canvas.drawWideLine(25, 15, 15, 25, 1.4f, A_DIM);
  return true;
}

// ---------- calendar ----------
static bool calendarApp() {
  if (appClose()) return false;
  struct tm t;
  nowTm(t);
  static bool greg = false;  // tap the title bar to switch Shamsi <-> Gregorian
  if (in.released && in.y < 44 && in.x > 60) { greg = !greg; soundPlay(SND_CLICK); }
  JDate j = toJalali(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  int gy = t.tm_year + 1900, gm = t.tm_mon + 1, gd = t.tm_mday;
  if (greg) {
    String title = String(GR_MONTHS_FA[gm - 1]) + " " + faDigits(String(gy)) + "  · لمس: شمسی";
    appFrame(title.c_str(), APP_COL[APP_CALENDAR]);
    static const char *WD[7] = {"یک", "دو", "سه", "چها", "پنج", "جمع", "شنب"};  // Sunday first, left to right
    const int cw = 42, ch = 27, x0 = 12, y0 = 48;
    for (int c = 0; c < 7; c++) faDraw(WD[c], x0 + c * cw + cw / 2, y0, faSmall, c == 0 ? 0xFF6B6B : A_DIM, 1);
    int firstCol = gDow(gy, gm, 1);
    static const int ML[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int len = ML[gm - 1] + (gm == 2 && ((gy % 4 == 0 && gy % 100) || gy % 400 == 0));
    for (int d = 1; d <= len; d++) {
      int idx = firstCol + d - 1, c = idx % 7, r = idx / 7;
      int cx = x0 + c * cw + cw / 2, cy = y0 + 32 + r * ch;
      bool hol = c == 0 || canadaHoliday(gy, gm, d);
      if (d == gd) canvas.fillSmoothCircle(cx, cy + 2, 12, APP_COL[APP_CALENDAR]);
      faDraw(faDigits(String(d)).c_str(), cx, cy - 8, faSmall, d == gd ? 0x111111 : (hol ? 0xFF6B6B : A_TEXT), 1);
    }
    const char *h = canadaHoliday(gy, gm, gd);
    if (h) faDraw((String("امروز: ") + h).c_str(), 160, 222, faSmall, 0xFF6B6B, 1);
    return true;
  }
  String title = String(FA_MONTHS[j.m - 1]) + " " + faDigits(String(j.y)) + "  · لمس: میلادی";
  appFrame(title.c_str(), APP_COL[APP_CALENDAR]);
  // weekday of the 1st of this Persian month
  time_t now = mktime(&t);
  time_t first = now - (time_t)(j.d - 1) * 86400;
  struct tm ft;
  localtime_r(&first, &ft);
  int firstCol = (ft.tm_wday + 1) % 7;  // 0 = Saturday
  int len = j.m <= 6 ? 31 : (j.m <= 11 ? 30 : 29);
  if (j.m == 12) {
    time_t d30 = first + 29L * 86400;
    struct tm t30;
    localtime_r(&d30, &t30);
    JDate j30 = toJalali(t30.tm_year + 1900, t30.tm_mon + 1, t30.tm_mday);
    if (j30.m == 12) len = 30;
  }
  static const char *WD[7] = {"ش", "ی", "د", "س", "چ", "پ", "ج"};
  const int cw = 42, ch = 27, x0 = 12, y0 = 48;
  for (int c = 0; c < 7; c++) faDraw(WD[c], 320 - x0 - c * cw - cw / 2, y0, faSmall, c == 6 ? 0xFF6B6B : A_DIM, 1);
  for (int d = 1; d <= len; d++) {
    int idx = firstCol + d - 1, c = idx % 7, r = idx / 7;
    int cx = 320 - x0 - c * cw - cw / 2, cy = y0 + 32 + r * ch;
    JDate jd = {j.y, j.m, d};
    bool hol = c == 6 || faHoliday(jd);
    if (d == j.d) canvas.fillSmoothCircle(cx, cy + 2, 12, APP_COL[APP_CALENDAR]);
    faDraw(faDigits(String(d)).c_str(), cx, cy - 8, faSmall, d == j.d ? 0x111111 : (hol ? 0xFF6B6B : A_TEXT), 1);
  }
  const char *h = faHoliday(j);
  if (!h) h = canadaHoliday(gy, gm, gd);
  if (h) faDraw((String("امروز: ") + h).c_str(), 160, 222, faSmall, 0xFF6B6B, 1);
  return true;
}

// ---------- to-do ----------
static float todoScroll = 0;
static bool todoApp() {
  if (appClose()) return false;
  appFrame("کارها", APP_COL[APP_TODO]);
  static int lastY = 0;
  static bool dragging = false;
  if (in.pressed) { lastY = in.y; dragging = false; }
  if (in.down && in.y > 44) {
    if (abs(in.y - in.sy) > 8) dragging = true;
    todoScroll += lastY - in.y;
    lastY = in.y;
  }
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  int n = cloud.todoCount;
  todoScroll = constrain(todoScroll, 0.0f, (float)max(0, n * 34 - 180));
  int open = 0;
  for (int i = 0; i < n; i++) if (!cloud.todos[i].done) open++;
  for (int i = 0; i < n; i++) {
    int y = 48 + i * 34 - (int)todoScroll;
    if (y < 10 || y > 240) continue;
    TodoItem &t = cloud.todos[i];
    canvas.fillSmoothRoundRect(10, y, 300, 30, 8, A_CARD);
    uint32_t pc = t.prio == 2 ? 0xFF4A3A : (t.prio == 1 ? 0xFFC24B : APP_COL[APP_TODO]);
    if (t.done) canvas.fillSmoothCircle(292, y + 15, 9, pc);
    else { canvas.fillSmoothCircle(292, y + 15, 9, pc); canvas.fillSmoothCircle(292, y + 15, 7, A_CARD); }
    String txt = t.text;
    while (txt.length() > 3 && faWidth(txt.c_str(), faSmall) > 260) txt = txt.substring(0, txt.length() - 4) + "…";
    faDraw(txt.c_str(), 276, y + 6, faSmall, t.done ? A_DIM : A_TEXT, 2);
    if (t.done) canvas.drawFastHLine(276 - faWidth(txt.c_str(), faSmall), y + 16, faWidth(txt.c_str(), faSmall), A_DIM);
    if (in.released && !dragging && in.y > y && in.y < y + 30 && in.y > 44) {
      t.done = !t.done;
      ToggleReq r = {'t', t.id, t.done};
      xQueueSend(toggleQueue, &r, 0);
      soundPlay(t.done ? SND_POINT : SND_CLICK);
      if (t.done) { lifeAddCoins(5); lifeAddXp(3); }
    }
  }
  xSemaphoreGive(cloudMux);
  if (!n) {
    faDraw("کاری ثبت نشده", 160, 100, faBig, A_DIM, 1);
    faDraw("از پنل آنلاین کار اضافه کنید", 160, 140, faSmall, A_DIM, 1);
  } else {
    String s = faDigits(String(open)) + " کار باقی مانده";
    faDraw(s.c_str(), 160, 9, faSmall, A_DIM, 1);
  }
  return true;
}

// ---------- weather ----------
static void wxIcon(int code, bool day, int cx, int cy) {
  bool sun = code <= 2, cloudy = code >= 1, rain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code >= 95,
       snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
  if (sun) {
    if (day) {
      for (int i = 0; i < 8; i++) {
        float a = i * PI / 4 + millis() / 4000.0f;
        canvas.drawWideLine(cx - 10 + cosf(a) * 30, cy - 10 + sinf(a) * 30, cx - 10 + cosf(a) * 38, cy - 10 + sinf(a) * 38, 2, 0xFFD45C);
      }
      canvas.fillSmoothCircle(cx - 10, cy - 10, 22, 0xFFD45C);
    } else {
      canvas.fillSmoothCircle(cx - 10, cy - 10, 22, 0xF4F1DE);
      canvas.fillSmoothCircle(cx - 1, cy - 18, 20, A_BG1);
    }
  }
  if (cloudy) {
    uint32_t cc = rain || snow ? 0x9AA8B6 : 0xE6EDF3;
    canvas.fillSmoothCircle(cx, cy + 6, 20, cc);
    canvas.fillSmoothCircle(cx + 22, cy + 12, 15, cc);
    canvas.fillSmoothCircle(cx - 20, cy + 14, 13, cc);
    canvas.fillSmoothRoundRect(cx - 32, cy + 12, 68, 16, 8, cc);
  }
  for (int i = 0; i < 4 && (rain || snow); i++) {
    float off = fmodf(millis() / 12.0f + i * 13, 26);
    int x = cx - 20 + i * 14;
    if (rain) canvas.drawWideLine(x, cy + 34 + off, x - 3, cy + 40 + off, 1.2f, 0x48A6FF);
    else canvas.fillSmoothCircle(x, cy + 36 + off, 2.5f, 0xFFFFFF);
  }
}

static bool weatherApp() {
  if (appClose()) return false;
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  bool ok = cloud.wxValid;
  float tmp = cloud.wxTemp, mn = cloud.wxMin, mx = cloud.wxMax;
  int code = cloud.wxCode;
  bool day = cloud.wxDay;
  String city = cloud.city, sr = cloud.sunrise, ss = cloud.sunset;
  xSemaphoreGive(cloudMux);
  appFrame(("هوای " + city).c_str(), APP_COL[APP_WEATHER]);
  if (!ok) {
    faDraw("هنوز اطلاعاتی نرسیده", 160, 100, faBig, A_DIM, 1);
    faDraw("وای‌فای و پنل را تنظیم کنید", 160, 140, faSmall, A_DIM, 1);
    return true;
  }
  wxIcon(code, day, 90, 110);
  // Latin GFX fonts have no "°" glyph, so the degree sign is drawn as a ring
  char b[24];
  snprintf(b, sizeof(b), "%d", (int)roundf(tmp));
  textC(b, 226, 100, &fonts::FreeSansBold24pt7b, A_TEXT);
  int tw = canvas.textWidth(b);
  canvas.fillSmoothCircle(226 + tw / 2 + 9, 84, 6, A_TEXT);
  canvas.fillSmoothCircle(226 + tw / 2 + 9, 84, 3, A_BG2);
  snprintf(b, sizeof(b), "%d  /  %d", (int)roundf(mn), (int)roundf(mx));
  faDraw(faDigits(String(b) + " °").c_str(), 226, 136, faBig, A_DIM, 1);
  String s = "طلوع " + faDigits(sr) + "   غروب " + faDigits(ss);
  faDraw(s.c_str(), 160, 200, faSmall, A_DIM, 1);
  return true;
}

// ---------- prices ----------
static bool pricesApp() {
  if (appClose()) return false;
  appFrame("قیمت‌ها (ریال)", APP_COL[APP_PRICES]);
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  String v[4] = {cloud.usd, cloud.eur, cloud.coin, cloud.gold18};
  xSemaphoreGive(cloudMux);
  static const char *L[4] = {"دلار", "یورو", "سکه امامی", "طلای ۱۸ عیار"};
  static const uint32_t C[4] = {0x3DDC84, 0x48E6FF, 0xFFC24B, 0xFFD45C};
  bool any = false;
  for (int i = 0; i < 4; i++) {
    int y = 50 + i * 46;
    canvas.fillSmoothRoundRect(10, y, 300, 40, 10, A_CARD);
    canvas.fillSmoothCircle(296, y + 20, 5, C[i]);
    faDraw(L[i], 284, y + 10, faSmall, A_TEXT, 2);
    String val = v[i].length() ? faDigits(v[i]) : "—";
    if (v[i].length()) any = true;
    faDraw(val.c_str(), 22, y + 7, faBig, C[i], 0);
  }
  if (!any) faDraw("قیمت‌ها هنوز نرسیده", 160, 226, faSmall, A_DIM, 1);
  return true;
}

// ---------- Hafez ----------
static String hafezLines[40];
static int hafezCount = 0;
static float hafezScroll = 0;
static bool hafezWaiting = false;

static void hafezRequest() {
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  cloud.hafezReady = false;
  xSemaphoreGive(cloudMux);
  hafezCount = 0;
  hafezScroll = 0;
  hafezWaiting = true;
  reqHafez = true;
}

static bool hafezApp() {
  if (appClose()) return false;
  appFrame("فال حافظ", APP_COL[APP_HAFEZ]);
  if (hafezWaiting) {
    xSemaphoreTake(cloudMux, portMAX_DELAY);
    bool ready = cloud.hafezReady;
    String txt = cloud.hafezText;
    xSemaphoreGive(cloudMux);
    if (ready) {
      hafezWaiting = false;
      hafezCount = 0;
      int start = 0;
      while (start < (int)txt.length() && hafezCount < 40) {
        int nl = txt.indexOf('\n', start);
        String line = nl < 0 ? txt.substring(start) : txt.substring(start, nl);
        line.trim();
        if (line.length()) hafezCount += faWrap(line.c_str(), faSmall, 290, hafezLines + hafezCount, 40 - hafezCount);
        if (nl < 0) break;
        start = nl + 1;
      }
      soundPlay(SND_DING);
    } else {
      for (int i = 0; i < 3; i++) {
        float p = sinf(millis() / 200.0f - i * 0.8f);
        canvas.fillSmoothCircle(140 + i * 20, 120, 5 + p * 2, APP_COL[APP_HAFEZ]);
      }
      faDraw("نیت کنید…", 160, 150, faBig, A_DIM, 1);
      return true;
    }
  }
  static int lastY = 0;
  if (in.pressed) lastY = in.y;
  if (in.down && in.y > 44) { hafezScroll += lastY - in.y; lastY = in.y; }
  hafezScroll = constrain(hafezScroll, 0.0f, (float)max(0, hafezCount * 24 - 180));
  for (int i = 0; i < hafezCount; i++) {
    int y = 50 + i * 24 - (int)hafezScroll;
    if (y < 30 || y > 236) continue;
    faDraw(hafezLines[i].c_str(), 160, y, faSmall, (i % 4) < 2 ? A_TEXT : 0xD8C8FF, 1);
  }
  if (in.imuValid && fabsf(sqrtf(imu.ax * imu.ax + imu.ay * imu.ay + imu.az * imu.az) - 1) > 0.9f) hafezRequest();
  return true;
}

// ---------- habits ----------
static bool habitsApp() {
  if (appClose()) return false;
  appFrame("عادت‌های امروز", APP_COL[APP_HABITS]);
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  int n = cloud.habitCount;
  for (int i = 0; i < n; i++) {
    HabitItem &h = cloud.habits[i];
    int y = 50 + i * 38;
    canvas.fillSmoothRoundRect(10, y, 300, 32, 9, h.done ? mix(A_CARD, APP_COL[APP_HABITS], 0.25f) : A_CARD);
    canvas.fillSmoothCircle(292, y + 16, 10, h.done ? APP_COL[APP_HABITS] : 0x2A3644);
    faDraw(h.title.c_str(), 274, y + 7, faSmall, A_TEXT, 2);
    if (in.released && in.y > y && in.y < y + 32) {
      h.done = !h.done;
      ToggleReq r = {'h', h.id, h.done};
      xQueueSend(toggleQueue, &r, 0);
      soundPlay(h.done ? SND_POINT : SND_CLICK);
      if (h.done) { lifeAddCoins(3); lifeAddXp(2); }
    }
  }
  xSemaphoreGive(cloudMux);
  if (!n) faDraw("از پنل عادت اضافه کنید", 160, 110, faBig, A_DIM, 1);
  return true;
}

// ---------- info / settings ----------
static bool infoApp() {
  if (appClose()) return false;
  appFrame("تنظیمات و اتصال", APP_COL[APP_INFO]);
  bool conn = WiFi.isConnected();
  xSemaphoreTake(cloudMux, portMAX_DELAY);
  bool ok = cloud.lastOk;
  uint32_t ago = cloud.lastOkMs ? (millis() - cloud.lastOkMs) / 1000 : 0;
  String err = cloud.lastError, fwv = cloud.fwVersion;
  int prog = cloud.otaProgress;
  xSemaphoreGive(cloudMux);

  String l1 = conn ? "وای‌فای: " + WiFi.SSID() : (apActive ? "حالت تنظیم: به وای‌فای Petbot-Setup وصل شوید" : "وای‌فای وصل نیست");
  String ip = conn ? WiFi.localIP().toString() : (apActive ? WiFi.softAPIP().toString() : "-");
  String l2 = ok ? "پنل: وصل (" + faDigits(String(ago)) + " ثانیه پیش)" : "پنل: وصل نیست " + err;
  String l3 = String("نسخه: ") + FW_VERSION + (fwv.length() && fwv != FW_VERSION ? "  ·  نسخه‌ی جدید: " + fwv : "");
  faDraw(l1.c_str(), 306, 50, faSmall, A_TEXT, 2);
  faDraw(("آدرس: " + ip).c_str(), 306, 74, faSmall, A_TEXT, 2);
  faDraw(l2.c_str(), 306, 98, faSmall, ok ? 0x5BE37D : 0xFFC24B, 2);
  faDraw(l3.c_str(), 306, 122, faSmall, A_DIM, 2);
  canvas.qrcode(("http://" + ip + "/").c_str(), 14, 50, 96, 3);

  // buttons
  int by = 160;
  canvas.fillSmoothRoundRect(170, by, 140, 34, 10, 0x2A3644);
  faDraw("تنظیم وای‌فای", 240, by + 8, faSmall, A_TEXT, 1);
  bool upd = fwv.length() && fwv != FW_VERSION;
  canvas.fillSmoothRoundRect(14, by, 146, 34, 10, upd ? 0xFF4A3A : 0x1C2630);
  String ul = prog >= 0 ? "در حال نصب " + faDigits(String(prog)) + "٪" : (upd ? "نصب نسخه‌ی جدید" : "به‌روز است");
  faDraw(ul.c_str(), 87, by + 8, faSmall, upd ? A_TEXT : A_DIM, 1);
  if (in.released && in.y > by && in.y < by + 34) {
    if (in.x > 170) { reqPortal = true; soundPlay(SND_CLICK); }
    else if (upd && prog < 0) { xSemaphoreTake(cloudMux, portMAX_DELAY); cloud.otaProgress = 0; xSemaphoreGive(cloudMux); }
  }
  // theme preview: cycles auto -> each theme -> auto
  int ty = 202;
  canvas.fillSmoothRoundRect(14, ty, 296, 32, 10, 0x2A3644);
  String tl = themeOverride < 0 ? String("تم: خودکار (") + THEME_FA[themeId] + ")  ·  ضربه = تم بعدی"
                                : String("تم: ") + THEME_FA[themeOverride] + "  ·  ضربه = تم بعدی";
  faDraw(tl.c_str(), 160, ty + 7, faSmall, A_TEXT, 1);
  if (in.released && in.y > ty && in.y < ty + 32) {
    themeOverride = themeOverride + 1 >= TH_COUNT ? -1 : themeOverride + 1;
    clockReadMs = 0;
    soundPlay(SND_CLICK);
  }
  return true;
}

#include "life.h"
#include "extras.h"

static int subSel = -1;  // tool / fun screen open inside the Tools / Fun apps

static void appOpen(int id) {
  if (id == APP_HAFEZ) hafezRequest();
  if (id == APP_TODO) todoScroll = 0;
  subSel = -1;
}

static bool appRun(int id) {
  if (id == APP_TOOLS || id == APP_FUN) {
    bool tools = id == APP_TOOLS;
    if (subSel < 0) {
      int r = tools ? subMenu("ابزارها", TOOL_NAMES, TOOL_COL, T_COUNT, APP_COL[id])
                    : subMenu("سرگرمی", FUN_NAMES, FUN_COL, F_COUNT, APP_COL[id]);
      if (r == -2) return false;
      if (r >= 0) { subSel = r; breathStart = 0; paintFresh = true; }
      return true;
    }
    if (!(tools ? toolRun(subSel) : funRun(subSel))) subSel = -1;
    return true;
  }
  switch (id) {
    case APP_PET: return petApp(hunger, joy, energy);
    case APP_CLOCK: return clockApp();
    case APP_CALENDAR: return calendarApp();
    case APP_TODO: return todoApp();
    case APP_WEATHER: return weatherApp();
    case APP_PRICES: return pricesApp();
    case APP_HAFEZ: return hafezApp();
    case APP_HABITS: return habitsApp();
    case APP_INFO: return infoApp();
  }
  return false;
}

// ---------- overlays ----------
static String overlayText;
static uint32_t overlayStart = 0;

static bool messageOverlay() {
  vGradient(0x0B0608, 0x1A0E12);
  float t = (millis() - overlayStart) / 1000.0f;
  for (int s = -1; s <= 1; s += 2) fillAAEllipse(160 + s * 26, 46 + sinf(t * 3) * 2, 14, 18, 0xFF72B6, 0x0B0608);
  canvas.fillSmoothRoundRect(14, 80, 292, 146, 14, A_CARD);
  String lines[6];
  int n = faWrap(overlayText.c_str(), faSmall, 270, lines, 6);
  for (int i = 0; i < n; i++) faDraw(lines[i].c_str(), 160, 92 + i * 22, faSmall, A_TEXT, 1);
  faDraw("پیام جدید · برای بستن ضربه بزنید", 160, 6, faSmall, A_DIM, 1);
  return !(in.pressed && t > 0.5f) && t < 30;
}

static String alarmLabel, alarmKind;
static bool alarmOverlay() {
  struct tm t;
  nowTm(t);
  float el = (millis() - overlayStart) / 1000.0f;
  bool strict = alarmKind == "alarm";
  uint32_t bg = strict ? ((millis() / 400) % 2 ? 0x2A0806 : 0x0B0304) : 0x06101A;
  vGradient(bg, 0x05070B);
  for (int s = -1; s <= 1; s += 2)
    fillAAEllipse(160 + s * 30 + (strict ? sinf(el * 30) * 2 : 0), 50, 16, strict ? 12 : 18, strict ? 0xFF2414 : 0x48E6FF, bg);
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
  canvas.setFont(&fonts::Font7);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(A_TEXT);
  canvas.drawString(b, 160, 120);
  String lab = alarmLabel.length() ? alarmLabel : (strict ? "بیدار شو!" : (alarmKind == "water" ? "وقت آب خوردن" : "یادآوری"));
  faDraw(lab.c_str(), 160, 160, faBig, A_TEXT, 1);
  faDraw(strict ? "برای خاموش کردن، برد را تکان بدهید" : "برای بستن ضربه بزنید", 160, 206, faSmall, A_DIM, 1);
  if (!soundBusy()) soundPlay(strict ? SND_ALARM : SND_DING);
  if (!strict && el > 8) return false;
  if (strict) {
    bool shaken = in.imuValid && fabsf(sqrtf(imu.ax * imu.ax + imu.ay * imu.ay + imu.az * imu.az) - 1) > 0.8f;
    return !shaken && el < 300;
  }
  return !(in.pressed && el > 0.5f);
}
