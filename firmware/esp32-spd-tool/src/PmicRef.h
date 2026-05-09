#pragma once
#include <Arduino.h>

uint8_t defaultPmicRefAddr();

bool cmdCapturePmic(uint8_t addr, uint8_t start, uint16_t len);
bool cmdComparePmic(uint8_t addr, uint8_t start, uint16_t len);
bool cmdClearPmic();
void cmdPmicRef();
