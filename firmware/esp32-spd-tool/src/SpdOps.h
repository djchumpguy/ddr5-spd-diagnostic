#pragma once
#include <Arduino.h>

bool dumpToBuffer(uint8_t addr);

bool cmdScan();
bool cmdPmicId(uint8_t addr);
bool cmdPmicDump(uint8_t addr, uint8_t start, uint16_t len);
bool cmdRegRead(uint8_t addr, uint8_t reg, uint16_t n);
bool cmdEditReg(uint8_t addr, uint8_t reg, uint8_t value);

void cmdStatus();
bool cmdRead(uint8_t addr, uint16_t off, uint16_t n);
bool cmdDump(uint8_t addr);
bool cmdCompare(uint8_t addr);
bool cmdCaptureGood(uint8_t addr);
bool cmdClearGood();
bool cmdVerifyGood(uint8_t addr);
bool cmdWriteGood(uint8_t addr, bool confirmed);
bool cmdAutoFix(uint8_t addr, bool confirmed);
void cmdFlashInfo();