#include "HardwareConfig.h"
#include "AppConfig.h"
#include "AppState.h"
#include "BoardControl.h"
#include "Log.h"
#include <Preferences.h>
#include <string.h>

static Preferences hwPrefs;

static constexpr const char* NVS_KEY_HW_VALID = "hwcfgvalid";
static constexpr const char* NVS_KEY_HW_PROFILE = "hwprofile";
static constexpr const char* NVS_KEY_HW_HSA = "hwhsa";
static constexpr const char* NVS_KEY_HW_VIN = "hwvin";
static constexpr const char* NVS_KEY_HW_PWREN = "hwpwren";
static constexpr const char* NVS_KEY_HW_PGOOD = "hwpgood";
static constexpr const char* NVS_KEY_HW_I2C = "hwi2c";
static constexpr const char* NVS_KEY_HW_PULLUPS = "hwpullups";

static bool strEq(const char* a, const char* b) {
  return a && b && strcmp(a, b) == 0;
}

const char* hwProfileName(HwProfile v) {
  switch (v) {
    case HW_PROFILE_FULL: return "full";
    case HW_PROFILE_MINIMUM_DIRECT: return "minimum_direct";
    case HW_PROFILE_MANUAL: return "manual";
    case HW_PROFILE_UNKNOWN:
    default: return "unknown";
  }
}

const char* hwHsaModeName(HwHsaMode v) {
  switch (v) {
    case HW_HSA_GPIO_SWITCHABLE: return "gpio_switchable";
    case HW_HSA_MANUAL_GROUND: return "manual_ground";
    case HW_HSA_MANUAL_RESISTOR_STRAP: return "manual_resistor_strap";
    case HW_HSA_MANUAL_FLOATING_HIGH: return "manual_floating_high";
    case HW_HSA_UNKNOWN:
    default: return "unknown";
  }
}

const char* hwVinBulkModeName(HwVinBulkMode v) {
  switch (v) {
    case HW_VIN_GPIO_SWITCHABLE: return "gpio_switchable";
    case HW_VIN_EXTERNAL_LOCKED_ON: return "external_locked_on";
    case HW_VIN_EXTERNAL_MANUAL_SWITCH: return "external_manual_switch";
    case HW_VIN_UNKNOWN:
    default: return "unknown";
  }
}

const char* hwPwrEnModeName(HwPwrEnMode v) {
  switch (v) {
    case HW_PWREN_GPIO_CONTROLLED: return "gpio_controlled";
    case HW_PWREN_PULLUP_ONLY: return "pullup_only";
    case HW_PWREN_NOT_CONNECTED: return "not_connected";
    case HW_PWREN_UNKNOWN:
    default: return "unknown";
  }
}

const char* hwPwrGoodModeName(HwPwrGoodMode v) {
  switch (v) {
    case HW_PGOOD_GPIO_READ: return "gpio_read";
    case HW_PGOOD_PULLUP_ONLY: return "pullup_only";
    case HW_PGOOD_NOT_CONNECTED: return "not_connected";
    case HW_PGOOD_UNKNOWN:
    default: return "unknown";
  }
}

const char* hwI2cModeName(HwI2cMode v) {
  switch (v) {
    case HW_I2C_PCA9306_LEVEL_SHIFTER: return "pca9306_level_shifter";
    case HW_I2C_DIRECT_ESP32_3V3: return "direct_esp32_3v3";
    case HW_I2C_OTHER_LEVEL_SHIFTER: return "other_level_shifter";
    case HW_I2C_UNKNOWN:
    default: return "unknown";
  }
}

const char* hwPullupModeName(HwPullupMode v) {
  switch (v) {
    case HW_PULLUPS_EXTERNAL_3V3: return "external_3v3";
    case HW_PULLUPS_ESP32_INTERNAL_ONLY: return "esp32_internal_only";
    case HW_PULLUPS_MODULE_OR_BOARD_PROVIDED: return "module_or_board_provided";
    case HW_PULLUPS_UNKNOWN:
    default: return "unknown";
  }
}

bool parseHwProfile(const char* s, HwProfile& out) {
  if (strEq(s, "full")) out = HW_PROFILE_FULL;
  else if (strEq(s, "minimum_direct") || strEq(s, "minimum") || strEq(s, "direct")) out = HW_PROFILE_MINIMUM_DIRECT;
  else if (strEq(s, "manual")) out = HW_PROFILE_MANUAL;
  else if (strEq(s, "unknown")) out = HW_PROFILE_UNKNOWN;
  else return false;
  return true;
}

