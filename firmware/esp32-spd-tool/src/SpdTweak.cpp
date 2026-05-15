#include "SpdTweak.h"
#include "Log.h"
#include "SpdHub.h"
#include <string.h>

static void addField(SpdTweakDecode& out, const char* key, const char* label, const char* value,
                     const char* source, const char* raw, bool editable, const char* status,
                     const char* note) {
  if (out.count >= sizeof(out.fields) / sizeof(out.fields[0])) return;
  SpdDecodedField& f = out.fields[out.count++];
  f.key = key;
  f.label = label;
  strlcpy(f.value, value ? value : "", sizeof(f.value));
  strlcpy(f.source, source ? source : "", sizeof(f.source));
  strlcpy(f.raw, raw ? raw : "", sizeof(f.raw));
  strlcpy(f.status, status ? status : "", sizeof(f.status));
  f.editable = editable;
  f.note = note ? note : "";
}

static void rawRange(char* out, size_t outLen, const uint8_t* spd, uint16_t off, uint8_t len) {
  if (!out || outLen == 0) return;
  out[0] = 0;
  char tmp[8];
  snprintf(out, outLen, "0x%03X:", off);
  for (uint8_t i = 0; i < len && (off + i) < SPD_NVM_SIZE; i++) {
    snprintf(tmp, sizeof(tmp), " %02X", spd[off + i]);
    strlcat(out, tmp, outLen);
  }
}

static bool findSignature(const uint8_t* spd, const char* sig, uint16_t& off) {
  size_t n = strlen(sig);
  if (!spd || n == 0 || n > SPD_NVM_SIZE) return false;
  for (uint16_t i = 0; i <= SPD_NVM_SIZE - n; i++) {
    bool match = true;
    for (size_t j = 0; j < n; j++) {
      if (spd[i + j] != (uint8_t)sig[j]) { match = false; break; }
    }
    if (match) {
      off = i;
      return true;
    }
  }
  return false;
}

static void asciiFromRange(char* out, size_t outLen, const uint8_t* spd, uint16_t off, uint8_t len) {
  if (!out || outLen == 0) return;
  uint8_t n = 0;
  for (uint8_t i = 0; i < len && n < outLen - 1 && (off + i) < SPD_NVM_SIZE; i++) {
    uint8_t c = spd[off + i];
    out[n++] = (c >= 0x20 && c <= 0x7E) ? (char)c : ' ';
  }
  out[n] = 0;
  while (n > 0 && out[n - 1] == ' ') out[--n] = 0;
}

static bool looksUsefulAscii(const char* s) {
  uint8_t useful = 0;
  if (!s) return false;
  while (*s) {
    if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9')) useful++;
    s++;
  }
  return useful >= 4;
}

static const char* memoryTypeName(uint8_t v) {
  if (v == 0x12) return "DDR5 SDRAM";
  return "not decoded yet";
}

static const char* moduleTypeName(uint8_t v) {
  switch (v & 0x0F) {
    case 0x01: return "RDIMM";
    case 0x02: return "UDIMM";
    case 0x03: return "SODIMM";
    case 0x04: return "LRDIMM";
    case 0x0A: return "DDIMM";
    default: return "unknown / raw module type";
  }
}

uint16_t spdReadLe16(const uint8_t* data, uint16_t off) {
  uint16_t out = 0;
  if (!spdReadLe16(data, SPD_NVM_SIZE, off, &out)) return 0;
  return out;
}

bool spdReadU8(const uint8_t* data, size_t len, uint16_t off, uint8_t* out) {
  if (out) *out = 0;
  if (!data || !out) return false;
  if ((size_t)off >= len) return false;
  *out = data[off];
  return true;
}

bool spdReadLe16(const uint8_t* data, size_t len, uint16_t off, uint16_t* out) {
  if (out) *out = 0;
  if (!data || !out) return false;
  if ((size_t)off + 1 >= len) return false;
  *out = (uint16_t)data[off] | ((uint16_t)data[off + 1] << 8);
  return true;
}

uint32_t spdRawMtpsFromTckPs(uint16_t tckPs) {
  if (tckPs == 0) return 0;
  return 2000000UL / tckPs;
}

uint32_t spdNearestDdr5Bin(uint32_t rawMtps) {
  static const uint16_t bins[] = {4000, 4400, 4800, 5200, 5600, 6000, 6200, 6400, 6800, 7200, 7600, 8000};
  if (rawMtps == 0) return 0;
  uint16_t best = bins[0];
  uint32_t bestDelta = (rawMtps > best) ? (rawMtps - best) : (best - rawMtps);
  for (uint8_t i = 1; i < sizeof(bins) / sizeof(bins[0]); i++) {
    uint32_t delta = (rawMtps > bins[i]) ? (rawMtps - bins[i]) : (bins[i] - rawMtps);
    if (delta < bestDelta) {
      best = bins[i];
      bestDelta = delta;
    }
  }
  return best;
}

uint32_t spdMclkFromMtps(uint32_t mtps) {
  return mtps / 2;
}

uint32_t spdNominalTckPsX1000FromMtps(uint32_t mtps) {
  uint32_t mclk = spdMclkFromMtps(mtps);
  if (mclk == 0) return 0;
  return (1000000000UL + (mclk / 2)) / mclk;
}

uint32_t spdNominalTckPsX1000FromTckPs(uint16_t rawTckPs) {
  uint32_t mtps = spdNearestDdr5Bin(spdRawMtpsFromTckPs(rawTckPs));
  return spdNominalTckPsX1000FromMtps(mtps);
}

uint32_t spdPsToCycles(uint32_t ps, uint32_t nominalTckPsX1000) {
  if (ps == 0 || nominalTckPsX1000 == 0) return 0;
  uint64_t num = (uint64_t)ps * 1000ULL;
  return (uint32_t)((num + nominalTckPsX1000 / 2) / nominalTckPsX1000);
}

uint32_t spdNsToCycles(uint32_t ns, uint32_t nominalTckPsX1000) {
  if (ns == 0 || nominalTckPsX1000 == 0) return 0;
  uint64_t num = (uint64_t)ns * 1000000ULL;
  return (uint32_t)((num + nominalTckPsX1000 / 2) / nominalTckPsX1000);
}

uint32_t spdCyclesToRaw256Ps(uint32_t cycles, uint32_t nominalTckPsX1000) {
  if (cycles == 0 || nominalTckPsX1000 == 0) return 0;
  uint64_t num = (uint64_t)cycles * nominalTckPsX1000;
  return (uint32_t)((num + 128000ULL) / 256000ULL);
}

uint32_t spdCyclesToRawPs(uint32_t cycles, uint32_t nominalTckPsX1000) {
  if (cycles == 0 || nominalTckPsX1000 == 0) return 0;
  uint64_t num = (uint64_t)cycles * nominalTckPsX1000;
  return (uint32_t)((num + 500ULL) / 1000ULL);
}

uint32_t spdCyclesToRawNs(uint32_t cycles, uint32_t nominalTckPsX1000) {
  if (cycles == 0 || nominalTckPsX1000 == 0) return 0;
  uint64_t num = (uint64_t)cycles * nominalTckPsX1000;
  return (uint32_t)((num + 500000ULL) / 1000000ULL);
}

static void fillRateFromTck(uint16_t tckPs, uint32_t& mtps, uint32_t& mclk) {
  mtps = 0;
  mclk = 0;
  if (tckPs == 0) return;
  mtps = spdNearestDdr5Bin(spdRawMtpsFromTckPs(tckPs));
  mclk = spdMclkFromMtps(mtps);
}

static bool addTimingRow(TimingSection& section, const char* label, const char* current,
                         const char* source, const char* status, bool editable) {
  if (section.count >= sizeof(section.rows) / sizeof(section.rows[0])) return false;
  TimingRow& row = section.rows[section.count++];
  row.label = label ? label : "";
  strlcpy(row.current, current ? current : "", sizeof(row.current));
  strlcpy(row.source, source ? source : "", sizeof(row.source));
  strlcpy(row.status, status ? status : "", sizeof(row.status));
  row.editable = editable;
  return true;
}

