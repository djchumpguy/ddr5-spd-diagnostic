#pragma once
#include <Arduino.h>
#include <cstddef>

// ===================== FEATURE TOGGLES =====================
static constexpr bool WIFI_ENABLE = true;

// ===================== WIFI SoftAP =====================
static constexpr const char* AP_SSID = "ESP32_SPD_TOOL";
static constexpr const char* AP_PASS = "spdtool123";   // >=8 chars or "" for open network
static constexpr uint16_t WEB_PORT = 80;

// ===================== NVS (flash cache) =====================
static constexpr const char* NVS_NS = "spdtool";
static constexpr const char* NVS_KEY_GOOD = "goodspd";   // 1024-byte blob
static constexpr const char* NVS_KEY_CRC  = "goodcrc";   // uint32
static constexpr const char* NVS_KEY_PMIC_VALID = "pmicvalid";
static constexpr const char* NVS_KEY_PMIC_ADDR  = "pmicaddr";
static constexpr const char* NVS_KEY_PMIC_START = "pmicstart";
static constexpr const char* NVS_KEY_PMIC_LEN   = "pmiclen";
static constexpr const char* NVS_KEY_PMIC_CRC   = "pmiccrc";
static constexpr const char* NVS_KEY_PMIC_BOOT  = "pmicboot";
static constexpr const char* NVS_KEY_PMIC_BLOB  = "pmicblob";

// ===================== CONFIG =====================
static constexpr int SDA_PIN = 21;
static constexpr int SCL_PIN = 22;
static constexpr uint32_t I2C_HZ = 100000;

static constexpr uint8_t DEFAULT_SPD_ADDR  = 0x50;
static constexpr uint8_t DEFAULT_PMIC_ADDR = 0x48;

// Startup polish
static constexpr uint32_t STARTUP_SETTLE_MS = 650;
static constexpr bool ANSI_CLEAR_ON_START = true;

// DIMM control/status (per your harness)
static constexpr int PIN_PWR_EN   = 33; // GPIO33 -> DIMM pin151 (PWR_EN)
static constexpr int PIN_PWR_GOOD = 34; // GPIO34 <- DIMM pin147 (PWR_GOOD) input-only
static constexpr int PIN_HSA      = 27; // GPIO27 -> 1k -> DIMM pin148 (HSA), external 100k pull-up to 3.3V
static constexpr int PIN_DIMM_PWR = 32; // GPIO32 -> MOSFET gate driver, HIGH = DIMM ON, LOW = DIMM OFF

// One remaining hardware LED
static constexpr int LED_RED = 23; // blink if !PWR_GOOD, solid if last command failed
static constexpr bool LED_ACTIVE_HIGH = true;

// SPD / buffers
static constexpr uint16_t SPD_NVM_SIZE = 1024;
static constexpr uint16_t PMIC_REF_MAX_LEN = 0x80;
static constexpr size_t LOG_CAP = 24000;
static constexpr size_t MAX_SCAN_ADDRS = 64;
static constexpr size_t MAX_ROLE_RECORDS = MAX_SCAN_ADDRS;
static constexpr size_t CLI_LINE_CAP = 260;