bool parseHwHsaMode(const char* s, HwHsaMode& out) {
  if (strEq(s, "gpio") || strEq(s, "gpio_switchable")) out = HW_HSA_GPIO_SWITCHABLE;
  else if (strEq(s, "ground") || strEq(s, "manual_ground")) out = HW_HSA_MANUAL_GROUND;
  else if (strEq(s, "resistor") || strEq(s, "strap") || strEq(s, "manual_resistor_strap")) out = HW_HSA_MANUAL_RESISTOR_STRAP;
  else if (strEq(s, "floating") || strEq(s, "high") || strEq(s, "manual_floating_high")) out = HW_HSA_MANUAL_FLOATING_HIGH;
  else if (strEq(s, "unknown")) out = HW_HSA_UNKNOWN;
  else return false;
  return true;
}

bool parseHwVinBulkMode(const char* s, HwVinBulkMode& out) {
  if (strEq(s, "gpio") || strEq(s, "gpio_switchable")) out = HW_VIN_GPIO_SWITCHABLE;
  else if (strEq(s, "locked") || strEq(s, "external_locked_on")) out = HW_VIN_EXTERNAL_LOCKED_ON;
  else if (strEq(s, "manual") || strEq(s, "external_manual_switch")) out = HW_VIN_EXTERNAL_MANUAL_SWITCH;
  else if (strEq(s, "unknown")) out = HW_VIN_UNKNOWN;
  else return false;
  return true;
}

bool parseHwPwrEnMode(const char* s, HwPwrEnMode& out) {
  if (strEq(s, "gpio") || strEq(s, "gpio_controlled")) out = HW_PWREN_GPIO_CONTROLLED;
  else if (strEq(s, "pullup") || strEq(s, "pullup_only")) out = HW_PWREN_PULLUP_ONLY;
  else if (strEq(s, "none") || strEq(s, "not_connected")) out = HW_PWREN_NOT_CONNECTED;
  else if (strEq(s, "unknown")) out = HW_PWREN_UNKNOWN;
  else return false;
  return true;
}

bool parseHwPwrGoodMode(const char* s, HwPwrGoodMode& out) {
  if (strEq(s, "gpio") || strEq(s, "gpio_read")) out = HW_PGOOD_GPIO_READ;
  else if (strEq(s, "pullup") || strEq(s, "pullup_only")) out = HW_PGOOD_PULLUP_ONLY;
  else if (strEq(s, "none") || strEq(s, "not_connected")) out = HW_PGOOD_NOT_CONNECTED;
  else if (strEq(s, "unknown")) out = HW_PGOOD_UNKNOWN;
  else return false;
  return true;
}

bool parseHwI2cMode(const char* s, HwI2cMode& out) {
  if (strEq(s, "pca9306") || strEq(s, "pca9306_level_shifter")) out = HW_I2C_PCA9306_LEVEL_SHIFTER;
  else if (strEq(s, "direct") || strEq(s, "direct_esp32_3v3")) out = HW_I2C_DIRECT_ESP32_3V3;
  else if (strEq(s, "other") || strEq(s, "other_level_shifter")) out = HW_I2C_OTHER_LEVEL_SHIFTER;
  else if (strEq(s, "unknown")) out = HW_I2C_UNKNOWN;
  else return false;
  return true;
}

bool parseHwPullupMode(const char* s, HwPullupMode& out) {
  if (strEq(s, "external") || strEq(s, "external_3v3")) out = HW_PULLUPS_EXTERNAL_3V3;
  else if (strEq(s, "internal") || strEq(s, "esp32_internal_only")) out = HW_PULLUPS_ESP32_INTERNAL_ONLY;
  else if (strEq(s, "module") || strEq(s, "board") || strEq(s, "module_or_board_provided")) out = HW_PULLUPS_MODULE_OR_BOARD_PROVIDED;
  else if (strEq(s, "unknown")) out = HW_PULLUPS_UNKNOWN;
  else return false;
  return true;
}

