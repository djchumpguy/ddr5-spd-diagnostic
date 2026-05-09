#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "AppConfig.h"
#include "AppState.h"
#include "BoardControl.h"
#include "Cli.h"
#include "GoodSpdStore.h"
#include "Log.h"
#include "WebUi.h"

void setup() {
  boardInit();

  Serial.begin(115200);
  delay(STARTUP_SETTLE_MS);
  while (Serial.available()) (void)Serial.read();

  if (ANSI_CLEAR_ON_START) ansiClear();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_HZ);

  storeInit();
  webInit();

  gBootCount++;

  outPrintln("=== ESP32 SPD Tool (Web UI) ===");
  outPrintf("Boot #%lu | Reset: %s\n", (unsigned long)gBootCount, resetReasonStr(esp_reset_reason()));
  outPrintf("I2C: SDA=%d  SCL=%d  %lu Hz  defaultSPD=0x%02X\n",
            SDA_PIN, SCL_PIN, (unsigned long)I2C_HZ, DEFAULT_SPD_ADDR);
  outPrintf("GPIO: PWR_EN=%d  PWR_GOOD=%d  HSA=%d  VIN_BULK_SW=%d\n",
            PIN_PWR_EN, PIN_PWR_GOOD, PIN_HSA, PIN_DIMM_PWR);
  outPrintln("Note: GPIO27 HSA control is optional/experimental.");

  if (WIFI_ENABLE) {
    outPrintf("WiFi AP: %s  pass: %s  ip: %s\n",
              AP_SSID, (strlen(AP_PASS) ? AP_PASS : "(open)"),
              WiFi.softAPIP().toString().c_str());
    outPrintln("Web UI: open the IP above in your phone browser.");
  }

  outPrintf("Known-good SPD reference: %s", gApp.goodSpdValid ? "PRESENT" : "MISSING");
  if (gApp.goodSpdValid) outPrintf("  crc32=0x%08lX\n", (unsigned long)gApp.goodCrc);
  else outPrintln();
  outPrintf("PMIC reference: %s", gApp.pmicRefValid ? "PRESENT" : "MISSING");
  if (gApp.pmicRefValid) {
    outPrintf("  addr=0x%02X start=0x%02X len=%u crc32=0x%08lX\n",
              gApp.pmicRefAddr,
              gApp.pmicRefStart,
              (unsigned)gApp.pmicRefLen,
              (unsigned long)gApp.pmicRefCrc);
  } else {
    outPrintln();
  }

  outPrintf("HSA GPIO: %s\n", isHsaLow() ? "DRIVING GND/OFFLINE" : "RELEASED");
  outPrintf("VIN_BULK: %s\n", isDimmPowerOn() ? "ON" : "OFF");
  outPrintln("Type 'h' for help (Serial fallback).");
  printHelp();
}

void loop() {
  webPoll();
  boardTick();
  cliPollSerial();
}
