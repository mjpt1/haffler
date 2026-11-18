// Minimal hardware test: no display, no sound, no WiFi.
// Prints GT911 touch status twice a second on USB serial.
#include <Arduino.h>
#include <Wire.h>

static uint8_t addr = 0;

static bool rd(uint16_t reg, uint8_t *b, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(addr, n) != n) return false;
  for (int i = 0; i < n; i++) b[i] = Wire.read();
  return true;
}
static void wr(uint16_t reg, uint8_t v) {
  Wire.beginTransmission(addr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(v);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  pinMode(46, OUTPUT);  // keep the buzzer pin quiet
  digitalWrite(46, LOW);
  Wire.begin(39, 42, 400000);
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf("I2C device at 0x%02X\n", a);
  }
  for (uint8_t a : {0x5D, 0x14}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { addr = a; break; }
  }
  Serial.printf("GT911 addr 0x%02X\n", addr);
  // GT911 reset by the INT/RST-less way: soft reset command, then back to coordinate mode
  wr(0x8040, 0x02);
  delay(100);
  wr(0x8040, 0x00);
  delay(50);
  wr(0x814E, 0x00);
  // also drive INT high briefly (some boards wire it with a pull-down), then release
  pinMode(40, OUTPUT);
  digitalWrite(40, HIGH);
  delay(60);
  pinMode(40, INPUT);
}

void loop() {
  static uint32_t last = 0;
  uint8_t s = 0;
  if (addr && rd(0x814E, &s, 1)) {
    if (s & 0x80) {
      uint8_t p[4] = {0};
      rd(0x8150, p, 4);
      if (s & 0x0F) Serial.printf("TOUCH %u,%u\n", p[0] | (p[1] << 8), p[2] | (p[3] << 8));
      wr(0x814E, 0);
    }
  }
  if (millis() - last > 500) {
    last = millis();
    Serial.printf("status 0x%02X int(IO40)=%d\n", s, digitalRead(40));
  }
  delay(10);
}
