#include "PmicRef.h"

#include "AppConfig.h"
#include "AppState.h"
#include "GoodSpdStore.h"
#include "I2cHelpers.h"
#include "Log.h"

static bool clampPmicRange(uint16_t& len) {
  if (len == 0) len = 0x80;
  if (len > PMIC_REF_MAX_LEN) {
    outPrintf("PMIC reference len capped at 0x%02X bytes.\n", PMIC_REF_MAX_LEN);
    len = PMIC_REF_MAX_LEN;
  }
  return len > 0;
}

uint8_t defaultPmicRefAddr() {
  for (size_t i = 0; i < gApp.roleRecordCount; i++) {
    if (gApp.roleRecords[i].role == ROLE_PMIC) return gApp.roleRecords[i].address;
  }
  for (size_t i = 0; i < gApp.lastScanCount; i++) {
    if (gApp.lastScanAddrs[i] == 0x4B) return 0x4B;
  }
  return DEFAULT_PMIC_ADDR;
}

static const char* pmicDiffNote(uint8_t reg) {
  if (reg <= 0x0F) return "status/volatile?";
  if (reg >= 0x30 && reg <= 0x3F) return "id/status?";
  return "";
}

static void printPmicRefMetadata(const char* prefix) {
  outPrintf("%s addr=0x%02X start=0x%02X len=%u crc32=0x%08lX boot=%lu\n",
            prefix,
            gApp.pmicRefAddr,
            gApp.pmicRefStart,
            (unsigned)gApp.pmicRefLen,
            (unsigned long)gApp.pmicRefCrc,
            (unsigned long)gApp.pmicRefBoot);
}

void cmdPmicRef() {
  outPrintln();
  outPrintln("PMIC REFERENCE");
  if (!gApp.pmicRefValid) {
    outPrintln("  status: MISSING");
  } else {
    outPrintln("  status: PRESENT");
    printPmicRefMetadata("  reference:");
  }
  outPrintln();
  latchCommandResult(true);
}

bool cmdCapturePmic(uint8_t addr, uint8_t start, uint16_t len) {
  if (!clampPmicRange(len)) {
    outPrintln("CAPTUREPMIC blocked: invalid len.");
    latchCommandResult(false);
    return false;
  }

  uint8_t buf[PMIC_REF_MAX_LEN] = {0};
  outPrintf("CAPTUREPMIC: reading PMIC regs addr=0x%02X start=0x%02X len=%u...\n",
            addr, start, (unsigned)len);

  bool ok = i2cRegReadRetry(addr, start, buf, len, 6);
  if (!ok) {
    outPrintln("CAPTUREPMIC: read failed. No PMIC reference was saved.");
    latchCommandResult(false);
    return false;
  }

  ok = savePmicRefToNvs(addr, start, len, buf, gBootCount);
  if (!ok) {
    outPrintln("CAPTUREPMIC: NVS save failed.");
    latchCommandResult(false);
    return false;
  }

  outPrintf("CAPTUREPMIC: saved PMIC reference crc32=0x%08lX\n", (unsigned long)gApp.pmicRefCrc);
  latchCommandResult(true);
  return true;
}

bool cmdComparePmic(uint8_t addr, uint8_t start, uint16_t len) {
  if (!gApp.pmicRefValid) {
    outPrintln("COMPAREPMIC blocked: no PMIC reference stored. Use: capturepmic");
    latchCommandResult(false);
    return false;
  }

  if (len == 0) len = gApp.pmicRefLen;
  if (!clampPmicRange(len)) {
    outPrintln("COMPAREPMIC blocked: invalid len.");
    latchCommandResult(false);
    return false;
  }

  if (len != gApp.pmicRefLen || start != gApp.pmicRefStart) {
    outPrintln("COMPAREPMIC blocked: requested range does not match stored PMIC reference.");
    printPmicRefMetadata("  reference:");
    latchCommandResult(false);
    return false;
  }

  uint8_t cur[PMIC_REF_MAX_LEN] = {0};
  outPrintf("COMPAREPMIC: reading PMIC regs addr=0x%02X start=0x%02X len=%u...\n",
            addr, start, (unsigned)len);

  bool ok = i2cRegReadRetry(addr, start, cur, len, 6);
  if (!ok) {
    outPrintln("COMPAREPMIC: read failed.");
    latchCommandResult(false);
    return false;
  }

  uint32_t curCrc = crc32_buf(cur, len);
  uint16_t diffs = 0;
  for (uint16_t i = 0; i < len; i++) {
    if (cur[i] != gApp.pmicRef[i]) diffs++;
  }

  outPrintln();
  outPrintln("PMIC COMPARE");
  outPrintf("  result: %s\n", diffs == 0 ? "MATCH" : "DIFFERENT");
  printPmicRefMetadata("  reference:");
  outPrintf("  current:   addr=0x%02X start=0x%02X len=%u crc32=0x%08lX\n",
            addr, start, (unsigned)len, (unsigned long)curCrc);
  outPrintf("  differing bytes: %u\n", diffs);

  if (diffs > 0) {
    outPrintln("  reg | reference | current | note");
    for (uint16_t i = 0; i < len; i++) {
      if (cur[i] == gApp.pmicRef[i]) continue;
      uint8_t reg = (uint8_t)(start + i);
      outPrintf("  0x%02X |   0x%02X    |  0x%02X   | %s\n",
                reg, gApp.pmicRef[i], cur[i], pmicDiffNote(reg));
    }
  }

  outPrintln();
  latchCommandResult(diffs == 0);
  return diffs == 0;
}

bool cmdClearPmic() {
  clearPmicRefNvs();
  outPrintln("CLEARPMIC: erased PMIC reference from flash.");
  latchCommandResult(true);
  return true;
}
