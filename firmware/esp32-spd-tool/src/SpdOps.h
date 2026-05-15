#pragma once
#include <Arduino.h>

bool dumpToBuffer(uint8_t addr);
bool dumpToBufferWithSource(uint8_t addr, const char* source);
void printCurrentSpdDumpState();

bool cmdScan();
bool cmdPmicId(uint8_t addr);
bool cmdPmicDump(uint8_t addr, uint8_t start, uint16_t len);
bool cmdRegRead(uint8_t addr, uint8_t reg, uint16_t n);
bool cmdEditReg(uint8_t addr, uint8_t reg, uint8_t value);

void cmdStatus();
bool cmdRead(uint8_t addr, uint16_t off, uint16_t n);
bool cmdDump(uint8_t addr, const char* source = "dump");
bool cmdCompare(uint8_t addr);
bool cmdCaptureGood(uint8_t addr);
bool cmdClearGood();
bool cmdVerifyGood(uint8_t addr);
bool cmdWriteGood(uint8_t addr, bool confirmed);
bool cmdAutoFix(uint8_t addr, bool confirmed);
void cmdFlashInfo();

struct SpdTweakSafetyPlan {
  uint8_t addr = 0x50;
  bool currentRead = false;
  bool automaticBackupSaved = false;
  bool backupVerified = false;
  bool diffPreviewReady = false;
  bool userConfirmed = false;
  bool fieldMapVerified = false;
  bool crcUpdaterVerified = false;
  bool patchWritten = false;
  bool rereadDone = false;
  bool postWriteVerified = false;
};

bool spdTweakWritesAllowed(const SpdTweakSafetyPlan& plan);
bool spdTweakFlowComplete(const SpdTweakSafetyPlan& plan);
bool cmdBackupSpd(uint8_t addr);
bool cmdBackupInfo();
bool cmdClearBackup();
bool cmdRestoreBackup(uint8_t addr, bool confirmed);
bool cmdSpdTweak(int nt, char* tok[]);
bool cmdQuickHealth();
bool cmdSpeedTest(uint8_t addr, uint16_t len, uint16_t passes, uint16_t delayMs, bool includeScan, bool includePmic);
bool cmdFullDiag();
