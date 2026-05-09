#pragma once

#include <Arduino.h>

bool cmdTimeReg(uint8_t addr = 0x50, uint8_t reg = 0x00, uint16_t len = 16, uint16_t passes = 20);