static void formatRateValue(char* out, size_t outLen, uint16_t tckPs) {
  uint32_t mtps = 0, mclk = 0;
  fillRateFromTck(tckPs, mtps, mclk);
  if (tckPs == 0 || mtps == 0) {
    strlcpy(out, "decode unavailable", outLen);
    return;
  }
  snprintf(out, outLen, "DDR5-%lu / MCLK ~%lu / tCK %u ps",
           (unsigned long)mtps, (unsigned long)mclk, (unsigned)tckPs);
}

static void formatPsCycleValue(char* out, size_t outLen, uint32_t ps, uint32_t tckX1000) {
  uint32_t cycles = spdPsToCycles(ps, tckX1000);
  if (cycles == 0) strlcpy(out, "decode unavailable", outLen);
  else snprintf(out, outLen, "%luT / %lu.%03lu ns", (unsigned long)cycles,
                (unsigned long)(ps / 1000), (unsigned long)(ps % 1000));
}

static uint32_t psToCyclesCeil(uint32_t ps, uint32_t tckX1000) {
  uint32_t cycles = 0;
  if (ps > 0 && tckX1000 > 0) {
    uint64_t num = (uint64_t)ps * 1000ULL;
    cycles = (uint32_t)((num + tckX1000 - 1) / tckX1000);
  }
  return cycles;
}

static uint32_t nsToCyclesCeil(uint32_t ns, uint32_t tckX1000) {
  uint32_t cycles = 0;
  if (ns > 0 && tckX1000 > 0) {
    uint64_t num = (uint64_t)ns * 1000000ULL;
    cycles = (uint32_t)((num + tckX1000 - 1) / tckX1000);
  }
  return cycles;
}

static void formatPsCycleValueCeil(char* out, size_t outLen, uint32_t ps, uint32_t tckX1000) {
  uint32_t cycles = psToCyclesCeil(ps, tckX1000);
  if (cycles == 0) strlcpy(out, "decode unavailable", outLen);
  else snprintf(out, outLen, "%luT / %lu.%03lu ns", (unsigned long)cycles,
                (unsigned long)(ps / 1000), (unsigned long)(ps % 1000));
}

static void formatPsMinValueNsFirst(char* out, size_t outLen, uint32_t ps, uint32_t tckX1000) {
  uint32_t cycles = psToCyclesCeil(ps, tckX1000);
  if (cycles == 0) strlcpy(out, "decode unavailable", outLen);
  else snprintf(out, outLen, "%lu.%03lu ns / %luT min",
                (unsigned long)(ps / 1000), (unsigned long)(ps % 1000), (unsigned long)cycles);
}

static void formatNsCycleValue(char* out, size_t outLen, uint32_t ns, uint32_t tckX1000) {
  uint32_t cycles = spdNsToCycles(ns, tckX1000);
  if (cycles == 0) strlcpy(out, "decode unavailable", outLen);
  else snprintf(out, outLen, "%luT / %lu ns", (unsigned long)cycles, (unsigned long)ns);
}

