#include "SpdEditMap.h"
#include "AppState.h"
#include "GoodSpdStore.h"
#include "Log.h"
#include "SpdBackupStore.h"
#include "SpdHub.h"
#include "SpdTweak.h"
#include <string.h>

static const SpdEditField kFields[] = {
  {"expo_sig", "EXPO signature", "EXPO", 0x340, 4, "raw", false, false, false, true, 0, 0},
  {"expo_header", "EXPO raw header bytes", "EXPO", 0x340, 16, "raw", false, false, false, true, 0, 0},
  {"expo_vdd", "EXPO VDD", "EXPO", 0x34A, 1, "V", false, true, false, false, 1100, 1400},
  {"expo_vddq", "EXPO VDDQ", "EXPO", 0x34B, 1, "V", false, true, false, false, 1100, 1400},
  {"expo_cas_mask", "EXPO CAS/profile mask", "EXPO", 0x34C, 2, "raw", true, false, false, true, 0, 0},
  {"expo_tck", "EXPO data rate / tCK", "EXPO", 0x34E, 2, "MT/s", true, true, false, false, 3200, 9000},
  {"expo_taa", "EXPO tAA / CL", "EXPO", 0x351, 1, "cycles", false, true, false, false, 1, 255},
  {"expo_trcd", "EXPO tRCD", "EXPO", 0x353, 1, "cycles", false, true, false, false, 1, 255},
  {"expo_trp", "EXPO tRP", "EXPO", 0x355, 1, "cycles", false, true, false, false, 1, 255},
  {"expo_tras", "EXPO tRAS", "EXPO", 0x357, 1, "cycles", false, true, false, false, 1, 512},
  {"expo_trc", "EXPO tRC", "EXPO", 0x359, 1, "cycles", false, true, false, false, 1, 512},
  {"expo_twr", "EXPO tWR", "EXPO", 0x35A, 2, "cycles", true, true, false, false, 1, 1024},
  {"expo_trfc1", "EXPO tRFC1", "EXPO", 0x35C, 2, "cycles", true, true, false, false, 1, 4096},
  {"expo_trfc2", "EXPO tRFC2", "EXPO", 0x35E, 2, "cycles", true, true, false, false, 1, 4096},
  {"expo_trfcsb", "EXPO tRFCsb", "EXPO", 0x360, 1, "cycles", false, true, false, false, 1, 4096},
  {"xmp_vdd", "XMP VDD", "XMP", 0x2C1, 1, "V", false, true, false, false, 1100, 1400},
  {"xmp_vddq", "XMP VDDQ", "XMP", 0x2C2, 1, "V", false, true, false, false, 1100, 1400},
  {"xmp_tck", "XMP data rate / tCK", "XMP", 0x2C5, 2, "MT/s", true, true, false, false, 3200, 9000},
  {"xmp_taa", "XMP tAA / CL", "XMP", 0x2CE, 1, "cycles", false, true, false, false, 1, 255},
  {"xmp_trcd", "XMP tRCD", "XMP", 0x2D0, 1, "cycles", false, true, false, false, 1, 255},
  {"xmp_trp", "XMP tRP", "XMP", 0x2D2, 1, "cycles", false, true, false, false, 1, 255},
  {"xmp_tras", "XMP tRAS", "XMP", 0x2D4, 1, "cycles", false, true, false, false, 1, 512},
  {"xmp_trc", "XMP tRC", "XMP", 0x2D6, 1, "cycles", false, true, false, false, 1, 512},
  {"xmp_twr", "XMP tWR", "XMP", 0x2D7, 2, "cycles", true, true, false, false, 1, 1024},
  {"xmp_trfc1", "XMP tRFC1", "XMP", 0x2D9, 2, "cycles", true, true, false, false, 1, 4096},
  {"xmp_trfc2", "XMP tRFC2", "XMP", 0x2DB, 2, "cycles", true, true, false, false, 1, 4096},
  {"xmp_trfcsb", "XMP tRFCsb", "XMP", 0x2DD, 1, "cycles", false, true, false, false, 1, 4096},
};

static bool gPreviewValid = false;
static uint32_t gPreviewSig = 0;
static uint32_t gPreviewGeneration = 0;

static constexpr uint16_t EXPO_CRC_START = 0x340;
static constexpr uint16_t EXPO_CRC_END = 0x3BD;
static constexpr uint16_t EXPO_CRC_LO = 0x3BE;
static constexpr uint16_t EXPO_CRC_HI = 0x3BF;
static constexpr uint16_t XMP_CRC_START = 0x2C0;
static constexpr uint16_t XMP_CRC_END = 0x2FD;
static constexpr uint16_t XMP_CRC_LO = 0x2FE;
static constexpr uint16_t XMP_CRC_HI = 0x2FF;

