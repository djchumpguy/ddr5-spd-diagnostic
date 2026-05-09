#pragma once
#include <Arduino.h>

bool spdRegRead(uint8_t devAddr, uint8_t reg, uint8_t& val);
bool spdRegWrite(uint8_t devAddr, uint8_t reg, uint8_t val);
bool spdRefreshMR11(uint8_t devAddr);

bool spdPollReady(uint8_t devAddr, uint32_t timeoutMs = 200);
bool spdSetPagePointerIfNeeded(uint8_t devAddr, uint8_t page);

bool spdNvmRead(uint8_t devAddr, uint16_t offset, uint8_t* buf, size_t len);
bool spdNvmWrite(uint8_t devAddr, uint16_t offset, const uint8_t* data, size_t len);

bool unlockAllNvmBlocks(uint8_t devAddr);