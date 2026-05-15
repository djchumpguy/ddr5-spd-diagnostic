#include "SpdOps.h"
#include "AppConfig.h"
#include "AppState.h"
#include "BoardControl.h"
#include "GoodSpdStore.h"
#include "HardwareConfig.h"
#include "I2cHelpers.h"
#include "MapOps.h"
#include "PmicRef.h"
#include "RoleDetect.h"
#include "SpdHub.h"
#include "SpdBackupStore.h"
#include "SpdTweak.h"
#include "Log.h"
#include <WiFi.h>
#include <Wire.h>
#include <string.h>

static bool i2cScanToList(uint8_t* outList, size_t maxList, size_t& outCount) {
  outCount = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      if (outCount < maxList) outList[outCount++] = addr;
    }
    delay(2);
  }
  return (outCount > 0);
}

static bool scanListsEqual(const uint8_t* a, size_t ac, const uint8_t* b, size_t bc) {
  if (ac != bc) return false;
  for (size_t i = 0; i < ac; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

static uint8_t firstRoleAddr(DeviceRole role, uint8_t fallback) {
  for (size_t i = 0; i < gApp.roleRecordCount; i++) {
    if (gApp.roleRecords[i].role == role && gApp.roleRecords[i].probeOk) return gApp.roleRecords[i].address;
  }
  return fallback;
}

static uint8_t resolvedSpdDiagAddr() {
  if (gApp.currentSpdDump.valid) return gApp.currentSpdDump.addr;
  for (size_t i = 0; i < gApp.roleRecordCount; i++) {
    if (gApp.roleRecords[i].role == ROLE_SPD_HUB && gApp.roleRecords[i].probeOk) return gApp.roleRecords[i].address;
  }
  uint8_t expected = 0;
  const char* desc = nullptr;
  if (hwExpectedSpdAddr(expected, desc)) return expected;
  return DEFAULT_SPD_ADDR;
}

static bool looksDdr5Header(const uint8_t* b, size_t len) {
  if (!b || len < 4) return false;
  if (b[0] == 0x30 && b[1] == 0x10 && b[2] == 0x12) return true;
  if (b[0] == 0x30 && b[1] == 0x10) return true;
  if (b[0] == 0x30 && b[2] == 0x12) return true;
  return false;
}

static bool readSpdRange(uint8_t addr, uint16_t off, uint8_t* buf, uint16_t len) {
  if (!buf || off >= SPD_NVM_SIZE || (uint32_t)off + len > SPD_NVM_SIZE) return false;
  const uint16_t chunkMax = 32;
  for (uint16_t pos = 0; pos < len; pos += chunkMax) {
    uint16_t chunk = len - pos;
    if (chunk > chunkMax) chunk = chunkMax;
    if (!spdNvmRead(addr, off + pos, &buf[pos], chunk)) return false;
  }
  return true;
}

static bool findMismatch(const uint8_t* ref, const uint8_t* cur, uint16_t len, uint16_t& idx, uint8_t& exp, uint8_t& got) {
  for (uint16_t i = 0; i < len; i++) {
    if (ref[i] != cur[i]) {
      idx = i;
      exp = ref[i];
      got = cur[i];
      return true;
    }
  }
  return false;
}

static const char* pgoodDiagLabel(bool& trusted, bool& ready) {
  trusted = (gApp.hwConfig.pwrGood == HW_PGOOD_GPIO_READ);
  ready = pwrGoodReady();
  if (!trusted) return "untrusted/unavailable by harness config";
  return ready ? "READY" : "LOW";
}

bool cmdScan() {
  setProcessing(true);
  outPrintln("I2C scan:");
  gApp.lastScanCount = 0;
  gApp.roleRecordCount = 0;

  bool ok = i2cScanToList(gApp.lastScanAddrs, MAX_SCAN_ADDRS, gApp.lastScanCount);
  for (size_t i = 0; i < gApp.lastScanCount; i++) {
    outPrintf("  Found device at 0x%02X\n", gApp.lastScanAddrs[i]);
  }
  if (!ok) outPrintln("  (none found)");

  setProcessing(false);

  if (ok) gApp.scanOK = true;
  latchCommandResult(ok);
  return ok;
}

bool cmdQuickHealth() {
  outPrintln();
  outPrintln("QUICK HEALTH CHECK (read-only)");
  outPrintln("  Does not write SPD, change HSA, change PWR_EN, cold-cycle VIN_BULK, or modify saved references.");

  uint8_t scanList[MAX_SCAN_ADDRS] = {0};
  size_t scanCount = 0;
  bool scanOk = i2cScanToList(scanList, MAX_SCAN_ADDRS, scanCount);
  gApp.lastScanCount = scanCount;
  memcpy(gApp.lastScanAddrs, scanList, scanCount);
  gApp.scanOK = scanOk;

  bool fail = false;
  bool warn = false;
  if (!scanOk) {
    outPrintln("  scan: FAIL no I2C devices found");
    fail = true;
  } else {
    outPrintf("  scan: PASS %u device(s)", (unsigned)scanCount);
    for (size_t i = 0; i < scanCount; i++) outPrintf(" 0x%02X", scanList[i]);
    outPrintln();
  }

  cmdAutoDetectRoles();
  uint8_t spdAddr = firstRoleAddr(ROLE_SPD_HUB, resolvedSpdDiagAddr());
  uint8_t header[16] = {0};
  bool spdOk = scanOk && readSpdRange(spdAddr, 0, header, sizeof(header));
  bool headerOk = spdOk && looksDdr5Header(header, sizeof(header));
  if (!spdOk) {
    outPrintf("  SPD/HUB: FAIL read header at 0x%02X failed\n", spdAddr);
    fail = true;
  } else if (!headerOk) {
    outPrintf("  SPD/HUB: FAIL header at 0x%02X starts %02X %02X %02X %02X\n",
              spdAddr, header[0], header[1], header[2], header[3]);
    fail = true;
  } else {
    outPrintf("  SPD/HUB: PASS addr=0x%02X header=%02X %02X %02X %02X\n",
              spdAddr, header[0], header[1], header[2], header[3]);
  }

  uint8_t mr11 = 0;
  if (spdRegRead(spdAddr, 0x0B, mr11)) outPrintf("  MR11: PASS 0x%02X\n", mr11);
  else { outPrintln("  MR11: WARN unavailable"); warn = true; }

  uint8_t pmicAddr = firstRoleAddr(ROLE_PMIC, defaultPmicRefAddr());
  bool pmicDetected = false;
  for (size_t i = 0; i < gApp.roleRecordCount; i++) {
    if (gApp.roleRecords[i].role == ROLE_PMIC) pmicDetected = true;
  }
  uint8_t pmicId[2] = {0};
  bool pmicOk = i2cRegReadRetry(pmicAddr, 0x3C, pmicId, sizeof(pmicId), 4);
  if (pmicOk) {
    outPrintf("  PMIC ID: PASS addr=0x%02X id=%02X %02X\n", pmicAddr, pmicId[0], pmicId[1]);
  } else if (pmicDetected || gApp.pmicRefValid) {
    outPrintf("  PMIC ID: FAIL expected addr=0x%02X read failed\n", pmicAddr);
    fail = true;
  } else {
    outPrintln("  PMIC ID: WARN not detected/skipped");
    warn = true;
  }

  bool pgoodTrusted = false, pgoodReadyNow = false;
  const char* pgood = pgoodDiagLabel(pgoodTrusted, pgoodReadyNow);
  if (pgoodTrusted && !pgoodReadyNow) fail = true;
  if (!pgoodTrusted) warn = true;
  outPrintf("  PWR_GOOD: %s\n", pgood);

  outPrintf("  Diagnostic ref: %s", gApp.goodSpdValid ? "PRESENT" : "MISSING");
  if (gApp.goodSpdValid) outPrintf(" crc32=0x%08lX\n", (unsigned long)gApp.goodCrc);
  else { outPrintln(); warn = true; }

  outPrintf("  Tweak checkpoint: %s", gApp.spdBackupValid ? "PRESENT" : "MISSING");
  if (gApp.spdBackupValid) outPrintf(" addr=0x%02X crc32=0x%08lX\n", gApp.spdBackupAddr, (unsigned long)gApp.spdBackupCrc);
  else { outPrintln(); warn = true; }

  if (gApp.hwConfig.pullups == HW_PULLUPS_ESP32_INTERNAL_ONLY) {
    outPrintln("  SDA/SCL pullups: WARN ESP32 internal only");
    warn = true;
  }

  const char* result = fail ? "FAIL" : (warn ? "WARN" : "PASS");
  outPrintf("QUICK HEALTH RESULT: %s\n", result);
  latchCommandResult(!fail);
  return !fail;
}

bool cmdSpeedTest(uint8_t addr, uint16_t len, uint16_t passes, uint16_t delayMs, bool includeScan, bool includePmic) {
  if (len == 0) len = 32;
  if (len > SPD_NVM_SIZE) len = SPD_NVM_SIZE;
  if (passes == 0) passes = 20;
  if (passes > 500) passes = 500;
  if (delayMs > 10000) delayMs = 10000;

  static uint8_t ref[SPD_NVM_SIZE];
  static uint8_t cur[SPD_NVM_SIZE];
  memset(ref, 0, len);
  memset(cur, 0, len);

  outPrintln();
  outPrintln("SPD/I2C SPEED & STABILITY TEST (read-only)");
  outPrintln("  Tests SPD/I2C/PMIC diagnostic read stability only. This does not test RAM bandwidth or memory-cell stability.");
  outPrintf("  addr=0x%02X len=%u passes=%u delay_ms=%u scan=%s pmic=%s\n",
            addr, len, passes, delayMs, includeScan ? "yes" : "no", includePmic ? "yes" : "no");

  uint16_t successes = 0, failures = 0, mismatches = 0, pgoodLow = 0;
  uint16_t firstMismatchPass = 0, firstMismatchOff = 0;
  uint8_t firstExp = 0, firstGot = 0;
  uint32_t minUs = 0xFFFFFFFFUL, maxUs = 0, totalUs = 0;
  bool haveRef = false;
  bool fail = false, warn = false;

  uint8_t baselineScan[MAX_SCAN_ADDRS] = {0};
  size_t baselineScanCount = 0;
  bool haveBaselineScan = false;
  uint16_t scanChanges = 0, scanFailures = 0;
  uint8_t pmicAddr = firstRoleAddr(ROLE_PMIC, defaultPmicRefAddr());
  uint8_t pmicBase[2] = {0};
  bool pmicHaveBase = false;
  uint16_t pmicFailures = 0, pmicChanges = 0;

  for (uint16_t pass = 0; pass < passes; pass++) {
    boardTick();
    if (gApp.hwConfig.pwrGood == HW_PGOOD_GPIO_READ && !pwrGoodReady()) pgoodLow++;

    uint32_t t0 = micros();
    bool ok = readSpdRange(addr, 0, cur, len);
    uint32_t dt = micros() - t0;
    if (!ok) {
      failures++;
      fail = true;
    } else {
      successes++;
      totalUs += dt;
      if (dt < minUs) minUs = dt;
      if (dt > maxUs) maxUs = dt;
      if (!haveRef) {
        memcpy(ref, cur, len);
        haveRef = true;
      } else {
        uint16_t idx = 0;
        uint8_t exp = 0, got = 0;
        if (findMismatch(ref, cur, len, idx, exp, got)) {
          mismatches++;
          fail = true;
          if (firstMismatchPass == 0) {
            firstMismatchPass = pass + 1;
            firstMismatchOff = idx;
            firstExp = exp;
            firstGot = got;
          }
        }
      }
    }

    if (includeScan) {
      uint8_t list[MAX_SCAN_ADDRS] = {0};
      size_t count = 0;
      bool scanOk = i2cScanToList(list, MAX_SCAN_ADDRS, count);
      if (!scanOk) {
        scanFailures++;
        fail = true;
      } else if (!haveBaselineScan) {
        memcpy(baselineScan, list, count);
        baselineScanCount = count;
        haveBaselineScan = true;
      } else if (!scanListsEqual(baselineScan, baselineScanCount, list, count)) {
        scanChanges++;
        fail = true;
      }
    }

    if (includePmic) {
      uint8_t id[2] = {0};
      bool pmicOk = i2cRegReadRetry(pmicAddr, 0x3C, id, sizeof(id), 3);
      if (!pmicOk) {
        pmicFailures++;
        if (gApp.pmicRefValid || firstRoleAddr(ROLE_PMIC, 0) != 0) fail = true;
        else warn = true;
      } else if (!pmicHaveBase) {
        memcpy(pmicBase, id, sizeof(id));
        pmicHaveBase = true;
      } else if (memcmp(pmicBase, id, sizeof(id)) != 0) {
        pmicChanges++;
        fail = true;
      }
    }

    if ((pass + 1) < passes && delayMs > 0) delay(delayMs);
  }

  uint32_t avgUs = successes ? (totalUs / successes) : 0;
  uint32_t bytesPerSec = avgUs ? ((uint32_t)len * 1000000UL) / avgUs : 0;
  if (successes > 0 && avgUs > 100000UL) warn = true;
  if (gApp.hwConfig.pwrGood == HW_PGOOD_GPIO_READ && pgoodLow > 0) fail = true;
  if (gApp.hwConfig.pwrGood != HW_PGOOD_GPIO_READ) warn = true;
  if (includePmic && !pmicHaveBase) warn = true;

  outPrintln("SPEEDTEST SUMMARY");
  outPrintf("  completed=%u/%u failures=%u mismatches=%u\n", successes, passes, failures, mismatches);
  outPrintf("  time_us: min=%lu avg=%lu max=%lu bytes_per_sec=%lu\n",
            (unsigned long)(minUs == 0xFFFFFFFFUL ? 0 : minUs),
            (unsigned long)avgUs,
            (unsigned long)maxUs,
            (unsigned long)bytesPerSec);
  if (firstMismatchPass) {
    outPrintf("  first_mismatch: pass=%u offset=0x%04X expected=%02X got=%02X\n",
              firstMismatchPass, firstMismatchOff, firstExp, firstGot);
  }
  if (len == SPD_NVM_SIZE && haveRef) outPrintf("  baseline_crc32=0x%08lX\n", (unsigned long)crc32_buf(ref, SPD_NVM_SIZE));
  if (includeScan) outPrintf("  scan: failures=%u address_list_changes=%u\n", scanFailures, scanChanges);
  if (includePmic) outPrintf("  pmic_quick: addr=0x%02X failures=%u id_changes=%u\n", pmicAddr, pmicFailures, pmicChanges);
  outPrintf("  PWR_GOOD low events=%u%s\n", pgoodLow, gApp.hwConfig.pwrGood == HW_PGOOD_GPIO_READ ? "" : " (untrusted/unavailable)");

  const char* result = fail ? "FAIL" : (warn ? "WARN" : "PASS");
  outPrintf("SPEEDTEST RESULT: %s\n", result);
  latchCommandResult(!fail);
  return !fail;
}

bool cmdFullDiag() {
  outPrintln();
  outPrintln("FULL SYSTEM DIAGNOSTIC REPORT (read-only)");
  outPrintln("  Suitable for GitHub issues / ChatGPT. No writes or strap/power changes are performed.");
  outPrintln();

  cmdStatus();
  cmdScan();
  cmdAutoDetectRoles();

  uint8_t spdAddr = firstRoleAddr(ROLE_SPD_HUB, resolvedSpdDiagAddr());
  uint8_t pmicAddr = firstRoleAddr(ROLE_PMIC, defaultPmicRefAddr());
  bool pgoodTrusted = false, pgoodReadyNow = false;
  const char* pgood = pgoodDiagLabel(pgoodTrusted, pgoodReadyNow);

  uint8_t header[16] = {0};
  bool headerOk = readSpdRange(spdAddr, 0, header, sizeof(header));
  bool ddr5ish = headerOk && looksDdr5Header(header, sizeof(header));
  outPrintf("SPD header addr=0x%02X read=%s ddr5ish=%s bytes=%02X %02X %02X %02X\n",
            spdAddr, headerOk ? "PASS" : "FAIL", ddr5ish ? "yes" : "no",
            header[0], header[1], header[2], header[3]);

  bool dumpOk = dumpToBufferWithSource(spdAddr, "fulldiag");
  outPrintf("SPD 1024-byte dump CRC: %s", dumpOk ? "PASS" : "FAIL");
  if (dumpOk) outPrintf(" crc32=0x%08lX\n", (unsigned long)gApp.currentSpdDump.crc32);
  else outPrintln();

  const char* spdCompare = "SKIPPED";
  if (dumpOk && gApp.goodSpdValid) {
    uint32_t mism = 0;
    uint16_t first = 0;
    uint8_t rb = 0, eb = 0;
    for (uint16_t i = 0; i < SPD_NVM_SIZE; i++) {
      if (gApp.lastDump[i] != gApp.goodSpd[i]) {
        if (mism == 0) { first = i; rb = gApp.lastDump[i]; eb = gApp.goodSpd[i]; }
        mism++;
      }
    }
    if (mism == 0) spdCompare = "PASS";
    else {
      spdCompare = "FAIL";
      outPrintf("SPD compare to diagnostic reference: FAIL mismatches=%lu first=0x%04X read=%02X ref=%02X\n",
                (unsigned long)mism, first, rb, eb);
    }
  }
  if (strcmp(spdCompare, "FAIL") != 0) outPrintf("SPD compare to diagnostic reference: %s\n", spdCompare);

  uint8_t pmicId[2] = {0};
  bool pmicIdOk = i2cRegReadRetry(pmicAddr, 0x3C, pmicId, sizeof(pmicId), 4);
  outPrintf("PMIC ID addr=0x%02X: %s", pmicAddr, pmicIdOk ? "PASS" : "WARN/FAIL");
  if (pmicIdOk) outPrintf(" id=%02X %02X\n", pmicId[0], pmicId[1]);
  else outPrintln();
  if (gApp.pmicRefValid) cmdComparePmic(gApp.pmicRefAddr, gApp.pmicRefStart, gApp.pmicRefLen);
  else outPrintln("PMIC compare: SKIPPED no PMIC reference");

  cmdRegRead(spdAddr, 0x00, 16);
  bool speedOk = cmdSpeedTest(spdAddr, 32, 20, 20, true, pmicIdOk);

  bool fail = !headerOk || !ddr5ish || !dumpOk || strcmp(spdCompare, "FAIL") == 0 || !speedOk ||
              (pgoodTrusted && !pgoodReadyNow);
  bool warn = !gApp.goodSpdValid || !gApp.spdBackupValid || !pmicIdOk || !pgoodTrusted ||
              strcmp(spdCompare, "SKIPPED") == 0;
  const char* finalResult = fail ? "FAIL" : (warn ? "WARN" : "PASS");

  outPrintln();
  outPrintln("FULL DIAGNOSTIC SUMMARY");
  outPrintf("  SPD/HUB addr: 0x%02X\n", spdAddr);
  outPrintf("  PMIC addr: 0x%02X\n", pmicAddr);
  outPrintf("  PWR_GOOD: %s\n", pgood);
  outPrintf("  Diagnostic ref: %s\n", gApp.goodSpdValid ? "PRESENT" : "MISSING");
  outPrintf("  Tweak checkpoint: %s\n", gApp.spdBackupValid ? "PRESENT" : "MISSING");
  outPrintf("  SPD compare: %s\n", spdCompare);
  outPrintf("  PMIC compare: %s\n", gApp.pmicRefValid ? "see PMIC COMPARE above" : "SKIPPED");
  outPrintf("  Speed/stability: %s\n", speedOk ? "PASS/WARN see SPEEDTEST RESULT" : "FAIL");
  outPrintf("FULLDIAG RESULT: %s\n", finalResult);
  latchCommandResult(!fail);
  return !fail;
}

static void updateCurrentSpdDump(uint8_t addr, const char* source) {
  memcpy(gApp.currentSpdDump.data, gApp.lastDump, SPD_NVM_SIZE);
  gApp.currentSpdDump.valid = true;
  gApp.currentSpdDump.addr = addr;
  gApp.currentSpdDump.len = SPD_NVM_SIZE;
  gApp.currentSpdDump.crc32 = crc32_buf(gApp.currentSpdDump.data, SPD_NVM_SIZE);
  gApp.currentSpdDump.generation++;
  strlcpy(gApp.currentSpdDump.source, source ? source : "dump", sizeof(gApp.currentSpdDump.source));
}

bool dumpToBufferWithSource(uint8_t addr, const char* source) {
  const uint8_t chunk = 32;
  for (uint16_t off = 0; off < SPD_NVM_SIZE; off += chunk) {
    if (!spdNvmRead(addr, off, &gApp.lastDump[off], chunk)) {
      gApp.lastDumpValid = false;
      return false;
    }
    delay(1);
  }
  gApp.lastDumpValid = true;
  updateCurrentSpdDump(addr, source);
  return true;
}

bool dumpToBuffer(uint8_t addr) {
  return dumpToBufferWithSource(addr, "dump");
}

void printCurrentSpdDumpState() {
  outPrintln();
  outPrintln("CURRENT SPD DUMP");
  if (!gApp.currentSpdDump.valid) {
    outPrintln("  valid: no");
    outPrintf("  free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
    outPrintln("  No current 1024-byte SPD dump captured. Run dump <addr> first.");
    latchCommandResult(false);
    return;
  }
  outPrintln("  valid: yes");
  outPrintf("  addr: 0x%02X\n", gApp.currentSpdDump.addr);
  outPrintf("  len: %u\n", (unsigned)gApp.currentSpdDump.len);
  outPrintf("  crc32: 0x%08lX\n", (unsigned long)gApp.currentSpdDump.crc32);
  outPrintf("  generation: %lu\n", (unsigned long)gApp.currentSpdDump.generation);
  outPrintf("  source: %s\n", gApp.currentSpdDump.source);
  outPrintf("  free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
  latchCommandResult(true);
}

static bool compareDumpToGood(uint32_t& mismatches, uint16_t& firstOff, uint8_t& readB, uint8_t& expB) {
  mismatches = 0;
  firstOff = 0xFFFF;
  readB = expB = 0;

  if (!gApp.goodSpdValid || !gApp.lastDumpValid) return false;

  for (uint16_t i = 0; i < SPD_NVM_SIZE; i++) {
    uint8_t r = gApp.lastDump[i];
    uint8_t e = gApp.goodSpd[i];
    if (r != e) {
      mismatches++;
      if (firstOff == 0xFFFF) {
        firstOff = i;
        readB = r;
        expB = e;
      }
    }
  }
  return true;
}

static bool verifyStickAgainstGood(uint8_t addr) {
  if (!gApp.goodSpdValid) return false;

  uint8_t buf[32];
  for (uint16_t off = 0; off < SPD_NVM_SIZE; off += sizeof(buf)) {
    if (!spdNvmRead(addr, off, buf, sizeof(buf))) return false;
    for (uint16_t i = 0; i < sizeof(buf); i++) {
      if (buf[i] != gApp.goodSpd[off + i]) return false;
    }
  }
  return true;
}

static bool verifyStickAgainstBackup(uint8_t addr) {
  if (!gApp.spdBackupValid) return false;

  uint8_t buf[32];
  for (uint16_t off = 0; off < SPD_NVM_SIZE; off += sizeof(buf)) {
    if (!spdNvmRead(addr, off, buf, sizeof(buf))) return false;
    for (uint16_t i = 0; i < sizeof(buf); i++) {
      if (buf[i] != gApp.spdBackup[off + i]) return false;
    }
  }
  return true;
}

static const char* addrObservationLabel(uint8_t addr) {
  if (addr >= 0x50 && addr <= 0x57) return "observed SPD/HUB address";
  return "explicit address";
}

static void printSpdTweakWarning() {
  outPrintln("WARNING: SPD edits can make a DIMM fail to boot or train. A tweak checkpoint is required before writing. Continue only if this is a sacrificial/test module or you know how to restore the SPD with this tool.");
}

static void printBackupSummary() {
  outPrintf("  Tweak checkpoint=%s", gApp.spdBackupValid ? "PRESENT" : "MISSING");
  if (gApp.spdBackupValid) {
    outPrintf("  addr=0x%02X (%s) crc32=0x%08lX save_count=%lu boot=%lu label=\"%s\"\n",
              gApp.spdBackupAddr,
              addrObservationLabel(gApp.spdBackupAddr),
              (unsigned long)gApp.spdBackupCrc,
              (unsigned long)gApp.spdBackupSaveCount,
              (unsigned long)gApp.spdBackupBoot,
              gApp.spdBackupLabel);
  } else {
    outPrintln();
  }
}

static bool pwrGoodAllowsOperation(const char* opName) {
  if (gApp.hwConfig.pwrGood != HW_PGOOD_GPIO_READ) return true;
  if (pwrGoodReady()) return true;

  if (gApp.scanOK || gApp.readOK || gApp.dumpOK) {
    outPrintf("%s warning: PWR_GOOD reads LOW, but sideband reads are succeeding; verify PWR_GOOD wiring/config before treating this as a rail fault.\n",
              opName);
    return true;
  }

  outPrintf("%s blocked: PWR_GOOD GPIO34 measured LOW. Check wiring/readiness or set hardware config if PWR_GOOD is not authoritative.\n",
            opName);
  return false;
}

static void printObservedScanAddrs() {
  if (gApp.lastScanCount == 0) {
    outPrintln("    SPD/HUB address: not observed yet");
    return;
  }

  bool foundSpdHub = false;
  for (size_t i = 0; i < gApp.lastScanCount; i++) {
    uint8_t a = gApp.lastScanAddrs[i];
    if (a >= 0x50 && a <= 0x57) {
      if (!foundSpdHub) outPrint("    SPD/HUB address:");
      outPrintf(" 0x%02X", a);
      foundSpdHub = true;
    }
  }
  if (foundSpdHub) outPrintln();
  else outPrintln("    SPD/HUB address: not observed in last scan");

  outPrint("    all observed I2C addresses:");
  for (size_t i = 0; i < gApp.lastScanCount; i++) outPrintf(" 0x%02X", gApp.lastScanAddrs[i]);
  outPrintln();
}

static void printConfiguredHsaMatch() {
  uint8_t expected = 0;
  const char* desc = nullptr;
  if (!hwExpectedSpdAddr(expected, desc)) {
    outPrintln("    declared HSA config has no single tested-observed address.");
    outPrintln("    observed address is evidence only and does not prove mode.");
    return;
  }

  bool found = false;
  bool foundOther = false;
  uint8_t other = 0;
  for (size_t i = 0; i < gApp.lastScanCount; i++) {
    uint8_t a = gApp.lastScanAddrs[i];
    if (a == expected) found = true;
    else if (a >= 0x50 && a <= 0x57 && !foundOther) {
      foundOther = true;
      other = a;
    }
  }

  if (found) {
    outPrintf("    Observed address matches the address previously seen for this config on tested hardware: 0x%02X\n", expected);
  } else if (foundOther) {
    outPrintf("    Observed SPD/HUB address differs from the address previously associated with the declared HSA config on tested hardware: previous 0x%02X, observed 0x%02X\n",
              expected, other);
  } else {
    outPrintf("    Address previously seen for this declared HSA config on tested hardware was 0x%02X; it was not observed in the last scan.\n", expected);
  }
  outPrintln("    HSA strap interpretation comes from declared hardware config, not address alone.");
}

bool spdTweakWritesAllowed(const SpdTweakSafetyPlan& plan) {
  return plan.currentRead &&
         plan.automaticBackupSaved &&
         plan.backupVerified &&
         plan.diffPreviewReady &&
         plan.userConfirmed &&
         plan.fieldMapVerified &&
         plan.crcUpdaterVerified;
}

bool spdTweakFlowComplete(const SpdTweakSafetyPlan& plan) {
  return spdTweakWritesAllowed(plan) &&
         plan.patchWritten &&
         plan.rereadDone &&
         plan.postWriteVerified;
}

bool cmdPmicId(uint8_t addr) {
  setProcessing(true);
  outPrintf("PMICID: reading ID bytes from 0x%02X regs 0x3C/0x3D...\n", addr);

  uint8_t b[2] = {0};
  bool ok = i2cRegReadRetry(addr, 0x3C, b, 2, 6);
  setProcessing(false);

  if (!ok) {
    outPrintln("PMICID: read failed.");
    latchCommandResult(false);
    return false;
  }

  uint8_t bank = (uint8_t)(b[0] & 0x7F);
  uint8_t code = (uint8_t)(b[1] & 0x7F);
  outPrintf("PMICID raw: 0x%02X 0x%02X\n", b[0], b[1]);
  outPrintf("PMICID JEP106-ish: bank/cont=%u  code=0x%02X\n", (unsigned)bank, (unsigned)code);

  latchCommandResult(true);
  return true;
}

bool cmdPmicDump(uint8_t addr, uint8_t start, uint16_t len) {
  if (len == 0) len = 0x80;
  if (len > 256) len = 256;

  setProcessing(true);
  outPrintf("PMICDUMP: addr 0x%02X start 0x%02X len %u\n", addr, start, (unsigned)len);

  uint8_t buf[256];
  bool ok = i2cRegReadRetry(addr, start, buf, len, 6);
  setProcessing(false);

  if (!ok) {
    outPrintln("PMICDUMP: read failed.");
    latchCommandResult(false);
    return false;
  }

  for (uint16_t i = 0; i < len; i += 16) {
    uint16_t chunk = len - i;
    if (chunk > 16) chunk = 16;
    outPrintf("%02X: ", (unsigned)(start + i));
    for (uint16_t j = 0; j < chunk; j++) outPrintf("%02X ", buf[i + j]);
    outPrintln();
  }

  latchCommandResult(true);
  return true;
}

bool cmdRegRead(uint8_t addr, uint8_t reg, uint16_t n) {
  if (n == 0) n = 1;
  if (n > 256) n = 256;

  setProcessing(true);
  outPrintf("REG: addr 0x%02X reg 0x%02X len %u\n", addr, reg, (unsigned)n);

  uint8_t buf[256];
  bool ok = i2cRegReadRetry(addr, reg, buf, n, 6);
  setProcessing(false);

  if (!ok) {
    outPrintln("REG: read failed.");
    latchCommandResult(false);
    return false;
  }

  for (uint16_t i = 0; i < n; i += 16) {
    uint16_t chunk = n - i;
    if (chunk > 16) chunk = 16;
    outPrintf("%02X: ", (unsigned)(reg + i));
    for (uint16_t j = 0; j < chunk; j++) outPrintf("%02X ", buf[i + j]);
    outPrintln();
  }

  latchCommandResult(true);
  return true;
}

bool cmdEditReg(uint8_t addr, uint8_t reg, uint8_t value) {
  setProcessing(true);

  outPrintln("EDITREG: confirmed dangerous register write.");
  outPrintf("EDITREG: addr 0x%02X reg 0x%02X <= 0x%02X\n", addr, reg, value);

  uint8_t before = 0x00;
  bool okBefore = spdRegRead(addr, reg, before);
  if (okBefore) {
    outPrintf("  before: 0x%02X\n", before);
  } else {
    outPrintln("  before: read failed");
  }

  bool okWrite = spdRegWrite(addr, reg, value);
  if (!okWrite) {
    setProcessing(false);
    outPrintln("  write failed");
    latchCommandResult(false);
    return false;
  }

  uint8_t after = 0x00;
  bool okAfter = spdRegRead(addr, reg, after);

  setProcessing(false);

  if (!okAfter) {
    outPrintln("  readback failed");
    latchCommandResult(false);
    return false;
  }

  outPrintf("  after:  0x%02X\n", after);

  bool verified = (after == value);
  outPrintf("  verify: %s\n", verified ? "PASS" : "FAIL");

  latchCommandResult(verified);
  return verified;
}

void cmdStatus() {
  outPrintln();
  outPrintln("STATUS");

  outPrintln("  Declared HSA physical config:");
  if (gApp.hwConfig.hsa == HW_HSA_MANUAL_GROUND) {
    outPrintln("    manual_ground");
  } else if (gApp.hwConfig.hsa == HW_HSA_MANUAL_RESISTOR_STRAP) {
    outPrintln("    manual_resistor_strap");
  } else if (gApp.hwConfig.hsa == HW_HSA_MANUAL_FLOATING_HIGH) {
    outPrintln("    manual_floating_high");
  } else if (gApp.hwConfig.hsa == HW_HSA_GPIO_SWITCHABLE) {
    outPrintln("    gpio_switchable");
  } else {
    outPrintln("    unknown");
  }
  outPrintf("    interpretation: %s\n", hwDeclaredHsaInterpretation());
  outPrintln("    Observed address is evidence only and does not prove mode.");

  outPrintln();
  outPrintln("  Effective harness state:");
  if (gApp.hwConfig.vinBulk == HW_VIN_EXTERNAL_LOCKED_ON) {
    outPrintln("    VIN_BULK: external_locked_on -> assumed externally powered; GPIO32 not authoritative");
  } else if (gApp.hwConfig.vinBulk == HW_VIN_EXTERNAL_MANUAL_SWITCH) {
    outPrintln("    VIN_BULK: external_manual_switch -> controlled outside firmware; GPIO32 not authoritative");
  } else if (gApp.hwConfig.vinBulk == HW_VIN_GPIO_SWITCHABLE) {
    outPrintf("    VIN_BULK: gpio_switchable -> GPIO32 says %s\n", isDimmPowerOn() ? "ON" : "OFF");
  } else if (gApp.hwConfig.vinBulk == HW_VIN_UNKNOWN) {
    outPrintln("    VIN_BULK: unknown -> GPIO32 raw state is not authoritative");
  }

  if (gApp.hwConfig.pwrEn == HW_PWREN_PULLUP_ONLY) {
    outPrintln("    PWR_EN: pullup_only -> assumed enabled by external 10k pull-up; GPIO33 not authoritative and should be high-Z");
  } else if (gApp.hwConfig.pwrEn == HW_PWREN_GPIO_CONTROLLED) {
    outPrintf("    PWR_EN: gpio_controlled -> GPIO33 measured %s\n", readPwrEnLevel() ? "HIGH/RELEASED" : "LOW/PULLED LOW");
  } else if (gApp.hwConfig.pwrEn == HW_PWREN_NOT_CONNECTED) {
    outPrintln("    PWR_EN: not_connected -> firmware GPIO33 not authoritative");
  } else {
    outPrintln("    PWR_EN: unknown -> firmware GPIO33 not authoritative");
  }

  if (gApp.hwConfig.pwrGood == HW_PGOOD_GPIO_READ) {
    outPrintf("    PWR_GOOD: gpio_read -> GPIO34 measured %s\n", pwrGoodReady() ? "HIGH/READY" : "LOW");
    if (!pwrGoodReady() && (gApp.scanOK || gApp.readOK || gApp.dumpOK)) {
      outPrintln("      PWR_GOOD reads LOW, but sideband reads are succeeding; verify PWR_GOOD wiring/config before treating this as a rail fault.");
    } else if (!pwrGoodReady()) {
      outPrintln("      PWR_GOOD measured LOW; check rails or PWR_GOOD wiring/config.");
    }
  } else if (gApp.hwConfig.pwrGood == HW_PGOOD_PULLUP_ONLY) {
    outPrintln("    PWR_GOOD: pullup_only -> pulled up / not used as authoritative readiness input");
  } else {
    outPrintln("    PWR_GOOD: not measured by firmware");
  }

  outPrintf("    I2C interface: %s\n", hwI2cModeName(gApp.hwConfig.i2c));
  outPrintf("    SDA/SCL pullups: %s\n", hwPullupModeName(gApp.hwConfig.pullups));
  if (gApp.hwConfig.pullups == HW_PULLUPS_ESP32_INTERNAL_ONLY) {
    outPrintln("    note: external 10k SDA/SCL pull-ups may be needed if reads are flaky.");
  }

  outPrintln();
  outPrintln("  Runtime GPIO debug:");
  outPrintf("    GPIO33 raw: %s", readPwrEnLevel() ? "HIGH" : "LOW");
  if (gApp.hwConfig.pwrEn == HW_PWREN_PULLUP_ONLY) outPrint(", ignored because PWR_EN mode is pullup_only");
  else if (gApp.hwConfig.pwrEn != HW_PWREN_GPIO_CONTROLLED) outPrint(", not authoritative for configured PWR_EN mode");
  outPrintln();
  outPrintf("    GPIO34 raw: %s", pwrGoodReady() ? "HIGH" : "LOW");
  if (gApp.hwConfig.pwrGood != HW_PGOOD_GPIO_READ) outPrint(", not authoritative for configured PWR_GOOD mode");
  outPrintln();
  outPrintf("    GPIO32 raw: %s", isDimmPowerOn() ? "ON" : "OFF");
  if (gApp.hwConfig.vinBulk != HW_VIN_GPIO_SWITCHABLE) outPrintf(", ignored because VIN_BULK mode is %s", hwVinBulkModeName(gApp.hwConfig.vinBulk));
  outPrintln();
  outPrintf("    GPIO27 raw: %s", isHsaLow() ? "DRIVING GND/OFFLINE" : "RELEASED");
  if (gApp.hwConfig.hsa != HW_HSA_GPIO_SWITCHABLE) outPrintf(", ignored because HSA mode is %s", hwHsaModeName(gApp.hwConfig.hsa));
  outPrintln();

  outPrintln();
  outPrintln("  Observed bus:");
  outPrintf("    scanOK=%s readOK=%s dumpOK=%s\n",
            gApp.scanOK ? "yes" : "no",
            gApp.readOK ? "yes" : "no",
            gApp.dumpOK ? "yes" : "no");
  outPrintf("    lastScanCount=%u\n", (unsigned)gApp.lastScanCount);
  printObservedScanAddrs();
  printConfiguredHsaMatch();

  outPrintf("  Diagnostic SPD reference=%s", gApp.goodSpdValid ? "PRESENT" : "MISSING");
  if (gApp.goodSpdValid) outPrintf("  crc32=0x%08lX\n", (unsigned long)gApp.goodCrc);
  else outPrintln();
  printBackupSummary();
  outPrintf("  PMIC reference=%s", gApp.pmicRefValid ? "PRESENT" : "MISSING");
  if (gApp.pmicRefValid) {
    outPrintf("  addr=0x%02X start=0x%02X len=%u crc32=0x%08lX boot=%lu\n",
              gApp.pmicRefAddr,
              gApp.pmicRefStart,
              (unsigned)gApp.pmicRefLen,
              (unsigned long)gApp.pmicRefCrc,
              (unsigned long)gApp.pmicRefBoot);
  } else {
    outPrintln();
  }

  if (spdRefreshMR11(DEFAULT_SPD_ADDR)) {
    outPrintf("  MR11@0x%02X=0x%02X  (addrMode=%s, page=%u)\n",
              DEFAULT_SPD_ADDR,
              gApp.mr11,
              (gApp.mr11 & 0x08) ? "2-byte" : "1-byte",
              (unsigned)(gApp.mr11 & 0x07));
  } else {
    outPrintf("  MR11@0x%02X=?? (read failed)\n", DEFAULT_SPD_ADDR);
  }

  if (WIFI_ENABLE) {
    outPrintf("  WiFi AP: %s  pass: %s  ip: %s\n",
              AP_SSID, (strlen(AP_PASS) ? AP_PASS : "(open)"),
              WiFi.softAPIP().toString().c_str());
  }

  outPrintln();
  latchCommandResult(true);
}

bool cmdRead(uint8_t addr, uint16_t off, uint16_t n) {
  setProcessing(true);

  if (n == 0) n = 16;
  if (n > 256) n = 256;

  uint8_t buf[256];
  outPrintf("Reading %u bytes (NVM) from 0x%02X off 0x%04X...\n", n, addr, off);

  bool ok = spdNvmRead(addr, off, buf, n);
  setProcessing(false);

  if (!ok) {
    outPrintln("Read failed.");
    latchCommandResult(false);
    return false;
  }

  outPrint("Data: ");
  for (uint16_t i = 0; i < n; i++) outPrintf("%02X ", buf[i]);
  outPrintln();

  gApp.readOK = true;
  latchCommandResult(true);
  return true;
}

bool cmdDump(uint8_t addr, const char* source) {
  setProcessing(true);
  outPrintf("Dumping 1024 bytes (NVM) from 0x%02X...\n", addr);

  bool ok = dumpToBufferWithSource(addr, source ? source : "dump");

  if (!ok) {
    setProcessing(false);
    outPrintln("Dump failed.");
    latchCommandResult(false);
    return false;
  }

  for (uint16_t off = 0; off < SPD_NVM_SIZE; off += 16) {
    outPrintf("%04X: ", off);
    for (int i = 0; i < 16; i++) outPrintf("%02X ", gApp.lastDump[off + i]);
    outPrintln();
    delay(2);
  }

  setProcessing(false);
  outPrintln("Done.");

  gApp.dumpOK = true;
  latchCommandResult(true);
  return true;
}

bool cmdCompare(uint8_t addr) {
  if (!gApp.goodSpdValid) {
    outPrintln("COMPARE blocked: no diagnostic SPD reference stored. Use: capturegood");
    latchCommandResult(false);
    return false;
  }
  if (!pwrGoodAllowsOperation("COMPARE")) {
    latchCommandResult(false);
    return false;
  }

  setProcessing(true);
  outPrintf("COMPARE: dumping current SPD from 0x%02X...\n", addr);
  bool ok = dumpToBufferWithSource(addr, "compare");
  setProcessing(false);

  if (!ok) {
    outPrintln("COMPARE: dump failed.");
    latchCommandResult(false);
    return false;
  }

  gApp.dumpOK = true;
  gApp.readOK = true;

  uint32_t mism = 0;
  uint16_t firstOff;
  uint8_t rb, eb;
  compareDumpToGood(mism, firstOff, rb, eb);

  if (mism == 0) {
    outPrintln("COMPARE: MATCH");
    latchCommandResult(true);
    return true;
  }

  outPrintf("COMPARE: mismatches=%lu  first=0x%04X read=%02X exp=%02X\n",
            (unsigned long)mism, (unsigned)firstOff, rb, eb);
  latchCommandResult(false);
  return false;
}

bool cmdCaptureGood(uint8_t addr) {
  if (!pwrGoodAllowsOperation("CAPTUREGOOD")) {
    latchCommandResult(false);
    return false;
  }

  setProcessing(true);
  outPrintf("CAPTUREGOOD: dumping current SPD from 0x%02X and saving diagnostic reference to flash...\n", addr);
  bool ok = dumpToBufferWithSource(addr, "capturegood");
  setProcessing(false);

  if (!ok) {
    outPrintln("CAPTUREGOOD: dump failed.");
    latchCommandResult(false);
    return false;
  }

  ok = saveGoodToNvs(gApp.lastDump);
  if (!ok) {
    outPrintln("CAPTUREGOOD: NVS save failed.");
    latchCommandResult(false);
    return false;
  }

  outPrintf("CAPTUREGOOD: saved  crc32=0x%08lX\n", (unsigned long)gApp.goodCrc);
  latchCommandResult(true);
  return true;
}

bool cmdBackupSpd(uint8_t addr) {
  if (gApp.hwConfig.pwrGood == HW_PGOOD_GPIO_READ && !pwrGoodReady()) {
    outPrintln("BACKUPSPD warning: PWR_GOOD reads LOW; verify PWR_GOOD wiring/config if this conflicts with successful sideband reads.");
  }

  setProcessing(true);
  outPrintf("BACKUPSPD: dumping current SPD from 0x%02X (%s) and saving tweak checkpoint to flash...\n",
            addr, addrObservationLabel(addr));
  bool ok = dumpToBufferWithSource(addr, "backupspd");
  setProcessing(false);

  if (!ok) {
    outPrintln("BACKUPSPD: dump failed.");
    latchCommandResult(false);
    return false;
  }

  ok = saveSpdBackupToNvs(addr, gApp.lastDump, gBootCount, "tweak checkpoint");
  if (!ok) {
    outPrintln("BACKUPSPD: NVS save failed.");
    latchCommandResult(false);
    return false;
  }

  ok = loadSpdBackupFromNvs() && verifyLoadedSpdBackupCrc();
  outPrintf("BACKUPSPD: verify stored backup CRC: %s  crc32=0x%08lX  save_count=%lu\n",
            ok ? "PASS" : "FAIL",
            (unsigned long)gApp.spdBackupCrc,
            (unsigned long)gApp.spdBackupSaveCount);

  latchCommandResult(ok);
  return ok;
}

bool cmdBackupInfo() {
  outPrintln();
  outPrintln("SPD TWEAK CHECKPOINT");
  outPrintln("  Diagnostic reference: capturegood comparison/reference slot (separate)");
  outPrintln("  Tweak checkpoint: edit rollback target used by Advanced SPD Editing");
  printBackupSummary();
  if (gApp.spdBackupValid) {
    bool crcOk = verifyLoadedSpdBackupCrc();
    outPrintf("  RAM CRC check=%s\n", crcOk ? "PASS" : "FAIL");
  }
  outPrintln();
  latchCommandResult(true);
  return true;
}

bool cmdClearBackup() {
  outPrintln("CLEARBACKUP: clearing only the tweak checkpoint slot.");
  outPrintln("CLEARBACKUP: diagnostic reference is not changed.");
  clearSpdBackupNvs();
  outPrintln("CLEARBACKUP: tweak checkpoint erased.");
  latchCommandResult(true);
  return true;
}

bool cmdRestoreBackup(uint8_t addr, bool confirmed) {
  if (!gApp.spdBackupValid) {
    outPrintln("RESTOREBACKUP blocked: no tweak checkpoint stored. Use: backupspd");
    latchCommandResult(false);
    return false;
  }
  if (!verifyLoadedSpdBackupCrc()) {
    outPrintln("RESTOREBACKUP blocked: stored backup CRC check failed in RAM.");
    latchCommandResult(false);
    return false;
  }
  if (!confirmed) {
    outPrintln("RESTOREBACKUP blocked: this overwrites current SPD contents.");
    outPrintln("Usage: restorebackup <addr> YES_RESTORE_SPD_BACKUP");
    outPrintln("   or: restorelast [addr] YES_RESTORE_SPD_BACKUP");
    latchCommandResult(false);
    return false;
  }
  if (gApp.hwConfig.pwrGood == HW_PGOOD_GPIO_READ && !pwrGoodReady()) {
    outPrintln("RESTOREBACKUP warning: PWR_GOOD reads LOW; verify PWR_GOOD wiring/config if this conflicts with successful sideband reads.");
  }

  outPrintln("RESTOREBACKUP: confirmed. This will not change the diagnostic reference.");

  if (!unlockAllNvmBlocks(addr)) {
    outPrintln("RESTOREBACKUP blocked: could not unlock NVM blocks.");
    latchCommandResult(false);
    return false;
  }

  setProcessing(true);
  outPrintf("RESTOREBACKUP: writing saved 1024-byte tweak checkpoint to 0x%02X (%s) in 16-byte chunks...\n",
            addr, addrObservationLabel(addr));

  for (uint16_t off = 0; off < SPD_NVM_SIZE; off += 16) {
    if (!spdNvmWrite(addr, off, &gApp.spdBackup[off], 16)) {
      setProcessing(false);
      outPrintf("RESTOREBACKUP failed at offset 0x%04X\n", off);
      latchCommandResult(false);
      return false;
    }
    if ((off % 64) == 0) outPrintf("  wrote through 0x%04X\n", (unsigned)(off + 15));
  }

  spdPollReady(addr, 600);
  delay(25);

  setProcessing(false);
  outPrintln("RESTOREBACKUP: write pass complete. Re-reading and verifying...");

  bool ok = verifyStickAgainstBackup(addr);
  outPrintf("RESTOREBACKUP: verify re-read image matches backup: %s\n", ok ? "PASS" : "FAIL");

  latchCommandResult(ok);
  return ok;
}

static uint8_t defaultSpdTweakAddr() {
  uint8_t expectedAddr = 0;
  const char* desc = nullptr;
  if (hwExpectedSpdAddr(expectedAddr, desc)) return expectedAddr;
  return isHsaLow() ? 0x50 : 0x53;
}

static bool parseAddrToken(const char* s, uint8_t& addr) {
  if (!s || !*s) return false;
  char* end = nullptr;
  unsigned long v = strtoul(s, &end, 0);
  if (!end || *end != 0 || v > 0x7F) return false;
  addr = (uint8_t)v;
  return true;
}

static bool writePatchedSpd(uint8_t addr, const uint8_t* patched) {
  if (!patched) return false;
  if (!unlockAllNvmBlocks(addr)) {
    outPrintln("SPDTWEAK blocked: could not unlock NVM blocks.");
    return false;
  }

  setProcessing(true);
  outPrintf("SPDTWEAK: writing patched SPD to 0x%02X in 16-byte chunks...\n", addr);
  for (uint16_t off = 0; off < SPD_NVM_SIZE; off += 16) {
    if (!spdNvmWrite(addr, off, &patched[off], 16)) {
      setProcessing(false);
      outPrintf("SPDTWEAK failed at offset 0x%04X\n", off);
      return false;
    }
    if ((off % 64) == 0) outPrintf("  wrote through 0x%04X\n", (unsigned)(off + 15));
  }
  spdPollReady(addr, 600);
  delay(25);
  setProcessing(false);
  return true;
}

bool cmdSpdTweak(int nt, char* tok[]) {
  String subcmd = (nt >= 2) ? String(tok[1]) : String("status");
  subcmd.toLowerCase();
  if (subcmd.length() == 0) subcmd = "status";

  printSpdTweakWarning();
  outPrintln();
  outPrintln("ADVANCED SPD EDITING");
  printBackupSummary();
  uint8_t currentAddr = defaultSpdTweakAddr();
  uint8_t argStart = 2;
  if (nt >= 3 && parseAddrToken(tok[2], currentAddr)) argStart = 3;
  outPrintf("  Observed/resolved SPD/HUB address: 0x%02X (%s)\n",
            currentAddr, addrObservationLabel(currentAddr));
  outPrintf("  Declared HSA physical config: %s\n", hwHsaModeName(gApp.hwConfig.hsa));
  outPrintf("  GPIO27 raw: %s%s\n",
            isHsaLow() ? "DRIVING GND/OFFLINE" : "RELEASED",
            gApp.hwConfig.hsa == HW_HSA_GPIO_SWITCHABLE ? "" : " (not authoritative for declared HSA config)");
  outPrintln("  Address note: observed address selects where reads/writes are attempted; it does not prove HSA mode.");

  if (subcmd == "help") {
    outPrintln();
    outPrintln("Commands:");
    outPrintln("  backupspd [addr]                 = save current SPD as tweak checkpoint");
    outPrintln("  backupinfo                       = show tweak checkpoint metadata");
    outPrintln("  restoreinfo                      = show edit rollback checkpoint metadata");
    outPrintln("  Restore:");
    outPrintln("    restorelast [addr] YES_RESTORE_SPD_BACKUP");
    outPrintln("    restorebackup <addr> YES_RESTORE_SPD_BACKUP");
    outPrintln("    restore writes SPD, re-reads, verifies, and does not change diagnostic reference");
    outPrintln("  clearbackup                      = clear only the tweak checkpoint slot");
    outPrintln("  spdtweak status                  = compact decode current 1024-byte SPD dump");
    outPrintln("  spdtweak decode [summary|base|expo|xmp|all] = staged crash-safe decode");
    outPrintln("  spdtweak selftest                = run compact decoder safety checks");
    outPrintln("  spdtweak preview [addr] field=value ... = preview supported byte changes");
    outPrintln("  spdtweak apply [addr] field=value ... YES_WRITE_SPD_TWEAK = backup, write, re-read, verify");
    outPrintln("  Note: speed/timing/profile writes are blocked until verified DDR5 field map and CRC updater exist.");
    outPrintln("  Validation flow: test edits on sacrificial module; confirm CRC/checksum PASS; confirm restorebackup works;");
    outPrintln("                   only then test on good module. Never modify JEDEC/base defaults.");
    outPrintln("                   Save diagnostic reference and tweak checkpoint before any write.");
    latchCommandResult(true);
    return true;
  }

  if (subcmd != "status" && subcmd != "decode" && subcmd != "selftest" && subcmd != "preview" && subcmd != "apply") {
    outPrintf("SPDTWEAK: unknown subcommand '%s'. Use: spdtweak help\n", subcmd.c_str());
    latchCommandResult(false);
    return false;
  }

  if (subcmd == "selftest") {
    outPrintln("SPDTWEAK SELFTEST: enter");
    outPrintf("  free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
    outPrintf("  minimum free heap: %lu\n", (unsigned long)ESP.getMinFreeHeap());
    outPrintf("  max alloc heap: %lu\n", (unsigned long)ESP.getMaxAllocHeap());
    delay(1);
    bool ok = spdTweakSelfTest();
    outPrintf("  bounds checks: %s\n", ok ? "PASS" : "FAIL");
    outPrintf("  tCK zero handling: %s\n", ok ? "PASS" : "FAIL");
    outPrintf("  missing EXPO signature: %s\n", ok ? "PASS" : "FAIL");
    outPrintf("  missing XMP profile: %s\n", ok ? "PASS" : "FAIL");
    if (gApp.currentSpdDump.valid && gApp.currentSpdDump.len == SPD_NVM_SIZE) {
      SpdCompactDecodeSummary current;
      bool currentOk = spdTweakDecodeCompactSummary(gApp.currentSpdDump.data, gApp.currentSpdDump.len, current);
      outPrintf("  current dump decode: %s\n", currentOk ? "PASS" : "FAIL");
    } else {
      outPrintln("  current dump decode: SKIP (no current 1024-byte SPD dump)");
    }
    outPrintf("  free heap after: %lu\n", (unsigned long)ESP.getFreeHeap());
    outPrintln("SPDTWEAK SELFTEST: done");
    latchCommandResult(ok);
    return ok;
  }

  if (subcmd == "status" || subcmd == "decode") {
    String stage = "summary";
    if (subcmd == "decode" && nt >= 3) {
      stage = tok[2];
      stage.toLowerCase();
      if (stage.length() == 0) stage = "summary";
    }
    if (stage != "summary" && stage != "base" && stage != "expo" && stage != "xmp" && stage != "all") {
      outPrintf("SPDTWEAK: unknown decode section '%s'. Use summary|base|expo|xmp|all.\n", stage.c_str());
      latchCommandResult(false);
      return false;
    }
    outPrintln("SPDTWEAK DECODE: enter");
    outPrintf("  free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
    outPrintf("  minimum free heap: %lu\n", (unsigned long)ESP.getMinFreeHeap());
    outPrintf("  max alloc heap: %lu\n", (unsigned long)ESP.getMaxAllocHeap());
    outPrintf("  current dump valid: %u\n", gApp.currentSpdDump.valid ? 1 : 0);
    outPrintf("  current dump len: %u\n", (unsigned)gApp.currentSpdDump.len);
    outPrintf("  current dump crc: 0x%08lX\n", (unsigned long)gApp.currentSpdDump.crc32);
    outPrintf("  source: %s\n", gApp.currentSpdDump.source);
    delay(1);
    if (!gApp.currentSpdDump.valid || gApp.currentSpdDump.len != SPD_NVM_SIZE) {
      outPrintln("  no current 1024-byte SPD dump captured. Run dump <addr> first.");
      outPrintf("  free heap after: %lu\n", (unsigned long)ESP.getFreeHeap());
      outPrintf("  minimum free heap after: %lu\n", (unsigned long)ESP.getMinFreeHeap());
      outPrintf("  max alloc heap after: %lu\n", (unsigned long)ESP.getMaxAllocHeap());
      outPrintln("SPDTWEAK DECODE: done");
      latchCommandResult(false);
      return false;
    }
    currentAddr = gApp.currentSpdDump.addr;
    outPrintf("  decoding current SPD dump addr=0x%02X crc32=0x%08lX source=%s generation=%lu\n",
              currentAddr,
              (unsigned long)gApp.currentSpdDump.crc32,
              gApp.currentSpdDump.source,
              (unsigned long)gApp.currentSpdDump.generation);
    delay(1);
    bool ok = true;
    if (stage == "summary" || stage == "all") {
      SpdCompactDecodeSummary decoded;
      bool sectionOk = spdTweakDecodeCompactSummary(gApp.currentSpdDump.data, gApp.currentSpdDump.len, decoded);
      if (sectionOk) spdTweakPrintCompactSummary(decoded);
      else outPrintf("  compact decode failed: %s\n", decoded.base.error[0] ? decoded.base.error : "unknown error");
      ok = ok && sectionOk;
      delay(1);
    }
    if (stage == "base" || stage == "all") {
      TimingSection section;
      bool sectionOk = spdTweakDecodeBaseSection(gApp.currentSpdDump.data, gApp.currentSpdDump.len, section);
      spdTweakPrintTimingSection(section);
      ok = ok && sectionOk;
      delay(1);
    }
    if (stage == "expo" || stage == "all") {
      TimingSection section;
      bool sectionOk = spdTweakDecodeExpoSection(gApp.currentSpdDump.data, gApp.currentSpdDump.len, section);
      spdTweakPrintTimingSection(section);
      ok = ok && sectionOk;
      delay(1);
    }
    if (stage == "xmp" || stage == "all") {
      TimingSection section;
      bool sectionOk = spdTweakDecodeXmpSection(gApp.currentSpdDump.data, gApp.currentSpdDump.len, section);
      spdTweakPrintTimingSection(section);
      ok = ok && sectionOk;
      delay(1);
    }
    delay(1);
    outPrintf("  free heap after: %lu\n", (unsigned long)ESP.getFreeHeap());
    outPrintf("  minimum free heap after: %lu\n", (unsigned long)ESP.getMinFreeHeap());
    outPrintf("  max alloc heap after: %lu\n", (unsigned long)ESP.getMaxAllocHeap());
    outPrintln("SPDTWEAK DECODE: done");
    outPrintln("  Write status: unchanged; staged decode is read-only in this path.");
    latchCommandResult(ok);
    return ok;
  } else {
    setProcessing(true);
    outPrintf("SPDTWEAK: dumping current SPD from 0x%02X...\n", currentAddr);
    bool ok = dumpToBufferWithSource(currentAddr, "spdtweak");
    setProcessing(false);
    if (!ok) {
      outPrintln("SPDTWEAK: SPD dump failed.");
      latchCommandResult(false);
      return false;
    }
  }

  bool confirmed = false;
  char* assignments[16] = {0};
  int assignmentCount = 0;
  for (int i = argStart; i < nt; i++) {
    if (strcmp(tok[i], SPD_TWEAK_WRITE_TOKEN) == 0) {
      confirmed = true;
      continue;
    }
    if (strchr(tok[i], '=') && assignmentCount < 16) assignments[assignmentCount++] = tok[i];
  }

  SpdTweakPatch patch;
  bool ok = spdTweakBuildPatch(gApp.currentSpdDump.data, assignmentCount, assignments, patch);
  outPrintf("SPDTWEAK: %s\n", patch.message);
  spdTweakPrintDiff(patch);

  if (subcmd == "preview") {
    latchCommandResult(ok);
    return ok;
  }

  if (!confirmed) {
    outPrintln("SPDTWEAK apply blocked: append YES_WRITE_SPD_TWEAK to confirm.");
    latchCommandResult(false);
    return false;
  }
  if (!patch.ok || !patch.hasChanges) {
    outPrintln("SPDTWEAK apply blocked: no supported byte changes are available.");
    latchCommandResult(false);
    return false;
  }
  if (!patch.crcRepairSupported) {
    outPrintln("SPDTWEAK apply blocked: no verified DDR5 SPD profile field map / CRC updater is implemented yet.");
    latchCommandResult(false);
    return false;
  }

  outPrintln("SPDTWEAK: saving automatic tweak checkpoint...");
  ok = saveSpdBackupToNvs(currentAddr, gApp.lastDump, gBootCount, "tweak checkpoint");
  ok = ok && loadSpdBackupFromNvs() && verifyLoadedSpdBackupCrc();
  outPrintf("SPDTWEAK: backup verify: %s\n", ok ? "PASS" : "FAIL");
  if (!ok) {
    outPrintln("SPDTWEAK apply blocked: tweak checkpoint save/verify failed.");
    latchCommandResult(false);
    return false;
  }

  ok = writePatchedSpd(currentAddr, patch.patched);
  if (!ok) {
    latchCommandResult(false);
    return false;
  }

  outPrintln("SPDTWEAK: write complete. Re-reading and verifying patched image...");
  ok = spdTweakVerifyImage(currentAddr, patch.patched);
  outPrintf("SPDTWEAK: verify re-read image matches patched image: %s\n", ok ? "PASS" : "FAIL");
  latchCommandResult(ok);
  return ok;
}

bool cmdClearGood() {
  clearGoodNvs();
  outPrintln("CLEARGOOD: erased diagnostic SPD reference from flash");
  latchCommandResult(true);
  return true;
}

bool cmdVerifyGood(uint8_t addr) {
  if (!gApp.goodSpdValid) {
    outPrintln("VERIFYGOOD blocked: no diagnostic SPD reference stored.");
    latchCommandResult(false);
    return false;
  }
  if (!pwrGoodAllowsOperation("VERIFYGOOD")) {
    latchCommandResult(false);
    return false;
  }

  setProcessing(true);
  outPrintf("VERIFYGOOD: legacy compatibility check of 0x%02X against diagnostic SPD reference...\n", addr);
  bool ok = verifyStickAgainstGood(addr);
  setProcessing(false);

  if (ok) outPrintln("VERIFYGOOD: MATCH");
  else outPrintln("VERIFYGOOD: MISMATCH");

  latchCommandResult(ok);
  return ok;
}

bool cmdWriteGood(uint8_t addr, bool confirmed) {
  if (!confirmed) {
    outPrintln("WRITEGOOD: blocked. You must confirm: writegood yes");
    latchCommandResult(false);
    return false;
  }
  if (!gApp.goodSpdValid) {
    outPrintln("WRITEGOOD blocked: no diagnostic SPD reference stored. Use: capturegood");
    latchCommandResult(false);
    return false;
  }
  if (!pwrGoodAllowsOperation("WRITEGOOD")) {
    latchCommandResult(false);
    return false;
  }

  if (!unlockAllNvmBlocks(addr)) {
    outPrintln("WRITEGOOD blocked: could not unlock NVM blocks.");
    latchCommandResult(false);
    return false;
  }

  setProcessing(true);
  outPrintf("WRITEGOOD: writing diagnostic SPD reference to 0x%02X in 16-byte chunks...\n", addr);

  for (uint16_t off = 0; off < SPD_NVM_SIZE; off += 16) {
    if (!spdNvmWrite(addr, off, &gApp.goodSpd[off], 16)) {
      setProcessing(false);
      outPrintf("WRITEGOOD failed at offset 0x%04X\n", off);
      latchCommandResult(false);
      return false;
    }
    if ((off % 64) == 0) outPrintf("  wrote through 0x%04X\n", (unsigned)(off + 15));
  }

  spdPollReady(addr, 600);
  delay(25);

  setProcessing(false);
  outPrintln("WRITEGOOD: write pass complete. Verifying...");

  bool ok = verifyStickAgainstGood(addr);
  if (ok) outPrintln("WRITEGOOD: VERIFIED");
  else outPrintln("WRITEGOOD: VERIFY FAILED");

  latchCommandResult(ok);
  return ok;
}

bool cmdAutoFix(uint8_t addr, bool confirmed) {
  (void)addr;
  (void)confirmed;
  outPrintln("AutoFix is disabled in this build. Use explicit read/compare/write workflows.");
  latchCommandResult(false);
  return false;
}

void cmdFlashInfo() {
  uint32_t chip = ESP.getFlashChipSize();
  uint32_t sketch = ESP.getSketchSize();
  uint32_t freeSketch = ESP.getFreeSketchSpace();

  outPrintln();
  outPrintln("FLASH");
  outPrintf("  Flash chip size: %lu bytes (%.2f MB)\n", (unsigned long)chip, chip / (1024.0f * 1024.0f));
  outPrintf("  Sketch size:     %lu bytes\n", (unsigned long)sketch);
  outPrintf("  Free sketch spc: %lu bytes\n", (unsigned long)freeSketch);
  outPrintf("  Heap free:       %lu bytes\n", (unsigned long)ESP.getFreeHeap());
  outPrintln();

  latchCommandResult(true);
}
