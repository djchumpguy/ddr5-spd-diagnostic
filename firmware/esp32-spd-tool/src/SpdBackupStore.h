#pragma once
#include <Arduino.h>

void spdBackupStoreInit();
bool loadSpdBackupFromNvs();
bool saveSpdBackupToNvs(uint8_t addr, const uint8_t* spd1024, uint32_t bootCount, const char* label);
bool verifyLoadedSpdBackupCrc();
void clearSpdBackupNvs();
