#include "BoardControl.h"
#include "AppConfig.h"
#include "AppState.h"

static inline void ledWrite(int pin, bool on) {
  bool level = LED_ACTIVE_HIGH ? on : !on;
  digitalWrite(pin, level ? HIGH : LOW);
}

void setProcessing(bool on) {
  (void)on;
}

void latchCommandResult(bool ok) {
  gApp.lastCmdFailed = !ok;
}

bool pwrGoodReady() {
  return digitalRead(PIN_PWR_GOOD) == HIGH;
}

void setPwrEnEnabled(bool enabled) {
  if (enabled) {
    pinMode(PIN_PWR_EN, INPUT); // release to external pull-up
  } else {
    pinMode(PIN_PWR_EN, OUTPUT);
    digitalWrite(PIN_PWR_EN, LOW);
  }
}

bool readPwrEnLevel() {
  return digitalRead(PIN_PWR_EN) == HIGH;
}

void setDimmPower(bool on) {
  pinMode(PIN_DIMM_PWR, OUTPUT);
  digitalWrite(PIN_DIMM_PWR, on ? HIGH : LOW);
}

bool isDimmPowerOn() {
  return digitalRead(PIN_DIMM_PWR) == HIGH;
}

void cycleDimmPower(uint32_t offMs, uint32_t onSettleMs) {
  setDimmPower(false);
  delay(offMs);
  setDimmPower(true);
  delay(onSettleMs);
}

void setHsaLow(bool low) {
  if (low) {
    pinMode(PIN_HSA, OUTPUT);
    digitalWrite(PIN_HSA, LOW);
    gApp.hsaLow = true;
  } else {
    pinMode(PIN_HSA, INPUT); // release to external pull-up
    gApp.hsaLow = false;
  }
}

bool isHsaLow() {
  return gApp.hsaLow;
}

void updateRedLed() {
  if (!pwrGoodReady()) {
    uint32_t now = millis();
    if (now - gApp.redBlinkT0 >= 250) {
      gApp.redBlinkT0 = now;
      gApp.redBlinkState = !gApp.redBlinkState;
      ledWrite(LED_RED, gApp.redBlinkState);
    }
    return;
  }

  if (gApp.lastCmdFailed) {
    ledWrite(LED_RED, true);
    return;
  }

  ledWrite(LED_RED, false);
}

void boardInit() {
  pinMode(PIN_PWR_GOOD, INPUT);
  setPwrEnEnabled(true);
  setHsaLow(false);
  setDimmPower(true);

  pinMode(LED_RED, OUTPUT);
  ledWrite(LED_RED, false);

  gApp.lastCmdFailed = false;
}

void boardTick() {
  updateRedLed();
}

const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT_RESET";
    case ESP_RST_SW:        return "SW_RESET";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}