static void setUnknownConfig() {
  gApp.hwConfig.profile = HW_PROFILE_UNKNOWN;
  gApp.hwConfig.hsa = HW_HSA_UNKNOWN;
  gApp.hwConfig.vinBulk = HW_VIN_UNKNOWN;
  gApp.hwConfig.pwrEn = HW_PWREN_UNKNOWN;
  gApp.hwConfig.pwrGood = HW_PGOOD_UNKNOWN;
  gApp.hwConfig.i2c = HW_I2C_UNKNOWN;
  gApp.hwConfig.pullups = HW_PULLUPS_UNKNOWN;
}

void applyHardwarePreset(HwProfile profile) {
  setUnknownConfig();
  gApp.hwConfig.profile = profile;

  if (profile == HW_PROFILE_FULL) {
    gApp.hwConfig.hsa = HW_HSA_GPIO_SWITCHABLE;
    gApp.hwConfig.vinBulk = HW_VIN_GPIO_SWITCHABLE;
    gApp.hwConfig.pwrEn = HW_PWREN_GPIO_CONTROLLED;
    gApp.hwConfig.pwrGood = HW_PGOOD_GPIO_READ;
    gApp.hwConfig.i2c = HW_I2C_PCA9306_LEVEL_SHIFTER;
    gApp.hwConfig.pullups = HW_PULLUPS_MODULE_OR_BOARD_PROVIDED;
  } else if (profile == HW_PROFILE_MINIMUM_DIRECT) {
    gApp.hwConfig.hsa = HW_HSA_MANUAL_GROUND;
    gApp.hwConfig.vinBulk = HW_VIN_EXTERNAL_LOCKED_ON;
    gApp.hwConfig.pwrEn = HW_PWREN_PULLUP_ONLY;
    gApp.hwConfig.pwrGood = HW_PGOOD_GPIO_READ;
    gApp.hwConfig.i2c = HW_I2C_DIRECT_ESP32_3V3;
    gApp.hwConfig.pullups = HW_PULLUPS_ESP32_INTERNAL_ONLY;
  } else if (profile == HW_PROFILE_MANUAL) {
    gApp.hwConfig.profile = HW_PROFILE_MANUAL;
  }

  enforceHardwareConfigPins();
}

bool loadHardwareConfigFromNvs() {
  bool valid = hwPrefs.getBool(NVS_KEY_HW_VALID, false);
  if (!valid) {
    setUnknownConfig();
    return false;
  }

  gApp.hwConfig.profile = (HwProfile)hwPrefs.getUChar(NVS_KEY_HW_PROFILE, HW_PROFILE_UNKNOWN);
  gApp.hwConfig.hsa = (HwHsaMode)hwPrefs.getUChar(NVS_KEY_HW_HSA, HW_HSA_UNKNOWN);
  gApp.hwConfig.vinBulk = (HwVinBulkMode)hwPrefs.getUChar(NVS_KEY_HW_VIN, HW_VIN_UNKNOWN);
  gApp.hwConfig.pwrEn = (HwPwrEnMode)hwPrefs.getUChar(NVS_KEY_HW_PWREN, HW_PWREN_UNKNOWN);
  gApp.hwConfig.pwrGood = (HwPwrGoodMode)hwPrefs.getUChar(NVS_KEY_HW_PGOOD, HW_PGOOD_UNKNOWN);
  gApp.hwConfig.i2c = (HwI2cMode)hwPrefs.getUChar(NVS_KEY_HW_I2C, HW_I2C_UNKNOWN);
  gApp.hwConfig.pullups = (HwPullupMode)hwPrefs.getUChar(NVS_KEY_HW_PULLUPS, HW_PULLUPS_UNKNOWN);
  enforceHardwareConfigPins();
  return true;
}

bool saveHardwareConfigToNvs() {
  hwPrefs.putBool(NVS_KEY_HW_VALID, true);
  hwPrefs.putUChar(NVS_KEY_HW_PROFILE, (uint8_t)gApp.hwConfig.profile);
  hwPrefs.putUChar(NVS_KEY_HW_HSA, (uint8_t)gApp.hwConfig.hsa);
  hwPrefs.putUChar(NVS_KEY_HW_VIN, (uint8_t)gApp.hwConfig.vinBulk);
  hwPrefs.putUChar(NVS_KEY_HW_PWREN, (uint8_t)gApp.hwConfig.pwrEn);
  hwPrefs.putUChar(NVS_KEY_HW_PGOOD, (uint8_t)gApp.hwConfig.pwrGood);
  hwPrefs.putUChar(NVS_KEY_HW_I2C, (uint8_t)gApp.hwConfig.i2c);
  hwPrefs.putUChar(NVS_KEY_HW_PULLUPS, (uint8_t)gApp.hwConfig.pullups);
  enforceHardwareConfigPins();
  return true;
}

