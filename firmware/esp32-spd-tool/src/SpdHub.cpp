#include "SpdHub.h"
#include "AppConfig.h"
#include "AppState.h"
#include "BoardControl.h"
#include "Log.h"
#include "I2cHelpers.h"
#include <Wire.h>

bool spdRegRead(uint8_t devAddr, uint8_t reg, uint8_t& val) {
  // Minimal safe improvement:
  // use the same retry helper that the generic REG command already uses.
  uint8_t tmp = 0;
  if (!i2cRegReadRetry(devAddr, reg, &tmp, 1, 6)) return false;
  val = tmp;
  return true;
}

bool spdRegWrite(uint8_t devAddr, uint8_t reg, uint8_t val) {
  // Keep write path simple/original to avoid collateral weirdness.
  Wire.beginTransmission(devAddr);
  Wire.write(reg);
  Wire.write(val);
  uint8_t e = Wire.endTransmission(true);
  if (e != 0) return false;
  delay(2);
  return true;
}

bool spdRefreshMR11(uint8_t devAddr) {
  uint8_t v = 0;
  if (!spdRegRead(devAddr, 0x0B, v)) {
    gApp.mr11Valid = false;
    return false;
  }
  gApp.mr11 = v;
  gApp.mr11Valid = true;
  return true;
}

bool spdPollReady(uint8_t devAddr, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    Wire.beginTransmission(devAddr);
    uint8_t e = Wire.endTransmission(true);
    if (e == 0) return true;
    delay(2);
  }
  return false;
}

bool spdSetPagePointerIfNeeded(uint8_t devAddr, uint8_t page) {
  if (!gApp.mr11Valid) return false;

  uint8_t want = (uint8_t)((gApp.mr11 & 0xF8) | (page & 0x07));
  if (want == gApp.mr11) return true;

  if (!spdRegWrite(devAddr, 0x0B, want)) return false;

  uint8_t rb = 0;
  if (!spdRegRead(devAddr, 0x0B, rb)) return false;

  gApp.mr11 = rb;
  return ((gApp.mr11 & 0x07) == (page & 0x07));
}

static bool spdNvmReadOnce(uint8_t devAddr, uint16_t offset, uint8_t* buf, size_t len) {
  if (!gApp.mr11Valid && !spdRefreshMR11(devAddr)) return false;
  bool twoByte = (gApp.mr11 & 0x08) != 0;

  if (!twoByte) {
    uint8_t page = (uint8_t)(offset >> 7);
    uint8_t within = (uint8_t)(offset & 0x7F);
    if (!spdSetPagePointerIfNeeded(devAddr, page)) return false;

    uint8_t a0 = (uint8_t)(0x80 | ((within & 0x40) ? 0x40 : 0x00) | (within & 0x3F));

    Wire.beginTransmission(devAddr);
    Wire.write(a0);
    uint8_t e = Wire.endTransmission(false);
    if (e != 0) return false;

    size_t got = Wire.requestFrom((int)devAddr, (int)len, (int)true);
    if (got != len) return false;

    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)Wire.read();
    return true;
  } else {
    uint8_t block  = (uint8_t)(offset >> 6);
    uint8_t within = (uint8_t)(offset & 0x3F);
    uint8_t a0 = (uint8_t)(0x80 | ((block & 0x01) ? 0x40 : 0x00) | (within & 0x3F));
    uint8_t a1 = (uint8_t)((block >> 1) & 0x0F);

    Wire.beginTransmission(devAddr);
    Wire.write(a0);
    Wire.write(a1);
    uint8_t e = Wire.endTransmission(false);
    if (e != 0) return false;

    size_t got = Wire.requestFrom((int)devAddr, (int)len, (int)true);
    if (got != len) return false;

    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)Wire.read();
    return true;
  }
}

bool spdNvmRead(uint8_t devAddr, uint16_t offset, uint8_t* buf, size_t len) {
  if (offset >= SPD_NVM_SIZE || offset + len > SPD_NVM_SIZE) return false;

  while (len > 0) {
    updateRedLed();

    size_t chunk = len;
    if (chunk > 32) chunk = 32;

    if (!gApp.mr11Valid) spdRefreshMR11(devAddr);
    bool twoByte = gApp.mr11Valid ? ((gApp.mr11 & 0x08) != 0) : false;

    if (!twoByte) {
      uint8_t within = (uint8_t)(offset & 0x7F);
      size_t maxInPage = 128 - within;
      if (chunk > maxInPage) chunk = maxInPage;
    }

    bool ok = false;
    for (int attempt = 0; attempt < 8; attempt++) {
      if (attempt > 0) {
        spdPollReady(devAddr, 150);
        delay(4);
      }
      ok = spdNvmReadOnce(devAddr, offset, buf, chunk);
      if (ok) break;
    }
    if (!ok) return false;

    buf += chunk;
    offset += (uint16_t)chunk;
    len -= chunk;
  }

  return true;
}

