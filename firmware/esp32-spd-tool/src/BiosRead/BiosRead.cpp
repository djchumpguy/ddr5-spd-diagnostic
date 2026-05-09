#include "BiosRead.h"
#include "AppConfig.h"
#include "AppState.h"
#include "I2cHelpers.h"
#include "Log.h"
#include <Arduino.h>
#include <Wire.h>

// -----------------------------------------------------------------------------
// Local-only helpers
// These do NOT touch shared SPD state in SpdOps/SpdHub.
// -----------------------------------------------------------------------------

static bool biosRegReadLocal(uint8_t devAddr, uint8_t reg, uint8_t& val) {
  uint8_t tmp = 0;
  if (!i2cRegReadRetry(devAddr, reg, &tmp, 1, 6)) return false;
  val = tmp;
  return true;
}

static bool biosRegWriteLocal(uint8_t devAddr, uint8_t reg, uint8_t val) {
  for (int attempt = 0; attempt < 6; attempt++) {
    Wire.beginTransmission(devAddr);
    Wire.write(reg);
    Wire.write(val);
    uint8_t e = Wire.endTransmission(true);
    if (e == 0) {
      delay(2);
      return true;
    }
    delay(2);
  }
  return false;
}

static bool biosLegacyReadByteCurrentPageLocal(uint8_t devAddr, uint16_t absOff, uint8_t& value) {
  if (absOff >= SPD_NVM_SIZE) return false;

  // Legacy 1-byte read pointer:
  // bit7 = MemReg/NVM select
  // low 7 bits = offset inside current 128-byte page
  uint8_t ptr = (uint8_t)(0x80 | (absOff & 0x7F));

  Wire.beginTransmission(devAddr);
  Wire.write(ptr);
  uint8_t e = Wire.endTransmission(false);
  if (e != 0) return false;

  size_t got = Wire.requestFrom((int)devAddr, 1, (int)true);
  if (got != 1 || !Wire.available()) return false;

  value = (uint8_t)Wire.read();
  return true;
}

static bool biosSetLegacyPageLocal(uint8_t devAddr, uint8_t page, uint8_t& oldMr11, uint8_t& newMr11) {
  if (page > 7) return false;
  if (!biosRegReadLocal(devAddr, 0x0B, oldMr11)) return false;

  // Preserve upper nibble, force legacy 1-byte mode (bit3 = 0), set page.
  newMr11 = (uint8_t)((oldMr11 & 0xF0) | (page & 0x07));

  if (!biosRegWriteLocal(devAddr, 0x0B, newMr11)) return false;

  uint8_t verify = 0;
  if (!biosRegReadLocal(devAddr, 0x0B, verify)) return false;

  return ((verify & 0x0F) == (newMr11 & 0x0F));
}

static bool biosRestoreMr11Local(uint8_t devAddr, uint8_t oldMr11) {
  return biosRegWriteLocal(devAddr, 0x0B, oldMr11);
}

// -----------------------------------------------------------------------------
// Commands
// -----------------------------------------------------------------------------

bool cmdBiosMr11(uint8_t addr) {
  uint8_t mr11 = 0;
  bool ok = biosRegReadLocal(addr, 0x0B, mr11);

  if (!ok) {
    outPrintf("BIOSMR11: addr 0x%02X read failed\n", addr);
    latchCommandResult(false);
    return false;
  }

  outPrintf("BIOSMR11: addr 0x%02X MR11=0x%02X (mode=%s page=%u)\n",
            addr,
            mr11,
            (mr11 & 0x08) ? "2-byte" : "1-byte",
            (unsigned)(mr11 & 0x07));

  latchCommandResult(true);
  return true;
}

bool cmdBiosRead(uint8_t addr, uint16_t absOff) {
  if (absOff >= SPD_NVM_SIZE) {
    outPrintf("BIOSREAD: offset 0x%04X out of range\n", absOff);
    latchCommandResult(false);
    return false;
  }

  outPrintf("BIOSREAD: addr 0x%02X off 0x%04X\n", addr, absOff);

  uint8_t mr11Before = 0;
  if (!biosRegReadLocal(addr, 0x0B, mr11Before)) {
    outPrintln("  MR11 before: read failed");
    latchCommandResult(false);
    return false;
  }

  outPrintf("  MR11 before: 0x%02X (mode=%s page=%u)\n",
            mr11Before,
            (mr11Before & 0x08) ? "2-byte" : "1-byte",
            (unsigned)(mr11Before & 0x07));

  uint8_t oldMr11 = 0;
  uint8_t newMr11 = 0;
  uint8_t page = (uint8_t)((absOff >> 7) & 0x07);

  if (!biosSetLegacyPageLocal(addr, page, oldMr11, newMr11)) {
    outPrintln("  set legacy page failed");
    latchCommandResult(false);
    return false;
  }

  outPrintf("  MR11 page set: 0x%02X\n", newMr11);

  uint8_t value = 0;
  bool okRead = biosLegacyReadByteCurrentPageLocal(addr, absOff, value);
  bool okRestore = biosRestoreMr11Local(addr, oldMr11);

  uint8_t mr11After = 0;
  bool okAfter = biosRegReadLocal(addr, 0x0B, mr11After);

  if (!okRead) {
    outPrintln("  legacy read failed");
    if (!okRestore) outPrintln("  MR11 restore failed");
    if (okAfter) outPrintf("  MR11 after:  0x%02X\n", mr11After);
    latchCommandResult(false);
    return false;
  }

  outPrintf("  DATA: 0x%02X\n", value);

  if (!okRestore) outPrintln("  MR11 restore failed");
  if (okAfter) {
    outPrintf("  MR11 after:  0x%02X (mode=%s page=%u)\n",
              mr11After,
              (mr11After & 0x08) ? "2-byte" : "1-byte",
              (unsigned)(mr11After & 0x07));
  } else {
    outPrintln("  MR11 after: read failed");
  }

  bool ok = okRead && okRestore && okAfter;
  latchCommandResult(ok);
  return ok;
}

