#pragma once
// Minimal DS3231 RTC driver (I2C 0x68, shares the bus with the IMU and touch).
// If the RTC holds no valid date, it is set from the firmware build time.
#include <Wire.h>

#define RTC_ADDR 0x68

struct DateTime {
  int year = 2026, month = 1, day = 1, hour = 12, minute = 0, second = 0;
};

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static bool rtcRead(DateTime &t) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(RTC_ADDR, 7) != 7) return false;
  uint8_t r[7];
  for (int i = 0; i < 7; i++) r[i] = Wire.read();
  t.second = bcd2bin(r[0] & 0x7F);
  t.minute = bcd2bin(r[1] & 0x7F);
  t.hour = bcd2bin(r[2] & 0x3F);  // 24h mode (we always write 24h)
  t.day = bcd2bin(r[4] & 0x3F);
  t.month = bcd2bin(r[5] & 0x1F);
  t.year = 2000 + bcd2bin(r[6]);
  return true;
}

static void rtcWrite(const DateTime &t) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x00);
  Wire.write(bin2bcd(t.second));
  Wire.write(bin2bcd(t.minute));
  Wire.write(bin2bcd(t.hour));
  Wire.write(1);  // day of week (unused)
  Wire.write(bin2bcd(t.day));
  Wire.write(bin2bcd(t.month));
  Wire.write(bin2bcd(t.year - 2000));
  Wire.endTransmission();
}

static DateTime buildTime() {
  DateTime t;
  const char *d = __DATE__;  // "Oct 22 2026"
  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char m3[4] = {d[0], d[1], d[2], 0};
  const char *p = strstr(months, m3);
  t.month = p ? (p - months) / 3 + 1 : 1;
  t.day = atoi(d + 4);
  t.year = atoi(d + 7);
  const char *tm = __TIME__;  // "hh:mm:ss"
  t.hour = atoi(tm);
  t.minute = atoi(tm + 3);
  t.second = atoi(tm + 6);
  return t;
}

static bool rtcBegin(DateTime &now) {
  if (!rtcRead(now)) {
    Serial.println("RTC: not found, using build time");
    now = buildTime();
    return false;
  }
  if (now.year < 2025 || now.month < 1 || now.month > 12 || now.day < 1 || now.day > 31) {
    now = buildTime();
    rtcWrite(now);
    Serial.println("RTC: was empty, set from build time");
  }
  Serial.printf("RTC: %04d-%02d-%02d %02d:%02d\n", now.year, now.month, now.day, now.hour, now.minute);
  return true;
}