void clearHardwareConfig() {
  hwPrefs.remove(NVS_KEY_HW_VALID);
  hwPrefs.remove(NVS_KEY_HW_PROFILE);
  hwPrefs.remove(NVS_KEY_HW_HSA);
  hwPrefs.remove(NVS_KEY_HW_VIN);
  hwPrefs.remove(NVS_KEY_HW_PWREN);
  hwPrefs.remove(NVS_KEY_HW_PGOOD);
  hwPrefs.remove(NVS_KEY_HW_I2C);
  hwPrefs.remove(NVS_KEY_HW_PULLUPS);
  setUnknownConfig();
}

bool hwExpectedSpdAddr(uint8_t& addr, const char*& desc) {
  switch (gApp.hwConfig.hsa) {
    case HW_HSA_MANUAL_GROUND:
      addr = 0x50;
      desc = "address previously seen for manual_ground on tested hardware";
      return true;
    case HW_HSA_MANUAL_RESISTOR_STRAP:
      addr = 0x53;
      desc = "address previously seen for manual_resistor_strap on tested hardware";
      return true;
    case HW_HSA_MANUAL_FLOATING_HIGH:
      addr = 0x57;
      desc = "address previously seen for manual_floating_high on tested hardware";
      return true;
    default:
      addr = 0;
      desc = "runtime-dependent / unknown; address is observation only";
      return false;
  }
}

const char* hwExpectedHsaText() {
  uint8_t addr = 0;
  const char* desc = nullptr;
  hwExpectedSpdAddr(addr, desc);
  return desc;
}

const char* hwDeclaredHsaInterpretation() {
  switch (gApp.hwConfig.hsa) {
    case HW_HSA_MANUAL_GROUND:
      return "Physical direct-ground strap. On the tested module this exposed offline/write-protect-override behavior. Do not assume the observed address alone proves this mode.";
    case HW_HSA_MANUAL_RESISTOR_STRAP:
      return "Normal motherboard-style HID strap behavior. Actual address may depend on resistor value and hub/vendor behavior.";
    case HW_HSA_MANUAL_FLOATING_HIGH:
      return "Experimental/diagnostic in this harness. Observed on tested hardware, but do not label it normal or write behavior globally.";
    case HW_HSA_GPIO_SWITCHABLE:
      return "ESP32 GPIO27 controlled HSA path. HSA is sampled at cold power-up, so changing GPIO27 does not change effective HSA mode unless the DIMM is fully power-cycled.";
    default:
      return "Unknown HSA wiring. GPIO27 raw state and observed address are not authoritative until hardware config is declared.";
  }
}

void warnIfVinBulkNotSwitchable() {
  if (gApp.hwConfig.vinBulk == HW_VIN_EXTERNAL_LOCKED_ON ||
      gApp.hwConfig.vinBulk == HW_VIN_EXTERNAL_MANUAL_SWITCH) {
    outPrintln("WARNING: Hardware config says VIN_BULK is not firmware-switchable. GPIO32 does not actually switch DIMM power in this harness.");
  } else if (gApp.hwConfig.vinBulk == HW_VIN_UNKNOWN) {
    outPrintln("WARNING: Hardware config says VIN_BULK switching is unknown. GPIO32 state may not reflect actual DIMM power.");
  }
}

void warnIfPwrEnNotGpioControlled() {
  if (gApp.hwConfig.pwrEn == HW_PWREN_PULLUP_ONLY) {
    outPrintln("WARNING: Hardware config says PWR_EN is pull-up only. GPIO33 control may not affect the DIMM and will not be treated as authoritative.");
  } else if (gApp.hwConfig.pwrEn == HW_PWREN_NOT_CONNECTED || gApp.hwConfig.pwrEn == HW_PWREN_UNKNOWN) {
    outPrintln("WARNING: Hardware config says PWR_EN GPIO33 control is not authoritative for this harness.");
  }
}

