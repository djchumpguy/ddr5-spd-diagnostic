#include "TimeReg.h"

#include <string.h>

#include "AppConfig.h"
#include "AppState.h"
#include "BoardControl.h"
#include "I2cHelpers.h"
#include "Log.h"

static bool findFirstMismatchReg(const uint8_t* ref,
                                 const uint8_t* cur,
                                 uint16_t len,
                                 uint16_t& mismatchIndex,
                                 uint8_t& expB,
                                 uint8_t& gotB) {
  for (uint16_t i = 0; i < len; i++) {
    if (ref[i] != cur[i]) {
      mismatchIndex = i;
      expB = ref[i];
      gotB = cur[i];
      return true;
    }
  }
  return false;
}

bool cmdTimeReg(uint8_t addr, uint8_t reg, uint16_t len, uint16_t passes) {
  if (passes == 0) passes = 1;
  if (passes > 500) passes = 500;

  if (len == 0) len = 1;
  if (len > 256) len = 256;

  uint8_t refBuf[256];
  uint8_t curBuf[256];
  bool haveRef = false;

  uint16_t successCount = 0;
  uint16_t failCount = 0;
  uint16_t matchCount = 0;
  uint16_t mismatchCount = 0;

  uint16_t firstMismatchPass = 0xFFFF;
  uint16_t firstMismatchIndex = 0xFFFF;
  uint8_t firstMismatchExp = 0;
  uint8_t firstMismatchGot = 0;

  uint32_t timeMinUs = 0xFFFFFFFFUL;
  uint32_t timeMaxUs = 0;
  uint32_t timeTotalUs = 0;

  setProcessing(true);

  outPrintln();
  outPrintln("TIMEREG (safe / read-only)");
  outPrintf("  addr=0x%02X  reg=0x%02X  len=%u  passes=%u\n",
            addr, reg, len, passes);
  outPrintf("  start: PWR_EN/VR=%s  VIN_BULK=%s  HSA=%s  PWR_GOOD=%s\n",
            readPwrEnLevel() ? "ON" : "OFF",
            isDimmPowerOn() ? "ON" : "OFF",
            isHsaLow() ? "DRIVING-GND/OFFLINE" : "GPIO-RELEASED",
            pwrGoodReady() ? "READY" : "LOW");

  for (uint16_t pass = 0; pass < passes; pass++) {
    boardTick();

    uint32_t t0 = micros();
    bool ok = i2cRegReadRetry(addr, reg, curBuf, len, 6);
    uint32_t dt = micros() - t0;

    if (!ok) {
      failCount++;
      outPrintf("  pass %u/%u: FAIL  time=%lu us\n",
                pass + 1, passes, (unsigned long)dt);
      continue;
    }

    successCount++;
    timeTotalUs += dt;
    if (dt < timeMinUs) timeMinUs = dt;
    if (dt > timeMaxUs) timeMaxUs = dt;

    if (!haveRef) {
      memcpy(refBuf, curBuf, len);
      haveRef = true;

      outPrintf("  pass %u/%u: OK    time=%lu us  reference\n",
                pass + 1, passes, (unsigned long)dt);

      if (len <= 32 || gApp.verbose) {
        outPrint("    data:");
        for (uint16_t i = 0; i < len; i++) outPrintf(" %02X", curBuf[i]);
        outPrintln();
      }
      continue;
    }

    uint16_t mismatchIndex = 0xFFFF;
    uint8_t expB = 0, gotB = 0;
    bool same = !findFirstMismatchReg(refBuf, curBuf, len, mismatchIndex, expB, gotB);

    if (same) {
      matchCount++;
      outPrintf("  pass %u/%u: OK    time=%lu us  match\n",
                pass + 1, passes, (unsigned long)dt);
    } else {
      mismatchCount++;

      if (firstMismatchPass == 0xFFFF) {
        firstMismatchPass = pass + 1;
        firstMismatchIndex = mismatchIndex;
        firstMismatchExp = expB;
        firstMismatchGot = gotB;
      }

      outPrintf("  pass %u/%u: OK    time=%lu us  MISMATCH at +0x%02X exp=%02X got=%02X\n",
                pass + 1, passes, (unsigned long)dt,
                mismatchIndex, expB, gotB);
    }
  }

  setProcessing(false);

  uint32_t avgUs = successCount ? (timeTotalUs / successCount) : 0;

  outPrintln();
  outPrintln("TIMEREG SUMMARY");
  outPrintf("  successes=%u  fails=%u\n", successCount, failCount);

  if (successCount > 0) {
    outPrintf("  time: min=%lu us  avg=%lu us  max=%lu us\n",
              (unsigned long)(timeMinUs == 0xFFFFFFFFUL ? 0 : timeMinUs),
              (unsigned long)avgUs,
              (unsigned long)timeMaxUs);
  } else {
    outPrintln("  time: no successful reads");
  }

  if (successCount <= 1) {
    outPrintln("  consistency: only one successful read, no comparison set");
  } else {
    outPrintf("  consistency: matches=%u  mismatches=%u\n", matchCount, mismatchCount);
    if (firstMismatchPass != 0xFFFF) {
      outPrintf("  first mismatch: pass=%u  byte=+0x%02X  exp=%02X  got=%02X\n",
                firstMismatchPass,
                firstMismatchIndex,
                firstMismatchExp,
                firstMismatchGot);
    }
  }

  bool pass = (successCount > 0 && failCount == 0 && mismatchCount == 0);
  outPrintf("  RESULT: %s\n", pass ? "PASS" : "FAIL");
  latchCommandResult(pass);
  return pass;
}
