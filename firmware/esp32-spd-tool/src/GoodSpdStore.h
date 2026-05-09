#pragma once
#include <Arduino.h>

void storeInit();
bool loadGoodFromNvs();
bool saveGoodToNvs(const uint8_t* spd1024);
void clearGoodNvs();
uint32_t crc32_buf(const uint8_t* data, size_t len);

bool loadPmicRefFromNvs();
bool savePmicRefToNvs(uint8_t addr, uint8_t start, uint16_t len, const uint8_t* data, uint32_t bootCount);
void clearPmicRefNvs();
