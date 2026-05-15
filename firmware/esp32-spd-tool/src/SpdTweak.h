#pragma once
#include <Arduino.h>
#include "AppConfig.h"

static constexpr const char* SPD_TWEAK_WRITE_TOKEN = "YES_WRITE_SPD_TWEAK";

struct SpdDecodedField {
  const char* key = "";
  const char* label = "";
  char value[64] = {0};
  char source[48] = {0};
  char raw[96] = {0};
  char status[48] = {0};
  bool editable = false;
  const char* note = "";
};

struct SpdTweakDecode {
  SpdDecodedField fields[12];
  uint8_t count = 0;
  bool ddr5Like = false;
  bool crcRepairSupported = false;
  char summary[96] = {0};
};

struct SpdTweakPatch {
  uint8_t original[SPD_NVM_SIZE] = {0};
  uint8_t patched[SPD_NVM_SIZE] = {0};
  uint16_t diffOffsets[32] = {0};
  uint8_t diffCount = 0;
  bool ok = false;
  bool hasChanges = false;
  bool crcRepairSupported = false;
  char message[160] = {0};
};

struct SpdBaseDecodeSummary {
  bool ok = false;
  bool ddr5Like = false;
  uint8_t header[4] = {0};
  uint8_t generationRaw = 0;
  uint8_t moduleTypeRaw = 0;
  uint16_t tckPs = 0;
  uint32_t mtps = 0;
  uint32_t mclk = 0;
  char error[48] = {0};
};

struct SpdExpoDecodeSummary {
  bool detected = false;
  bool complete = false;
  uint16_t tckPs = 0;
  uint32_t mtps = 0;
  uint32_t mclk = 0;
  char status[48] = {0};
};

struct SpdXmpDecodeSummary {
  bool detected = false;
  bool complete = false;
  uint16_t tckPs = 0;
  uint32_t mtps = 0;
  uint32_t mclk = 0;
  char status[48] = {0};
};

struct SpdCompactDecodeSummary {
  SpdBaseDecodeSummary base;
  SpdExpoDecodeSummary expo;
  SpdXmpDecodeSummary xmp;
};

struct TimingRow {
  const char* label = "";
  char current[48] = {0};
  char source[24] = {0};
  char status[24] = {0};
  bool editable = false;
};

struct TimingSection {
  const char* title = "";
  TimingRow rows[14];
  uint8_t count = 0;
  bool ok = false;
  char error[64] = {0};
};

bool spdReadU8(const uint8_t* data, size_t len, uint16_t off, uint8_t* out);
bool spdReadLe16(const uint8_t* data, size_t len, uint16_t off, uint16_t* out);
bool spdTweakDecodeBaseSummary(const uint8_t* data, size_t len, SpdBaseDecodeSummary& out);
bool spdTweakDecodeExpoSummary(const uint8_t* data, size_t len, SpdExpoDecodeSummary& out);
bool spdTweakDecodeXmpSummary(const uint8_t* data, size_t len, SpdXmpDecodeSummary& out);
bool spdTweakDecodeCompactSummary(const uint8_t* data, size_t len, SpdCompactDecodeSummary& out);
void spdTweakPrintCompactSummary(const SpdCompactDecodeSummary& decoded);
String spdTweakCompactDecodeJson(uint8_t addr, const uint8_t* data, size_t len);
bool spdTweakDecodeBaseSection(const uint8_t* data, size_t len, TimingSection& out);
bool spdTweakDecodeExpoSection(const uint8_t* data, size_t len, TimingSection& out);
bool spdTweakDecodeXmpSection(const uint8_t* data, size_t len, TimingSection& out);
void spdTweakPrintTimingSection(const TimingSection& section);
String spdTweakTimingSectionJson(const TimingSection& section);
bool spdTweakExpoCrcStatus(const uint8_t* data, size_t len, bool& present, bool& pass, uint16_t& stored, uint16_t& calculated);
bool spdTweakSelfTest();

void spdTweakDecodeBuffer(const uint8_t* spd, SpdTweakDecode& out);
uint16_t spdReadLe16(const uint8_t* data, uint16_t off);
uint32_t spdRawMtpsFromTckPs(uint16_t tckPs);
uint32_t spdNearestDdr5Bin(uint32_t rawMtps);
uint32_t spdMclkFromMtps(uint32_t mtps);
uint32_t spdNominalTckPsX1000FromMtps(uint32_t mtps);
uint32_t spdNominalTckPsX1000FromTckPs(uint16_t rawTckPs);
uint32_t spdPsToCycles(uint32_t ps, uint32_t nominalTckPsX1000);
uint32_t spdNsToCycles(uint32_t ns, uint32_t nominalTckPsX1000);
uint32_t spdCyclesToRaw256Ps(uint32_t cycles, uint32_t nominalTckPsX1000);
uint32_t spdCyclesToRawPs(uint32_t cycles, uint32_t nominalTckPsX1000);
uint32_t spdCyclesToRawNs(uint32_t cycles, uint32_t nominalTckPsX1000);
bool spdTweakBuildPatch(const uint8_t* spd, int assignmentCount, char* assignments[], SpdTweakPatch& patch);
bool spdTweakVerifyImage(uint8_t addr, const uint8_t* expected);
void spdTweakPrintDecoded(const SpdTweakDecode& decoded);
void spdTweakPrintDiff(const SpdTweakPatch& patch);
String spdTweakDecodeJson(uint8_t addr, const uint8_t* spd);
