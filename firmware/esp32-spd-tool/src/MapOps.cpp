#include "MapOps.h"
#include "AppState.h"
#include "BoardControl.h"
#include "SpdOps.h"
#include "Log.h"
#include "HardwareConfig.h"

static void mapBanner(const char* title) {
  outPrintln();
  outPrintf("========== %s ==========\n", title);
}

static const char* modeNameNow() {
  return hwHsaModeName(gApp.hwConfig.hsa);
}

static bool inRange(uint8_t v, uint8_t lo, uint8_t hi) {
  return (v >= lo && v <= hi);
}

static bool findFirstScannedInRange(uint8_t lo, uint8_t hi, uint8_t &outAddr) {
  for (size_t i = 0; i < gApp.lastScanCount; i++) {
    uint8_t a = gApp.lastScanAddrs[i];
    if (inRange(a, lo, hi)) {
      outAddr = a;
      return true;
    }
  }
  return false;
}

static bool findHubFromScan(uint8_t &hubAddr) {
  // SPD hub family: 0x50..0x57
  return findFirstScannedInRange(0x50, 0x57, hubAddr);
}

static bool findPmicFromScan(uint8_t &pmicAddr) {
  // Your modules have shown the 0x48..0x4F family, but allow the other
  // documented PMIC families too so the mapper is more DDR5-universal.
  if (findFirstScannedInRange(0x48, 0x4F, pmicAddr)) return true;
  if (findFirstScannedInRange(0x40, 0x47, pmicAddr)) return true;
  if (findFirstScannedInRange(0x60, 0x67, pmicAddr)) return true;
  return false;
}

static void printDetectedDevices() {
  mapBanner("SCAN CLASSIFICATION");

  uint8_t hubAddr = 0;
  uint8_t pmicAddr = 0;

  if (findHubFromScan(hubAddr)) {
    outPrintf("Detected SPD/HUB candidate: 0x%02X\n", hubAddr);
  } else {
    outPrintln("Detected SPD/HUB candidate: none");
  }

  if (findPmicFromScan(pmicAddr)) {
    outPrintf("Detected PMIC candidate:    0x%02X\n", pmicAddr);
  } else {
    outPrintln("Detected PMIC candidate:    none");
  }

  for (size_t i = 0; i < gApp.lastScanCount; i++) {
    uint8_t a = gApp.lastScanAddrs[i];
    const char* kind = "OTHER";

    if (inRange(a, 0x50, 0x57)) kind = "SPD/HUB";
    else if (inRange(a, 0x48, 0x4F) || inRange(a, 0x40, 0x47) || inRange(a, 0x60, 0x67)) kind = "PMIC?";
    else if (inRange(a, 0x10, 0x17)) kind = "TS0?";
    else if (inRange(a, 0x30, 0x37)) kind = "TS1?";
    else if (inRange(a, 0x58, 0x5F)) kind = "RCD?";
    else if (a == 0x7E) kind = "SPECIAL/CCC";
    else if (a == 0x0C) kind = "TS?";
    else kind = "OTHER";

    outPrintf("  0x%02X -> %s\n", a, kind);
  }
}

static void probeHubKeyRegs(uint8_t hubAddr) {
  mapBanner("HUB KEY REGS");
  cmdRegRead(hubAddr, 0x00, 7);   // MR0..MR6 identity / capability / WR recovery
  cmdRegRead(hubAddr, 0x0B, 4);   // MR11..MR14
  cmdRegRead(hubAddr, 0x12, 3);   // MR18..MR20
  cmdRegRead(hubAddr, 0x1A, 6);   // MR26..MR31
  cmdRegRead(hubAddr, 0x20, 6);   // MR32..MR37
  cmdRegRead(hubAddr, 0x30, 6);   // MR48..MR53
}

static void probeHubSweepAll(uint8_t hubAddr) {
  mapBanner("HUB FULL REGISTER SWEEP 0x00-0x7F");
  for (uint16_t base = 0x00; base <= 0x70; base += 0x10) {
    cmdRegRead(hubAddr, (uint8_t)base, 16);
  }
}

static void probePmicSweepAll(uint8_t pmicAddr) {
  mapBanner("PMIC FULL REGISTER SWEEP 0x00-0xFF");

  for (uint16_t base = 0x00; base <= 0xF0; base += 0x10) {
    cmdRegRead(pmicAddr, (uint8_t)base, 16);
  }
}

static void probePmicKeyRegs(uint8_t pmicAddr) {
  mapBanner("PMIC KEY REGS");
  cmdRegRead(pmicAddr, 0x04, 8);   // global status/history window
  cmdRegRead(pmicAddr, 0x08, 8);   // power-good / temp / bus error window
  cmdRegRead(pmicAddr, 0x2F, 4);   // protection / camp / config area
  cmdRegRead(pmicAddr, 0x35, 5);   // error injection / password / lock window
}

void cmdMapAll(uint8_t spdAddr, uint8_t pmicAddr, uint8_t hubAddr) {
  mapBanner("AUTO MAP");
  outPrintf("Declared HSA physical config: %s\n", modeNameNow());
  outPrintf("HSA GPIO raw: %s", isHsaLow() ? "DRIVING GND/OFFLINE" : "RELEASED");
  if (gApp.hwConfig.hsa != HW_HSA_GPIO_SWITCHABLE) outPrint(", not authoritative for declared HSA config");
  outPrintln();
  outPrintf("VIN_BULK: %s\n", isDimmPowerOn() ? "ON" : "OFF");
  outPrintf("Requested SPD addr:  0x%02X\n", spdAddr);
  outPrintf("Requested PMIC addr: 0x%02X\n", pmicAddr);
  outPrintf("Requested HUB addr:  0x%02X\n", hubAddr);
  outPrintln("NOTE: mapper is read-only and does NOT switch HSA or cold-cycle VIN_BULK.");
  outPrintln("NOTE: observed SPD/HUB addresses are observations only; HSA mode comes from hardware config.");

  mapBanner("BUS SCAN");
  cmdScan();

  printDetectedDevices();

  uint8_t resolvedHub  = hubAddr;
  uint8_t resolvedPmic = pmicAddr;

  uint8_t found = 0;
  if (findHubFromScan(found)) {
    resolvedHub = found;
  }
  if (findPmicFromScan(found)) {
    resolvedPmic = found;
  }

  mapBanner("RESOLVED TARGETS");
  outPrintf("Resolved SPD/HUB addr: 0x%02X\n", resolvedHub);
  outPrintf("Resolved PMIC addr:    0x%02X\n", resolvedPmic);

  mapBanner("SPD FULL DUMP (1024 BYTES)");
  cmdDump(resolvedHub, "map");

  probeHubKeyRegs(resolvedHub);
  probeHubSweepAll(resolvedHub);

  mapBanner("PMIC ID");
  cmdPmicId(resolvedPmic);

  probePmicKeyRegs(resolvedPmic);
  probePmicSweepAll(resolvedPmic);

  mapBanner("AUTO MAP COMPLETE");
  latchCommandResult(true);
}