void enforceHardwareConfigPins() {
  if (gApp.hwConfig.pwrEn == HW_PWREN_PULLUP_ONLY) {
    pinMode(PIN_PWR_EN, INPUT);
  }
}

void printHardwareConfigSummary(bool verbose) {
  outPrintln("Hardware config:");
  outPrintf("  profile: %s\n", hwProfileName(gApp.hwConfig.profile));
  outPrintf("  HSA: %s\n", hwHsaModeName(gApp.hwConfig.hsa));
  if (gApp.hwConfig.hsa == HW_HSA_GPIO_SWITCHABLE) {
    outPrintf("    declared physical config: ESP32/GPIO-controlled\n");
    outPrintf("    GPIO27 runtime: %s\n",
              isHsaLow() ? "DRIVING GND/OFFLINE" : "RELEASED");
    outPrintln("    note: HSA is sampled at cold power-up; power-cycle VIN_BULK after changing GPIO27");
  } else if (gApp.hwConfig.hsa == HW_HSA_UNKNOWN) {
    outPrintf("    declared physical config: unknown\n");
    outPrintf("    GPIO27 runtime: %s, not authoritative\n",
              isHsaLow() ? "DRIVING GND/OFFLINE" : "RELEASED");
  } else {
    uint8_t addr = 0;
    const char* desc = nullptr;
    if (hwExpectedSpdAddr(addr, desc)) {
      outPrintf("    observed-address note: 0x%02X was previously seen for this declared config on tested hardware\n", addr);
    }
    outPrintf("    GPIO27 runtime: %s, not authoritative\n", isHsaLow() ? "DRIVING GND/OFFLINE" : "RELEASED");
  }
  outPrintf("    interpretation: %s\n", hwDeclaredHsaInterpretation());

  outPrintf("  VIN_BULK: %s\n", hwVinBulkModeName(gApp.hwConfig.vinBulk));
  if (gApp.hwConfig.vinBulk == HW_VIN_GPIO_SWITCHABLE) {
    outPrintf("    GPIO32 runtime: %s\n", isDimmPowerOn() ? "ON" : "OFF");
  } else {
    outPrintf("    GPIO32 runtime: %s, not authoritative for actual DIMM power\n", isDimmPowerOn() ? "ON" : "OFF");
  }

  outPrintf("  PWR_EN: %s\n", hwPwrEnModeName(gApp.hwConfig.pwrEn));
  if (gApp.hwConfig.pwrEn == HW_PWREN_GPIO_CONTROLLED) {
    outPrintf("    GPIO33 runtime: %s\n", readPwrEnLevel() ? "ON/RELEASED" : "OFF/PULLED LOW");
  } else if (gApp.hwConfig.pwrEn == HW_PWREN_PULLUP_ONLY) {
    outPrintln("    required: pulled high/enabled");
    outPrintln("    GPIO33 runtime: not authoritative");
  } else {
    outPrintln("    GPIO33 runtime: not authoritative");
  }

  outPrintf("  PWR_GOOD: %s\n", hwPwrGoodModeName(gApp.hwConfig.pwrGood));
  if (gApp.hwConfig.pwrGood == HW_PGOOD_GPIO_READ) {
    outPrintf("    GPIO34 measured: %s\n", pwrGoodReady() ? "READY" : "LOW");
  } else {
    outPrintln("    PWR_GOOD: not measured by firmware");
  }

  outPrintf("  I2C interface: %s\n", hwI2cModeName(gApp.hwConfig.i2c));
  outPrintf("  SDA/SCL pullups: %s\n", hwPullupModeName(gApp.hwConfig.pullups));
  if (verbose && gApp.hwConfig.pullups == HW_PULLUPS_ESP32_INTERNAL_ONLY) {
    outPrintln("  notes: external 10k SDA/SCL pull-ups may be needed if reads are flaky");
  }
  if (verbose && gApp.hwConfig.profile == HW_PROFILE_UNKNOWN) {
    outPrintln("  notes: GPIO states may not reflect actual harness wiring until configured");
  }
}

