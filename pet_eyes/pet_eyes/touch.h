#pragma once
// Minimal GT911 capacitive touch driver (I2C, no external library).
// Shares the I2C bus with the IMU (SDA=IO39, SCL=IO42). Call after imuBegin().
#include <Wire.h>

static uint8_t gtAddr = 0;
static uint16_t gtMaxX = 240, gtMaxY = 320;
static bool gtDown = false;
static int gtX = 0, gtY = 0;

static bool gtRead(uint16_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(gtAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(gtAddr, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static void gtWrite8(uint16_t reg, uint8_t v) {
  Wire.beginTransmission(gtAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(v);
  Wire.endTransmission();
}

static bool touchBegin() {
  const uint8_t addrs[] = {0x5D, 0x14};
  for (uint8_t a : addrs) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { gtAddr = a; break; }
  }
  if (!gtAddr) {
    Serial.println("TOUCH: GT911 not found (0x5D / 0x14)");
    return false;
  }
  // The GT911 can freeze (INT stuck low, no new frames) and survive a power cycle.
  // Soft reset + a short INT pulse brings it back every boot.
  gtWrite8(0x8040, 0x02);  // command: soft reset
  delay(100);
  gtWrite8(0x8040, 0x00);  // command: read coordinates mode
  delay(50);
  pinMode(40, OUTPUT);
  digitalWrite(40, HIGH);
  delay(60);
  pinMode(40, INPUT);      // INT line released; we poll the status register instead
  gtWrite8(0x814E, 0x00); // clear buffer flag
  uint8_t id[4] = {0}, res[4] = {0};
  gtRead(0x8140, id, 4);
  if (gtRead(0x8048, res, 4)) {
    uint16_t mx = res[0] | (res[1] << 8), my = res[2] | (res[3] << 8);
    if (mx > 0 && my > 0 && mx < 2000 && my < 2000) { gtMaxX = mx; gtMaxY = my; }
  }
  Serial.printf("TOUCH: GT911 at 0x%02X id=%c%c%c%c res=%ux%u\n", gtAddr, id[0], id[1], id[2], id[3],
                gtMaxX, gtMaxY);
  return true;
}

// Updates gtDown / gtX / gtY (raw panel coordinates). Returns false on bus error.
static bool touchPoll() {
  uint8_t s;
  if (!gtRead(0x814E, &s, 1)) {
    static uint32_t lastErr = 0;
    if (millis() - lastErr > 1000) { lastErr = millis(); Serial.println("TOUCH: read error"); }
    return false;
  }
  if (!(s & 0x80)) return true;  // no new frame, keep last state
  uint8_t n = s & 0x0F;
  if (n > 0 && n <= 5) {
    uint8_t p[4];
    if (gtRead(0x8150, p, 4)) {
      gtX = p[0] | (p[1] << 8);
      gtY = p[2] | (p[3] << 8);
      gtDown = true;
    }
  } else {
    gtDown = false;
  }
  gtWrite8(0x814E, 0);
  return true;
}
