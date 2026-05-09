#pragma once

#include <Arduino.h>

bool cmdTimeSpd(uint8_t addr = 0x50, uint16_t off = 0x0000, uint16_t len = 32, uint16_t passes = 20);