void printHardwareConfigBootLine() {
  if (gApp.hwConfig.profile == HW_PROFILE_UNKNOWN) {
    outPrintln("Hardware config: unknown; GPIO states may not reflect actual harness wiring.");
    return;
  }

  uint8_t addr = 0;
  const char* desc = nullptr;
  bool hasExpected = hwExpectedSpdAddr(addr, desc);
  outPrintf("Hardware config: %s | HSA=%s",
            hwProfileName(gApp.hwConfig.profile),
            hwHsaModeName(gApp.hwConfig.hsa));
  if (hasExpected) outPrintf(" tested-observed-address 0x%02X", addr);
  outPrintf(" | VIN_BULK=%s | I2C=%s\n",
            hwVinBulkModeName(gApp.hwConfig.vinBulk),
            hwI2cModeName(gApp.hwConfig.i2c));
}

static void printHwConfigHelp() {
  outPrintln("Hardware config commands:");
  outPrintln("  hwconfig | hwcfg");
  outPrintln("  hwconfig preset full|minimum_direct|manual|unknown");
  outPrintln("  hwconfig hsa gpio|ground|resistor|floating|unknown");
  outPrintln("  hwconfig vinbulk gpio|locked|manual|unknown");
  outPrintln("  hwconfig pwren gpio|pullup|none|unknown");
  outPrintln("  hwconfig pgood gpio|pullup|none|unknown");
  outPrintln("  hwconfig i2c pca9306|direct|other|unknown");
  outPrintln("  hwconfig pullups external|internal|module|unknown");
  outPrintln("  hwconfig clear");
  outPrintln("Presets:");
  outPrintln("  full: ESP32 controls HSA/VIN_BULK/PWR_EN and reads PWR_GOOD");
  outPrintln("  minimum_direct: HSA tied GND, VIN_BULK externally on, PWR_EN pull-up, direct ESP32 3.3V I2C");
  outPrintln("                  tested hardware previously showed SPD/HUB at 0x50; address is evidence only");
  outPrintln("  manual: use individual setters");
}

bool cmdHardwareConfig(int nt, char* tok[]) {
  if (nt == 1) {
    printHardwareConfigSummary(true);
    latchCommandResult(true);
    return true;
  }

  String sub = tok[1];
  sub.toLowerCase();

  if (sub == "help") {
    printHwConfigHelp();
    latchCommandResult(true);
    return true;
  }

  if (sub == "clear") {
    clearHardwareConfig();
    outPrintln("Hardware config cleared; conservative unknown defaults active.");
    latchCommandResult(true);
    return true;
  }

  if (sub == "preset") {
    if (nt < 3) {
      outPrintln("Usage: hwconfig preset full|minimum_direct|manual|unknown");
      latchCommandResult(false);
      return false;
    }
    HwProfile p;
    if (!parseHwProfile(tok[2], p)) {
      outPrintln("Unknown hardware config preset.");
      latchCommandResult(false);
      return false;
    }
    applyHardwarePreset(p);
    saveHardwareConfigToNvs();
    outPrintf("Hardware config preset saved: %s\n", hwProfileName(gApp.hwConfig.profile));
    printHardwareConfigSummary(true);
    latchCommandResult(true);
    return true;
  }

  if (nt < 3) {
    outPrintln("Usage: hwconfig help");
    latchCommandResult(false);
    return false;
  }

  bool ok = false;
  if (sub == "hsa") ok = parseHwHsaMode(tok[2], gApp.hwConfig.hsa);
  else if (sub == "vinbulk" || sub == "vin") ok = parseHwVinBulkMode(tok[2], gApp.hwConfig.vinBulk);
  else if (sub == "pwren") ok = parseHwPwrEnMode(tok[2], gApp.hwConfig.pwrEn);
  else if (sub == "pgood" || sub == "pwrgood") ok = parseHwPwrGoodMode(tok[2], gApp.hwConfig.pwrGood);
  else if (sub == "i2c") ok = parseHwI2cMode(tok[2], gApp.hwConfig.i2c);
  else if (sub == "pullups" || sub == "pullup") ok = parseHwPullupMode(tok[2], gApp.hwConfig.pullups);

  if (!ok) {
    outPrintln("Unknown hardware config field/value. Use: hwconfig help");
    latchCommandResult(false);
    return false;
  }

  gApp.hwConfig.profile = HW_PROFILE_MANUAL;
  saveHardwareConfigToNvs();
  outPrintln("Hardware config saved.");
  printHardwareConfigSummary(true);
  latchCommandResult(true);
  return true;
}

void hardwareConfigInit() {
  hwPrefs.begin(NVS_NS, false);
  loadHardwareConfigFromNvs();
}