static uint16_t expoCrc16(const uint8_t* data) {
  uint16_t crc = 0;
  for (uint16_t off = EXPO_CRC_START; off <= EXPO_CRC_END; off++) {
    crc ^= (uint16_t)data[off] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static uint16_t crc16XmodemRange(const uint8_t* data, uint16_t start, uint16_t endInclusive) {
  uint16_t crc = 0;
  for (uint16_t off = start; off <= endInclusive; off++) {
    crc ^= (uint16_t)data[off] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static uint16_t sum16Range(const uint8_t* data, uint16_t start, uint16_t endInclusive) {
  uint32_t sum = 0;
  for (uint16_t off = start; off <= endInclusive; off++) sum += data[off];
  return (uint16_t)sum;
}

static uint16_t storedExpoCrc(const uint8_t* data) {
  return (uint16_t)data[EXPO_CRC_LO] | ((uint16_t)data[EXPO_CRC_HI] << 8);
}

static uint16_t storedXmpCrc(const uint8_t* data) {
  return (uint16_t)data[XMP_CRC_LO] | ((uint16_t)data[XMP_CRC_HI] << 8);
}

static bool expoSignaturePresent(const uint8_t* data) {
  return data[EXPO_CRC_START] == 'E' && data[EXPO_CRC_START + 1] == 'X' &&
         data[EXPO_CRC_START + 2] == 'P' && data[EXPO_CRC_START + 3] == 'O';
}

static bool fieldIsProfile(const SpdEditField& f, const char* profile) {
  return profile && strcmp(f.profile, profile) == 0;
}

static bool changesShareProfile(const SpdEditChange* changes, uint8_t changeCount, const char*& profile) {
  profile = nullptr;
  for (uint8_t i = 0; i < changeCount; i++) {
    if (!changes[i].field) return false;
    if (!profile) profile = changes[i].field->profile;
    else if (strcmp(profile, changes[i].field->profile) != 0) return false;
  }
  return profile != nullptr;
}

static bool changesAreCrcCovered(const SpdEditChange* changes, uint8_t changeCount, uint16_t start, uint16_t end) {
  for (uint8_t i = 0; i < changeCount; i++) {
    const SpdEditField& f = *changes[i].field;
    if (f.offset < start || (uint16_t)(f.offset + f.width - 1) > end) return false;
  }
  return true;
}

static bool buildRepairedProfileImage(const SpdEditChange* changes, uint8_t changeCount, uint8_t* image,
                                      uint8_t oldCrcBytes[2], uint8_t newCrcBytes[2],
                                      uint16_t& crcLo, const char*& profileOut, String& err) {
  if (!gApp.currentSpdDump.valid || gApp.currentSpdDump.len != SPD_NVM_SIZE) {
    err = "No current 1024-byte SPD dump in RAM. Click Dump 1024 or save a tweak checkpoint first.";
    return false;
  }
  const char* profile = nullptr;
  if (!changesShareProfile(changes, changeCount, profile)) {
    err = "Write blocked: edits must target exactly one profile region.";
    return false;
  }
  profileOut = profile;
  uint16_t start = 0, end = 0, lo = 0, hi = 0;
  if (strcmp(profile, "EXPO") == 0) {
    if (!expoSignaturePresent(gApp.currentSpdDump.data)) {
      err = "Write blocked: EXPO signature not present at 0x340.";
      return false;
    }
    start = EXPO_CRC_START; end = EXPO_CRC_END; lo = EXPO_CRC_LO; hi = EXPO_CRC_HI;
  } else if (strcmp(profile, "XMP") == 0) {
    start = XMP_CRC_START; end = XMP_CRC_END; lo = XMP_CRC_LO; hi = XMP_CRC_HI;
    uint16_t stored = storedXmpCrc(gApp.currentSpdDump.data);
    uint16_t calc = crc16XmodemRange(gApp.currentSpdDump.data, XMP_CRC_START, XMP_CRC_END);
    if (stored != calc) {
      err = "Write blocked: XMP CRC check failed; repair is unavailable for this image.";
      return false;
    }
  } else {
    err = "Write blocked: CRC/checksum repair is not implemented for this profile.";
    return false;
  }
  if (!changesAreCrcCovered(changes, changeCount, start, end)) {
    err = "Write blocked: selected field is outside the active profile CRC-covered region.";
    return false;
  }
  memcpy(image, gApp.currentSpdDump.data, SPD_NVM_SIZE);
  for (uint8_t i = 0; i < changeCount; i++) {
    const SpdEditField& f = *changes[i].field;
    memcpy(&image[f.offset], changes[i].newBytes, f.width);
  }
  oldCrcBytes[0] = gApp.currentSpdDump.data[lo];
  oldCrcBytes[1] = gApp.currentSpdDump.data[hi];
  uint16_t crc = (strcmp(profile, "EXPO") == 0) ? expoCrc16(image) : crc16XmodemRange(image, XMP_CRC_START, XMP_CRC_END);
  newCrcBytes[0] = (uint8_t)(crc & 0xFF);
  newCrcBytes[1] = (uint8_t)((crc >> 8) & 0xFF);
  image[lo] = newCrcBytes[0];
  image[hi] = newCrcBytes[1];
  crcLo = lo;
  return true;
}

static String expoCrcStatusJson() {
  if (!gApp.currentSpdDump.valid || gApp.currentSpdDump.len != SPD_NVM_SIZE) {
    return "{\"checked\":false,\"status\":\"not checked\",\"repair\":\"unavailable\",\"apply\":\"blocked\",\"stored\":\"--\",\"calculated\":\"--\"}";
  }
  if (!expoSignaturePresent(gApp.currentSpdDump.data)) {
    return "{\"checked\":true,\"status\":\"not present\",\"repair\":\"unavailable\",\"apply\":\"blocked\",\"stored\":\"--\",\"calculated\":\"--\"}";
  }
  uint16_t stored = storedExpoCrc(gApp.currentSpdDump.data);
  uint16_t calc = expoCrc16(gApp.currentSpdDump.data);
  char storedHex[12];
  char calcHex[12];
  snprintf(storedHex, sizeof(storedHex), "0x%04X", stored);
  snprintf(calcHex, sizeof(calcHex), "0x%04X", calc);
  String j = "{\"checked\":true,";
  j += "\"status\":\"" + String(stored == calc ? "PASS" : "FAIL") + "\",";
  j += "\"repair\":\"available\",";
  j += "\"apply\":\"preview required\",";
  j += "\"stored\":\"" + String(storedHex) + "\",";
  j += "\"calculated\":\"" + String(calcHex) + "\"}";
  return j;
}

static void appendXmpCandidate(String& j, bool& first, const char* algo, uint16_t start, uint16_t end, uint16_t calc, uint16_t stored) {
  if (!first) j += ",";
  first = false;
  char range[24];
  char calcHex[12];
  char storedHex[12];
  snprintf(range, sizeof(range), "0x%03X-0x%03X", start, end);
  snprintf(calcHex, sizeof(calcHex), "0x%04X", calc);
  snprintf(storedHex, sizeof(storedHex), "0x%04X", stored);
  j += "{\"algorithm\":\"" + String(algo) + "\",";
  j += "\"range\":\"" + String(range) + "\",";
  j += "\"calculated\":\"" + String(calcHex) + "\",";
  j += "\"stored\":\"" + String(storedHex) + "\",";
  j += "\"match\":" + String(calc == stored ? "true" : "false") + "}";
}

static String xmpCrcDiscoveryJson() {
  if (!gApp.currentSpdDump.valid || gApp.currentSpdDump.len != SPD_NVM_SIZE) {
    return "{\"status\":\"unknown\",\"repair\":\"unavailable\",\"apply\":\"blocked\",\"stored\":\"--\",\"candidates\":[]}";
  }
  const uint8_t* data = gApp.currentSpdDump.data;
  uint16_t stored = (uint16_t)data[0x2FE] | ((uint16_t)data[0x2FF] << 8);
  uint16_t primary = crc16XmodemRange(data, XMP_CRC_START, XMP_CRC_END);
  char storedHex[12];
  char calcHex[12];
  snprintf(storedHex, sizeof(storedHex), "0x%04X", stored);
  snprintf(calcHex, sizeof(calcHex), "0x%04X", primary);
  bool pass = stored == primary;
  String j = "{\"status\":\"" + String(pass ? "PASS" : "FAIL") + "\",";
  j += "\"repair\":\"" + String(pass ? "available" : "unavailable") + "\",";
  j += "\"apply\":\"" + String(pass ? "preview required" : "blocked until XMP CRC passes") + "\",";
  j += "\"stored\":\"" + String(storedHex) + "\",";
  j += "\"calculated\":\"" + String(calcHex) + "\",\"candidates\":[";
  bool first = true;
  appendXmpCandidate(j, first, "CRC16/XMODEM", 0x2C0, 0x2FD, primary, stored);
  appendXmpCandidate(j, first, "CRC16/XMODEM", 0x280, 0x2BD, crc16XmodemRange(data, 0x280, 0x2BD), stored);
  appendXmpCandidate(j, first, "CRC16/XMODEM", 0x280, 0x2FD, crc16XmodemRange(data, 0x280, 0x2FD), stored);
  appendXmpCandidate(j, first, "SUM16", 0x2C0, 0x2FD, sum16Range(data, 0x2C0, 0x2FD), stored);
  appendXmpCandidate(j, first, "SUM16", 0x280, 0x2FD, sum16Range(data, 0x280, 0x2FD), stored);
  j += "]}";
  return j;
}

static void appendSecondaryCandidate(String& j, bool& first, const char* profile, const char* label, uint16_t off) {
  if (!first) j += ",";
  first = false;
  char raw[16];
  snprintf(raw, sizeof(raw), "%02X", gApp.currentSpdDump.data[off]);
  j += "{\"profile\":\"" + String(profile) + "\",";
  j += "\"label\":\"" + String(label) + "\",";
  j += "\"source\":\"0x" + String(off, HEX) + "\",";
  j += "\"raw\":\"" + String(raw) + "\",";
  j += "\"guess\":\"unverified candidate\"}";
}

static String secondaryTimingCandidatesJson() {
  if (!gApp.currentSpdDump.valid || gApp.currentSpdDump.len != SPD_NVM_SIZE) return "[]";
  String j = "[";
  bool first = true;
  const char* labels[] = {"tRRDS", "tRRDL", "tFAW", "tRTP", "tCWL", "tCCD_L", "tCCD_L_WR"};
  for (uint8_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
    appendSecondaryCandidate(j, first, "EXPO", labels[i], (uint16_t)(0x361 + i));
  }
  for (uint8_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
    appendSecondaryCandidate(j, first, "XMP", labels[i], (uint16_t)(0x2DE + i));
  }
  j += "]";
  return j;
}

static uint32_t previewSig(const SpdEditChange* changes, uint8_t changeCount) {
  uint32_t sig = 2166136261u;
  for (uint8_t i = 0; i < changeCount; i++) {
    const SpdEditField& f = *changes[i].field;
    sig ^= f.offset; sig *= 16777619u;
    sig ^= f.width; sig *= 16777619u;
    for (uint8_t b = 0; b < f.width; b++) {
      sig ^= changes[i].newBytes[b]; sig *= 16777619u;
    }
  }
  return sig;
}

static String jsonEscapeLocal(const char* s) {
  String out;
  if (!s) return out;
  while (*s) {
    char c = *s++;
    if (c == '"' || c == '\\') out += '\\';
    if ((uint8_t)c >= 0x20) out += c;
  }
  return out;
}

static void rawBytes(char* out, size_t outLen, const uint8_t* data, uint16_t off, uint8_t width) {
  if (!out || outLen == 0) return;
  out[0] = 0;
  char tmp[8];
  for (uint8_t i = 0; i < width && (off + i) < SPD_NVM_SIZE; i++) {
    if (i) strlcat(out, " ", outLen);
    snprintf(tmp, sizeof(tmp), "%02X", data[off + i]);
    strlcat(out, tmp, outLen);
  }
}

static void byteList(char* out, size_t outLen, const uint8_t* data, uint8_t width) {
  if (!out || outLen == 0) return;
  out[0] = 0;
  char tmp[8];
  for (uint8_t i = 0; i < width; i++) {
    if (i) strlcat(out, " ", outLen);
    snprintf(tmp, sizeof(tmp), "%02X", data[i]);
    strlcat(out, tmp, outLen);
  }
}

static bool formatTckRate(char* out, size_t outLen, uint16_t tckPs) {
  if (!out || outLen == 0 || tckPs == 0) return false;
  uint32_t mtps = spdRawMtpsFromTckPs(tckPs);
  if (mtps == 0) return false;
  snprintf(out, outLen, "~DDR5-%lu / tCK %u ps",
           (unsigned long)mtps, (unsigned)tckPs);
  return true;
}

static uint32_t nominalTckForField(const SpdEditField& f, const uint8_t* data) {
  uint16_t off = fieldIsProfile(f, "XMP") ? 0x2C5 : 0x34E;
  return (uint32_t)spdReadLe16(data, off) * 1000UL;
}

static bool fieldUsesRaw256Ps(const SpdEditField& f) {
  return f.offset == 0x351 || f.offset == 0x353 || f.offset == 0x355 ||
         f.offset == 0x357 || f.offset == 0x359 ||
         f.offset == 0x2CE || f.offset == 0x2D0 || f.offset == 0x2D2 ||
         f.offset == 0x2D4 || f.offset == 0x2D6;
}

static bool fieldUsesPs(const SpdEditField& f) {
  return f.offset == 0x35A || f.offset == 0x2D7;
}

static bool fieldUsesNs(const SpdEditField& f) {
  return f.offset == 0x35C || f.offset == 0x35E || f.offset == 0x360 ||
         f.offset == 0x2D9 || f.offset == 0x2DB || f.offset == 0x2DD;
}

static bool fieldUsesVoltage(const SpdEditField& f) {
  return strcmp(f.id, "expo_vdd") == 0 || strcmp(f.id, "expo_vddq") == 0 ||
         strcmp(f.id, "xmp_vdd") == 0 || strcmp(f.id, "xmp_vddq") == 0;
}

static bool fieldUsesTck(const SpdEditField& f) {
  return strcmp(f.id, "expo_tck") == 0 || strcmp(f.id, "xmp_tck") == 0;
}

static bool fieldUsesCycleInput(const SpdEditField& f) {
  return fieldUsesRaw256Ps(f) || fieldUsesPs(f) || fieldUsesNs(f);
}

static uint32_t decodedCyclesForField(const uint8_t* data, const SpdEditField& f) {
  uint32_t tck = nominalTckForField(f, data);
  if (fieldUsesRaw256Ps(f)) return spdPsToCycles((uint32_t)data[f.offset] * 256UL, tck);
  if (fieldUsesPs(f)) return spdPsToCycles(spdReadLe16(data, f.offset), tck);
  if (fieldUsesNs(f)) {
    uint32_t ns = (f.width == 1) ? data[f.offset] : spdReadLe16(data, f.offset);
    return spdNsToCycles(ns, tck);
  }
  return 0;
}

static uint32_t absDeltaU32(uint32_t a, uint32_t b) {
  return a > b ? (a - b) : (b - a);
}

static bool parseVoltageMv(const char* s, uint32_t& mv) {
  mv = 0;
  if (!s || !*s) return false;
  const char* dot = strchr(s, '.');
  if (!dot) {
    uint32_t whole = 0;
    if (!spdEditParseValue(s, whole)) return false;
    mv = whole > 10 ? whole : whole * 1000UL;
    return true;
  }
  char wholeBuf[4] = {0};
  size_t wholeLen = (size_t)(dot - s);
  if (wholeLen == 0 || wholeLen >= sizeof(wholeBuf)) return false;
  memcpy(wholeBuf, s, wholeLen);
  uint32_t whole = 0;
  if (!spdEditParseValue(wholeBuf, whole)) return false;
  const char* frac = dot + 1;
  if (!*frac || strlen(frac) > 3) return false;
  uint16_t fracVal = 0;
  for (const char* p = frac; *p; p++) {
    if (*p < '0' || *p > '9') return false;
    fracVal = (uint16_t)(fracVal * 10 + (*p - '0'));
  }
  if (strlen(frac) == 1) fracVal *= 100;
  else if (strlen(frac) == 2) fracVal *= 10;
  mv = whole * 1000UL + fracVal;
  return true;
}

static bool voltageMvToRaw(uint32_t mv, uint8_t& raw) {
  switch (mv) {
    case 1100: raw = 0x10; return true;
    case 1250: raw = 0x25; return true;
    case 1350: raw = 0x35; return true;
    case 1400: raw = 0x40; return true;
    default: raw = 0; return false;
  }
}

static bool parseSpeedMtps(const char* s, uint32_t& mtps) {
  mtps = 0;
  if (!s || !*s) return false;
  if (strncmp(s, "DDR5-", 5) == 0 || strncmp(s, "ddr5-", 5) == 0) s += 5;
  if (!spdEditParseValue(s, mtps)) return false;
  return mtps >= 3200 && mtps <= 9000;
}

static bool decodeProfileVoltageTenthsHundredthsCandidate(uint8_t raw, uint16_t& millivolts) {
  switch (raw) {
    case 0x10: millivolts = 1100; return true;
    case 0x25: millivolts = 1250; return true;
    case 0x35: millivolts = 1350; return true;
    case 0x40: millivolts = 1400; return true;
    default: millivolts = 0; return false;
  }
}

static void formatVoltage(char* out, size_t outLen, uint8_t raw) {
  uint16_t mv = 0;
  if (decodeProfileVoltageTenthsHundredthsCandidate(raw, mv)) {
    snprintf(out, outLen, "%u.%02u V", (unsigned)(mv / 1000), (unsigned)((mv % 1000) / 10));
  } else {
    snprintf(out, outLen, "raw 0x%02X / voltage decode unverified", raw);
  }
}

static uint32_t readValue(const uint8_t* data, const SpdEditField& f) {
  if (f.width == 1) return data[f.offset];
  if (f.width == 2) {
    if (f.littleEndian) return (uint16_t)data[f.offset] | ((uint16_t)data[f.offset + 1] << 8);
    return ((uint16_t)data[f.offset] << 8) | data[f.offset + 1];
  }
  return 0;
}

static void encodeValue(const SpdEditField& f, uint32_t value, uint8_t* out) {
  if (f.width == 1) {
    out[0] = (uint8_t)value;
  } else if (f.width == 2) {
    if (f.littleEndian) {
      out[0] = (uint8_t)(value & 0xFF);
      out[1] = (uint8_t)((value >> 8) & 0xFF);
    } else {
      out[0] = (uint8_t)((value >> 8) & 0xFF);
      out[1] = (uint8_t)(value & 0xFF);
    }
  }
}

const SpdEditField* spdEditFields(size_t& count) {
  count = sizeof(kFields) / sizeof(kFields[0]);
  return kFields;
}

const SpdEditField* spdEditFindField(const char* id) {
  if (!id) return nullptr;
  size_t count = 0;
  const SpdEditField* fields = spdEditFields(count);
  for (size_t i = 0; i < count; i++) {
    if (strcmp(fields[i].id, id) == 0) return &fields[i];
  }
  return nullptr;
}

bool spdEditParseValue(const char* s, uint32_t& out) {
  if (!s || !*s) return false;
  char* end = nullptr;
  out = strtoul(s, &end, 0);
  return end && *end == 0;
}

static bool spdEditBuildChangeWithImage(const SpdEditField& field, const char* valueText, const uint8_t* encodeData,
                                        SpdEditChange& change, char* err, size_t errLen) {
  memset(&change, 0, sizeof(change));
  change.field = &field;

  if (!gApp.currentSpdDump.valid || gApp.currentSpdDump.len != SPD_NVM_SIZE) {
    strlcpy(err, "No current 1024-byte SPD dump in RAM. Click Dump 1024 or save a tweak checkpoint first.", errLen);
    return false;
  }
  if (!field.editable) {
    strlcpy(err, "Field is read-only or unmapped.", errLen);
    return false;
  }
  if (field.offset + field.width > SPD_NVM_SIZE || field.width == 0 || field.width > sizeof(change.newBytes)) {
    strlcpy(err, "Field offset/width is invalid.", errLen);
    return false;
  }
  if (!encodeData) {
    strlcpy(err, "No staged SPD image available for encoding.", errLen);
    return false;
  }
  memcpy(change.oldBytes, &gApp.currentSpdDump.data[field.offset], field.width);
  uint32_t value = 0;
  uint32_t rawValue = 0;

  if (fieldUsesVoltage(field)) {
    uint32_t mv = 0;
    uint8_t raw = 0;
    if (!parseVoltageMv(valueText, mv) || !voltageMvToRaw(mv, raw)) {
      strlcpy(err, "Voltage must be one of verified encodings: 1.10, 1.25, 1.35, 1.40.", errLen);
      return false;
    }
    value = mv;
    rawValue = raw;
  } else if (fieldUsesTck(field)) {
    uint32_t mtps = 0;
    if (!parseSpeedMtps(valueText, mtps)) {
      strlcpy(err, "Speed must be a sane DDR5 profile data rate from 3200 to 9000 MT/s.", errLen);
      return false;
    }
    value = mtps;
    rawValue = 2000000UL / mtps;
  } else {
    if (!spdEditParseValue(valueText, value)) {
      strlcpy(err, "Value must be an integer cycle count.", errLen);
      return false;
    }
    if (value < field.minValue || value > field.maxValue) {
      strlcpy(err, "Value is outside allowed range.", errLen);
      return false;
    }
    if (field.editable && value == 0) {
      strlcpy(err, "Cycle count must be non-zero.", errLen);
      return false;
    }
    rawValue = value;
  }
  change.value = value;
  uint32_t nominalTck = nominalTckForField(field, encodeData);
  if (fieldUsesRaw256Ps(field)) {
    rawValue = spdCyclesToRaw256Ps(value, nominalTck);
  } else if (fieldUsesPs(field)) {
    rawValue = spdCyclesToRawPs(value, nominalTck);
  } else if (fieldUsesNs(field)) {
    rawValue = spdCyclesToRawNs(value, nominalTck);
  }
  if (rawValue == 0) {
    strlcpy(err, "Cycle count converts to an invalid zero raw value.", errLen);
    return false;
  }
  if ((field.width == 1 && rawValue > 0xFF) || (field.width == 2 && rawValue > 0xFFFF)) {
    strlcpy(err, "Cycle count converts outside the raw field width.", errLen);
    return false;
  }
  encodeValue(field, rawValue, change.newBytes);
  change.changed = memcmp(change.oldBytes, change.newBytes, field.width) != 0;
  return true;
}

bool spdEditBuildChange(const SpdEditField& field, const char* valueText, SpdEditChange& change, char* err, size_t errLen) {
  return spdEditBuildChangeWithImage(field, valueText, gApp.currentSpdDump.data, change, err, errLen);
}

static void appendFieldJson(String& j, const SpdEditField& f) {
  char raw[64];
  char off[20];
  char val[32];
  const uint8_t* data = gApp.currentSpdDump.data;

  if (f.width == 1) snprintf(off, sizeof(off), "0x%03X", f.offset);
  else snprintf(off, sizeof(off), "0x%03X-0x%03X", f.offset, (uint16_t)(f.offset + f.width - 1));

  if (gApp.currentSpdDump.valid && gApp.currentSpdDump.len == SPD_NVM_SIZE) {
    rawBytes(raw, sizeof(raw), data, f.offset, f.width);
    if (!f.editable && strcmp(f.id, "expo_sig") == 0 && f.offset + 3 < SPD_NVM_SIZE) {
      snprintf(val, sizeof(val), "%c%c%c%c", data[f.offset], data[f.offset + 1], data[f.offset + 2], data[f.offset + 3]);
    } else if (fieldUsesVoltage(f)) {
      formatVoltage(val, sizeof(val), data[f.offset]);
    } else if (fieldUsesTck(f)) {
      if (!formatTckRate(val, sizeof(val), spdReadLe16(data, f.offset))) strlcpy(val, raw, sizeof(val));
    } else if (strcmp(f.id, "expo_cas_mask") == 0) {
      snprintf(val, sizeof(val), "0x%04X", spdReadLe16(data, f.offset));
    } else if (!f.editable) {
      strlcpy(val, raw, sizeof(val));
    } else {
      uint32_t cycles = decodedCyclesForField(data, f);
      if (f.width == 1) snprintf(val, sizeof(val), "%luT (raw 0x%02X)", (unsigned long)cycles, data[f.offset]);
      else snprintf(val, sizeof(val), "%luT (raw %s)", (unsigned long)cycles, raw);
    }
  } else {
    strlcpy(raw, "no current dump", sizeof(raw));
    strlcpy(val, "not loaded", sizeof(val));
  }

  j += "{";
  j += "\"id\":\"" + jsonEscapeLocal(f.id) + "\",";
  j += "\"label\":\"" + jsonEscapeLocal(f.label) + "\",";
  j += "\"profile\":\"" + jsonEscapeLocal(f.profile) + "\",";
  j += "\"offset\":\"" + String(off) + "\",";
  j += "\"offset_num\":" + String((unsigned)f.offset) + ",";
  j += "\"width\":" + String((unsigned)f.width) + ",";
  j += "\"units\":\"" + jsonEscapeLocal(f.units) + "\",";
  j += "\"current\":\"" + String(val) + "\",";
  j += "\"raw\":\"" + String(raw) + "\",";
  j += "\"editable\":" + String(f.editable ? "true" : "false") + ",";
  j += "\"preview_only\":" + String(f.previewOnly ? "true" : "false") + ",";
  j += "\"experimental\":" + String(f.experimental ? "true" : "false") + ",";
  j += "\"min\":" + String((unsigned)f.minValue) + ",";
  j += "\"max\":" + String((unsigned)f.maxValue) + ",";
  const char* status = "decoded / read-only";
  if (f.previewOnly) status = "preview-only / CRC repair shown / apply blocked";
  else if (fieldUsesVoltage(f) && f.editable && fieldIsProfile(f, "XMP")) status = "editable / verified XMP voltage / crc-covered";
  else if (fieldUsesVoltage(f) && f.editable) status = "editable / verified EXPO voltage / crc-covered";
  else if (fieldUsesTck(f) && f.editable && fieldIsProfile(f, "XMP")) status = "editable / verified XMP tCK / crc-covered";
  else if (fieldUsesTck(f) && f.editable) status = "editable / verified EXPO tCK / crc-covered";
  else if (f.editable && fieldIsProfile(f, "XMP")) status = "editable / verified XMP timing / crc-covered";
  else if (f.editable) status = "editable / verified EXPO timing / crc-covered";
  j += "\"status\":\"" + String(status) + "\"";
  j += "}";
}

String spdEditFieldsJson() {
  if (!gApp.currentSpdDump.valid || gApp.currentSpdDump.len != SPD_NVM_SIZE) {
    return "{\"ok\":false,\"error\":\"No current 1024-byte SPD dump in RAM. Click Dump 1024 or save a tweak checkpoint first.\"}";
  }

  String j = "{\"ok\":true,\"dump\":{";
  j += "\"addr\":\"0x" + String(gApp.currentSpdDump.addr, HEX) + "\",";
  j += "\"crc32\":\"0x" + String(gApp.currentSpdDump.crc32, HEX) + "\",";
  j += "\"source\":\"" + jsonEscapeLocal(gApp.currentSpdDump.source) + "\"";
  j += "},\"crc_status\":" + expoCrcStatusJson() + ",";
  j += "\"xmp_crc_discovery\":" + xmpCrcDiscoveryJson() + ",";
  j += "\"secondary_candidates\":" + secondaryTimingCandidatesJson() + ",";
  char rawExpo[256];
  char rawXmp[256];
  rawBytes(rawExpo, sizeof(rawExpo), gApp.currentSpdDump.data, 0x340, 64);
  rawBytes(rawXmp, sizeof(rawXmp), gApp.currentSpdDump.data, 0x2C0, 64);
  j += "\"raw_expo\":\"" + String(rawExpo) + "\",";
  j += "\"raw_xmp\":\"" + String(rawXmp) + "\",";
  j += "\"expo_speed\":{";
  if (expoSignaturePresent(gApp.currentSpdDump.data)) {
    char speed[64];
    char raw[16];
    uint16_t tckPs = (uint16_t)gApp.currentSpdDump.data[0x34E] | ((uint16_t)gApp.currentSpdDump.data[0x34F] << 8);
    rawBytes(raw, sizeof(raw), gApp.currentSpdDump.data, 0x34E, 2);
    if (formatTckRate(speed, sizeof(speed), tckPs)) {
      j += "\"current\":\"" + String(speed) + "\",";
      j += "\"status\":\"decoded / crc-covered / editable\",";
    } else {
      j += "\"current\":\"EXPO speed: decode unavailable\",";
      j += "\"status\":\"decode unavailable / crc-covered\",";
    }
    j += "\"source\":\"0x34E-0x34F\",";
    j += "\"raw\":\"" + String(raw) + "\",";
    j += "\"editable\":true";
  } else {
    j += "\"current\":\"EXPO: not detected\",";
    j += "\"source\":\"0x34E-0x34F\",";
    j += "\"raw\":\"source offset not mapped\",";
    j += "\"status\":\"not decoded yet / read-only\",";
    j += "\"editable\":false";
  }
  j += "},\"fields\":[";
  size_t count = 0;
  const SpdEditField* fields = spdEditFields(count);
  for (size_t i = 0; i < count; i++) {
    if (i) j += ",";
    appendFieldJson(j, fields[i]);
  }
  j += "]}";
  return j;
}

static bool buildChanges(int count, const char* ids[], const char* values[], SpdEditChange* changes, uint8_t& changeCount, String& err) {
  changeCount = 0;
  if (!gApp.currentSpdDump.valid || gApp.currentSpdDump.len != SPD_NVM_SIZE) {
    err = "No current 1024-byte SPD dump in RAM. Click Dump 1024 or save a tweak checkpoint first.";
    return false;
  }
  if (count > 16) {
    err = "Too many fields selected for one preview.";
    return false;
  }

  const SpdEditField* fields[16];
  const char* texts[16];
  SpdEditChange stagedChanges[16];
  bool built[16];
  uint8_t staged[SPD_NVM_SIZE];
  memset(fields, 0, sizeof(fields));
  memset(texts, 0, sizeof(texts));
  memset(stagedChanges, 0, sizeof(stagedChanges));
  memset(built, 0, sizeof(built));
  memcpy(staged, gApp.currentSpdDump.data, SPD_NVM_SIZE);

  const char* profile = nullptr;
  for (int i = 0; i < count; i++) {
    if (ids[i] && (strcmp(ids[i], "expo_034a") == 0 || strcmp(ids[i], "friendly_datarate_candidate") == 0)) {
      err = "Old data-rate alias removed. EXPO data rate is decoded from tCK at 0x34E-0x34F.";
      return false;
    }
    const SpdEditField* f = spdEditFindField(ids[i]);
    if (!f) {
      err = String("Unknown field: ") + (ids[i] ? ids[i] : "(null)");
      return false;
    }
    if (!profile) profile = f->profile;
    else if (strcmp(profile, f->profile) != 0) {
      err = "Preview blocked: edits must target exactly one profile region.";
      return false;
    }
    fields[i] = f;
    texts[i] = values[i];
  }

  for (uint8_t pass = 0; pass < 2; pass++) {
    bool wantTck = pass == 0;
    for (int i = 0; i < count; i++) {
      const SpdEditField* f = fields[i];
      if (!f || fieldUsesTck(*f) != wantTck) continue;
      char e[96];
      if (!spdEditBuildChangeWithImage(*f, texts[i], staged, stagedChanges[i], e, sizeof(e))) {
        err = String(f->id) + ": " + e;
        return false;
      }
      memcpy(&staged[f->offset], stagedChanges[i].newBytes, f->width);
      if (fieldUsesCycleInput(*f)) {
        uint32_t finalCycles = decodedCyclesForField(staged, *f);
        if (finalCycles != stagedChanges[i].value) {
          err = String(f->id) + ": requested " + String((unsigned long)stagedChanges[i].value) +
                "T encodes to " + String((unsigned long)finalCycles) + "T with staged tCK.";
          return false;
        }
      }
      built[i] = true;
    }
  }

  for (int i = 0; i < count; i++) {
    if (!built[i]) {
      err = String(fields[i] ? fields[i]->id : "(unknown)") + ": internal preview staging failed.";
      return false;
    }
    changes[changeCount++] = stagedChanges[i];
  }
  return true;
}

static void appendDiffJson(String& j, const SpdEditChange& c, bool& first, const uint8_t* finalImage) {
  const SpdEditField& f = *c.field;
  char oldRaw[32];
  char newRaw[32];
  byteList(oldRaw, sizeof(oldRaw), c.oldBytes, f.width);
  byteList(newRaw, sizeof(newRaw), c.newBytes, f.width);
  if (!first) j += ",";
  first = false;
  uint32_t oldCycles = decodedCyclesForField(gApp.currentSpdDump.data, f);
  uint32_t finalCycles = finalImage ? decodedCyclesForField(finalImage, f) : c.value;
  j += "{";
  j += "\"profile\":\"" + jsonEscapeLocal(f.profile) + "\",";
  j += "\"field\":\"" + jsonEscapeLocal(f.id) + "\",";
  j += "\"label\":\"" + jsonEscapeLocal(f.label) + "\",";
  if (fieldUsesVoltage(f)) {
    char oldV[32];
    char newV[32];
    formatVoltage(oldV, sizeof(oldV), c.oldBytes[0]);
    formatVoltage(newV, sizeof(newV), c.newBytes[0]);
    j += "\"input\":\"" + String((unsigned long)(c.value / 1000)) + "." + (c.value % 1000 < 100 ? String("0") : String("")) + String((unsigned long)((c.value % 1000) / 10)) + " V\",";
    j += "\"old_decoded\":\"" + String(oldV) + "\",";
    j += "\"new_decoded\":\"" + String(newV) + "\",";
  } else if (fieldUsesTck(f)) {
    char oldSpeed[64];
    char newSpeed[64];
    formatTckRate(oldSpeed, sizeof(oldSpeed), (uint16_t)readValue(gApp.currentSpdDump.data, f));
    uint16_t newTck = finalImage ? spdReadLe16(finalImage, f.offset)
                                 : (f.littleEndian ? ((uint16_t)c.newBytes[0] | ((uint16_t)c.newBytes[1] << 8))
                                                   : (((uint16_t)c.newBytes[0] << 8) | c.newBytes[1]));
    uint32_t derivedMtps = spdRawMtpsFromTckPs(newTck);
    snprintf(newSpeed, sizeof(newSpeed), "staged tCK %u ps / derived ~DDR5-%lu",
             (unsigned)newTck, (unsigned long)derivedMtps);
    j += "\"input\":\"DDR5-" + String((unsigned long)c.value) + " requested\",";
    j += "\"old_decoded\":\"" + String(oldSpeed) + "\",";
    j += "\"new_decoded\":\"" + String(newSpeed) + "\",";
  } else {
    j += "\"input_cycles\":" + String((unsigned long)c.value) + ",";
    j += "\"old_cycles\":" + String((unsigned long)oldCycles) + ",";
    j += "\"new_cycles\":" + String((unsigned long)finalCycles) + ",";
  }
  j += "\"offset\":\"0x" + String(f.offset, HEX) + (f.width > 1 ? String("-0x") + String((uint16_t)(f.offset + f.width - 1), HEX) : String("")) + "\",";
  j += "\"old\":\"0x" + String(oldRaw) + "\",";
  j += "\"new\":\"0x" + String(newRaw) + "\",";
  j += "\"apply_allowed\":" + String(f.previewOnly ? "false" : "true") + ",";
  j += "\"noop\":" + String(c.changed ? "false" : "true");
  j += "}";
}

static uint8_t changedFieldCount(const SpdEditChange* changes, uint8_t changeCount) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < changeCount; i++) {
    if (changes[i].changed) n++;
  }
  return n;
}

static bool changesAreApplyAllowed(const SpdEditChange* changes, uint8_t changeCount) {
  for (uint8_t i = 0; i < changeCount; i++) {
    if (changes[i].field && changes[i].field->previewOnly) return false;
  }
  return true;
}

static bool isPrimaryTimingField(const SpdEditField& f) {
  return strstr(f.id, "_taa") || strstr(f.id, "_trcd") || strstr(f.id, "_trp") ||
         strstr(f.id, "_tras") || strstr(f.id, "_trc");
}

static uint16_t profileTckOffset(const char* profile) {
  return (profile && strcmp(profile, "XMP") == 0) ? 0x2C5 : 0x34E;
}

static uint16_t profileVddOffset(const char* profile) {
  return (profile && strcmp(profile, "XMP") == 0) ? 0x2C1 : 0x34A;
}

static uint16_t profileVddqOffset(const char* profile) {
  return (profile && strcmp(profile, "XMP") == 0) ? 0x2C2 : 0x34B;
}

static void appendWarningJson(String& j, bool& first, const char* msg) {
  if (!msg || !*msg) return;
  if (!first) j += ",";
  first = false;
  j += "\"" + jsonEscapeLocal(msg) + "\"";
}

static String previewWarningsJson(const SpdEditChange* changes, uint8_t changeCount, const uint8_t* repaired, const char* profile) {
  bool speedChanged = false;
  bool primaryTimingChanged = false;
  for (uint8_t i = 0; i < changeCount; i++) {
    if (!changes[i].field || !changes[i].changed) continue;
    if (fieldUsesTck(*changes[i].field)) speedChanged = true;
    if (isPrimaryTimingField(*changes[i].field)) primaryTimingChanged = true;
  }

  String j = "[";
  bool first = true;
  if (speedChanged && !primaryTimingChanged) {
    appendWarningJson(j, first, "Data rate / tCK changed while primary timings were left unchanged. BIOS readability will improve, but the profile may be inconsistent until tAA/tRCD/tRP/tRAS/tRC are reviewed.");
  }
  for (uint8_t i = 0; i < changeCount; i++) {
    if (!changes[i].field || !changes[i].changed || !fieldUsesTck(*changes[i].field)) continue;
    const SpdEditField& f = *changes[i].field;
    uint16_t stagedTck = spdReadLe16(repaired, f.offset);
    uint32_t derivedMtps = spdRawMtpsFromTckPs(stagedTck);
    if (derivedMtps && absDeltaU32(derivedMtps, changes[i].value) > 50) {
      appendWarningJson(j, first, "Requested data rate and staged tCK-derived rate differ beyond rounding tolerance. Preview uses the staged tCK bytes as the timing source of truth.");
    }
  }

  uint16_t tckPs = spdReadLe16(repaired, profileTckOffset(profile));
  uint32_t mtps = spdRawMtpsFromTckPs(tckPs);
  uint16_t vddMv = 0;
  uint16_t vddqMv = 0;
  bool vddOk = decodeProfileVoltageTenthsHundredthsCandidate(repaired[profileVddOffset(profile)], vddMv);
  bool vddqOk = decodeProfileVoltageTenthsHundredthsCandidate(repaired[profileVddqOffset(profile)], vddqMv);
  if (mtps >= 6000 && vddOk && vddqOk && (vddMv < 1250 || vddqMv < 1250)) {
    appendWarningJson(j, first, "High-speed profile with VDD/VDDQ below 1.25 V. This is a warning only; adjust voltage if your module/profile requires it.");
  }
  j += "]";
  return j;
}

String spdEditPreviewJson(int count, const char* ids[], const char* values[]) {
  SpdEditChange changes[16];
  uint8_t changeCount = 0;
  String err;
  if (!buildChanges(count, ids, values, changes, changeCount, err)) {
    gPreviewValid = false;
    return "{\"ok\":false,\"error\":\"" + jsonEscapeLocal(err.c_str()) + "\"}";
  }
  if (changeCount == 0) {
    gPreviewValid = false;
    return "{\"ok\":false,\"error\":\"No changed bytes to preview.\"}";
  }
  uint8_t repaired[SPD_NVM_SIZE];
  uint8_t oldCrc[2] = {0};
  uint8_t newCrc[2] = {0};
  uint16_t crcLo = 0;
  const char* profile = nullptr;
  if (!buildRepairedProfileImage(changes, changeCount, repaired, oldCrc, newCrc, crcLo, profile, err)) {
    gPreviewValid = false;
    return "{\"ok\":false,\"error\":\"" + jsonEscapeLocal(err.c_str()) + "\",\"crc_repair_supported\":false}";
  }
  uint8_t actualChanged = changedFieldCount(changes, changeCount);
  bool applyAllowed = actualChanged > 0 && changesAreApplyAllowed(changes, changeCount);
  gPreviewValid = applyAllowed;
  gPreviewSig = previewSig(changes, changeCount);
  gPreviewGeneration = gApp.currentSpdDump.generation;

  String j = "{\"ok\":true,\"changed_fields\":" + String((unsigned)actualChanged) + ",";
  j += "\"requested_fields\":" + String((unsigned)changeCount) + ",";
  j += "\"apply_allowed\":" + String(applyAllowed ? "true" : "false") + ",";
  j += "\"crc_repair_supported\":true,";
  j += "\"profile\":\"" + jsonEscapeLocal(profile) + "\",";
  j += "\"crc_warning\":\"Automatic " + String(profile) + " CRC repair will update 0x" + String(crcLo, HEX) + "-0x" + String((uint16_t)(crcLo + 1), HEX) + ".\",";
  j += "\"crc_status\":" + String(strcmp(profile, "XMP") == 0 ? xmpCrcDiscoveryJson() : expoCrcStatusJson()) + ",";
  j += "\"warnings\":" + previewWarningsJson(changes, changeCount, repaired, profile) + ",";
  j += "\"diff\":[";
  bool first = true;
  for (uint8_t i = 0; i < changeCount; i++) appendDiffJson(j, changes[i], first, repaired);
  for (uint8_t i = 0; i < 2; i++) {
    if (oldCrc[i] == newCrc[i]) continue;
    if (!first) j += ",";
    first = false;
    j += "{";
    j += "\"field\":\"profile_crc\",";
    j += "\"label\":\"Automatic " + String(profile) + " CRC repair\",";
    j += "\"offset\":\"0x" + String((uint16_t)(crcLo + i), HEX) + "\",";
    j += "\"old\":\"0x" + String(oldCrc[i], HEX) + "\",";
    j += "\"new\":\"0x" + String(newCrc[i], HEX) + "\"";
    j += "}";
  }
  j += "]}";
  return j;
}

String spdEditApplyJson(bool armed, int count, const char* ids[], const char* values[]) {
  if (!armed) return "{\"ok\":false,\"error\":\"Lab SPD Write Mode is not armed. Check the risk acknowledgement and type LABMODE.\"}";
  if (!gApp.spdBackupValid) return "{\"ok\":false,\"error\":\"Write blocked: tweak checkpoint is missing.\"}";

  SpdEditChange changes[16];
  uint8_t changeCount = 0;
  String err;
  if (!buildChanges(count, ids, values, changes, changeCount, err)) {
    return "{\"ok\":false,\"error\":\"" + jsonEscapeLocal(err.c_str()) + "\"}";
  }
  if (changeCount == 0 || changedFieldCount(changes, changeCount) == 0) return "{\"ok\":false,\"error\":\"No changed bytes to write.\"}";
  if (!changesAreApplyAllowed(changes, changeCount)) {
    return "{\"ok\":false,\"error\":\"Write blocked: one or more selected fields are preview-only until validated.\"}";
  }
  uint8_t repaired[SPD_NVM_SIZE];
  uint8_t oldCrc[2] = {0};
  uint8_t newCrc[2] = {0};
  uint16_t crcLo = 0;
  const char* profile = nullptr;
  if (!buildRepairedProfileImage(changes, changeCount, repaired, oldCrc, newCrc, crcLo, profile, err)) {
    return "{\"ok\":false,\"error\":\"" + jsonEscapeLocal(err.c_str()) + "\"}";
  }
  if (!gPreviewValid || gPreviewGeneration != gApp.currentSpdDump.generation || gPreviewSig != previewSig(changes, changeCount)) {
    return "{\"ok\":false,\"error\":\"Write blocked: preview diff has not been generated for these exact changes.\"}";
  }

  uint8_t addr = gApp.currentSpdDump.addr;
  if (!unlockAllNvmBlocks(addr)) return "{\"ok\":false,\"error\":\"Unlock failed; SPD NVM was not written.\"}";

  for (uint8_t i = 0; i < changeCount; i++) {
    const SpdEditField& f = *changes[i].field;
    for (uint8_t b = 0; b < f.width; b++) {
      if (changes[i].oldBytes[b] == changes[i].newBytes[b]) continue;
      uint16_t off = f.offset + b;
      uint8_t v = repaired[off];
      if (!spdNvmWrite(addr, off, &v, 1)) {
        return "{\"ok\":false,\"error\":\"SPD byte write failed at 0x" + String(off, HEX) + "\"}";
      }
    }
  }
  for (uint8_t b = 0; b < 2; b++) {
    if (oldCrc[b] == newCrc[b]) continue;
    uint16_t off = crcLo + b;
    uint8_t v = repaired[off];
    if (!spdNvmWrite(addr, off, &v, 1)) {
      return "{\"ok\":false,\"error\":\"Profile CRC byte write failed at 0x" + String(off, HEX) + "\"}";
    }
  }

  for (uint8_t i = 0; i < changeCount; i++) {
    const SpdEditField& f = *changes[i].field;
    uint8_t rb[4] = {0};
    if (!spdNvmRead(addr, f.offset, rb, f.width)) {
      return "{\"ok\":false,\"error\":\"Readback failed at 0x" + String(f.offset, HEX) + "\"}";
    }
    if (memcmp(rb, &repaired[f.offset], f.width) != 0) {
      return "{\"ok\":false,\"error\":\"Readback verify mismatch at 0x" + String(f.offset, HEX) + "\"}";
    }
    memcpy(&gApp.currentSpdDump.data[f.offset], &repaired[f.offset], f.width);
    memcpy(&gApp.lastDump[f.offset], &repaired[f.offset], f.width);
  }
  for (uint8_t b = 0; b < 2; b++) {
    uint16_t off = crcLo + b;
    uint8_t rb = 0;
    if (!spdNvmRead(addr, off, &rb, 1)) {
      return "{\"ok\":false,\"error\":\"Profile CRC readback failed at 0x" + String(off, HEX) + "\"}";
    }
    if (rb != repaired[off]) {
      return "{\"ok\":false,\"error\":\"Profile CRC readback verify mismatch at 0x" + String(off, HEX) + "\"}";
    }
    gApp.currentSpdDump.data[off] = rb;
    gApp.lastDump[off] = rb;
  }

  gApp.currentSpdDump.crc32 = crc32_buf(gApp.currentSpdDump.data, SPD_NVM_SIZE);
  gApp.currentSpdDump.generation++;
  strlcpy(gApp.currentSpdDump.source, "spdedit", sizeof(gApp.currentSpdDump.source));
  gApp.lastDumpValid = true;
  gPreviewValid = false;

  String j = "{\"ok\":true,\"message\":\"PASS: changed bytes written and readback verified\",";
  j += "\"crc32\":\"0x" + String(gApp.currentSpdDump.crc32, HEX) + "\",";
  j += "\"generation\":" + String((unsigned long)gApp.currentSpdDump.generation) + "}";
  return j;
}

void cmdSpdEdit(int nt, char* tok[]) {
  String sub = (nt >= 2) ? String(tok[1]) : String("fields");
  sub.toLowerCase();
  if (sub == "fields") {
    size_t count = 0;
    const SpdEditField* fields = spdEditFields(count);
    outPrintln("SPD EDIT FIELDS");
    for (size_t i = 0; i < count; i++) {
      const SpdEditField& f = fields[i];
      outPrintf("  %s  %s  %s  off=0x%03X width=%u %s\n",
                f.id, f.profile, f.label, f.offset, (unsigned)f.width,
                f.previewOnly ? "preview-only" : (f.editable ? "editable/experimental" : "read-only"));
    }
    latchCommandResult(true);
    return;
  }

  bool apply = (sub == "apply");
  bool preview = (sub == "preview");
  if (!apply && !preview) {
    outPrintln("Usage: spdedit fields | spdedit preview field=value... | spdedit apply labmode field=value...");
    latchCommandResult(false);
    return;
  }

  const char* ids[16] = {0};
  const char* vals[16] = {0};
  int n = 0;
  bool labmode = false;
  for (int i = 2; i < nt; i++) {
    if (strcmp(tok[i], "labmode") == 0 || strcmp(tok[i], "LABMODE") == 0) {
      labmode = true;
      continue;
    }
    char* eq = strchr(tok[i], '=');
    if (eq && n < 16) {
      *eq = 0;
      ids[n] = tok[i];
      vals[n] = eq + 1;
      n++;
    }
  }

  String j = apply ? spdEditApplyJson(labmode, n, ids, vals) : spdEditPreviewJson(n, ids, vals);
  outPrintln(j.c_str());
  latchCommandResult(j.indexOf("\"ok\":true") >= 0);
}
