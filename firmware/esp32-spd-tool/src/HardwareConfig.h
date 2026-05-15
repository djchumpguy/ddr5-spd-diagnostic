#pragma once
#include <Arduino.h>

enum HwProfile : uint8_t {
  HW_PROFILE_UNKNOWN = 0,
  HW_PROFILE_FULL,
  HW_PROFILE_MINIMUM_DIRECT,
  HW_PROFILE_MANUAL
};

enum HwHsaMode : uint8_t {
  HW_HSA_UNKNOWN = 0,
  HW_HSA_GPIO_SWITCHABLE,
  HW_HSA_MANUAL_GROUND,
  HW_HSA_MANUAL_RESISTOR_STRAP,
  HW_HSA_MANUAL_FLOATING_HIGH
};

enum HwVinBulkMode : uint8_t {
  HW_VIN_UNKNOWN = 0,
  HW_VIN_GPIO_SWITCHABLE,
  HW_VIN_EXTERNAL_LOCKED_ON,
  HW_VIN_EXTERNAL_MANUAL_SWITCH
};

enum HwPwrEnMode : uint8_t {
  HW_PWREN_UNKNOWN = 0,
  HW_PWREN_GPIO_CONTROLLED,
  HW_PWREN_PULLUP_ONLY,
  HW_PWREN_NOT_CONNECTED
};

enum HwPwrGoodMode : uint8_t {
  HW_PGOOD_UNKNOWN = 0,
  HW_PGOOD_GPIO_READ,
  HW_PGOOD_PULLUP_ONLY,
  HW_PGOOD_NOT_CONNECTED
};

enum HwI2cMode : uint8_t {
  HW_I2C_UNKNOWN = 0,
  HW_I2C_PCA9306_LEVEL_SHIFTER,
  HW_I2C_DIRECT_ESP32_3V3,
  HW_I2C_OTHER_LEVEL_SHIFTER
};

enum HwPullupMode : uint8_t {
  HW_PULLUPS_UNKNOWN = 0,
  HW_PULLUPS_EXTERNAL_3V3,
  HW_PULLUPS_ESP32_INTERNAL_ONLY,
  HW_PULLUPS_MODULE_OR_BOARD_PROVIDED
};

struct HardwareConfig {
  HwProfile profile = HW_PROFILE_UNKNOWN;
  HwHsaMode hsa = HW_HSA_UNKNOWN;
  HwVinBulkMode vinBulk = HW_VIN_UNKNOWN;
  HwPwrEnMode pwrEn = HW_PWREN_UNKNOWN;
  HwPwrGoodMode pwrGood = HW_PGOOD_UNKNOWN;
  HwI2cMode i2c = HW_I2C_UNKNOWN;
  HwPullupMode pullups = HW_PULLUPS_UNKNOWN;
};

void hardwareConfigInit();
bool loadHardwareConfigFromNvs();
bool saveHardwareConfigToNvs();
void clearHardwareConfig();
void applyHardwarePreset(HwProfile profile);

const char* hwProfileName(HwProfile v);
const char* hwHsaModeName(HwHsaMode v);
const char* hwVinBulkModeName(HwVinBulkMode v);
const char* hwPwrEnModeName(HwPwrEnMode v);
const char* hwPwrGoodModeName(HwPwrGoodMode v);
const char* hwI2cModeName(HwI2cMode v);
const char* hwPullupModeName(HwPullupMode v);

bool parseHwProfile(const char* s, HwProfile& out);
bool parseHwHsaMode(const char* s, HwHsaMode& out);
bool parseHwVinBulkMode(const char* s, HwVinBulkMode& out);
bool parseHwPwrEnMode(const char* s, HwPwrEnMode& out);
bool parseHwPwrGoodMode(const char* s, HwPwrGoodMode& out);
bool parseHwI2cMode(const char* s, HwI2cMode& out);
bool parseHwPullupMode(const char* s, HwPullupMode& out);

bool hwExpectedSpdAddr(uint8_t& addr, const char*& desc);
const char* hwExpectedHsaText();
const char* hwDeclaredHsaInterpretation();
void printHardwareConfigSummary(bool verbose = true);
void printHardwareConfigBootLine();
void warnIfVinBulkNotSwitchable();
void warnIfPwrEnNotGpioControlled();
void enforceHardwareConfigPins();

bool cmdHardwareConfig(int nt, char* tok[]);
