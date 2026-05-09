#pragma once
#include <Arduino.h>

bool i2cRegRead(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len);
bool i2cRegReadRetry(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len, int tries = 6);