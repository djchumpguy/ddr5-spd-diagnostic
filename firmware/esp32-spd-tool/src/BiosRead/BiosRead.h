#pragma once
#include <Arduino.h>

bool cmdBiosMr11(uint8_t addr);
bool cmdBiosRead(uint8_t addr, uint16_t absOff);
bool cmdBiosDump(uint8_t addr, uint16_t startOff, uint16_t count);
bool cmdBiosInteresting(uint8_t addr);