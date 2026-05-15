#include "GoodSpdStore.h"
#include "AppConfig.h"
#include "AppState.h"
#include "HardwareConfig.h"
#include "Log.h"
#include "SpdBackupStore.h"
#include <Preferences.h>
#include <string.h>

static Preferences prefs;

static uint32_t crc32_update(uint32_t crc, uint8_t data) {
  crc ^= data;
  for (int i = 0; i < 8; i++) {
    uint32_t mask = -(crc & 1u);
    crc = (crc >> 1) ^ (0xEDB88320u & mask);
  }
  return crc;
}

uint32_t crc32_buf(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) crc = crc32_update(crc, data[i]);
  return ~crc;
}

bool loadGoodFromNvs() {
  size_t n = prefs.getBytesLength(NVS_KEY_GOOD);
  if (n != SPD_NVM_SIZE) {
    gApp.goodSpdValid = false;
    gApp.goodCrc = 0;
    return false;
  }

  size_t got = prefs.getBytes(NVS_KEY_GOOD, gApp.goodSpd, SPD_NVM_SIZE);
  if (got != SPD_NVM_SIZE) {
    gApp.goodSpdValid = false;
    gApp.goodCrc = 0;
    return false;
  }

  uint32_t stored = prefs.getUInt(NVS_KEY_CRC, 0);
  uint32_t calc = crc32_buf(gApp.goodSpd, SPD_NVM_SIZE);
  gApp.goodCrc = calc;
  gApp.goodSpdValid = true;

  if (stored != 0 && stored != calc) {
    outPrintf("WARN: diagnostic SPD reference CRC mismatch (stored=0x%08lX calc=0x%08lX)\n",
              (unsigned long)stored, (unsigned long)calc);
  }

  return true;
}

bool saveGoodToNvs(const uint8_t* spd1024) {
  uint32_t c = crc32_buf(spd1024, SPD_NVM_SIZE);
  size_t put = prefs.putBytes(NVS_KEY_GOOD, spd1024, SPD_NVM_SIZE);
  prefs.putUInt(NVS_KEY_CRC, c);
  if (put != SPD_NVM_SIZE) return false;

  memcpy(gApp.goodSpd, spd1024, SPD_NVM_SIZE);
  gApp.goodSpdValid = true;
  gApp.goodCrc = c;
  return true;
}

void clearGoodNvs() {
  prefs.remove(NVS_KEY_GOOD);
  prefs.remove(NVS_KEY_CRC);
  gApp.goodSpdValid = false;
  gApp.goodCrc = 0;
  memset(gApp.goodSpd, 0, sizeof(gApp.goodSpd));
}

bool loadPmicRefFromNvs() {
  bool valid = prefs.getBool(NVS_KEY_PMIC_VALID, false);
  size_t n = prefs.getBytesLength(NVS_KEY_PMIC_BLOB);
  uint16_t len = prefs.getUShort(NVS_KEY_PMIC_LEN, 0);

  if (!valid || len == 0 || len > PMIC_REF_MAX_LEN || n != len) {
    gApp.pmicRefValid = false;
    gApp.pmicRefLen = 0;
    gApp.pmicRefCrc = 0;
    return false;
  }

  size_t got = prefs.getBytes(NVS_KEY_PMIC_BLOB, gApp.pmicRef, len);
  if (got != len) {
    gApp.pmicRefValid = false;
    gApp.pmicRefLen = 0;
    gApp.pmicRefCrc = 0;
    return false;
  }

  gApp.pmicRefAddr = prefs.getUChar(NVS_KEY_PMIC_ADDR, DEFAULT_PMIC_ADDR);
  gApp.pmicRefStart = prefs.getUChar(NVS_KEY_PMIC_START, 0x00);
  gApp.pmicRefLen = len;
  gApp.pmicRefBoot = prefs.getUInt(NVS_KEY_PMIC_BOOT, 0);

  uint32_t stored = prefs.getUInt(NVS_KEY_PMIC_CRC, 0);
  uint32_t calc = crc32_buf(gApp.pmicRef, len);
  gApp.pmicRefCrc = calc;
  gApp.pmicRefValid = true;

  if (stored != 0 && stored != calc) {
    outPrintf("WARN: PMIC diagnostic reference CRC mismatch (stored=0x%08lX calc=0x%08lX)\n",
              (unsigned long)stored, (unsigned long)calc);
  }

  return true;
}

bool savePmicRefToNvs(uint8_t addr, uint8_t start, uint16_t len, const uint8_t* data, uint32_t bootCount) {
  if (!data || len == 0 || len > PMIC_REF_MAX_LEN) return false;

  uint32_t c = crc32_buf(data, len);
  size_t put = prefs.putBytes(NVS_KEY_PMIC_BLOB, data, len);
  if (put != len) return false;

  prefs.putBool(NVS_KEY_PMIC_VALID, true);
  prefs.putUChar(NVS_KEY_PMIC_ADDR, addr);
  prefs.putUChar(NVS_KEY_PMIC_START, start);
  prefs.putUShort(NVS_KEY_PMIC_LEN, len);
  prefs.putUInt(NVS_KEY_PMIC_CRC, c);
  prefs.putUInt(NVS_KEY_PMIC_BOOT, bootCount);

  memcpy(gApp.pmicRef, data, len);
  gApp.pmicRefValid = true;
  gApp.pmicRefAddr = addr;
  gApp.pmicRefStart = start;
  gApp.pmicRefLen = len;
  gApp.pmicRefCrc = c;
  gApp.pmicRefBoot = bootCount;
  return true;
}

void clearPmicRefNvs() {
  prefs.remove(NVS_KEY_PMIC_VALID);
  prefs.remove(NVS_KEY_PMIC_ADDR);
  prefs.remove(NVS_KEY_PMIC_START);
  prefs.remove(NVS_KEY_PMIC_LEN);
  prefs.remove(NVS_KEY_PMIC_CRC);
  prefs.remove(NVS_KEY_PMIC_BOOT);
  prefs.remove(NVS_KEY_PMIC_BLOB);

  gApp.pmicRefValid = false;
  gApp.pmicRefAddr = DEFAULT_PMIC_ADDR;
  gApp.pmicRefStart = 0x00;
  gApp.pmicRefLen = 0;
  gApp.pmicRefCrc = 0;
  gApp.pmicRefBoot = 0;
  memset(gApp.pmicRef, 0, sizeof(gApp.pmicRef));
}

void storeInit() {
  prefs.begin(NVS_NS, false);
  hardwareConfigInit();
  loadGoodFromNvs();
  loadPmicRefFromNvs();
  spdBackupStoreInit();
}
