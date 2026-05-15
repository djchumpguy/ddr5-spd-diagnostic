#include "RoleDetect.h"

#include <Wire.h>
#include <string.h>

#include "AppConfig.h"
#include "HardwareConfig.h"
#include "I2cHelpers.h"
#include "Log.h"
#include "SpdHub.h"

static bool addrInRange(uint8_t a, uint8_t lo, uint8_t hi) {
  return a >= lo && a <= hi;
}

const char* roleName(DeviceRole role) {
  switch (role) {
    case ROLE_SPD_HUB: return "SPD_HUB";
    case ROLE_PMIC: return "PMIC";
    case ROLE_TEMP_SENSOR_CANDIDATE: return "TEMP_SENSOR_CANDIDATE";
    case ROLE_I3C_RESERVED_OR_BROADCAST: return "RESERVED";
    case ROLE_UNKNOWN:
    default: return "UNKNOWN";
  }
}

const char* roleUiName(DeviceRole role) {
  switch (role) {
    case ROLE_SPD_HUB: return "SPD/HUB";
    case ROLE_PMIC: return "PMIC";
    case ROLE_TEMP_SENSOR_CANDIDATE: return "Temp candidate";
    case ROLE_I3C_RESERVED_OR_BROADCAST: return "Reserved/Unknown";
    case ROLE_UNKNOWN:
    default: return "Unknown";
  }
}

const char* confidenceName(uint8_t confidence) {
  if (confidence >= 80) return "high";
  if (confidence >= 45) return "medium";
  return "low";
}

DeviceRoleRecord* findRoleRecord(uint8_t addr) {
  for (size_t i = 0; i < gApp.roleRecordCount; i++) {
    if (gApp.roleRecords[i].address == addr) return &gApp.roleRecords[i];
  }
  return nullptr;
}

static void setReason(DeviceRoleRecord& rec, const char* fmt, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0) {
  snprintf(rec.reason, sizeof(rec.reason), fmt, a, b, c, d);
}

static bool looksDdr5Spd(const uint8_t* b, size_t len) {
  if (len < 4) return false;
  if (b[0] == 0x30 && b[1] == 0x10 && b[2] == 0x12 && b[3] == 0x02) return true;
  if (b[0] == 0x30 && b[1] == 0x10) return true;
  if (b[0] == 0x30 && b[2] == 0x12) return true;
  return false;
}

static bool allSameBad(const uint8_t* b, size_t len) {
  if (len == 0) return true;
  bool all00 = true;
  bool allFF = true;
  for (size_t i = 0; i < len; i++) {
    if (b[i] != 0x00) all00 = false;
    if (b[i] != 0xFF) allFF = false;
  }
  return all00 || allFF;
}

static bool probeSpdHub(uint8_t addr, DeviceRoleRecord& rec) {
  uint8_t b[16] = {0};
  bool ok = spdNvmRead(addr, 0x0000, b, sizeof(b));
  rec.probeOk = ok;
  if (!ok) {
    rec.role = ROLE_UNKNOWN;
    rec.confidence = 25;
    setReason(rec, "SPD range, but offset 0x0000 read failed");
    return false;
  }

  if (looksDdr5Spd(b, sizeof(b))) {
    rec.role = ROLE_SPD_HUB;
    rec.confidence = (b[0] == 0x30 && b[1] == 0x10 && b[2] == 0x12 && b[3] == 0x02) ? 95 : 80;
    setReason(rec, "SPD offset 0x0000 read starts %02X %02X %02X %02X", b[0], b[1], b[2], b[3]);
    return true;
  }

  rec.role = ROLE_UNKNOWN;
  rec.confidence = 45;
  setReason(rec, "SPD range read succeeded; bytes start %02X %02X %02X %02X", b[0], b[1], b[2], b[3]);
  return false;
}

