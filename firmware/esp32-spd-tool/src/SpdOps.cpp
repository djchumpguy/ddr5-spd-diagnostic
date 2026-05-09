#include "SpdOps.h"
#include "AppConfig.h"
#include "AppState.h"
#include "BoardControl.h"
#include "GoodSpdStore.h"
#include "I2cHelpers.h"
#include "SpdHub.h"
#include "Log.h"
#include <WiFi.h>
#include <Wire.h>

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

bool dumpToBuffer(uint8_t addr) {
  const uint8_t chunk = 32;
  for (uint16_t off = 0; off < SPD_NVM_SIZE; off += chunk) {
    if (!spdNvmRead(addr, off, &gApp.lastDump[off], chunk)) {
      gApp.lastDumpValid = false;
      return false;
    }
  }
  gApp.lastDumpValid = true;
  return true;
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
  outPrintf("  PWR_EN / PMIC VR enable (GPIO33): %s (read=%s)\n",
            readPwrEnLevel() ? "ON/RELEASED" : "OFF/PULLED LOW",
            readPwrEnLevel() ? "HIGH" : "LOW");
  outPrintf("  PWR_GOOD (GPIO34): %s\n", pwrGoodReady() ? "READY" : "LOW - check wiring/readiness before trusting reads");
  outPrintf("  VIN_BULK switch (GPIO32): %s\n", isDimmPowerOn() ? "ON" : "OFF");
  outPrintf("  HSA GPIO (GPIO27): %s\n", isHsaLow() ? "DRIVING GND/OFFLINE" : "RELEASED");
  outPrintln("  HSA note: GPIO27 control is experimental; released GPIO lets the external strap decide.");
  outPrintln("  Auto-detect determines the active SPD/HUB address. Cold-cycle VIN_BULK after HSA changes.");
  outPrintf("  scanOK=%s readOK=%s dumpOK=%s\n",
            gApp.scanOK ? "yes" : "no",
            gApp.readOK ? "yes" : "no",
            gApp.dumpOK ? "yes" : "no");
  outPrintf("  Known-good SPD reference=%s", gApp.goodSpdValid ? "PRESENT" : "MISSING");
  if (gApp.goodSpdValid) outPrintf("  crc32=0x%08lX\n", (unsigned long)gApp.goodCrc);
  else outPrintln();
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

  outPrintf("  lastScanCount=%u\n", (unsigned)gApp.lastScanCount);
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

bool cmdDump(uint8_t addr) {
  setProcessing(true);
  outPrintf("Dumping 1024 bytes (NVM) from 0x%02X...\n", addr);

  bool ok = dumpToBuffer(addr);

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
    outPrintln("COMPARE blocked: no known-good SPD reference stored. Use: capturegood");
    latchCommandResult(false);
    return false;
  }
  if (!pwrGoodReady()) {
    outPrintln("COMPARE blocked: PWR_GOOD LOW. Check wiring/readiness before trusting SPD/PMIC reads.");
    latchCommandResult(false);
    return false;
  }

  setProcessing(true);
  outPrintf("COMPARE: dumping current SPD from 0x%02X...\n", addr);
  bool ok = dumpToBuffer(addr);
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
  if (!pwrGoodReady()) {
    outPrintln("CAPTUREGOOD blocked: PWR_GOOD LOW. Check wiring/readiness before trusting SPD/PMIC reads.");
    latchCommandResult(false);
    return false;
  }

  setProcessing(true);
  outPrintf("CAPTUREGOOD: dumping current SPD from 0x%02X and saving known-good reference to flash...\n", addr);
  bool ok = dumpToBuffer(addr);
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

bool cmdClearGood() {
  clearGoodNvs();
  outPrintln("CLEARGOOD: erased known-good SPD reference from flash");
  latchCommandResult(true);
  return true;
}

bool cmdVerifyGood(uint8_t addr) {
  if (!gApp.goodSpdValid) {
    outPrintln("VERIFYGOOD blocked: no known-good SPD reference stored.");
    latchCommandResult(false);
    return false;
  }
  if (!pwrGoodReady()) {
    outPrintln("VERIFYGOOD blocked: PWR_GOOD LOW. Check wiring/readiness before trusting SPD/PMIC reads.");
    latchCommandResult(false);
    return false;
  }

  setProcessing(true);
  outPrintf("VERIFYGOOD: verifying 0x%02X vs known-good SPD reference...\n", addr);
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
    outPrintln("WRITEGOOD blocked: no known-good SPD reference stored. Use: capturegood");
    latchCommandResult(false);
    return false;
  }
  if (!pwrGoodReady()) {
    outPrintln("WRITEGOOD blocked: PWR_GOOD LOW. Check wiring/readiness before trusting SPD/PMIC reads.");
    latchCommandResult(false);
    return false;
  }

  if (!unlockAllNvmBlocks(addr)) {
    outPrintln("WRITEGOOD blocked: could not unlock NVM blocks.");
    latchCommandResult(false);
    return false;
  }

  setProcessing(true);
  outPrintf("WRITEGOOD: writing known-good SPD reference to 0x%02X in 16-byte chunks...\n", addr);

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
