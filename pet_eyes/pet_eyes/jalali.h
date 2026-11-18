#pragma once
// Gregorian -> Persian (Jalali) date, plus fixed Persian holidays.

struct JDate { int y, m, d; };

static JDate toJalali(int gy, int gm, int gd) {
  static const int gdm[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int gy2 = (gm > 2) ? (gy + 1) : gy;
  long days = 355666L + (365L * gy) + ((gy2 + 3) / 4) - ((gy2 + 99) / 100) + ((gy2 + 399) / 400) + gd + gdm[gm - 1];
  int jy = -1595 + (33 * (days / 12053));
  days %= 12053;
  jy += 4 * (days / 1461);
  days %= 1461;
  if (days > 365) {
    jy += (days - 1) / 365;
    days = (days - 1) % 365;
  }
  int jm, jd;
  if (days < 186) { jm = 1 + days / 31; jd = 1 + days % 31; }
  else { jm = 7 + (days - 186) / 30; jd = 1 + (days - 186) % 30; }
  return {jy, jm, jd};
}

static const char *FA_MONTHS[12] = {"فروردین", "اردیبهشت", "خرداد", "تیر", "مرداد", "شهریور",
                                    "مهر", "آبان", "آذر", "دی", "بهمن", "اسفند"};
// index = tm_wday (0 = Sunday)
static const char *FA_WEEKDAYS[7] = {"یکشنبه", "دوشنبه", "سه‌شنبه", "چهارشنبه", "پنجشنبه", "جمعه", "شنبه"};

// Fixed solar holidays (month, day, name). Lunar holidays move every year and are not listed.
struct FaHoliday { uint8_t m, d; const char *name; };
static const FaHoliday FA_HOLIDAYS[] = {
  {1, 1, "نوروز"}, {1, 2, "نوروز"}, {1, 3, "نوروز"}, {1, 4, "نوروز"}, {1, 12, "روز جمهوری اسلامی"},
  {1, 13, "سیزده‌بدر"}, {3, 14, "رحلت امام خمینی"}, {3, 15, "قیام ۱۵ خرداد"}, {11, 22, "پیروزی انقلاب"},
  {12, 29, "ملی شدن صنعت نفت"}, {9, 30, "شب یلدا"}, {12, 25, "چهارشنبه‌سوری (حدودی)"},
};

// ---------- Canadian statutory holidays (Gregorian) ----------
static int gDow(int y, int m, int d) {  // 0 = Sunday
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
static int nthDow(int y, int m, int dow, int n) { return 1 + (dow - gDow(y, m, 1) + 7) % 7 + (n - 1) * 7; }
static void easter(int y, int &m, int &d) {
  int a = y % 19, b = y / 100, c = y % 100, dd = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3,
      h = (19 * a + b - dd - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7, mm = (a + 11 * h + 22 * l) / 451;
  m = (h + l - 7 * mm + 114) / 31;
  d = (h + l - 7 * mm + 114) % 31 + 1;
}
static const char *canadaHoliday(int y, int m, int d) {
  if (m == 1 && d == 1) return "سال نو میلادی";
  if (m == 2 && d == nthDow(y, 2, 1, 3)) return "روز خانواده";
  int em, ed;
  easter(y, em, ed);
  int gm = em, gd = ed - 2;  // Good Friday
  if (gd < 1) { gm--; gd += 31; }  // Easter is in Mar/Apr; Mar has 31 days
  if (m == gm && d == gd) return "جمعه‌ی نیک";
  if (m == 5) { int v = 24; while (gDow(y, 5, v) != 1) v--; if (d == v) return "روز ویکتوریا"; }
  if (m == 7 && d == 1) return "روز کانادا";
  if (m == 8 && d == nthDow(y, 8, 1, 1)) return "تعطیلی مدنی";
  if (m == 9 && d == nthDow(y, 9, 1, 1)) return "روز کارگر";
  if (m == 9 && d == 30) return "روز حقیقت و آشتی";
  if (m == 10 && d == nthDow(y, 10, 1, 2)) return "روز شکرگزاری";
  if (m == 11 && d == 11) return "روز یادبود";
  if (m == 12 && d == 25) return "کریسمس";
  if (m == 12 && d == 26) return "باکسینگ دی";
  return nullptr;
}
static const char *GR_MONTHS_FA[12] = {"ژانویه", "فوریه", "مارس", "آوریل", "مه", "ژوئن", "ژوئیه", "اوت", "سپتامبر", "اکتبر", "نوامبر", "دسامبر"};

static const char *faHoliday(const JDate &j) {
  for (auto &h : FA_HOLIDAYS) if (h.m == j.m && h.d == j.d) return h.name;
  return nullptr;
}