static bool probePmic(uint8_t addr, DeviceRoleRecord& rec) {
  uint8_t id[2] = {0};
  bool idOk = i2cRegReadRetry(addr, 0x3C, id, sizeof(id), 4);
  rec.probeOk = idOk;
  if (idOk && !allSameBad(id, sizeof(id))) {
    rec.role = ROLE_PMIC;
    rec.confidence = 90;
    setReason(rec, "PMIC ID regs 0x3C/0x3D returned 0x%02X 0x%02X", id[0], id[1]);
    return true;
  }

  uint8_t regs[0x40] = {0};
  bool dumpOk = i2cRegReadRetry(addr, 0x00, regs, sizeof(regs), 4);
  rec.probeOk = rec.probeOk || dumpOk;
  if (dumpOk && !allSameBad(regs, sizeof(regs))) {
    rec.role = ROLE_PMIC;
    rec.confidence = 70;
    setReason(rec, "PMIC range register dump 0x00-0x3F succeeded");
    return true;
  }

  rec.role = ROLE_UNKNOWN;
  rec.confidence = 25;
  setReason(rec, "PMIC range, but ID/register probes failed or returned empty values");
  return false;
}

static void classifyAddress(uint8_t addr, DeviceRoleRecord& rec) {
  rec.address = addr;
  rec.role = ROLE_UNKNOWN;
  rec.confidence = 20;
  rec.probeOk = false;
  rec.reason[0] = 0;

  if (addr == 0x7E) {
    rec.role = ROLE_I3C_RESERVED_OR_BROADCAST;
    rec.confidence = 70;
    setReason(rec, "I3C reserved/broadcast style address; no actions assigned");
    return;
  }

  if (addrInRange(addr, 0x50, 0x57)) {
    probeSpdHub(addr, rec);
    return;
  }

  if (addrInRange(addr, 0x48, 0x4F)) {
    probePmic(addr, rec);
    return;
  }

  if (addrInRange(addr, 0x10, 0x17) || addrInRange(addr, 0x30, 0x37) || addr == 0x0C) {
    rec.role = ROLE_TEMP_SENSOR_CANDIDATE;
    rec.confidence = 45;
    setReason(rec, "common temp-sensor candidate address; no temp probe implemented");
    return;
  }

  setReason(rec, "address scanned but no known DDR5 sideband role matched");
}

static bool scanBus() {
  gApp.lastScanCount = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0 && gApp.lastScanCount < MAX_SCAN_ADDRS) {
      gApp.lastScanAddrs[gApp.lastScanCount++] = addr;
    }
    delay(2);
  }
  gApp.scanOK = gApp.lastScanCount > 0;
  return gApp.scanOK;
}

bool cmdAutoDetectRoles() {
  outPrintln();
  outPrintln("Auto-detect roles (read-only)");
  outPrintln("  No PWR_EN, HSA, or VIN_BULK state changes are performed.");
  if (!scanBus()) {
    gApp.roleRecordCount = 0;
    outPrintln("  No I2C devices found.");
    latchCommandResult(false);
    return false;
  }

  gApp.roleRecordCount = 0;
  for (size_t i = 0; i < gApp.lastScanCount && gApp.roleRecordCount < MAX_ROLE_RECORDS; i++) {
    DeviceRoleRecord& rec = gApp.roleRecords[gApp.roleRecordCount++];
    classifyAddress(gApp.lastScanAddrs[i], rec);
  }

  outPrintln("Auto-detect roles:");
  uint8_t expectedAddr = 0;
  const char* expectedDesc = nullptr;
  bool hasExpected = hwExpectedSpdAddr(expectedAddr, expectedDesc);
  bool foundExpected = false;
  bool foundOtherSpd = false;
  uint8_t otherSpd = 0;

  for (size_t i = 0; i < gApp.roleRecordCount; i++) {
    DeviceRoleRecord& rec = gApp.roleRecords[i];
    if (rec.role == ROLE_SPD_HUB) {
      if (hasExpected && rec.address == expectedAddr) foundExpected = true;
      if (hasExpected && rec.address != expectedAddr && !foundOtherSpd) {
        foundOtherSpd = true;
        otherSpd = rec.address;
      }
    }
    outPrintf("  0x%02X  %-22s confidence=%s  reason=%s\n",
              rec.address, roleName(rec.role), confidenceName(rec.confidence), rec.reason);
  }

  if (hasExpected && foundExpected) {
    outPrintln("Observed address matches the address previously seen for this config on tested hardware.");
    outPrintln("HSA mode still comes from declared hardware config, not address alone.");
  } else if (hasExpected && foundOtherSpd && !foundExpected) {
    outPrintf("WARNING: Observed SPD/HUB address differs from the address previously associated with the declared HSA config on tested hardware (%s), observed 0x%02X.\n",
              expectedDesc, otherSpd);
  }
  outPrintln();

  latchCommandResult(true);
  return true;
}
