#pragma once
#include <Arduino.h>

void cmdMapAll(uint8_t spdAddr, uint8_t pmicAddr, uint8_t hubAddr);

enum class MapSectionResult : uint8_t {
  Ok,
  Partial,
  Fail
};

struct MapOptions {
  uint8_t spdAddr  = 0x50;
  uint8_t pmicAddr = 0x48;
  bool includeScan = true;
  bool includeSpd  = true;
  bool includeHub  = true;
  bool includePmic = true;
};

const char* dimmModeName();
bool runAutoMap(Print& out, const MapOptions& opt = {});