static bool nvmWriteBoundaryOK(uint16_t off, size_t len) {
  if (len == 0) return false;
  if (off >= SPD_NVM_SIZE) return false;
  if (off + len > SPD_NVM_SIZE) return false;
  uint16_t in16 = off & 0x0F;
  return (in16 + len) <= 16;
}

static bool spdNvmWriteOnce(uint8_t devAddr, uint16_t offset, const uint8_t* data, size_t len) {
  if (!nvmWriteBoundaryOK(offset, len)) return false;
  if (!gApp.mr11Valid && !spdRefreshMR11(devAddr)) return false;
  bool twoByte = (gApp.mr11 & 0x08) != 0;

  if (!twoByte) {
    uint8_t page = (uint8_t)(offset >> 7);
    uint8_t within = (uint8_t)(offset & 0x7F);
    if (!spdSetPagePointerIfNeeded(devAddr, page)) return false;

    uint8_t a0 = (uint8_t)(0x80 | ((within & 0x40) ? 0x40 : 0x00) | (within & 0x3F));
    Wire.beginTransmission(devAddr);
    Wire.write(a0);
    for (size_t i = 0; i < len; i++) Wire.write(data[i]);
    uint8_t e = Wire.endTransmission(true);
    if (e != 0) return false;
  } else {
    uint8_t block  = (uint8_t)(offset >> 6);
    uint8_t within = (uint8_t)(offset & 0x3F);
    uint8_t a0 = (uint8_t)(0x80 | ((block & 0x01) ? 0x40 : 0x00) | (within & 0x3F));
    uint8_t a1 = (uint8_t)((block >> 1) & 0x0F);

    Wire.beginTransmission(devAddr);
    Wire.write(a0);
    Wire.write(a1);
    for (size_t i = 0; i < len; i++) Wire.write(data[i]);
    uint8_t e = Wire.endTransmission(true);
    if (e != 0) return false;
  }

  return spdPollReady(devAddr, 350);
}

bool spdNvmWrite(uint8_t devAddr, uint16_t offset, const uint8_t* data, size_t len) {
  for (int attempt = 0; attempt < 6; attempt++) {
    if (attempt > 0) {
      spdPollReady(devAddr, 250);
      delay(6);
    }
    if (spdNvmWriteOnce(devAddr, offset, data, len)) return true;
  }
  return false;
}

bool unlockAllNvmBlocks(uint8_t devAddr) {
  uint8_t mr48 = 0, mr12 = 0xFF, mr13 = 0xFF;

  if (!spdRegRead(devAddr, 0x30, mr48)) {
    outPrintln("Unlock: MR48 read failed.");
    return false;
  }

  bool offline = (mr48 & 0x04) != 0;
  outPrintf("Unlock: MR48=0x%02X (offlineTester=%s)\n", mr48, offline ? "YES" : "NO");

  if (!offline) {
    outPrintln("Unlock blocked: not in OFFLINE TESTER mode.");
    outPrintln("Set HSA / Address Strap to HARD-GROUND / OFFLINE before power-up.");
    outPrintln("Manual strap preferred; GPIO27 control is experimental.");
    outPrintln("Changing HSA requires a full VIN_BULK cold-cycle.");
    return false;
  }

  if (!spdRegWrite(devAddr, 0x0C, 0x00)) { outPrintln("Unlock: MR12 write failed."); return false; }
  if (!spdRegWrite(devAddr, 0x0D, 0x00)) { outPrintln("Unlock: MR13 write failed."); return false; }

  if (!spdRegRead(devAddr, 0x0C, mr12) || !spdRegRead(devAddr, 0x0D, mr13)) {
    outPrintln("Unlock: MR12/MR13 readback failed.");
    return false;
  }

  outPrintf("Unlock: MR12=0x%02X MR13=0x%02X\n", mr12, mr13);
  if (mr12 != 0x00 || mr13 != 0x00) {
    outPrintln("Unlock blocked: MR12/MR13 did not clear to 0x00 (writes may be ignored).");
    return false;
  }

  outPrintln("Unlock: OK (all NVM blocks writable).");
  return true;
}
