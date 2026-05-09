#include "I2cHelpers.h"
#include <Wire.h>

bool i2cRegRead(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  uint8_t e = Wire.endTransmission(false);
  if (e != 0) return false;

  size_t got = Wire.requestFrom((int)addr, (int)len, (int)true);
  if (got != len) return false;

  for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)Wire.read();
  return true;
}

bool i2cRegReadRetry(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len, int tries) {
  for (int i = 0; i < tries; i++) {
    if (i > 0) delay(3);
    if (i2cRegRead(addr, reg, buf, len)) return true;
  }
  return false;
}