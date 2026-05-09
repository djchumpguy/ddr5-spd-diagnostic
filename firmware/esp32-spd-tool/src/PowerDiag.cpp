#include "PowerDiag.h"

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "AppState.h"
#include "BoardControl.h"
#include "Log.h"

static const size_t POWERDIAG_ADDR_SPACE = 128;
static const size_t POWERDIAG_LIST_CAP   = 64;

static size_t powerDiagScan(bool seen[POWERDIAG_ADDR_SPACE], uint8_t outList[POWERDIAG_LIST_CAP]) {
  memset(seen, 0, POWERDIAG_ADDR_SPACE * sizeof(bool));

  size_t outCount = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    boardTick();

    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      seen[addr] = true;
      if (outCount < POWERDIAG_LIST_CAP) outList[outCount++] = addr;
    }

    delay(2);
  }

  return outCount;
}

static bool scanMapsEqual(const bool a[POWERDIAG_ADDR_SPACE], const bool b[POWERDIAG_ADDR_SPACE]) {
  for (size_t i = 0; i < POWERDIAG_ADDR_SPACE; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

static const char* stabilityLabel(uint16_t seen, uint16_t passes) {
  if (seen == 0) return "missing";
  if (seen == passes) return "stable";
  return "intermittent";
}

static void printAddrList(const uint8_t* addrs, size_t count) {
  outPrint("    addrs:");
  if (count == 0) {
    outPrint(" (none)");
  } else {
    for (size_t i = 0; i < count; i++) {
      outPrintf(" 0x%02X", addrs[i]);
    }
  }
  outPrintln();
}

bool cmdPowerDiag(uint16_t passes, uint16_t intervalMs) {
  if (passes == 0) passes = 1;
  if (passes > 250) passes = 250;
  if (intervalMs > 10000) intervalMs = 10000;

  const bool expectPowered = readPwrEnLevel() && isDimmPowerOn();

  bool prevSeen[POWERDIAG_ADDR_SPACE] = {false};
  bool currSeen[POWERDIAG_ADDR_SPACE] = {false};
  uint16_t seenCount[POWERDIAG_ADDR_SPACE];
  memset(seenCount, 0, sizeof(seenCount));

  uint8_t addrList[POWERDIAG_LIST_CAP] = {0};

  bool prevValid = false;
  bool prevPgood = pwrGoodReady();

  uint16_t pgoodHigh = 0;
  uint16_t pgoodLow = 0;
  uint16_t pgoodTransitions = 0;
  uint16_t scanChanges = 0;

  uint16_t scanCountMin = 0xFFFF;
  uint16_t scanCountMax = 0;

  uint32_t scanTimeMinUs = 0xFFFFFFFFUL;
  uint32_t scanTimeMaxUs = 0;
  uint32_t scanTimeTotalUs = 0;

  outPrintln();
  outPrintln("POWERDIAG (safe / read-only)");
  outPrintf("  passes=%u  intervalMs=%u\n", passes, intervalMs);
  outPrintf("  start: PWR_EN/VR=%s  VIN_BULK=%s  HSA=%s  PWR_GOOD=%s\n",
            readPwrEnLevel() ? "ON" : "OFF",
            isDimmPowerOn() ? "ON" : "OFF",
            isHsaLow() ? "DRIVING-GND/OFFLINE" : "GPIO-RELEASED",
            prevPgood ? "READY" : "LOW");

  for (uint16_t pass = 0; pass < passes; pass++) {
    boardTick();

    bool pgood = pwrGoodReady();
    if (pgood) pgoodHigh++;
    else pgoodLow++;

    if (pass > 0 && pgood != prevPgood) pgoodTransitions++;
    prevPgood = pgood;

    uint32_t t0 = micros();
    size_t scanCount = powerDiagScan(currSeen, addrList);
    uint32_t scanTimeUs = micros() - t0;

    scanTimeTotalUs += scanTimeUs;
    if (scanTimeUs < scanTimeMinUs) scanTimeMinUs = scanTimeUs;
    if (scanTimeUs > scanTimeMaxUs) scanTimeMaxUs = scanTimeUs;

    if (scanCount < scanCountMin) scanCountMin = (uint16_t)scanCount;
    if (scanCount > scanCountMax) scanCountMax = (uint16_t)scanCount;

    bool changed = false;
    if (prevValid && !scanMapsEqual(prevSeen, currSeen)) {
      changed = true;
      scanChanges++;
    }

    for (size_t addr = 1; addr < 127; addr++) {
      if (currSeen[addr]) seenCount[addr]++;
    }

    outPrintf("  pass %u/%u: pgood=%s  scanCount=%u  scanTime=%lu us%s\n",
              pass + 1,
              passes,
              pgood ? "HIGH" : "LOW",
              (unsigned)scanCount,
              (unsigned long)scanTimeUs,
              changed ? "  CHANGED" : "");

    if (gApp.verbose || pass == 0 || changed) {
      printAddrList(addrList, scanCount);
    }

    memcpy(prevSeen, currSeen, sizeof(prevSeen));
    prevValid = true;

    if ((pass + 1) < passes && intervalMs > 0) delay(intervalMs);
  }

  uint32_t scanTimeAvgUs = passes ? (scanTimeTotalUs / passes) : 0;

  outPrintln();
  outPrintln("POWERDIAG SUMMARY");
  outPrintf("  PWR_GOOD: ready=%u  low=%u  transitions=%u\n",
            pgoodHigh, pgoodLow, pgoodTransitions);
  outPrintf("  scanCount: min=%u  max=%u  mapChanges=%u\n",
            scanCountMin == 0xFFFF ? 0 : scanCountMin,
            scanCountMax,
            scanChanges);
  outPrintf("  scanTime: min=%lu us  avg=%lu us  max=%lu us\n",
            (unsigned long)(scanTimeMinUs == 0xFFFFFFFFUL ? 0 : scanTimeMinUs),
            (unsigned long)scanTimeAvgUs,
            (unsigned long)scanTimeMaxUs);

  outPrintln("  Address stability:");
  for (size_t addr = 1; addr < 127; addr++) {
    if (seenCount[addr] > 0) {
      outPrintf("    0x%02X  seen=%u/%u  %s\n",
                (unsigned)addr,
                seenCount[addr],
                passes,
                stabilityLabel(seenCount[addr], passes));
    }
  }

  bool ok = true;

  // Only treat power instability as a failure if the board is supposed to be powered.
  if (expectPowered) {
    if (pgoodLow > 0) ok = false;
    if (pgoodTransitions > 0) ok = false;
    if (scanChanges > 0) ok = false;
  }

  outPrintf("  RESULT: %s\n", ok ? "PASS" : "FAIL");
  latchCommandResult(ok);
  return ok;
}
