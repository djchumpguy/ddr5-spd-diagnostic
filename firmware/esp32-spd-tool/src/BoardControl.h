#pragma once
#include <Arduino.h>
#include "esp_system.h"

void boardInit();
void boardTick();

void setProcessing(bool on);
void latchCommandResult(bool ok);

bool pwrGoodReady();

void setPwrEnEnabled(bool enabled);
bool readPwrEnLevel();

void setDimmPower(bool on);
bool isDimmPowerOn();
void cycleDimmPower(uint32_t offMs = 1000, uint32_t onSettleMs = 1000);

void setHsaLow(bool low);
bool isHsaLow();

void updateRedLed();

const char* resetReasonStr(esp_reset_reason_t r);