bool cmdBiosDump(uint8_t addr, uint16_t startOff, uint16_t count) {
  if (startOff >= SPD_NVM_SIZE) {
    outPrintf("BIOSDUMP: start 0x%04X out of range\n", startOff);
    latchCommandResult(false);
    return false;
  }

  if (count == 0) count = 16;
  if ((uint32_t)startOff + (uint32_t)count > SPD_NVM_SIZE) {
    count = (uint16_t)(SPD_NVM_SIZE - startOff);
  }

  outPrintf("BIOSDUMP: addr 0x%02X start 0x%04X len %u\n", addr, startOff, (unsigned)count);

  uint8_t originalMr11 = 0;
  if (!biosRegReadLocal(addr, 0x0B, originalMr11)) {
    outPrintln("  MR11 before: read failed");
    latchCommandResult(false);
    return false;
  }

  uint8_t currentPage = 0xFF;
  bool ok = true;

  for (uint16_t i = 0; i < count; i += 16) {
    uint16_t lineOff = (uint16_t)(startOff + i);
    uint16_t chunk = (uint16_t)(count - i);
    if (chunk > 16) chunk = 16;

    outPrintf("%04X: ", lineOff);

    for (uint16_t j = 0; j < chunk; j++) {
      uint16_t absOff = (uint16_t)(lineOff + j);
      uint8_t page = (uint8_t)((absOff >> 7) & 0x07);

      if (page != currentPage) {
        uint8_t oldMr11 = 0, newMr11 = 0;
        if (!biosSetLegacyPageLocal(addr, page, oldMr11, newMr11)) {
          ok = false;
          break;
        }
        currentPage = page;
      }

      uint8_t value = 0;
      if (!biosLegacyReadByteCurrentPageLocal(addr, absOff, value)) {
        ok = false;
        break;
      }

      outPrintf("%02X ", value);
    }

    outPrintln();
    if (!ok) break;
    delay(2);
  }

  bool okRestore = biosRestoreMr11Local(addr, originalMr11);

  if (!ok) {
    outPrintln("BIOSDUMP: failed");
    if (!okRestore) outPrintln("  MR11 restore failed");
    latchCommandResult(false);
    return false;
  }

  if (!okRestore) {
    outPrintln("BIOSDUMP: done, but MR11 restore failed");
    latchCommandResult(false);
    return false;
  }

  latchCommandResult(true);
  return true;
}

bool cmdBiosInteresting(uint8_t addr) {
  static const uint16_t targets[] = {
    0x00D7, 0x00D9, 0x00DA, 0x00DB, 0x00DC
  };

  outPrintf("BIOSINTERESTING: addr 0x%02X\n", addr);

  uint8_t mr11Before = 0;
  if (!biosRegReadLocal(addr, 0x0B, mr11Before)) {
    outPrintln("  MR11 before: read failed");
    latchCommandResult(false);
    return false;
  }

  outPrintf("  MR11 before: 0x%02X\n", mr11Before);

  uint8_t oldMr11 = 0, newMr11 = 0;
  if (!biosSetLegacyPageLocal(addr, 1, oldMr11, newMr11)) {
    outPrintln("  set page 1 failed");
    latchCommandResult(false);
    return false;
  }

  outPrintf("  MR11 page set: 0x%02X\n", newMr11);

  bool allOk = true;
  for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
    uint8_t value = 0;
    bool ok = biosLegacyReadByteCurrentPageLocal(addr, targets[i], value);
    if (!ok) {
      outPrintf("  SPD[0x%04X] = <READ FAIL>\n", (unsigned)targets[i]);
      allOk = false;
    } else {
      outPrintf("  SPD[0x%04X] = 0x%02X\n", (unsigned)targets[i], value);
    }
  }

  bool okRestore = biosRestoreMr11Local(addr, oldMr11);
  uint8_t mr11After = 0;
  bool okAfter = biosRegReadLocal(addr, 0x0B, mr11After);

  if (!okRestore) {
    outPrintln("  MR11 restore failed");
    allOk = false;
  }

  if (okAfter) outPrintf("  MR11 after:  0x%02X\n", mr11After);
  else {
    outPrintln("  MR11 after: read failed");
    allOk = false;
  }

  latchCommandResult(allOk);
  return allOk;
}