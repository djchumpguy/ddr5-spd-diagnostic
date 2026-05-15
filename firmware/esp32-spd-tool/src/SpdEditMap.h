#pragma once
#include <Arduino.h>
#include "AppConfig.h"

struct SpdEditField {
  const char* id;
  const char* label;
  const char* profile;
  uint16_t offset;
  uint8_t width;
  const char* units;
  bool littleEndian;
  bool editable;
  bool previewOnly;
  bool experimental;
  uint16_t minValue;
  uint16_t maxValue;
};

struct SpdEditChange {
  const SpdEditField* field = nullptr;
  uint32_t value = 0;
  uint8_t oldBytes[4] = {0};
  uint8_t newBytes[4] = {0};
  bool changed = false;
};

const SpdEditField* spdEditFields(size_t& count);
const SpdEditField* spdEditFindField(const char* id);
bool spdEditParseValue(const char* s, uint32_t& out);
bool spdEditBuildChange(const SpdEditField& field, const char* valueText, SpdEditChange& change, char* err, size_t errLen);
String spdEditFieldsJson();
String spdEditPreviewJson(int count, const char* ids[], const char* values[]);
String spdEditApplyJson(bool armed, int count, const char* ids[], const char* values[]);
void cmdSpdEdit(int nt, char* tok[]);
