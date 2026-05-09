#pragma once
#include <Arduino.h>
#include "AppConfig.h"

enum DeviceRole : uint8_t {
  ROLE_UNKNOWN = 0,
  ROLE_SPD_HUB,
  ROLE_PMIC,
  ROLE_TEMP_SENSOR_CANDIDATE,
  ROLE_I3C_RESERVED_OR_BROADCAST
};

struct DeviceRoleRecord {
  uint8_t address = 0;
  DeviceRole role = ROLE_UNKNOWN;
  uint8_t confidence = 0;
  char reason[96] = {0};
  bool probeOk = false;
};

struct AppState {
  bool verbose = false;

  bool scanOK = false;
  bool readOK = false;
  bool dumpOK = false;

  bool lastCmdFailed = false;

  uint32_t redBlinkT0 = 0;
  bool redBlinkState = false;

  // GOOD SPD cache (loaded from NVS)
  uint8_t goodSpd[SPD_NVM_SIZE] = {0};
  bool goodSpdValid = false;
  uint32_t goodCrc = 0;

  // Known-good PMIC register reference (loaded from NVS)
  uint8_t pmicRef[PMIC_REF_MAX_LEN] = {0};
  bool pmicRefValid = false;
  uint8_t pmicRefAddr = DEFAULT_PMIC_ADDR;
  uint8_t pmicRefStart = 0x00;
  uint16_t pmicRefLen = 0;
  uint32_t pmicRefCrc = 0;
  uint32_t pmicRefBoot = 0;

  // LAST dump buffer (RAM)
  uint8_t lastDump[SPD_NVM_SIZE] = {0};
  bool lastDumpValid = false;

  // SPD hub MR11 cached
  bool mr11Valid = false;
  uint8_t mr11 = 0x00;

  // HSA state
  bool hsaLow = false;

  // Last scan results for UI
  uint8_t lastScanAddrs[MAX_SCAN_ADDRS] = {0};
  size_t lastScanCount = 0;

  // Last read-only role detection results for UI
  DeviceRoleRecord roleRecords[MAX_ROLE_RECORDS];
  size_t roleRecordCount = 0;

  // Terminal log ring buffer
  char logBuf[LOG_CAP] = {0};
  size_t logHead = 0;
  bool logWrapped = false;
};

extern AppState gApp;
extern uint32_t gBootCount;

// Shared command-result latch used by CLI / Web UI / helper modules
void latchCommandResult(bool ok);
