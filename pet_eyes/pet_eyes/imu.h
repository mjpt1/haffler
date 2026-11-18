#pragma once
// Minimal ICM-42688-P driver (I2C, no external library).
// Board: SDA=IO39, SCL=IO42, address 0x69 (DS3231 RTC shares the bus at 0x68).
#include <Wire.h>

#define IMU_SDA  39
#define IMU_SCL  42
#define IMU_ADDR 0x69

struct ImuData {
  float ax = 0, ay = 0, az = 0;  // g
  float gx = 0, gy = 0, gz = 0;  // deg/s
};

static void imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static bool imuBegin() {
  Wire.begin(IMU_SDA, IMU_SCL, 400000);
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x75);  // WHO_AM_I
  if (Wire.endTransmission(false) != 0) {
    Serial.println("IMU: no answer at 0x69");
    return false;
  }
  Wire.requestFrom(IMU_ADDR, 1);
  uint8_t id = Wire.read();
  Serial.printf("IMU: WHO_AM_I = 0x%02X (expect 0x47)\n", id);
  imuWrite(0x4E, 0x0F);  // PWR_MGMT0: gyro + accel in low-noise mode
  delay(50);             // gyro start-up time
  return true;
}

// Default ranges: accel +-16 g (2048 LSB/g), gyro +-2000 dps (16.4 LSB/dps).
static bool imuRead(ImuData &d) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x1F);  // ACCEL_DATA_X1
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(IMU_ADDR, 12) != 12) return false;
  int16_t v[6];
  for (int i = 0; i < 6; i++) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    v[i] = (int16_t)((hi << 8) | lo);
  }
  d.ax = v[0] / 2048.0f;
  d.ay = v[1] / 2048.0f;
  d.az = v[2] / 2048.0f;
  d.gx = v[3] / 16.4f;
  d.gy = v[4] / 16.4f;
  d.gz = v[5] / 16.4f;
  return true;
}
