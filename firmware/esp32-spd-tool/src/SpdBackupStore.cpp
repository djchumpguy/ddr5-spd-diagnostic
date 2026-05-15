#include "SpdBackupStore.h"
#include "AppConfig.h"
#include "AppState.h"
#include "GoodSpdStore.h"
#include "Log.h"
#include <Preferences.h>
#include <string.h>

static Preferences spdBakPrefs;

static void copyBackupLabel(const char* label) {
  memset(gApp.spdBackupLabel, 0, sizeof(gApp.spdBackupLabel));
  if (!label || !*label) label = "tweak checkpoint";
  strlcpy(gApp.spdBackupLabel, label, sizeof(gApp.spdBackupLabel));
}

bool loadSpdBackupFromNvs() {
  bool valid = spdBakPrefs.getBool(NVS_KEY_SPD_BAK_VALID, false);
  size_t n = spdBakPrefs.getBytesLength(NVS_KEY_SPD_BAK_BLOB);

  if (!valid || n != SPD_NVM_SIZE) {
    gApp.spdBackupValid = false;
    gApp.spdBackupAddr = DEFAULT_SPD_ADDR;
    gApp.spdBackupCrc = 0;
    gApp.spdBackupBoot = 0;
    gApp.spdBackupSaveCount = spdBakPrefs.getUInt(NVS_KEY_SPD_BAK_COUNT, 0);
    memset(gApp.spdBackup, 0, sizeof(gApp.spdBackup));
    copyBackupLabel("");
    return false;
  }

  size_t got = spdBakPrefs.getBytes(NVS_KEY_SPD_BAK_BLOB, gApp.spdBackup, SPD_NVM_SIZE);
  if (got != SPD_NVM_SIZE) {
    gApp.spdBackupValid = false;
    gApp.spdBackupCrc = 0;
    return false;
  }

  gApp.spdBackupAddr = spdBakPrefs.getUChar(NVS_KEY_SPD_BAK_ADDR, DEFAULT_SPD_ADDR);
  gApp.spdBackupBoot = spdBakPrefs.getUInt(NVS_KEY_SPD_BAK_BOOT, 0);
  gApp.spdBackupSaveCount = spdBakPrefs.getUInt(NVS_KEY_SPD_BAK_COUNT, 0);
  String label = spdBakPrefs.getString(NVS_KEY_SPD_BAK_LABEL, "tweak checkpoint");
  copyBackupLabel(label.c_str());

  uint32_t stored = spdBakPrefs.getUInt(NVS_KEY_SPD_BAK_CRC, 0);
  uint32_t calc = crc32_buf(gApp.spdBackup, SPD_NVM_SIZE);
  gApp.spdBackupCrc = calc;
  gApp.spdBackupValid = true;

  if (stored != 0 && stored != calc) {
    outPrintf("WARN: tweak checkpoint CRC mismatch (stored=0x%08lX calc=0x%08lX)\n",
              (unsigned long)stored, (unsigned long)calc);
  }

  return true;
}

bool saveSpdBackupToNvs(uint8_t addr, const uint8_t* spd1024, uint32_t bootCount, const char* label) {
  if (!spd1024) return false;

  uint32_t c = crc32_buf(spd1024, SPD_NVM_SIZE);
  uint32_t nextCount = spdBakPrefs.getUInt(NVS_KEY_SPD_BAK_COUNT, 0) + 1;

  size_t put = spdBakPrefs.putBytes(NVS_KEY_SPD_BAK_BLOB, spd1024, SPD_NVM_SIZE);
  if (put != SPD_NVM_SIZE) return false;

  spdBakPrefs.putBool(NVS_KEY_SPD_BAK_VALID, true);
  spdBakPrefs.putUChar(NVS_KEY_SPD_BAK_ADDR, addr);
  spdBakPrefs.putUInt(NVS_KEY_SPD_BAK_CRC, c);
  spdBakPrefs.putUInt(NVS_KEY_SPD_BAK_BOOT, bootCount);
  spdBakPrefs.putUInt(NVS_KEY_SPD_BAK_COUNT, nextCount);
  spdBakPrefs.putString(NVS_KEY_SPD_BAK_LABEL, label && *label ? label : "tweak checkpoint");

  memcpy(gApp.spdBackup, spd1024, SPD_NVM_SIZE);
  gApp.spdBackupValid = true;
  gApp.spdBackupAddr = addr;
  gApp.spdBackupCrc = c;
  gApp.spdBackupBoot = bootCount;
  gApp.spdBackupSaveCount = nextCount;
  copyBackupLabel(label);
  return true;
}

bool verifyLoadedSpdBackupCrc() {
  if (!gApp.spdBackupValid) return false;
  uint32_t calc = crc32_buf(gApp.spdBackup, SPD_NVM_SIZE);
  return calc == gApp.spdBackupCrc;
}

void clearSpdBackupNvs() {
  spdBakPrefs.remove(NVS_KEY_SPD_BAK_VALID);
  spdBakPrefs.remove(NVS_KEY_SPD_BAK_BLOB);
  spdBakPrefs.remove(NVS_KEY_SPD_BAK_CRC);
  spdBakPrefs.remove(NVS_KEY_SPD_BAK_ADDR);
  spdBakPrefs.remove(NVS_KEY_SPD_BAK_BOOT);
  spdBakPrefs.remove(NVS_KEY_SPD_BAK_COUNT);
  spdBakPrefs.remove(NVS_KEY_SPD_BAK_LABEL);

  gApp.spdBackupValid = false;
  gApp.spdBackupAddr = DEFAULT_SPD_ADDR;
  gApp.spdBackupCrc = 0;
  gApp.spdBackupBoot = 0;
  gApp.spdBackupSaveCount = 0;
  memset(gApp.spdBackup, 0, sizeof(gApp.spdBackup));
  copyBackupLabel("");
}

void spdBackupStoreInit() {
  spdBakPrefs.begin(NVS_NS, false);
  loadSpdBackupFromNvs();
}