static void formatNsCycleValueCeil(char* out, size_t outLen, uint32_t ns, uint32_t tckX1000) {
  uint32_t cycles = nsToCyclesCeil(ns, tckX1000);
  if (cycles == 0) strlcpy(out, "decode unavailable", outLen);
  else snprintf(out, outLen, "%luT / %lu ns", (unsigned long)cycles, (unsigned long)ns);
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

static void formatProfileVoltageValue(char* out, size_t outLen, uint8_t raw) {
  uint16_t mv = 0;
  if (decodeProfileVoltageTenthsHundredthsCandidate(raw, mv)) {
    snprintf(out, outLen, "%u.%02u V", (unsigned)(mv / 1000), (unsigned)((mv % 1000) / 10));
  } else {
    snprintf(out, outLen, "raw 0x%02X / voltage decode unverified", raw);
  }
}

static bool readTckX1000(const uint8_t* data, size_t len, uint16_t off, uint16_t& tckPs, uint32_t& tckX1000) {
  tckPs = 0;
  tckX1000 = 0;
  if (!spdReadLe16(data, len, off, &tckPs) || tckPs == 0) return false;
  tckX1000 = spdNominalTckPsX1000FromTckPs(tckPs);
  return tckX1000 > 0;
}

static bool appendPs16Row(TimingSection& section, const uint8_t* data, size_t len, const char* label,
                          uint16_t off, uint32_t tckX1000, bool editable) {
  uint16_t raw = 0;
  char value[48];
  char source[24];
  snprintf(source, sizeof(source), "0x%03X-0x%03X", off, (uint16_t)(off + 1));
  if (!spdReadLe16(data, len, off, &raw)) strlcpy(value, "unmapped", sizeof(value));
  else formatPsCycleValue(value, sizeof(value), raw, tckX1000);
  return addTimingRow(section, label, value, source, editable ? "editable" : "read-only", editable);
}

static bool appendPs16MinRow(TimingSection& section, const uint8_t* data, size_t len, const char* label,
                             uint16_t off, uint32_t tckX1000) {
  uint16_t raw = 0;
  char value[48];
  char source[24];
  snprintf(source, sizeof(source), "0x%03X-0x%03X", off, (uint16_t)(off + 1));
  if (!spdReadLe16(data, len, off, &raw)) strlcpy(value, "unmapped", sizeof(value));
  else formatPsMinValueNsFirst(value, sizeof(value), raw, tckX1000);
  return addTimingRow(section, label, value, source, "read-only", false);
}

static bool appendPs16CeilRow(TimingSection& section, const uint8_t* data, size_t len, const char* label,
                              uint16_t off, uint32_t tckX1000) {
  uint16_t raw = 0;
  char value[48];
  char source[24];
  snprintf(source, sizeof(source), "0x%03X-0x%03X", off, (uint16_t)(off + 1));
  if (!spdReadLe16(data, len, off, &raw)) strlcpy(value, "unmapped", sizeof(value));
  else formatPsCycleValueCeil(value, sizeof(value), raw, tckX1000);
  return addTimingRow(section, label, value, source, "read-only", false);
}

static bool appendPs256U8Row(TimingSection& section, const uint8_t* data, size_t len, const char* label,
                             uint16_t off, uint32_t tckX1000, bool editable) {
  uint8_t raw = 0;
  char value[48];
  char source[24];
  snprintf(source, sizeof(source), "0x%03X", off);
  if (!spdReadU8(data, len, off, &raw)) strlcpy(value, "unmapped", sizeof(value));
  else formatPsCycleValue(value, sizeof(value), (uint32_t)raw * 256UL, tckX1000);
  return addTimingRow(section, label, value, source, editable ? "editable" : "read-only", editable);
}

static bool appendNs16CeilRow(TimingSection& section, const uint8_t* data, size_t len, const char* label,
                              uint16_t off, uint32_t tckX1000) {
  uint16_t raw = 0;
  char value[48];
  char source[24];
  snprintf(source, sizeof(source), "0x%03X-0x%03X", off, (uint16_t)(off + 1));
  if (!spdReadLe16(data, len, off, &raw)) strlcpy(value, "unmapped", sizeof(value));
  else formatNsCycleValueCeil(value, sizeof(value), raw, tckX1000);
  return addTimingRow(section, label, value, source, "read-only", false);
}

static bool appendNs16Row(TimingSection& section, const uint8_t* data, size_t len, const char* label,
                          uint16_t off, uint32_t tckX1000, bool editable) {
  uint16_t raw = 0;
  char value[48];
  char source[24];
  snprintf(source, sizeof(source), "0x%03X-0x%03X", off, (uint16_t)(off + 1));
  if (!spdReadLe16(data, len, off, &raw)) strlcpy(value, "unmapped", sizeof(value));
  else formatNsCycleValue(value, sizeof(value), raw, tckX1000);
  return addTimingRow(section, label, value, source, editable ? "editable" : "read-only", editable);
}

static bool appendNsU8Row(TimingSection& section, const uint8_t* data, size_t len, const char* label,
                          uint16_t off, uint32_t tckX1000, bool editable) {
  uint8_t raw = 0;
  char value[48];
  char source[24];
  snprintf(source, sizeof(source), "0x%03X", off);
  if (!spdReadU8(data, len, off, &raw)) strlcpy(value, "unmapped", sizeof(value));
  else formatNsCycleValue(value, sizeof(value), raw, tckX1000);
  return addTimingRow(section, label, value, source, editable ? "editable" : "read-only", editable);
}

static bool appendNsU8CeilRow(TimingSection& section, const uint8_t* data, size_t len, const char* label,
                              uint16_t off, uint32_t tckX1000) {
  uint8_t raw = 0;
  char value[48];
  char source[24];
  snprintf(source, sizeof(source), "0x%03X", off);
  if (!spdReadU8(data, len, off, &raw)) strlcpy(value, "unmapped", sizeof(value));
  else formatNsCycleValueCeil(value, sizeof(value), raw, tckX1000);
  return addTimingRow(section, label, value, source, "read-only", false);
}

bool spdTweakExpoCrcStatus(const uint8_t* data, size_t len, bool& present, bool& pass, uint16_t& stored, uint16_t& calculated) {
  present = false;
  pass = false;
  stored = 0;
  calculated = 0;
  uint8_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;
  if (!spdReadU8(data, len, 0x340, &b0) || !spdReadU8(data, len, 0x341, &b1) ||
      !spdReadU8(data, len, 0x342, &b2) || !spdReadU8(data, len, 0x343, &b3)) return false;
  present = (b0 == 'E' && b1 == 'X' && b2 == 'P' && b3 == 'O');
  if (!present) return true;
  if (!spdReadLe16(data, len, 0x3BE, &stored)) return false;
  if (len <= 0x3BD) return false;
  uint16_t crc = 0;
  for (uint16_t off = 0x340; off <= 0x3BD; off++) {
    uint8_t v = 0;
    if (!spdReadU8(data, len, off, &v)) return false;
    crc ^= (uint16_t)v << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  calculated = crc;
  pass = (stored == calculated);
  return true;
}

bool spdTweakDecodeBaseSummary(const uint8_t* data, size_t len, SpdBaseDecodeSummary& out) {
  memset(&out, 0, sizeof(out));
  if (!data) {
    strlcpy(out.error, "null SPD buffer", sizeof(out.error));
    return false;
  }
  if (len < SPD_NVM_SIZE) {
    strlcpy(out.error, "SPD buffer shorter than 1024 bytes", sizeof(out.error));
    return false;
  }
  for (uint8_t i = 0; i < 4; i++) {
    if (!spdReadU8(data, len, i, &out.header[i])) {
      strlcpy(out.error, "header read failed", sizeof(out.error));
      return false;
    }
  }
  spdReadU8(data, len, 0x02, &out.generationRaw);
  spdReadU8(data, len, 0x03, &out.moduleTypeRaw);
  out.ddr5Like = (out.header[0] == 0x30 && out.header[1] == 0x10 && out.header[2] == 0x12);
  if (spdReadLe16(data, len, 0x14, &out.tckPs) && out.tckPs > 0) {
    fillRateFromTck(out.tckPs, out.mtps, out.mclk);
  }
  out.ok = true;
  return true;
}

bool spdTweakDecodeExpoSummary(const uint8_t* data, size_t len, SpdExpoDecodeSummary& out) {
  memset(&out, 0, sizeof(out));
  if (!data || len < SPD_NVM_SIZE) {
    strlcpy(out.status, "EXPO not decoded: invalid buffer", sizeof(out.status));
    return false;
  }
  uint8_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;
  if (!spdReadU8(data, len, 0x340, &b0) || !spdReadU8(data, len, 0x341, &b1) ||
      !spdReadU8(data, len, 0x342, &b2) || !spdReadU8(data, len, 0x343, &b3)) {
    strlcpy(out.status, "EXPO not detected", sizeof(out.status));
    return true;
  }
  out.detected = (b0 == 'E' && b1 == 'X' && b2 == 'P' && b3 == 'O');
  if (!out.detected) {
    strlcpy(out.status, "EXPO not detected", sizeof(out.status));
    return true;
  }
  if (spdReadLe16(data, len, 0x34E, &out.tckPs) && out.tckPs > 0) {
    fillRateFromTck(out.tckPs, out.mtps, out.mclk);
    out.complete = (out.mtps > 0);
    strlcpy(out.status, out.complete ? "EXPO detected" : "EXPO detected, decode incomplete", sizeof(out.status));
  } else {
    strlcpy(out.status, "EXPO detected, decode incomplete", sizeof(out.status));
  }
  return true;
}

bool spdTweakDecodeXmpSummary(const uint8_t* data, size_t len, SpdXmpDecodeSummary& out) {
  memset(&out, 0, sizeof(out));
  if (!data || len < SPD_NVM_SIZE) {
    strlcpy(out.status, "XMP not decoded: invalid buffer", sizeof(out.status));
    return false;
  }
  uint8_t marker = 0, taa = 0, trcd = 0, trfcsb = 0;
  uint16_t tck = 0;
  if (!spdReadU8(data, len, 0x2C0, &marker) || !spdReadLe16(data, len, 0x2C5, &tck) ||
      !spdReadU8(data, len, 0x2CE, &taa) || !spdReadU8(data, len, 0x2D0, &trcd) ||
      !spdReadU8(data, len, 0x2DD, &trfcsb)) {
    strlcpy(out.status, "XMP not detected/unmapped", sizeof(out.status));
    return true;
  }
  out.detected = (marker == 0x30 && tck >= 250 && tck <= 600 && taa != 0 && trcd != 0 && trfcsb != 0);
  if (!out.detected) {
    strlcpy(out.status, "XMP not detected/unmapped", sizeof(out.status));
    return true;
  }
  out.tckPs = tck;
  fillRateFromTck(out.tckPs, out.mtps, out.mclk);
  out.complete = (out.mtps > 0);
  strlcpy(out.status, out.complete ? "XMP detected" : "XMP detected, decode incomplete", sizeof(out.status));
  return true;
}

bool spdTweakDecodeCompactSummary(const uint8_t* data, size_t len, SpdCompactDecodeSummary& out) {
  memset(&out, 0, sizeof(out));
  bool baseOk = spdTweakDecodeBaseSummary(data, len, out.base);
  spdTweakDecodeExpoSummary(data, len, out.expo);
  spdTweakDecodeXmpSummary(data, len, out.xmp);
  return baseOk;
}

void spdTweakPrintCompactSummary(const SpdCompactDecodeSummary& decoded) {
  outPrintf("  Header bytes: %02X %02X %02X %02X\n",
            decoded.base.header[0], decoded.base.header[1], decoded.base.header[2], decoded.base.header[3]);
  outPrintf("  DDR generation: %s (raw 0x%02X)\n", memoryTypeName(decoded.base.generationRaw), decoded.base.generationRaw);
  outPrintf("  Module form factor: %s (raw 0x%02X)\n", moduleTypeName(decoded.base.moduleTypeRaw), decoded.base.moduleTypeRaw);
  if (decoded.base.tckPs > 0 && decoded.base.mtps > 0) {
    outPrintf("  Base tCK/data rate: %u ps, DDR5-%lu, MCLK ~%lu MHz\n",
              decoded.base.tckPs, (unsigned long)decoded.base.mtps, (unsigned long)decoded.base.mclk);
  } else {
    outPrintln("  Base tCK/data rate: decode unavailable");
  }
  if (decoded.expo.detected) {
    if (decoded.expo.tckPs > 0 && decoded.expo.mtps > 0) {
      outPrintf("  EXPO detected: yes, tCK %u ps, DDR5-%lu, MCLK ~%lu MHz\n",
                decoded.expo.tckPs, (unsigned long)decoded.expo.mtps, (unsigned long)decoded.expo.mclk);
    } else {
      outPrintln("  EXPO detected: yes, decode incomplete");
    }
  } else {
    outPrintln("  EXPO detected: no");
  }
  if (decoded.xmp.detected) {
    if (decoded.xmp.tckPs > 0 && decoded.xmp.mtps > 0) {
      outPrintf("  XMP detected: yes, tCK %u ps, DDR5-%lu, MCLK ~%lu MHz\n",
                decoded.xmp.tckPs, (unsigned long)decoded.xmp.mtps, (unsigned long)decoded.xmp.mclk);
    } else {
      outPrintln("  XMP detected: yes, decode incomplete");
    }
  } else {
    outPrintln("  XMP detected: no/unmapped");
  }
}

static void appendJsonRate(String& j, const char* key, bool detected, uint16_t tckPs, uint32_t mtps, uint32_t mclk, const char* status) {
  j += "\"" + String(key) + "\":{";
  j += "\"detected\":" + String(detected ? "true" : "false") + ",";
  j += "\"tck_ps\":" + String((unsigned)tckPs) + ",";
  j += "\"mtps\":" + String((unsigned long)mtps) + ",";
  j += "\"mclk\":" + String((unsigned long)mclk) + ",";
  j += "\"status\":\"";
  j += status ? status : "";
  j += "\"}";
}

String spdTweakCompactDecodeJson(uint8_t addr, const uint8_t* data, size_t len) {
  SpdCompactDecodeSummary decoded;
  bool ok = spdTweakDecodeCompactSummary(data, len, decoded);
  if (!ok) {
    return "{\"ok\":false,\"error\":\"compact SPD decode failed\"}";
  }
  char header[16];
  snprintf(header, sizeof(header), "%02X %02X %02X %02X",
           decoded.base.header[0], decoded.base.header[1], decoded.base.header[2], decoded.base.header[3]);
  String j;
  j.reserve(640);
  j += "{\"ok\":true,\"compact_summary\":true,";
  j += "\"addr\":\"0x" + String(addr, HEX) + "\",";
  j += "\"header\":\"" + String(header) + "\",";
  j += "\"generation\":\"" + String(memoryTypeName(decoded.base.generationRaw)) + "\",";
  j += "\"module_type\":\"" + String(moduleTypeName(decoded.base.moduleTypeRaw)) + "\",";
  j += "\"base\":{\"tck_ps\":" + String((unsigned)decoded.base.tckPs) + ",";
  j += "\"mtps\":" + String((unsigned long)decoded.base.mtps) + ",";
  j += "\"mclk\":" + String((unsigned long)decoded.base.mclk) + "},";
  appendJsonRate(j, "expo", decoded.expo.detected, decoded.expo.tckPs, decoded.expo.mtps, decoded.expo.mclk, decoded.expo.status);
  j += ",";
  appendJsonRate(j, "xmp", decoded.xmp.detected, decoded.xmp.tckPs, decoded.xmp.mtps, decoded.xmp.mclk, decoded.xmp.status);
  j += "}";
  return j;
}

bool spdTweakDecodeBaseSection(const uint8_t* data, size_t len, TimingSection& out) {
  memset(&out, 0, sizeof(out));
  out.title = "Base / JEDEC Profile";
  if (!data || len < SPD_NVM_SIZE) {
    strlcpy(out.error, "No current 1024-byte SPD dump", sizeof(out.error));
    return false;
  }
  uint16_t tckPs = 0;
  uint32_t tckX1000 = 0;
  char value[48];
  if (readTckX1000(data, len, 0x14, tckPs, tckX1000)) formatRateValue(value, sizeof(value), tckPs);
  else strlcpy(value, "decode unavailable", sizeof(value));
  addTimingRow(out, "Data rate / tCK", value, "0x014-0x015", "read-only", false);
  appendPs16MinRow(out, data, len, "tAA min", 0x1E, tckX1000);
  appendPs16CeilRow(out, data, len, "tRCD", 0x20, tckX1000);
  appendPs16CeilRow(out, data, len, "tRP", 0x22, tckX1000);
  appendPs16CeilRow(out, data, len, "tRAS", 0x24, tckX1000);
  appendPs16CeilRow(out, data, len, "tRC", 0x26, tckX1000);
  appendPs16CeilRow(out, data, len, "tWR", 0x28, tckX1000);
  appendNs16CeilRow(out, data, len, "tRFC1", 0x2A, tckX1000);
  appendNs16CeilRow(out, data, len, "tRFC2", 0x2C, tckX1000);
  appendNsU8CeilRow(out, data, len, "tRFCsb", 0x2E, tckX1000);
  out.ok = true;
  return true;
}

bool spdTweakDecodeExpoSection(const uint8_t* data, size_t len, TimingSection& out) {
  memset(&out, 0, sizeof(out));
  out.title = "EXPO Profile 1 Timings";
  if (!data || len < SPD_NVM_SIZE) {
    strlcpy(out.error, "No current 1024-byte SPD dump", sizeof(out.error));
    return false;
  }
  SpdExpoDecodeSummary summary;
  spdTweakDecodeExpoSummary(data, len, summary);
  if (!summary.detected) {
    strlcpy(out.error, "EXPO not detected", sizeof(out.error));
    return false;
  }
  uint16_t tckPs = 0;
  uint32_t tckX1000 = 0;
  char value[48];
  if (readTckX1000(data, len, 0x34E, tckPs, tckX1000)) formatRateValue(value, sizeof(value), tckPs);
  else strlcpy(value, "decode unavailable", sizeof(value));
  addTimingRow(out, "Data rate / tCK", value, "0x34E-0x34F", "read-only", false);
  uint8_t raw8 = 0;
  if (spdReadU8(data, len, 0x34A, &raw8)) formatProfileVoltageValue(value, sizeof(value), raw8);
  else strlcpy(value, "unmapped", sizeof(value));
  addTimingRow(out, "VDD", value, "0x34A", "read-only", false);
  if (spdReadU8(data, len, 0x34B, &raw8)) formatProfileVoltageValue(value, sizeof(value), raw8);
  else strlcpy(value, "unmapped", sizeof(value));
  addTimingRow(out, "VDDQ", value, "0x34B", "read-only", false);
  appendPs256U8Row(out, data, len, "tAA / CL", 0x351, tckX1000, true);
  appendPs256U8Row(out, data, len, "tRCD", 0x353, tckX1000, true);
  appendPs256U8Row(out, data, len, "tRP", 0x355, tckX1000, true);
  appendPs256U8Row(out, data, len, "tRAS", 0x357, tckX1000, true);
  appendPs256U8Row(out, data, len, "tRC", 0x359, tckX1000, true);
  appendPs16Row(out, data, len, "tWR", 0x35A, tckX1000, true);
  appendNs16Row(out, data, len, "tRFC1", 0x35C, tckX1000, true);
  appendNs16Row(out, data, len, "tRFC2", 0x35E, tckX1000, true);
  appendNsU8Row(out, data, len, "tRFCsb", 0x360, tckX1000, true);
  uint16_t mask = 0;
  if (spdReadLe16(data, len, 0x34C, &mask)) snprintf(value, sizeof(value), "0x%04X", mask);
  else strlcpy(value, "unmapped", sizeof(value));
  addTimingRow(out, "CAS/profile mask", value, "0x34C-0x34D", "read-only", false);
  bool present = false, pass = false;
  uint16_t stored = 0, calc = 0;
  if (spdTweakExpoCrcStatus(data, len, present, pass, stored, calc) && present) {
    snprintf(value, sizeof(value), "%s stored 0x%04X calc 0x%04X", pass ? "PASS" : "FAIL", stored, calc);
    addTimingRow(out, "EXPO CRC", value, "0x3BE-0x3BF", "read-only", false);
  }
  out.ok = true;
  return true;
}

bool spdTweakDecodeXmpSection(const uint8_t* data, size_t len, TimingSection& out) {
  memset(&out, 0, sizeof(out));
  out.title = "XMP 3.0 Performance Profile Timings";
  if (!data || len < SPD_NVM_SIZE) {
    strlcpy(out.error, "No current 1024-byte SPD dump", sizeof(out.error));
    return false;
  }
  SpdXmpDecodeSummary summary;
  spdTweakDecodeXmpSummary(data, len, summary);
  if (!summary.detected) {
    strlcpy(out.error, "XMP not detected/unmapped", sizeof(out.error));
    return false;
  }
  uint16_t tckPs = 0;
  uint32_t tckX1000 = 0;
  char value[48];
  if (readTckX1000(data, len, 0x2C5, tckPs, tckX1000)) formatRateValue(value, sizeof(value), tckPs);
  else strlcpy(value, "decode unavailable", sizeof(value));
  addTimingRow(out, "Data rate / tCK", value, "0x2C5-0x2C6", "read-only", false);
  uint8_t raw8 = 0;
  if (spdReadU8(data, len, 0x2C1, &raw8)) formatProfileVoltageValue(value, sizeof(value), raw8);
  else strlcpy(value, "unmapped", sizeof(value));
  addTimingRow(out, "VDD", value, "0x2C1", "read-only", false);
  if (spdReadU8(data, len, 0x2C2, &raw8)) formatProfileVoltageValue(value, sizeof(value), raw8);
  else strlcpy(value, "unmapped", sizeof(value));
  addTimingRow(out, "VDDQ", value, "0x2C2", "read-only", false);
  appendPs256U8Row(out, data, len, "tAA / CL", 0x2CE, tckX1000, false);
  appendPs256U8Row(out, data, len, "tRCD", 0x2D0, tckX1000, false);
  appendPs256U8Row(out, data, len, "tRP", 0x2D2, tckX1000, false);
  appendPs256U8Row(out, data, len, "tRAS", 0x2D4, tckX1000, false);
  appendPs256U8Row(out, data, len, "tRC", 0x2D6, tckX1000, false);
  appendPs16Row(out, data, len, "tWR", 0x2D7, tckX1000, false);
  appendNs16Row(out, data, len, "tRFC1", 0x2D9, tckX1000, false);
  appendNs16Row(out, data, len, "tRFC2", 0x2DB, tckX1000, false);
  appendNsU8Row(out, data, len, "tRFCsb", 0x2DD, tckX1000, false);
  uint16_t crc = 0;
  if (spdReadLe16(data, len, 0x2FE, &crc)) snprintf(value, sizeof(value), "stored 0x%04X read-only", crc);
  else strlcpy(value, "unmapped", sizeof(value));
  addTimingRow(out, "XMP/Profile CRC", value, "0x2FE-0x2FF", "read-only", false);
  out.ok = true;
  return true;
}

void spdTweakPrintTimingSection(const TimingSection& section) {
  outPrintf("%s\n", section.title ? section.title : "SPD timing section");
  if (!section.ok) {
    outPrintf("  %s\n", section.error[0] ? section.error : "decode failed");
    return;
  }
  for (uint8_t i = 0; i < section.count; i++) {
    const TimingRow& r = section.rows[i];
    outPrintf("  %-18s %-36s %-14s %s\n", r.label, r.current, r.source, r.status);
  }
}

String spdTweakTimingSectionJson(const TimingSection& section) {
  String j;
  j.reserve(1400);
  j += "{\"ok\":";
  j += section.ok ? "true" : "false";
  j += ",\"title\":\"";
  j += section.title ? section.title : "";
  j += "\",\"error\":\"";
  j += section.error;
  j += "\",\"rows\":[";
  for (uint8_t i = 0; i < section.count; i++) {
    if (i) j += ",";
    const TimingRow& r = section.rows[i];
    j += "{\"label\":\"";
    j += r.label;
    j += "\",\"current\":\"";
    j += r.current;
    j += "\",\"source\":\"";
    j += r.source;
    j += "\",\"status\":\"";
    j += r.status;
    j += "\",\"editable\":";
    j += r.editable ? "true" : "false";
    j += "}";
  }
  j += "]}";
  return j;
}

bool spdTweakSelfTest() {
  uint8_t tiny[32] = {0};
  static uint8_t minimal[SPD_NVM_SIZE];
  memset(minimal, 0, sizeof(minimal));
  minimal[0] = 0x30;
  minimal[1] = 0x10;
  minimal[2] = 0x12;
  minimal[3] = 0x02;
  minimal[0x14] = 0x00;
  minimal[0x15] = 0x00;

  uint8_t u8 = 0xAA;
  uint16_t u16 = 0xAAAA;
  bool pass = true;
  pass = pass && !spdReadU8(tiny, sizeof(tiny), 1023, &u8);
  pass = pass && !spdReadLe16(tiny, sizeof(tiny), 31, &u16);
  SpdCompactDecodeSummary decoded;
  pass = pass && spdTweakDecodeCompactSummary(minimal, sizeof(minimal), decoded);
  pass = pass && decoded.base.ok;
  pass = pass && decoded.base.tckPs == 0;
  pass = pass && !decoded.expo.detected;
  pass = pass && !decoded.xmp.detected;
  return pass;
}

static bool decodeTckRate(char* value, size_t valueLen, uint16_t tckPs) {
  if (!value || valueLen == 0 || tckPs == 0) return false;
  uint32_t rawMtps = spdRawMtpsFromTckPs(tckPs);
  uint32_t bin = spdNearestDdr5Bin(rawMtps);
  if (bin == 0) return false;
  snprintf(value, valueLen, "DDR5-%lu / MCLK ~%lu MHz", (unsigned long)bin, (unsigned long)spdMclkFromMtps(bin));
  return true;
}

static void formatVoltage(char* out, size_t outLen, uint8_t raw) {
  formatProfileVoltageValue(out, outLen, raw);
}

static void formatTckDetail(char* out, size_t outLen, uint16_t tckPs) {
  uint32_t mtps = spdNearestDdr5Bin(spdRawMtpsFromTckPs(tckPs));
  uint32_t mclk = spdMclkFromMtps(mtps);
  uint32_t nominal = spdNominalTckPsX1000FromMtps(mtps);
  snprintf(out, outLen, "DDR5-%lu / MCLK ~%lu MHz / tCK %u ps / nominal %lu.%03lu ps",
           (unsigned long)mtps, (unsigned long)mclk, (unsigned)tckPs,
           (unsigned long)(nominal / 1000), (unsigned long)(nominal % 1000));
}

static void formatPsTiming(char* out, size_t outLen, uint32_t ps, uint32_t nominalTckPsX1000) {
  uint32_t cycles = spdPsToCycles(ps, nominalTckPsX1000);
  snprintf(out, outLen, "%luT / %lu.%03lu ns", (unsigned long)cycles,
           (unsigned long)(ps / 1000), (unsigned long)(ps % 1000));
}

static void formatNsTiming(char* out, size_t outLen, uint32_t ns, uint32_t nominalTckPsX1000) {
  uint32_t cycles = spdNsToCycles(ns, nominalTckPsX1000);
  snprintf(out, outLen, "%luT / %lu ns", (unsigned long)cycles, (unsigned long)ns);
}

static void addPsField(SpdTweakDecode& out, const char* key, const char* label, const uint8_t* spd,
                       uint16_t off, uint8_t width, uint32_t ps, uint32_t nominalTckPsX1000,
                       bool editable, const char* status, const char* note) {
  char value[64];
  char raw[96];
  char source[64];
  formatPsTiming(value, sizeof(value), ps, nominalTckPsX1000);
  rawRange(raw, sizeof(raw), spd, off, width);
  if (width == 1) snprintf(source, sizeof(source), "0x%03X raw * 256 ps", off);
  else snprintf(source, sizeof(source), "0x%03X-0x%03X little-endian ps", off, (uint16_t)(off + width - 1));
  addField(out, key, label, value, source, raw, editable, status, note);
}

static void addNsField(SpdTweakDecode& out, const char* key, const char* label, const uint8_t* spd,
                       uint16_t off, uint8_t width, uint32_t ns, uint32_t nominalTckPsX1000,
                       bool editable, const char* status, const char* note) {
  char value[64];
  char raw[96];
  char source[64];
  formatNsTiming(value, sizeof(value), ns, nominalTckPsX1000);
  rawRange(raw, sizeof(raw), spd, off, width);
  if (width == 1) snprintf(source, sizeof(source), "0x%03X ns", off);
  else snprintf(source, sizeof(source), "0x%03X-0x%03X little-endian ns", off, (uint16_t)(off + width - 1));
  addField(out, key, label, value, source, raw, editable, status, note);
}

static uint16_t crc16CcittRange(const uint8_t* spd, uint16_t start, uint16_t endInclusive) {
  uint16_t crc = 0;
  for (uint16_t off = start; off <= endInclusive; off++) {
    crc ^= (uint16_t)spd[off] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

void spdTweakDecodeBuffer(const uint8_t* spd, SpdTweakDecode& out) {
  memset(&out, 0, sizeof(out));
  if (!spd) {
    strlcpy(out.summary, "No SPD buffer loaded", sizeof(out.summary));
    return;
  }

  SpdCompactDecodeSummary compact;
  if (!spdTweakDecodeCompactSummary(spd, SPD_NVM_SIZE, compact)) {
    strlcpy(out.summary, compact.base.error[0] ? compact.base.error : "Compact SPD decode failed", sizeof(out.summary));
    return;
  }
  out.ddr5Like = compact.base.ddr5Like;
  out.crcRepairSupported = false;
  strlcpy(out.summary, "Compact SPD decode only; full tables temporarily disabled", sizeof(out.summary));
  char compactValue[64];
  char compactRaw[32];
  snprintf(compactRaw, sizeof(compactRaw), "%02X %02X %02X %02X", compact.base.header[0], compact.base.header[1], compact.base.header[2], compact.base.header[3]);
  addField(out, "header", "Header bytes", compactRaw, "0x000-0x003", compactRaw, false, "decoded / read-only", "compact decoder");
  addField(out, "generation", "DDR generation", memoryTypeName(compact.base.generationRaw), "0x002", "", false, "decoded / read-only", "compact decoder");
  addField(out, "module_type", "Module form factor", moduleTypeName(compact.base.moduleTypeRaw), "0x003", "", false, "decoded / read-only", "compact decoder");
  if (compact.base.tckPs > 0 && compact.base.mtps > 0) {
    snprintf(compactValue, sizeof(compactValue), "DDR5-%lu / MCLK ~%lu MHz / tCK %u ps",
             (unsigned long)compact.base.mtps, (unsigned long)compact.base.mclk, compact.base.tckPs);
  } else {
    strlcpy(compactValue, "decode unavailable", sizeof(compactValue));
  }
  addField(out, "base_speed", "Base tCK/data rate", compactValue, "0x014-0x015", "", false, "decoded / read-only", "compact decoder");
  addField(out, "expo", "EXPO", compact.expo.status, "0x340 signature, 0x34E tCK", "", false, "decoded / read-only", "compact decoder");
  addField(out, "xmp", "XMP", compact.xmp.status, "0x2C0-0x2FF observed block", "", false, "decoded / read-only", "compact decoder");
  return;

  out.ddr5Like = (spd[0] == 0x30 && spd[1] == 0x10 && spd[2] == 0x12);
  out.crcRepairSupported = false;
  strlcpy(out.summary,
          out.ddr5Like ? "DDR5-like SPD header detected" : "SPD header is not recognized as supported DDR5",
          sizeof(out.summary));

  char tmp[96];
  char raw[96];
  snprintf(tmp, sizeof(tmp), "0x%02X 0x%02X 0x%02X 0x%02X", spd[0], spd[1], spd[2], spd[3]);
  rawRange(raw, sizeof(raw), spd, 0, 4);
  addField(out, "header", "Base SPD header bytes", tmp, "SPD bytes 0x000-0x003", raw, false,
           "decoded / read-only", "read-only sanity check");

  rawRange(raw, sizeof(raw), spd, 2, 1);
  addField(out, "generation", "Module type / DDR generation", memoryTypeName(spd[2]), "SPD byte 0x002", raw, false,
           spd[2] == 0x12 ? "decoded / read-only" : "not decoded yet",
           spd[2] == 0x12 ? "decoded from SPD byte 2" : "memory type byte not recognized by this decoder");

  snprintf(tmp, sizeof(tmp), "%s (raw 0x%02X)", moduleTypeName(spd[3]), spd[3]);
  rawRange(raw, sizeof(raw), spd, 3, 1);
  addField(out, "module_type", "Module form factor", tmp, "SPD byte 0x003 low nibble", raw, false,
           "decoded / read-only", "decoded conservatively from SPD byte 3 low nibble");

  char ascii[64];
  asciiFromRange(ascii, sizeof(ascii), spd, 521, 30);
  rawRange(raw, sizeof(raw), spd, 521, 8);
  addField(out, "part_number", "Module part number", looksUsefulAscii(ascii) ? ascii : "not decoded yet",
           "common DDR SPD module part-number range 0x209-0x226", raw, false,
           looksUsefulAscii(ascii) ? "decoded / read-only" : "not decoded yet",
           looksUsefulAscii(ascii) ? "ASCII candidate from module part-number bytes" : "bytes did not contain a usable ASCII part-number candidate");

  rawRange(raw, sizeof(raw), spd, 512, 4);
  snprintf(tmp, sizeof(tmp), "0x%02X 0x%02X 0x%02X 0x%02X", spd[512], spd[513], spd[514], spd[515]);
  addField(out, "manufacturer", "Manufacturer ID bytes", tmp, "SPD bytes 0x200-0x203", raw, false,
           "decoded / read-only", "raw manufacturer/module ID bytes; vendor name table not implemented");

  uint16_t baseTckPs = spdReadLe16(spd, 0x14);
  uint32_t baseMtps = spdNearestDdr5Bin(spdRawMtpsFromTckPs(baseTckPs));
  uint32_t baseNominalTckPsX1000 = spdNominalTckPsX1000FromMtps(baseMtps);
  rawRange(raw, sizeof(raw), spd, 0x14, 2);
  if (baseTckPs != 0) {
    formatTckDetail(tmp, sizeof(tmp), baseTckPs);
    addField(out, "base_tck", "Base tCK", tmp, "SPD bytes 0x014-0x015 little-endian", raw, false,
             "decoded / read-only", "DDR5 base tCK minimum source");
    decodeTckRate(tmp, sizeof(tmp), baseTckPs);
    addField(out, "base_speed", "Base data rate", tmp, "decoded from Base tCK at 0x014-0x015", raw, false,
             "decoded / read-only", "data rate is derived from tCK and rounded to the nearest supported DDR5 bin");
  } else {
    addField(out, "base_tck", "Base tCK", "decode unavailable", "SPD bytes 0x014-0x015 little-endian", raw, false,
             "decode unavailable", "zero tCK value");
    addField(out, "base_speed", "Base data rate", "decode unavailable", "decoded from Base tCK at 0x014-0x015", raw, false,
             "decode unavailable", "data rate is derived from tCK");
  }

  addPsField(out, "base_taa", "Base / JEDEC tAA / CL", spd, 0x1E, 2, spdReadLe16(spd, 0x1E),
             baseNominalTckPsX1000, false, "decoded / read-only", "JEDEC/base timing; writes are not supported");
  addPsField(out, "base_trcd", "Base / JEDEC tRCD", spd, 0x20, 2, spdReadLe16(spd, 0x20),
             baseNominalTckPsX1000, false, "decoded / read-only", "JEDEC/base timing; writes are not supported");
  addPsField(out, "base_trp", "Base / JEDEC tRP", spd, 0x22, 2, spdReadLe16(spd, 0x22),
             baseNominalTckPsX1000, false, "decoded / read-only", "JEDEC/base timing; writes are not supported");
  addPsField(out, "base_tras", "Base / JEDEC tRAS", spd, 0x24, 2, spdReadLe16(spd, 0x24),
             baseNominalTckPsX1000, false, "decoded / read-only", "JEDEC/base timing; writes are not supported");
  addPsField(out, "base_trc", "Base / JEDEC tRC", spd, 0x26, 2, spdReadLe16(spd, 0x26),
             baseNominalTckPsX1000, false, "decoded / read-only", "JEDEC/base timing; writes are not supported");
  addPsField(out, "base_twr", "Base / JEDEC tWR", spd, 0x28, 2, spdReadLe16(spd, 0x28),
             baseNominalTckPsX1000, false, "decoded / read-only", "JEDEC/base timing; writes are not supported");
  addNsField(out, "base_trfc1", "Base / JEDEC tRFC1", spd, 0x2A, 2, spdReadLe16(spd, 0x2A),
             baseNominalTckPsX1000, false, "decoded / read-only", "JEDEC/base timing; source is little-endian ns");
  addNsField(out, "base_trfc2", "Base / JEDEC tRFC2", spd, 0x2C, 2, spdReadLe16(spd, 0x2C),
             baseNominalTckPsX1000, false, "decoded / read-only", "JEDEC/base timing; source is little-endian ns");
  addNsField(out, "base_trfcsb", "Base / JEDEC tRFCsb", spd, 0x2E, 1, spd[0x2E],
             baseNominalTckPsX1000, false, "decoded / read-only", "JEDEC/base timing; single-byte ns source");

  uint16_t sigOff = 0;
  if (spd[0x340] == 'E' && spd[0x341] == 'X' && spd[0x342] == 'P' && spd[0x343] == 'O') {
    sigOff = 0x340;
    rawRange(raw, sizeof(raw), spd, sigOff, 16);
    snprintf(tmp, sizeof(tmp), "yes at 0x%03X", sigOff);
    addField(out, "expo", "EXPO signature/presence", tmp, "EXPO signature bytes 0x340-0x343", raw, false,
             "decoded / read-only", "EXPO Profile 1 timing offsets are mapped; header bytes remain read-only");
    uint16_t expoTckPs = spdReadLe16(spd, 0x34E);
    uint32_t expoMtps = spdNearestDdr5Bin(spdRawMtpsFromTckPs(expoTckPs));
    uint32_t expoNominalTckPsX1000 = spdNominalTckPsX1000FromMtps(expoMtps);
    rawRange(raw, sizeof(raw), spd, 0x34E, 2);
    if (expoTckPs != 0) {
      formatTckDetail(tmp, sizeof(tmp), expoTckPs);
      addField(out, "expo_tck", "EXPO tCK", tmp, "EXPO bytes 0x34E-0x34F little-endian", raw, false,
               "decoded / crc-covered / editor-backed", "EXPO profile tCK source; Advanced SPD Editing can preview/apply guarded edits");
      decodeTckRate(tmp, sizeof(tmp), expoTckPs);
      addField(out, "expo_speed", "EXPO data rate", tmp, "decoded from EXPO tCK at 0x34E-0x34F", raw, false,
               "decoded / crc-covered / editor-backed", "data rate is derived from tCK; Advanced SPD Editing writes the tCK bytes");
    } else {
      addField(out, "expo_tck", "EXPO tCK", "decode unavailable", "EXPO bytes 0x34E-0x34F little-endian", raw, false,
               "decode unavailable / crc-covered / experimental / read-only", "zero tCK value");
      addField(out, "expo_speed", "EXPO data rate", "decode unavailable", "decoded from EXPO tCK at 0x34E-0x34F", raw, false,
               "decode unavailable / crc-covered / experimental / read-only", "data rate is derived from tCK");
    }
    formatVoltage(tmp, sizeof(tmp), spd[0x34A]);
    rawRange(raw, sizeof(raw), spd, 0x34A, 1);
    addField(out, "expo_vdd", "EXPO VDD", tmp, "EXPO byte 0x34A, 20 mV units", raw, false,
             "decoded / crc-covered / editor-backed", "Advanced SPD Editing can preview/apply guarded voltage edits");
    formatVoltage(tmp, sizeof(tmp), spd[0x34B]);
    rawRange(raw, sizeof(raw), spd, 0x34B, 1);
    addField(out, "expo_vddq", "EXPO VDDQ", tmp, "EXPO byte 0x34B, 20 mV units", raw, false,
             "decoded / crc-covered / editor-backed", "Advanced SPD Editing can preview/apply guarded voltage edits");
    rawRange(raw, sizeof(raw), spd, 0x34C, 2);
    snprintf(tmp, sizeof(tmp), "0x%04X", spdReadLe16(spd, 0x34C));
    addField(out, "expo_cas_mask", "EXPO CAS/profile mask", tmp, "EXPO bytes 0x34C-0x34D little-endian", raw, false,
             "raw / read-only", "not labeled as CL; mapping remains unverified");
    addPsField(out, "expo_taa", "EXPO tAA / CL", spd, 0x351, 1, (uint32_t)spd[0x351] * 256UL,
               expoNominalTckPsX1000, true, "decoded / crc-covered / editable", "input accepts cycles; raw source is 256 ps units");
    addPsField(out, "expo_trcd", "EXPO tRCD", spd, 0x353, 1, (uint32_t)spd[0x353] * 256UL,
               expoNominalTckPsX1000, true, "decoded / crc-covered / editable", "input accepts cycles; raw source is 256 ps units");
    addPsField(out, "expo_trp", "EXPO tRP", spd, 0x355, 1, (uint32_t)spd[0x355] * 256UL,
               expoNominalTckPsX1000, true, "decoded / crc-covered / editable", "input accepts cycles; raw source is 256 ps units");
    addPsField(out, "expo_tras", "EXPO tRAS", spd, 0x357, 1, (uint32_t)spd[0x357] * 256UL,
               expoNominalTckPsX1000, true, "decoded / crc-covered / editable", "input accepts cycles; raw source is 256 ps units");
    addPsField(out, "expo_trc", "EXPO tRC", spd, 0x359, 1, (uint32_t)spd[0x359] * 256UL,
               expoNominalTckPsX1000, true, "decoded / crc-covered / editable", "input accepts cycles; raw source is 256 ps units");
    addPsField(out, "expo_twr", "EXPO tWR", spd, 0x35A, 2, spdReadLe16(spd, 0x35A),
               expoNominalTckPsX1000, true, "decoded / crc-covered / editable", "input accepts cycles; raw source is little-endian ps");
    addNsField(out, "expo_trfc1", "EXPO tRFC1", spd, 0x35C, 2, spdReadLe16(spd, 0x35C),
               expoNominalTckPsX1000, true, "decoded / crc-covered / editable", "input accepts cycles; raw source is little-endian ns");
    addNsField(out, "expo_trfc2", "EXPO tRFC2", spd, 0x35E, 2, spdReadLe16(spd, 0x35E),
               expoNominalTckPsX1000, true, "decoded / crc-covered / editable", "input accepts cycles; raw source is little-endian ns");
    addNsField(out, "expo_trfcsb", "EXPO tRFCsb", spd, 0x360, 1, spd[0x360],
               expoNominalTckPsX1000, true, "decoded / crc-covered / editable", "input accepts cycles; raw source is single-byte ns");
  } else {
    addField(out, "expo", "EXPO signature/presence", "not detected", "ASCII signature scan over SPD image",
             "source offset not mapped", false, "not decoded yet", "no EXPO ASCII signature found in the current 1024-byte dump");
    addField(out, "expo_tck", "EXPO tCK", "EXPO: not detected", "EXPO bytes 0x34E-0x34F", "source offset not mapped", false,
             "not decoded yet", "no EXPO signature at 0x340");
    addField(out, "expo_speed", "EXPO data rate", "EXPO: not detected", "decoded from EXPO tCK at 0x34E-0x34F",
             "source offset not mapped", false, "not decoded yet", "no EXPO signature at 0x340");
  }

  bool xmpPerfLooksValid = spd[0x2C0] == 0x30 && spdReadLe16(spd, 0x2C5) >= 250 && spdReadLe16(spd, 0x2C5) <= 600 &&
                           spd[0x2CE] != 0 && spd[0x2D0] != 0 && spd[0x2DD] != 0;
  if (xmpPerfLooksValid) {
    rawRange(raw, sizeof(raw), spd, 0x2C0, 16);
    addField(out, "xmp", "XMP 3.0 Performance profile presence", "detected", "observed timing block at 0x2C0-0x2FF", raw, false,
             "decoded / read-only", "detected from observed DDR5 XMP profile block; selected backed fields are editable through Advanced SPD Editing");
    uint16_t xmpTckPs = spdReadLe16(spd, 0x2C5);
    uint32_t xmpMtps = spdNearestDdr5Bin(spdRawMtpsFromTckPs(xmpTckPs));
    uint32_t xmpNominalTckPsX1000 = spdNominalTckPsX1000FromMtps(xmpMtps);
    rawRange(raw, sizeof(raw), spd, 0x2C5, 2);
    formatTckDetail(tmp, sizeof(tmp), xmpTckPs);
    addField(out, "xmp_tck", "XMP tCK", tmp, "XMP bytes 0x2C5-0x2C6 little-endian", raw, false,
             "decoded / crc-covered / editor-backed", "XMP profile tCK source; Advanced SPD Editing can preview/apply guarded edits");
    decodeTckRate(tmp, sizeof(tmp), xmpTckPs);
    addField(out, "xmp_speed", "XMP data rate", tmp, "decoded from XMP tCK at 0x2C5-0x2C6", raw, false,
             "decoded / crc-covered / editor-backed", "data rate is derived from tCK; Advanced SPD Editing writes the tCK bytes");
    formatVoltage(tmp, sizeof(tmp), spd[0x2C1]);
    rawRange(raw, sizeof(raw), spd, 0x2C1, 1);
    addField(out, "xmp_vdd", "XMP VDD", tmp, "XMP byte 0x2C1, 20 mV units", raw, false,
             "decoded / crc-covered / editor-backed", "Advanced SPD Editing can preview/apply guarded voltage edits");
    formatVoltage(tmp, sizeof(tmp), spd[0x2C2]);
    rawRange(raw, sizeof(raw), spd, 0x2C2, 1);
    addField(out, "xmp_vddq", "XMP VDDQ", tmp, "XMP byte 0x2C2, 20 mV units", raw, false,
             "decoded / crc-covered / editor-backed", "Advanced SPD Editing can preview/apply guarded voltage edits");
    addPsField(out, "xmp_taa", "XMP tAA / CL", spd, 0x2CE, 1, (uint32_t)spd[0x2CE] * 256UL,
               xmpNominalTckPsX1000, false, "decoded / read-only", "XMP writes are intentionally disabled");
    addPsField(out, "xmp_trcd", "XMP tRCD", spd, 0x2D0, 1, (uint32_t)spd[0x2D0] * 256UL,
               xmpNominalTckPsX1000, false, "decoded / read-only", "XMP writes are intentionally disabled");
    addPsField(out, "xmp_trp", "XMP tRP", spd, 0x2D2, 1, (uint32_t)spd[0x2D2] * 256UL,
               xmpNominalTckPsX1000, false, "decoded / read-only", "XMP writes are intentionally disabled");
    addPsField(out, "xmp_tras", "XMP tRAS", spd, 0x2D4, 1, (uint32_t)spd[0x2D4] * 256UL,
               xmpNominalTckPsX1000, false, "decoded / read-only", "XMP writes are intentionally disabled");
    addPsField(out, "xmp_trc", "XMP tRC", spd, 0x2D6, 1, (uint32_t)spd[0x2D6] * 256UL,
               xmpNominalTckPsX1000, false, "decoded / read-only", "XMP writes are intentionally disabled");
    addPsField(out, "xmp_twr", "XMP tWR", spd, 0x2D7, 2, spdReadLe16(spd, 0x2D7),
               xmpNominalTckPsX1000, false, "decoded / read-only", "XMP writes are intentionally disabled");
    addNsField(out, "xmp_trfc1", "XMP tRFC1", spd, 0x2D9, 2, spdReadLe16(spd, 0x2D9),
               xmpNominalTckPsX1000, false, "decoded / read-only", "XMP writes are intentionally disabled");
    addNsField(out, "xmp_trfc2", "XMP tRFC2", spd, 0x2DB, 2, spdReadLe16(spd, 0x2DB),
               xmpNominalTckPsX1000, false, "decoded / read-only", "XMP writes are intentionally disabled");
    addNsField(out, "xmp_trfcsb", "XMP tRFCsb", spd, 0x2DD, 1, spd[0x2DD],
               xmpNominalTckPsX1000, false, "decoded / read-only", "XMP writes are intentionally disabled");
    rawRange(raw, sizeof(raw), spd, 0x2FE, 2);
    uint16_t xmpStoredCrc = spdReadLe16(spd, 0x2FE);
    uint16_t xmpCalcCrc = crc16CcittRange(spd, 0x2C0, 0x2FD);
    snprintf(tmp, sizeof(tmp), "%s; stored 0x%04X calculated 0x%04X; repair not implemented / read-only",
             xmpStoredCrc == xmpCalcCrc ? "OK" : "FAIL", xmpStoredCrc, xmpCalcCrc);
    addField(out, "xmp_crc", "XMP/Profile CRC", tmp, "XMP bytes 0x2FE-0x2FF little-endian", raw, false,
             "observed / read-only", "CRC validation and repair are not implemented for XMP yet");
  } else {
    addField(out, "xmp", "XMP 3.0 Performance profile presence", "not detected", "observed timing block at 0x2C0-0x2FF",
             "source offset not mapped", false, "not decoded yet", "observed XMP performance timing block did not pass sanity checks");
  }

  addField(out, "crc", "CRC/checksum status", "EXPO CRC repair available; XMP read-only", "EXPO 0x340-0x3BF; XMP 0x2C0-0x2FF",
           "EXPO CRC 0x3BE-0x3BF; XMP CRC 0x2FE-0x2FF", false,
           "guarded / read-only outside EXPO", "normal write path repairs EXPO CRC automatically and blocks XMP writes");
}

bool spdTweakBuildPatch(const uint8_t* spd, int assignmentCount, char* assignments[], SpdTweakPatch& patch) {
  memset(&patch, 0, sizeof(patch));
  if (!spd) {
    strlcpy(patch.message, "No SPD buffer loaded.", sizeof(patch.message));
    return false;
  }

  memcpy(patch.original, spd, SPD_NVM_SIZE);
  memcpy(patch.patched, spd, SPD_NVM_SIZE);
  patch.crcRepairSupported = false;

  if (assignmentCount <= 0) {
    strlcpy(patch.message, "No editable SPD tweak fields are enabled yet; preview is read-only.", sizeof(patch.message));
    patch.ok = true;
    return true;
  }

  snprintf(patch.message, sizeof(patch.message),
           "SPD tweak field '%s' is not editable. Writes are blocked until verified DDR5 field map and CRC repair exist.",
           assignments[0] ? assignments[0] : "(null)");
  patch.ok = false;
  return false;
}

bool spdTweakVerifyImage(uint8_t addr, const uint8_t* expected) {
  if (!expected) return false;

  uint8_t buf[32];
  for (uint16_t off = 0; off < SPD_NVM_SIZE; off += sizeof(buf)) {
    if (!spdNvmRead(addr, off, buf, sizeof(buf))) return false;
    for (uint16_t i = 0; i < sizeof(buf); i++) {
      if (buf[i] != expected[off + i]) return false;
    }
  }
  return true;
}

void spdTweakPrintDecoded(const SpdTweakDecode& decoded) {
  outPrintf("  Decode: %s\n", decoded.summary);
  for (uint8_t i = 0; i < decoded.count; i++) {
    const SpdDecodedField& f = decoded.fields[i];
    outPrintf("  %-28s = %s%s\n", f.label, f.value, f.editable ? " (editable)" : "");
    if (f.source[0] || f.raw[0] || f.status[0]) {
      outPrintf("    source: %s  raw: %s  status: %s\n", f.source, f.raw, f.status);
    }
    if (f.note && *f.note) outPrintf("    note: %s\n", f.note);
  }
}

static String jsonEsc(const char* s) {
  String out;
  if (!s) return out;
  while (*s) {
    char c = *s++;
    if (c == '"' || c == '\\') out += '\\';
    if ((uint8_t)c >= 0x20) out += c;
  }
  return out;
}

void spdTweakPrintDiff(const SpdTweakPatch& patch) {
  if (!patch.hasChanges || patch.diffCount == 0) {
    outPrintln("  Diff: no byte changes");
    return;
  }

  outPrintf("  Diff bytes: %u\n", (unsigned)patch.diffCount);
  for (uint8_t i = 0; i < patch.diffCount; i++) {
    uint16_t off = patch.diffOffsets[i];
    outPrintf("    0x%04X: 0x%02X -> 0x%02X\n", off, patch.original[off], patch.patched[off]);
  }
}

String spdTweakDecodeJson(uint8_t addr, const uint8_t* spd) {
  return spdTweakCompactDecodeJson(addr, spd, SPD_NVM_SIZE);
#if 0
  SpdTweakDecode decoded;
  spdTweakDecodeBuffer(spd, decoded);

  String j = "{";
  j += "\"ok\":true,";
  j += "\"addr\":\"0x" + String(addr, HEX) + "\",";
  j += "\"summary\":\"" + String(decoded.summary) + "\",";
  j += "\"ddr5_like\":" + String(decoded.ddr5Like ? "true" : "false") + ",";
  j += "\"crc_repair_supported\":" + String(decoded.crcRepairSupported ? "true" : "false") + ",";
  j += "\"fields\":[";
  for (uint8_t i = 0; i < decoded.count; i++) {
    const SpdDecodedField& f = decoded.fields[i];
    if (i) j += ",";
    j += "{";
    j += "\"key\":\"" + jsonEsc(f.key) + "\",";
    j += "\"label\":\"" + jsonEsc(f.label) + "\",";
    j += "\"value\":\"" + jsonEsc(f.value) + "\",";
    j += "\"source\":\"" + jsonEsc(f.source) + "\",";
    j += "\"raw\":\"" + jsonEsc(f.raw) + "\",";
    j += "\"status\":\"" + jsonEsc(f.status) + "\",";
    j += "\"editable\":" + String(f.editable ? "true" : "false") + ",";
    j += "\"note\":\"" + jsonEsc(f.note) + "\"";
    j += "}";
  }
  j += "]}";
  return j;
#endif
}
