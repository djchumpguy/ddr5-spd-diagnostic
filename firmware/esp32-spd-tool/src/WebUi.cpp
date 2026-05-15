#include "WebUi.h"
#include "AppConfig.h"
#include "AppState.h"
#include "BoardControl.h"
#include "Cli.h"
#include "GoodSpdStore.h"
#include "HardwareConfig.h"
#include "Log.h"
#include "RoleDetect.h"
#include "SpdHub.h"
#include "SpdEditMap.h"
#include "SpdOps.h"
#include "SpdTweak.h"

#include <WiFi.h>
#include <WebServer.h>
#include <string.h>

static WebServer server(WEB_PORT);

// ===================== WEB HELPERS =====================
static String jsonBool(bool v) { return v ? "true" : "false"; }

static String jsonEscape(const char* s) {
  String out;
  if (!s) return out;
  while (*s) {
    char c = *s++;
    if (c == '"' || c == '\\') out += '\\';
    if ((uint8_t)c >= 0x20) out += c;
  }
  return out;
}

static void webSendJson(int code, const String& body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json", body);
}

static void webSendText(int code, const String& body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "text/plain", body);
}

static void webSendBinFromBuf(const uint8_t* buf, size_t len, const char* filename) {
  server.sendHeader("Content-Type", "application/octet-stream");
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + filename + "\"");
  server.setContentLength(len);
  server.send(200, "application/octet-stream", "");
  WiFiClient c = server.client();
  c.write(buf, len);
}

// ===================== WEB: API =====================
static void handleWebStatus() {
  bool mrOk = spdRefreshMR11(DEFAULT_SPD_ADDR);
  uint8_t expectedAddr = 0;
  const char* expectedDesc = nullptr;
  bool hasExpectedAddr = hwExpectedSpdAddr(expectedAddr, expectedDesc);

  String j = "{";
  j += "\"ok\":true,";
  j += "\"pwr_good\":" + jsonBool(pwrGoodReady()) + ",";
  j += "\"pwr_en_released\":" + jsonBool(readPwrEnLevel()) + ",";
  j += "\"dimm_power_on\":" + jsonBool(isDimmPowerOn()) + ",";
  j += "\"hsa_low\":" + jsonBool(isHsaLow()) + ",";
  j += "\"hw_profile\":\"" + String(hwProfileName(gApp.hwConfig.profile)) + "\",";
  j += "\"hw_hsa\":\"" + String(hwHsaModeName(gApp.hwConfig.hsa)) + "\",";
  j += "\"hw_vinbulk\":\"" + String(hwVinBulkModeName(gApp.hwConfig.vinBulk)) + "\",";
  j += "\"hw_pwren\":\"" + String(hwPwrEnModeName(gApp.hwConfig.pwrEn)) + "\",";
  j += "\"hw_pgood\":\"" + String(hwPwrGoodModeName(gApp.hwConfig.pwrGood)) + "\",";
  j += "\"hw_i2c\":\"" + String(hwI2cModeName(gApp.hwConfig.i2c)) + "\",";
  j += "\"hw_pullups\":\"" + String(hwPullupModeName(gApp.hwConfig.pullups)) + "\",";
  j += "\"hw_expected_addr\":\"" + String(hasExpectedAddr ? String("0x") + String(expectedAddr, HEX) : String("runtime-dependent")) + "\",";
  j += "\"hw_expected_desc\":\"" + jsonEscape(expectedDesc) + "\",";
  j += "\"hw_gpio_not_authoritative\":" + jsonBool(gApp.hwConfig.profile == HW_PROFILE_UNKNOWN ||
                                                   gApp.hwConfig.hsa != HW_HSA_GPIO_SWITCHABLE ||
                                                   gApp.hwConfig.vinBulk != HW_VIN_GPIO_SWITCHABLE ||
                                                   gApp.hwConfig.pwrEn != HW_PWREN_GPIO_CONTROLLED ||
                                                   gApp.hwConfig.pwrGood != HW_PGOOD_GPIO_READ) + ",";
  j += "\"scan_ok\":" + jsonBool(gApp.scanOK) + ",";
  j += "\"read_ok\":" + jsonBool(gApp.readOK) + ",";
  j += "\"dump_ok\":" + jsonBool(gApp.dumpOK) + ",";
  j += "\"good_valid\":" + jsonBool(gApp.goodSpdValid) + ",";
  j += "\"good_crc\":\"0x" + String(gApp.goodCrc, HEX) + "\",";
  j += "\"spd_backup_valid\":" + jsonBool(gApp.spdBackupValid) + ",";
  j += "\"spd_backup_crc\":\"0x" + String(gApp.spdBackupCrc, HEX) + "\",";
  j += "\"spd_backup_addr\":" + String(gApp.spdBackupAddr) + ",";
  j += "\"spd_backup_count\":" + String((unsigned long)gApp.spdBackupSaveCount) + ",";
  j += "\"pmic_ref_valid\":" + jsonBool(gApp.pmicRefValid) + ",";
  j += "\"pmic_ref_crc\":\"0x" + String(gApp.pmicRefCrc, HEX) + "\",";
  j += "\"last_dump_valid\":" + jsonBool(gApp.lastDumpValid) + ",";
  j += "\"current_spd_valid\":" + jsonBool(gApp.currentSpdDump.valid) + ",";
  j += "\"current_spd_addr\":\"0x" + String(gApp.currentSpdDump.addr, HEX) + "\",";
  j += "\"current_spd_len\":" + String((unsigned)gApp.currentSpdDump.len) + ",";
  j += "\"current_spd_crc\":\"0x" + String(gApp.currentSpdDump.crc32, HEX) + "\",";
  j += "\"current_spd_generation\":" + String((unsigned long)gApp.currentSpdDump.generation) + ",";
  j += "\"current_spd_source\":\"" + jsonEscape(gApp.currentSpdDump.source) + "\",";
  j += "\"mr11_ok\":" + jsonBool(mrOk) + ",";
  j += "\"mr11\":\"0x" + String(mrOk ? gApp.mr11 : 0, HEX) + "\",";
  j += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  j += "\"scan_count\":" + String((unsigned)gApp.lastScanCount) + ",";
  j += "\"scan_addrs\":[";
  for (size_t i = 0; i < gApp.lastScanCount; i++) {
    if (i) j += ",";
    j += String(gApp.lastScanAddrs[i]);
  }
  j += "],";
  j += "\"roles\":[";
  for (size_t i = 0; i < gApp.roleRecordCount; i++) {
    DeviceRoleRecord& rec = gApp.roleRecords[i];
    if (i) j += ",";
    j += "{";
    j += "\"addr\":" + String(rec.address) + ",";
    j += "\"role\":\"" + String(roleName(rec.role)) + "\",";
    j += "\"label\":\"" + String(roleUiName(rec.role)) + "\",";
    j += "\"confidence\":\"" + String(confidenceName(rec.confidence)) + "\",";
    j += "\"reason\":\"" + jsonEscape(rec.reason) + "\"";
    j += "}";
  }
  j += "]";
  j += "}";

  webSendJson(200, j);
}

static void handleWebLog() {
  webSendText(200, getLogString());
}

static void handleWebRun() {
  if (!server.hasArg("line")) {
    webSendText(400, "ERR: missing 'line'");
    return;
  }

  String line = server.arg("line");
  execCommandLine(line);
  webSendText(200, "OK");
}

static void handleWebClearLog() {
  clearLog();
  webSendText(200, "OK: log cleared");
}

static void handleWebHwConfigSave() {
  HwProfile profile;
  HwHsaMode hsa;
  HwVinBulkMode vin;
  HwPwrEnMode pwrEn;
  HwPwrGoodMode pGood;
  HwI2cMode i2c;
  HwPullupMode pullups;

  if (!server.hasArg("profile") || !server.hasArg("hsa") || !server.hasArg("vinbulk") ||
      !server.hasArg("pwren") || !server.hasArg("pgood") || !server.hasArg("i2c") ||
      !server.hasArg("pullups")) {
    webSendText(400, "ERR: missing hardware config field");
    return;
  }

  if (!parseHwProfile(server.arg("profile").c_str(), profile) ||
      !parseHwHsaMode(server.arg("hsa").c_str(), hsa) ||
      !parseHwVinBulkMode(server.arg("vinbulk").c_str(), vin) ||
      !parseHwPwrEnMode(server.arg("pwren").c_str(), pwrEn) ||
      !parseHwPwrGoodMode(server.arg("pgood").c_str(), pGood) ||
      !parseHwI2cMode(server.arg("i2c").c_str(), i2c) ||
      !parseHwPullupMode(server.arg("pullups").c_str(), pullups)) {
    webSendText(400, "ERR: invalid hardware config field");
    return;
  }

  gApp.hwConfig.profile = profile;
  gApp.hwConfig.hsa = hsa;
  gApp.hwConfig.vinBulk = vin;
  gApp.hwConfig.pwrEn = pwrEn;
  gApp.hwConfig.pwrGood = pGood;
  gApp.hwConfig.i2c = i2c;
  gApp.hwConfig.pullups = pullups;
  saveHardwareConfigToNvs();
  loadHardwareConfigFromNvs();
  webSendText(200, "OK: hardware config saved");
}

static uint8_t webParseSpdAddr() {
  if (server.hasArg("addr")) {
    String v = server.arg("addr");
    v.trim();
    v.toLowerCase();
    if (v.length() > 0 && v != "auto" && v != "current" && v != "observed") {
      return (uint8_t)strtoul(v.c_str(), nullptr, 0);
    }
  }

  for (size_t i = 0; i < gApp.roleRecordCount; i++) {
    if (gApp.roleRecords[i].role == ROLE_SPD_HUB) return gApp.roleRecords[i].address;
  }
  for (size_t i = 0; i < gApp.lastScanCount; i++) {
    uint8_t a = gApp.lastScanAddrs[i];
    if (a >= 0x50 && a <= 0x57) return a;
  }

  uint8_t expectedAddr = 0;
  const char* expectedDesc = nullptr;
  if (hwExpectedSpdAddr(expectedAddr, expectedDesc)) return expectedAddr;
  return DEFAULT_SPD_ADDR;
}

static String webDumpStateJson() {
  String j = "{";
  j += "\"ok\":true,";
  j += "\"valid\":" + jsonBool(gApp.currentSpdDump.valid && gApp.currentSpdDump.len == SPD_NVM_SIZE) + ",";
  j += "\"freeHeap\":" + String((unsigned long)ESP.getFreeHeap());
  if (gApp.currentSpdDump.valid && gApp.currentSpdDump.len == SPD_NVM_SIZE) {
    j += ",\"addr\":\"0x" + String(gApp.currentSpdDump.addr, HEX) + "\"";
    j += ",\"len\":" + String((unsigned)gApp.currentSpdDump.len);
    j += ",\"crc32\":\"0x" + String(gApp.currentSpdDump.crc32, HEX) + "\"";
    j += ",\"generation\":" + String((unsigned long)gApp.currentSpdDump.generation);
    j += ",\"source\":\"" + jsonEscape(gApp.currentSpdDump.source) + "\"";
  }
  j += "}";
  return j;
}

static void handleWebSpdDumpState() {
  webSendJson(200, webDumpStateJson());
}

static const char* webMemoryTypeName(uint8_t v) {
  if (v == 0x12) return "DDR5 SDRAM";
  return "not decoded yet";
}

static const char* webModuleTypeName(uint8_t v) {
  switch (v & 0x0F) {
    case 0x01: return "RDIMM";
    case 0x02: return "UDIMM";
    case 0x03: return "SODIMM";
    case 0x04: return "LRDIMM";
    case 0x0A: return "DDIMM";
    default: return "unknown / raw module type";
  }
}

static void webRawRange(char* out, size_t outLen, const uint8_t* spd, uint16_t off, uint8_t len) {
  if (!out || outLen == 0) return;
  out[0] = 0;
  char tmp[8];
  snprintf(out, outLen, "0x%03X-0x%03X =", off, (uint16_t)(off + len - 1));
  for (uint8_t i = 0; i < len && (off + i) < SPD_NVM_SIZE; i++) {
    snprintf(tmp, sizeof(tmp), " %02X", spd[off + i]);
    strlcat(out, tmp, outLen);
  }
}

static void webAsciiRange(char* out, size_t outLen, const uint8_t* spd, uint16_t off, uint8_t len) {
  if (!out || outLen == 0) return;
  uint8_t n = 0;
  for (uint8_t i = 0; i < len && n < outLen - 1 && (off + i) < SPD_NVM_SIZE; i++) {
    uint8_t c = spd[off + i];
    out[n++] = (c >= 0x20 && c <= 0x7E) ? (char)c : ' ';
  }
  out[n] = 0;
  while (n > 0 && out[n - 1] == ' ') out[--n] = 0;
}

static bool webFindSignature(const uint8_t* spd, const char* sig, uint16_t& off) {
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

static uint16_t webExpoCrc16(const uint8_t* spd) {
  uint16_t crc = 0;
  for (uint16_t off = 0x340; off <= 0x3BD; off++) {
    crc ^= (uint16_t)spd[off] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static void webAppendMeta(String& j, bool& first, const char* name, const char* value, const char* raw) {
  if (!first) j += ",";
  first = false;
  j += "{\"name\":\"" + jsonEscape(name) + "\",";
  j += "\"value\":\"" + jsonEscape(value) + "\",";
  j += "\"raw\":\"" + jsonEscape(raw) + "\"}";
}

static bool webFormatDdrRate(char* out, size_t outLen, uint16_t tckPs, bool withTck) {
  if (!out || outLen == 0 || tckPs == 0) return false;
  uint32_t mtps = spdNearestDdr5Bin(spdRawMtpsFromTckPs(tckPs));
  if (mtps == 0) return false;
  if (withTck) {
    snprintf(out, outLen, "DDR5-%lu / MCLK ~%lu MHz / tCK %u ps",
             (unsigned long)mtps, (unsigned long)spdMclkFromMtps(mtps), (unsigned)tckPs);
  } else {
    snprintf(out, outLen, "DDR5-%lu / MCLK ~%lu MHz",
             (unsigned long)mtps, (unsigned long)spdMclkFromMtps(mtps));
  }
  return true;
}

static void handleWebSpdDecodeCurrentTiny() {
  outPrintln("SPDDECODE API: enter");
  outPrintf("SPDDECODE API: dump valid=%u len=%u crc=0x%08lX freeHeap=%lu\n",
            gApp.currentSpdDump.valid ? 1 : 0,
            (unsigned)gApp.currentSpdDump.len,
            (unsigned long)gApp.currentSpdDump.crc32,
            (unsigned long)ESP.getFreeHeap());

  if (!gApp.currentSpdDump.valid || gApp.currentSpdDump.len != SPD_NVM_SIZE) {
    String err = "{\"ok\":false,\"error\":\"No current 1024-byte SPD dump in RAM. Click Dump 1024 or save a tweak checkpoint first.\"}";
    outPrintf("SPDDECODE API: response bytes=%u\n", (unsigned)err.length());
    webSendJson(200, err);
    outPrintln("SPDDECODE API: done");
    return;
  }

  const uint8_t* spd = gApp.currentSpdDump.data;
  delay(1);
  String compact = spdTweakCompactDecodeJson(gApp.currentSpdDump.addr, spd, gApp.currentSpdDump.len);
  delay(1);
  if (compact.endsWith("}")) {
    compact.remove(compact.length() - 1);
    compact += ",\"dump_crc\":\"0x" + String(gApp.currentSpdDump.crc32, HEX) + "\"";
    compact += ",\"dump_source\":\"" + jsonEscape(gApp.currentSpdDump.source) + "\"";
    compact += ",\"dump_generation\":" + String((unsigned long)gApp.currentSpdDump.generation);
    compact += ",\"free_heap\":" + String((unsigned long)ESP.getFreeHeap());
    compact += ",\"min_free_heap\":" + String((unsigned long)ESP.getMinFreeHeap());
    compact += ",\"max_alloc_heap\":" + String((unsigned long)ESP.getMaxAllocHeap());
    compact += "}";
    outPrintf("SPDDECODE API: compact response bytes=%u\n", (unsigned)compact.length());
    webSendJson(200, compact);
    outPrintln("SPDDECODE API: done");
    return;
  }

  char value[80];
  char raw[120];
  char baseSummary[32];
  char expoSummary[32];
  uint16_t baseTckPs = (uint16_t)spd[0x14] | ((uint16_t)spd[0x15] << 8);
  uint32_t baseMtps = spdNearestDdr5Bin(spdRawMtpsFromTckPs(baseTckPs));
  if (baseMtps) snprintf(baseSummary, sizeof(baseSummary), "DDR5-%lu", (unsigned long)baseMtps);
  else strlcpy(baseSummary, "decode unavailable", sizeof(baseSummary));
  if (spd[0x340] == 'E' && spd[0x341] == 'X' && spd[0x342] == 'P' && spd[0x343] == 'O') {
    uint16_t expoTckPs = (uint16_t)spd[0x34E] | ((uint16_t)spd[0x34F] << 8);
    uint32_t expoMtps = spdNearestDdr5Bin(spdRawMtpsFromTckPs(expoTckPs));
    if (expoMtps) snprintf(expoSummary, sizeof(expoSummary), "DDR5-%lu", (unsigned long)expoMtps);
    else strlcpy(expoSummary, "decode unavailable", sizeof(expoSummary));
  } else {
    strlcpy(expoSummary, "EXPO: not detected", sizeof(expoSummary));
  }
  String j = "{";
  j += "\"ok\":true,";
  j += "\"dump\":{";
  j += "\"addr\":\"0x" + String(gApp.currentSpdDump.addr, HEX) + "\",";
  j += "\"len\":" + String((unsigned)gApp.currentSpdDump.len) + ",";
  j += "\"crc32\":\"0x" + String(gApp.currentSpdDump.crc32, HEX) + "\",";
  j += "\"generation\":" + String((unsigned long)gApp.currentSpdDump.generation) + ",";
  j += "\"source\":\"" + jsonEscape(gApp.currentSpdDump.source) + "\",";
  j += "\"base_speed\":\"" + jsonEscape(baseSummary) + "\",";
  j += "\"expo_speed\":\"" + jsonEscape(expoSummary) + "\"";
  j += "},\"metadata\":[";
  bool first = true;

  snprintf(value, sizeof(value), "%02X %02X %02X %02X", spd[0], spd[1], spd[2], spd[3]);
  webAppendMeta(j, first, "Header bytes", value, "0x000-0x003");

  snprintf(raw, sizeof(raw), "0x002 = 0x%02X", spd[2]);
  webAppendMeta(j, first, "DDR generation", webMemoryTypeName(spd[2]), raw);

  snprintf(value, sizeof(value), "%s", webModuleTypeName(spd[3]));
  snprintf(raw, sizeof(raw), "0x003 = 0x%02X", spd[3]);
  webAppendMeta(j, first, "Module form factor", value, raw);

  webRawRange(raw, sizeof(raw), spd, 0x14, 2);
  if (baseTckPs) snprintf(value, sizeof(value), "%u ps", (unsigned)baseTckPs);
  else strlcpy(value, "decode unavailable", sizeof(value));
  webAppendMeta(j, first, "Base tCK", value, raw);

  if (!webFormatDdrRate(value, sizeof(value), baseTckPs, false)) strlcpy(value, "decode unavailable", sizeof(value));
  webAppendMeta(j, first, "Base data rate", value, "decoded from 0x014-0x015 tCK");

  webAsciiRange(value, sizeof(value), spd, 521, 30);
  if (value[0] == 0) strlcpy(value, "not decoded yet", sizeof(value));
  webRawRange(raw, sizeof(raw), spd, 521, 8);
  webAppendMeta(j, first, "Part number", value, raw);

  uint16_t sigOff = 0;
  if (webFindSignature(spd, "EXPO", sigOff)) {
    webRawRange(raw, sizeof(raw), spd, sigOff, 4);
    webAppendMeta(j, first, "EXPO signature", "present", raw);
    uint16_t expoTckPs = (uint16_t)spd[0x34E] | ((uint16_t)spd[0x34F] << 8);
    webRawRange(raw, sizeof(raw), spd, 0x34E, 2);
    if (expoTckPs) snprintf(value, sizeof(value), "%u ps", (unsigned)expoTckPs);
    else strlcpy(value, "decode unavailable", sizeof(value));
    webAppendMeta(j, first, "EXPO tCK", value, raw);
    if (!webFormatDdrRate(value, sizeof(value), expoTckPs, false)) strlcpy(value, "EXPO speed: decode unavailable", sizeof(value));
    webAppendMeta(j, first, "EXPO data rate", value, "decoded from 0x34E-0x34F tCK");
  } else {
    webAppendMeta(j, first, "EXPO signature", "not detected", "ASCII signature scan");
    webAppendMeta(j, first, "EXPO tCK", "EXPO: not detected", "0x34E-0x34F");
    webAppendMeta(j, first, "EXPO data rate", "EXPO: not detected", "decoded from 0x34E-0x34F tCK");
  }

  if (webFindSignature(spd, "XMP", sigOff)) {
    webRawRange(raw, sizeof(raw), spd, sigOff, 3);
    webAppendMeta(j, first, "XMP signature", "present", raw);
  } else {
    webAppendMeta(j, first, "XMP signature", "not detected", "ASCII signature scan");
  }

  if (spd[0x340] == 'E' && spd[0x341] == 'X' && spd[0x342] == 'P' && spd[0x343] == 'O') {
    uint16_t stored = (uint16_t)spd[0x3BE] | ((uint16_t)spd[0x3BF] << 8);
    uint16_t calc = webExpoCrc16(spd);
    snprintf(value, sizeof(value), "%s", stored == calc ? "PASS" : "FAIL");
    snprintf(raw, sizeof(raw), "stored 0x%04X at 0x3BE-0x3BF, calculated 0x%04X over 0x340-0x3BD", stored, calc);
    webAppendMeta(j, first, "EXPO CRC", value, raw);
    webAppendMeta(j, first, "EXPO CRC repair", "available", "automatic repair updates 0x3BE-0x3BF during preview/apply");
  } else {
    webAppendMeta(j, first, "EXPO CRC", "not checked", "EXPO signature not present at 0x340");
    webAppendMeta(j, first, "EXPO CRC repair", "unavailable", "no EXPO region detected for this dump");
  }

  j += "]";
  j += "}";
  outPrintf("SPDDECODE API: response bytes=%u\n", (unsigned)j.length());
  webSendJson(200, j);
  outPrintln("SPDDECODE API: done");
}

static void handleWebSpdTweakDecode() {
  handleWebSpdDecodeCurrentTiny();
}

static void handleWebSpdTweakDecodeCurrent() {
  handleWebSpdDecodeCurrentTiny();
}

static void handleWebSpdDecodeSection(const char* name) {
  if (!gApp.currentSpdDump.valid || gApp.currentSpdDump.len != SPD_NVM_SIZE) {
    webSendJson(200, "{\"ok\":false,\"error\":\"No current 1024-byte SPD dump in RAM. Click Dump 1024 or save a tweak checkpoint first.\"}");
    return;
  }
  static TimingSection section;
  bool ok = false;
  if (strcmp(name, "base") == 0) ok = spdTweakDecodeBaseSection(gApp.currentSpdDump.data, gApp.currentSpdDump.len, section);
  else if (strcmp(name, "expo") == 0) ok = spdTweakDecodeExpoSection(gApp.currentSpdDump.data, gApp.currentSpdDump.len, section);
  else if (strcmp(name, "xmp") == 0) ok = spdTweakDecodeXmpSection(gApp.currentSpdDump.data, gApp.currentSpdDump.len, section);
  else {
    webSendJson(404, "{\"ok\":false,\"error\":\"unknown decode section\"}");
    return;
  }
  delay(1);
  String j = spdTweakTimingSectionJson(section);
  if (!ok && section.error[0] == 0) {
    j = "{\"ok\":false,\"error\":\"section decode failed\",\"rows\":[]}";
  }
  webSendJson(200, j);
}

static void handleWebSpdDecodeSummary() { handleWebSpdDecodeCurrentTiny(); }
static void handleWebSpdDecodeBase() { handleWebSpdDecodeSection("base"); }
static void handleWebSpdDecodeExpo() { handleWebSpdDecodeSection("expo"); }
static void handleWebSpdDecodeXmp() { handleWebSpdDecodeSection("xmp"); }

static void handleWebDumpAndDecode() {
  webSendJson(400, "{\"ok\":false,\"error\":\"Web capture is disabled in this build. Use Dump 1024 or Map Current Mode, then Decode current SPD dump.\"}");
}

static void collectEditArgs(const char* ids[], const char* values[], int& count) {
  count = 0;
  size_t n = server.args();
  for (size_t i = 0; i < n && count < 16; i++) {
    String name = server.argName(i);
    if (name == "labmode") continue;
    if (name.startsWith("field_")) {
      ids[count] = server.argName(i).c_str() + 6;
      values[count] = server.arg(i).c_str();
      count++;
    }
  }
}

static void handleWebSpdEditFields() {
  webSendJson(200, spdEditFieldsJson());
}

static void handleWebSpdEditPreview() {
  const char* ids[16] = {0};
  const char* values[16] = {0};
  String idStore[16];
  String valStore[16];
  int count = 0;
  size_t n = server.args();
  for (size_t i = 0; i < n && count < 16; i++) {
    String name = server.argName(i);
    if (!name.startsWith("field_")) continue;
    idStore[count] = name.substring(6);
    valStore[count] = server.arg(i);
    ids[count] = idStore[count].c_str();
    values[count] = valStore[count].c_str();
    count++;
  }
  webSendJson(200, spdEditPreviewJson(count, ids, values));
}

static void handleWebSpdEditApply() {
  const char* ids[16] = {0};
  const char* values[16] = {0};
  String idStore[16];
  String valStore[16];
  int count = 0;
  size_t n = server.args();
  for (size_t i = 0; i < n && count < 16; i++) {
    String name = server.argName(i);
    if (!name.startsWith("field_")) continue;
    idStore[count] = name.substring(6);
    valStore[count] = server.arg(i);
    ids[count] = idStore[count].c_str();
    values[count] = valStore[count].c_str();
    count++;
  }
  bool armed = server.hasArg("labmode") && server.arg("labmode") == "LABMODE" &&
               server.hasArg("laback") && server.arg("laback") == "1";
  webSendJson(200, spdEditApplyJson(armed, count, ids, values));
}

static void handleWebWriteDiagnosticReference() {
  const char* phrase = "WRITE DIAGNOSTIC REFERENCE";
  bool armed = server.hasArg("labmode") && server.arg("labmode") == "LABMODE" &&
               server.hasArg("laback") && server.arg("laback") == "1";
  if (!armed) {
    webSendJson(403, "{\"ok\":false,\"error\":\"Blocked: Lab SPD Write Mode checkbox and LABMODE text are required.\"}");
    return;
  }
  if (!server.hasArg("confirm") || server.arg("confirm") != phrase) {
    webSendJson(403, "{\"ok\":false,\"error\":\"Blocked: exact confirmation phrase is required.\"}");
    return;
  }
  if (!gApp.goodSpdValid) {
    webSendJson(409, "{\"ok\":false,\"error\":\"Blocked: no Diagnostic SPD Reference is stored.\"}");
    return;
  }

  uint8_t addr = webParseSpdAddr();
  if (addr < 0x50 || addr > 0x57) {
    webSendJson(400, "{\"ok\":false,\"error\":\"Blocked: resolved SPD/HUB address is outside 0x50-0x57.\"}");
    return;
  }

  uint32_t currentCrc = (gApp.currentSpdDump.valid && gApp.currentSpdDump.len == SPD_NVM_SIZE)
    ? gApp.currentSpdDump.crc32
    : 0;
  bool ok = cmdWriteGood(addr, true);
  bool dumpOk = dumpToBufferWithSource(addr, "writegood-verify");
  uint32_t mismatches = 0;
  uint16_t first = 0;
  uint8_t readB = 0;
  uint8_t refB = 0;
  if (dumpOk) {
    for (uint16_t off = 0; off < SPD_NVM_SIZE; off++) {
      if (gApp.lastDump[off] != gApp.goodSpd[off]) {
        if (mismatches == 0) {
          first = off;
          readB = gApp.lastDump[off];
          refB = gApp.goodSpd[off];
        }
        mismatches++;
      }
    }
  }
  bool verified = ok && dumpOk && mismatches == 0;
  String j = "{";
  j += "\"ok\":" + jsonBool(verified) + ",";
  j += "\"addr\":\"0x" + String(addr, HEX) + "\",";
  j += "\"reference_crc\":\"0x" + String(gApp.goodCrc, HEX) + "\",";
  j += "\"current_crc_before\":\"" + String(currentCrc ? ("0x" + String(currentCrc, HEX)) : String("unavailable")) + "\",";
  j += "\"readback_crc\":\"" + String(dumpOk ? ("0x" + String(crc32_buf(gApp.lastDump, SPD_NVM_SIZE), HEX)) : String("unavailable")) + "\",";
  j += "\"write_result\":\"" + String(ok ? "PASS" : "FAIL") + "\",";
  j += "\"readback_dump\":\"" + String(dumpOk ? "PASS" : "FAIL") + "\",";
  j += "\"verify\":\"" + String(verified ? "PASS" : "FAIL") + "\",";
  j += "\"mismatches\":" + String((unsigned long)mismatches) + ",";
  j += "\"first_mismatch\":\"";
  if (mismatches) {
    j += "0x" + String(first, HEX) + " read=0x" + String(readB, HEX) + " ref=0x" + String(refB, HEX);
  }
  j += "\"";
  if (!verified) j += ",\"error\":\"Diagnostic reference write/readback verification failed.\"";
  j += "}";
  webSendJson(200, j);
}

static void handleWebSpdTweakPreview() {
  uint8_t addr = webParseSpdAddr();
  setProcessing(true);
  bool ok = dumpToBuffer(addr);
  setProcessing(false);
  if (!ok) {
    webSendJson(500, "{\"ok\":false,\"error\":\"SPD dump failed\"}");
    return;
  }

  char* assignments[1] = {0};
  SpdTweakPatch patch;
  ok = spdTweakBuildPatch(gApp.lastDump, 0, assignments, patch);
  String j = "{";
  j += "\"ok\":" + String(ok ? "true" : "false") + ",";
  j += "\"addr\":\"0x" + String(addr, HEX) + "\",";
  j += "\"message\":\"" + String(patch.message) + "\",";
  j += "\"has_changes\":" + String(patch.hasChanges ? "true" : "false") + ",";
  j += "\"crc_repair_supported\":" + String(patch.crcRepairSupported ? "true" : "false") + ",";
  j += "\"diff\":\"no byte changes\"";
  j += "}";
  webSendJson(200, j);
}

static void handleWebSpdTweakApply() {
  if (!server.hasArg("token") || server.arg("token") != SPD_TWEAK_WRITE_TOKEN) {
    webSendJson(403, "{\"ok\":false,\"error\":\"missing YES_WRITE_SPD_TWEAK\"}");
    return;
  }

  webSendJson(403, "{\"ok\":false,\"error\":\"SPD tweak apply is blocked: no verified DDR5 SPD profile field map / CRC updater is implemented yet.\"}");
}

// ===================== WEB: DOWNLOADS =====================
static void handleDownloadCurrent() {
  if (gApp.hwConfig.pwrGood == HW_PGOOD_GPIO_READ && !pwrGoodReady() &&
      !gApp.scanOK && !gApp.readOK && !gApp.dumpOK) {
    webSendText(400, "PWR_GOOD GPIO34 measured LOW; check wiring/readiness or hardware config before trusting SPD/PMIC reads");
    return;
  }

  if (!gApp.lastDumpValid) {
    bool ok = dumpToBuffer(DEFAULT_SPD_ADDR);
    if (!ok) {
      webSendText(500, "dump failed");
      return;
    }
    gApp.dumpOK = true;
    gApp.readOK = true;
  }

  webSendBinFromBuf(gApp.lastDump, SPD_NVM_SIZE, "current_spd.bin");
}

static void handleDownloadGood() {
  if (!gApp.goodSpdValid) {
    webSendText(404, "no diagnostic SPD reference stored");
    return;
  }

  webSendBinFromBuf(gApp.goodSpd, SPD_NVM_SIZE, "good_spd.bin");
}

// ===================== WEB UI (stored in flash) =====================
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>ESP32 SPD Tool</title>
<style>
  :root{
    --bg:#f5f7fb;
    --card:#ffffff;
    --line:#d8dee8;
    --text:#17212b;
    --muted:#5b6672;
    --term:#0b0f14;
    --termText:#d2e7ff;
    --green:#1f9d55;
    --red:#d64545;
    --blue:#2d6cdf;
    --amber:#b7791f;
    --input:#ffffff;
    --soft:#f8fafc;
    --button:#f8fafc;
    --buttonBorder:#b8c2cf;
    --disabledBg:#eef1f5;
    --disabledText:#7a8490;
    --focus:#2d6cdf;
    --pillOkBg:#ebfff3;
    --pillOkText:#11693d;
    --pillOkBorder:#b9e8cb;
    --pillBadBg:#fff0f0;
    --pillBadText:#9d2626;
    --pillBadBorder:#efb6b6;
    --pillNeutralBg:#eef3f8;
    --pillNeutralText:#415261;
    --pillNeutralBorder:#d2dce7;
    --warnBg:#fff6dd;
    --warnText:#7a4b00;
    --warnBorder:#f0c46a;
    --changedRow:#fff8e8;
    --readOnlyRow:#f7f8fa;
    color-scheme:light;
  }

  @media (prefers-color-scheme: dark){
    :root:not([data-theme="light"]){
      --bg:#111827;
      --card:#1f2937;
      --line:#374151;
      --text:#e5e7eb;
      --muted:#aeb7c2;
      --input:#0f172a;
      --soft:#162033;
      --button:#1d2a3d;
      --buttonBorder:#4b5563;
      --disabledBg:#253044;
      --disabledText:#a8b2c0;
      --focus:#7aa2ff;
      --pillOkBg:#123523;
      --pillOkText:#9ee6b8;
      --pillOkBorder:#276749;
      --pillBadBg:#3a171b;
      --pillBadText:#ffb4b4;
      --pillBadBorder:#7f2d35;
      --pillNeutralBg:#253044;
      --pillNeutralText:#c4ceda;
      --pillNeutralBorder:#4b5563;
      --warnBg:#3a2d12;
      --warnText:#ffd98a;
      --warnBorder:#8a661e;
      --changedRow:#342915;
      --readOnlyRow:#1b2535;
      color-scheme:dark;
    }
  }

  :root[data-theme="dark"]{
    --bg:#111827;
    --card:#1f2937;
    --line:#374151;
    --text:#e5e7eb;
    --muted:#aeb7c2;
    --input:#0f172a;
    --soft:#162033;
    --button:#1d2a3d;
    --buttonBorder:#4b5563;
    --disabledBg:#253044;
    --disabledText:#a8b2c0;
    --focus:#7aa2ff;
    --pillOkBg:#123523;
    --pillOkText:#9ee6b8;
    --pillOkBorder:#276749;
    --pillBadBg:#3a171b;
    --pillBadText:#ffb4b4;
    --pillBadBorder:#7f2d35;
    --pillNeutralBg:#253044;
    --pillNeutralText:#c4ceda;
    --pillNeutralBorder:#4b5563;
    --warnBg:#3a2d12;
    --warnText:#ffd98a;
    --warnBorder:#8a661e;
    --changedRow:#342915;
    --readOnlyRow:#1b2535;
    color-scheme:dark;
  }

  *{box-sizing:border-box}
  body{
    font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;
    margin:12px;
    background:var(--bg);
    color:var(--text);
  }

  h2{margin:0 0 10px 0}
  h3{margin:0 0 10px 0}

  .section{margin-top:12px}
  .card{
    border:1px solid var(--line);
    border-radius:14px;
    padding:12px;
    background:var(--card);
    box-shadow:0 1px 3px rgba(0,0,0,.04);
  }

  .grid{
    display:grid;
    grid-template-columns:1fr;
    gap:12px;
  }

  .hardware-layout{
    display:grid;
    grid-template-columns:1fr;
    gap:12px;
    align-items:stretch;
  }

  @media (min-width: 721px) and (max-width: 1100px){
    .hardware-layout{
      grid-template-columns:1fr 1fr;
    }
    .hardware-config-card{
      grid-column:1 / -1;
    }
  }

  @media (min-width: 1101px){
    .hardware-layout{
      grid-template-columns:2fr 1fr 1fr;
    }
  }

  .hardware-layout .card{
    padding:10px;
  }

  .hardware-layout h3{
    margin-bottom:6px;
  }

  .hardware-layout .muted,
  .hardware-layout .subtle,
  .hardware-layout .toggleMeta{
    font-size:11px;
    line-height:1.3;
  }

  .hardware-layout .fieldGrid{
    gap:6px;
    margin-top:8px;
  }

  .hardware-layout .fieldBox{
    gap:4px;
  }

  .hardware-layout select{
    min-width:0;
    width:100%;
  }

  .hardware-layout button{
    padding:8px 10px;
  }

  .power-card .toggleWrap{
    gap:8px;
  }

  .power-card .toggleRow,
  .hsa-card .toggleRow{
    padding:8px 9px;
  }

  .hsa-card .row button{
    flex:1 1 130px;
  }

  @media (min-width: 1000px){
    .grid.two{grid-template-columns:1fr 1fr;}
    .grid.three{grid-template-columns:1fr 1fr 1fr;}
  }

  .row{
    display:flex;
    flex-wrap:wrap;
    gap:8px;
    align-items:center;
  }

  .toolbar{
    display:flex;
    flex-wrap:wrap;
    gap:8px;
    align-items:center;
    justify-content:space-between;
    margin-bottom:10px;
  }

  .muted{color:var(--muted);font-size:13px}

  button{
    padding:10px 12px;
    border-radius:10px;
    border:1px solid var(--buttonBorder);
    background:var(--button);
    cursor:pointer;
    color:var(--text);
    font-weight:600;
  }

  button:hover{filter:brightness(.96)}
  button:active{transform:translateY(1px)}
  button:disabled{
    background:var(--disabledBg);
    color:var(--disabledText);
    border-color:var(--line);
    opacity:1;
    cursor:not-allowed;
    transform:none;
  }
  a button{cursor:pointer}

  .btnBlue{background:#edf4ff;border-color:#bfd4ff;color:#102a54}
  .btnAmber{background:#fff6e7;border-color:#f0d28f;color:#4c3100}
  .btnRed{background:#fff1f1;border-color:#f1b2b2;color:#5f1111}
  .btnGreen{background:#ecfff4;border-color:#b7e7c8;color:#0c3a20}

  :root[data-theme="dark"] .btnBlue{background:#14325d;border-color:#315f9d;color:#cfe2ff}
  :root[data-theme="dark"] .btnAmber{background:#3a2d12;border-color:#8a661e;color:#ffd98a}
  :root[data-theme="dark"] .btnRed{background:#3a171b;border-color:#7f2d35;color:#ffb4b4}
  :root[data-theme="dark"] .btnGreen{background:#123523;border-color:#276749;color:#9ee6b8}

  @media (prefers-color-scheme: dark){
    :root:not([data-theme="light"]) .btnBlue{background:#14325d;border-color:#315f9d;color:#cfe2ff}
    :root:not([data-theme="light"]) .btnAmber{background:#3a2d12;border-color:#8a661e;color:#ffd98a}
    :root:not([data-theme="light"]) .btnRed{background:#3a171b;border-color:#7f2d35;color:#ffb4b4}
    :root:not([data-theme="light"]) .btnGreen{background:#123523;border-color:#276749;color:#9ee6b8}
  }

  input[type="text"], input[type="number"], textarea, select{
    padding:10px;
    border-radius:10px;
    border:1px solid var(--buttonBorder);
    min-width:240px;
    flex:1;
    background:var(--input);
    color:var(--text);
  }

  input::placeholder,
  textarea::placeholder{
    color:var(--muted);
    opacity:1;
  }

  input:focus,
  textarea:focus,
  select:focus{
    outline:2px solid var(--focus);
    outline-offset:1px;
  }

  input[disabled],
  input[readonly],
  textarea[disabled],
  textarea[readonly],
  select[disabled]{
    background:var(--disabledBg);
    color:var(--disabledText);
    border-color:var(--line);
    opacity:1;
    cursor:not-allowed;
  }

  select option{
    background:var(--input);
    color:var(--text);
  }

  pre{
    background:var(--term);
    color:var(--termText);
    padding:12px;
    border-radius:12px;
    overflow:auto;
    font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, "Liberation Mono", monospace;
    line-height:1.25;
    font-size:13px;
    margin:0;
    white-space:pre-wrap;
    word-break:break-word;
  }

  #term{height:62vh}

  .pills{
    display:flex;
    flex-wrap:wrap;
    gap:8px;
    margin-top:8px;
  }

  .pill{
    border-radius:999px;
    padding:6px 10px;
    font-size:12px;
    font-weight:700;
    border:1px solid transparent;
  }

  .pill.ok{
    background:var(--pillOkBg);
    color:var(--pillOkText);
    border-color:var(--pillOkBorder);
  }

  .pill.bad{
    background:var(--pillBadBg);
    color:var(--pillBadText);
    border-color:var(--pillBadBorder);
  }

  .pill.neutral{
    background:var(--pillNeutralBg);
    color:var(--pillNeutralText);
    border-color:var(--pillNeutralBorder);
  }

  .toggleWrap{
    display:flex;
    flex-direction:column;
    gap:12px;
  }

  .toggleRow{
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:12px;
    padding:10px 12px;
    border:1px solid var(--line);
    border-radius:12px;
    background:var(--soft);
  }

  .toggleLeft{
    display:flex;
    flex-direction:column;
    gap:4px;
  }

  .toggleTitle{
    font-weight:700;
  }

  .toggleMeta{
    font-size:12px;
    color:var(--muted);
  }

  .toggleRight{
    display:flex;
    align-items:center;
    gap:10px;
  }

  .stateBadge{
    min-width:44px;
    text-align:center;
    border-radius:999px;
    padding:5px 10px;
    font-size:12px;
    font-weight:800;
    border:1px solid transparent;
  }

  .stateOn{background:var(--pillOkBg);color:var(--pillOkText);border-color:var(--pillOkBorder)}
  .stateOff{background:var(--pillBadBg);color:var(--pillBadText);border-color:var(--pillBadBorder)}

  .switch{
    position:relative;
    display:inline-block;
    width:62px;
    height:34px;
    flex:0 0 auto;
  }

  .switch input{
    opacity:0;
    width:0;
    height:0;
  }

  .slider{
    position:absolute;
    cursor:pointer;
    inset:0;
    background:var(--red);
    transition:.2s;
    border-radius:999px;
    box-shadow: inset 0 0 0 1px rgba(0,0,0,.08);
  }

  .slider:before{
    content:"";
    position:absolute;
    height:26px;
    width:26px;
    left:4px;
    top:4px;
    background:white;
    transition:.2s;
    border-radius:50%;
    box-shadow:0 1px 3px rgba(0,0,0,.25);
  }

  input:checked + .slider{
    background:var(--green);
  }

  input:checked + .slider:before{
    transform:translateX(28px);
  }

  input:disabled + .slider{
    opacity:1;
    cursor:not-allowed;
    background:var(--disabledBg);
  }

  .deviceList{
    display:grid;
    grid-template-columns:1fr;
    gap:10px;
    margin-top:10px;
  }

  @media (min-width: 1100px){
    .deviceList{
      grid-template-columns:1fr 1fr;
    }
  }

  .deviceCard{
    border:1px solid var(--line);
    border-radius:14px;
    padding:12px;
    background:var(--soft);
  }

  .deviceHeader{
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:12px;
    margin-bottom:10px;
  }

  .deviceAddr{
    font-weight:800;
    font-size:18px;
  }

  .deviceActions{
    display:flex;
    flex-wrap:wrap;
    gap:8px;
  }

  .subtle{
    font-size:12px;
    color:var(--muted);
    margin-top:6px;
  }

  select{min-width:180px}

  .fieldGrid{
    display:grid;
    grid-template-columns:1fr;
    gap:8px;
    margin-top:10px;
  }

  @media (min-width: 900px){
    .fieldGrid{grid-template-columns:1fr 1fr 1fr;}
  }

  .fieldBox{
    display:flex;
    flex-direction:column;
    gap:6px;
  }

  .fieldBox label{
    font-size:12px;
    font-weight:700;
    color:var(--muted);
  }

  .kvTable{
    width:100%;
    border-collapse:collapse;
    margin-top:10px;
    font-size:13px;
  }

  .kvTable th,.kvTable td{
    border-bottom:1px solid var(--line);
    padding:7px 6px;
    text-align:left;
    vertical-align:top;
  }

  .kvTable th{
    color:var(--muted);
    font-size:12px;
  }

  .tableScroll{
    overflow:auto;
    margin-top:6px;
    border:1px solid var(--line);
    border-radius:8px;
  }

  .timingTable{
    width:100%;
    min-width:980px;
    border-collapse:collapse;
    font-size:11px;
  }

  .timingTable th,.timingTable td{
    border-bottom:1px solid var(--line);
    border-right:1px solid var(--line);
    padding:5px 6px;
    text-align:left;
    vertical-align:top;
  }

  .timingTable th{
    color:var(--muted);
    background:var(--soft);
    font-size:10px;
  }

  .compact-table{
    min-width:760px;
  }

  .compact-table th,
  .compact-table td{
    padding:3px 5px;
    font-size:11px;
    vertical-align:middle;
  }

  .compact-input{
    min-height:24px;
    height:24px;
    padding:1px 5px !important;
    font-size:11px;
    min-width:70px !important;
    max-width:92px;
    border-radius:6px !important;
    flex:0 0 auto !important;
  }

  .status-badge{
    display:inline-block;
    padding:1px 5px;
    border-radius:6px;
    font-size:10px;
    font-weight:800;
    border:1px solid var(--pillNeutralBorder);
    background:var(--pillNeutralBg);
    color:var(--pillNeutralText);
    margin-right:3px;
    white-space:nowrap;
  }

  .status-badge.warn{
    border-color:var(--warnBorder);
    background:var(--warnBg);
    color:var(--warnText);
  }

  .status-badge.locked{
    border-color:var(--line);
    background:var(--disabledBg);
    color:var(--disabledText);
  }

  .profileSelector{
    display:flex;
    flex-wrap:wrap;
    gap:6px;
    align-items:center;
    margin-top:10px;
    font-size:12px;
    font-weight:700;
  }

  .profileSelector button{
    padding:5px 8px;
    font-size:12px;
  }

  .cellValue{
    font-weight:700;
    margin-bottom:3px;
  }

  .cellMeta{
    color:var(--muted);
    font-size:11px;
    line-height:1.25;
  }

  .sectionTitle{
    margin-top:10px;
    font-size:13px;
    font-weight:800;
  }

  details.advanced{
    border:1px solid var(--line);
    border-radius:14px;
    padding:0;
    background:var(--card);
    box-shadow:0 1px 3px rgba(0,0,0,.04);
  }

  details.advanced > summary{
    list-style:none;
    cursor:pointer;
    padding:10px;
  }

  details.advanced > summary::-webkit-details-marker{display:none}

  .advancedHeader{
    display:flex;
    flex-wrap:wrap;
    gap:8px;
    align-items:baseline;
    justify-content:space-between;
  }

  .advancedBody{
    border-top:1px solid var(--line);
    padding:10px;
  }

  .miniBlock{
    margin-top:10px;
  }

  .danger-banner{
    border:2px solid var(--pillBadBorder);
    background:var(--pillBadBg);
    color:var(--pillBadText);
    padding:10px 12px;
    border-radius:8px;
    margin-bottom:10px;
  }

  .danger-banner-title{
    font-weight:900;
    font-size:15px;
    margin-bottom:6px;
  }

  .danger-banner p{
    margin:5px 0;
    line-height:1.35;
  }

  .applyWarning{
    border:1px solid var(--warnBorder);
    background:var(--soft);
    color:var(--warnText);
    border-radius:8px;
    padding:6px 8px;
    font-size:11px;
    font-weight:700;
    margin-top:10px;
  }

  .statusStrip{
    display:flex;
    flex-wrap:wrap;
    gap:6px;
    align-items:center;
    border:1px solid var(--line);
    border-radius:8px;
    background:var(--soft);
    padding:6px 8px;
    margin-top:8px;
    font-size:11px;
  }

  .statusStrip strong{
    font-weight:900;
  }

  .guardPanel{
    border:1px solid var(--warnBorder);
    border-radius:8px;
    padding:8px 10px;
    background:var(--soft);
    margin-top:8px;
  }

  .guardPanel.armed{
    background:var(--soft);
    border-color:var(--warnBorder);
    color:var(--warnText);
  }

  .guardTitle{
    font-weight:900;
    font-size:13px;
  }

  .armControls{
    display:grid;
    grid-template-columns:1fr;
    gap:8px;
    margin-top:8px;
  }

  @media (min-width: 900px){
    .armControls{
      grid-template-columns:1.35fr minmax(180px,260px) 1.25fr;
      align-items:center;
    }
  }

  .rawDetails{
    margin-top:10px;
    border:1px solid var(--line);
    border-radius:8px;
    background:var(--soft);
  }

  .rawDetails > summary{
    cursor:pointer;
    padding:8px 10px;
    font-weight:800;
  }

  .rawDetailsBody{
    border-top:1px solid var(--line);
    padding:8px 10px 10px;
  }

  .collapsedBlock{
    margin-top:10px;
    border:1px solid var(--line);
    border-radius:8px;
    background:var(--soft);
  }

  .collapsedBlock > summary{
    cursor:pointer;
    padding:8px 10px;
    font-weight:800;
  }

  .collapsedBlockBody{
    border-top:1px solid var(--line);
    padding:8px 10px 10px;
  }

  .toolGrid{
    display:grid;
    grid-template-columns:1fr;
    gap:12px;
  }

  @media (min-width: 1100px){
    .toolGrid{grid-template-columns:1fr 1fr 1fr;}
  }

  .resultBadge{
    display:inline-flex;
    align-items:center;
    min-height:28px;
    padding:4px 9px;
    border-radius:999px;
    border:1px solid var(--line);
    font-weight:900;
    font-size:12px;
  }

  .resultBadge.pass{background:var(--pillOkBg);color:var(--pillOkText);border-color:var(--pillOkBorder)}
  .resultBadge.warn{background:var(--warnBg);color:var(--warnText);border-color:var(--warnBorder)}
  .resultBadge.fail{background:var(--pillBadBg);color:var(--pillBadText);border-color:var(--pillBadBorder)}
  .resultBadge.idle{background:var(--soft);color:var(--muted)}

  .metricTable{
    width:100%;
    border-collapse:collapse;
    margin-top:10px;
    font-size:12px;
  }

  .metricTable td{
    border-bottom:1px solid var(--line);
    padding:5px 4px;
    vertical-align:top;
  }

  .metricTable td:first-child{
    color:var(--muted);
    font-weight:700;
    width:42%;
  }

  .themeToggle button.active{
    outline:2px solid var(--blue);
  }

  tr.changedRow td{background:var(--changedRow)}
  tr.readOnlyRow td{background:var(--readOnlyRow);color:var(--disabledText)}
</style>
</head>
<body>
<h2>ESP32 SPD Tool</h2>

<div class="card">
  <div class="toolbar">
    <div class="muted">Live state</div>
    <div class="row">
      <div class="row themeToggle">
        <button id="themeLight" onclick="setThemeMode('light')">Light</button>
        <button id="themeDark" onclick="setThemeMode('dark')">Dark</button>
        <button id="themeAuto" onclick="setThemeMode('auto')">Auto</button>
      </div>
      <button onclick="refreshStatus()">Refresh</button>
      <button onclick="refreshLog()">Refresh Log</button>
    </div>
  </div>
  <div class="pills">
    <span id="pillPwrGood" class="pill neutral">PWR_GOOD: --</span>
    <span id="pillPwrEn" class="pill neutral">PWR_EN pin: --</span>
    <span id="pillDimm" class="pill neutral">VIN_BULK: --</span>
  <span id="pillHsa" class="pill neutral">HSA: --</span>
  <span id="pillHw" class="pill neutral">Hardware config: --</span>
  <span id="pillGood" class="pill neutral">Diagnostic ref: --</span>
    <span id="pillSpdBak" class="pill neutral">Tweak checkpoint: --</span>
    <span id="pillPmicRef" class="pill neutral">PMIC reference: --</span>
    <span id="pillScan" class="pill neutral">SCAN: --</span>
  </div>
</div>

<div class="hardware-layout section">
<div class="card hardware-config-card">
  <h3>Hardware Config</h3>
  <div class="muted">User-declared physical harness wiring. This is not auto-measured truth; it keeps status/logs honest when GPIOs are not actually wired to controls.</div>
  <div id="hwSummary" class="subtle">Hardware config: --</div>
  <div class="row" style="margin-top:10px">
    <select id="hwPreset">
      <option value="unknown">unknown</option>
      <option value="full">full</option>
      <option value="minimum_direct">minimum_direct</option>
      <option value="manual">manual</option>
    </select>
    <button class="btnBlue" onclick="saveHwPreset()">Save preset</button>
    <button onclick="sendCmdValue('hwconfig')">Show config</button>
    <button class="btnRed" onclick="confirmRun('hwconfig clear','Clear hardware config and return to conservative unknown defaults?')">Clear config</button>
  </div>
  <div class="fieldGrid">
    <div class="fieldBox">
      <label>HSA</label>
      <select id="hwHsa">
        <option value="unknown">unknown</option>
        <option value="gpio">gpio_switchable</option>
        <option value="ground">manual_ground</option>
        <option value="resistor">manual_resistor_strap</option>
        <option value="floating">manual_floating_high</option>
      </select>
    </div>
    <div class="fieldBox">
      <label>VIN_BULK</label>
      <select id="hwVin">
        <option value="unknown">unknown</option>
        <option value="gpio">gpio_switchable</option>
        <option value="locked">external_locked_on</option>
        <option value="manual">external_manual_switch</option>
      </select>
    </div>
    <div class="fieldBox">
      <label>PWR_EN</label>
      <select id="hwPwrEn">
        <option value="unknown">unknown</option>
        <option value="gpio">gpio_controlled</option>
        <option value="pullup">pullup_only</option>
        <option value="none">not_connected</option>
      </select>
    </div>
    <div class="fieldBox">
      <label>PWR_GOOD</label>
      <select id="hwPGood">
        <option value="unknown">unknown</option>
        <option value="gpio">gpio_read</option>
        <option value="pullup">pullup_only</option>
        <option value="none">not_connected</option>
      </select>
    </div>
    <div class="fieldBox">
      <label>I2C interface</label>
      <select id="hwI2c">
        <option value="unknown">unknown</option>
        <option value="pca9306">pca9306_level_shifter</option>
        <option value="direct">direct_esp32_3v3</option>
        <option value="other">other_level_shifter</option>
      </select>
    </div>
    <div class="fieldBox">
      <label>SDA/SCL pullups</label>
      <select id="hwPullups">
        <option value="unknown">unknown</option>
        <option value="external">external_3v3</option>
        <option value="internal">esp32_internal_only</option>
        <option value="module">module_or_board_provided</option>
      </select>
    </div>
  </div>
  <div class="row" style="margin-top:10px">
    <button class="btnGreen" onclick="saveHwManual()">Save config</button>
  </div>
</div>

  <div class="card power-card">
    <h3>VIN_BULK Power</h3>
    <div class="toggleWrap">
      <div class="toggleRow">
        <div class="toggleLeft">
          <div class="toggleTitle">VIN_BULK Power</div>
          <div class="toggleMeta">GPIO32 optional 5 V VIN_BULK switch</div>
        </div>
        <div class="toggleRight">
          <span id="dimmState" class="stateBadge stateOff">OFF</span>
          <label class="switch">
            <input type="checkbox" id="dimmToggle" onchange="toggleDimmPower()">
            <span class="slider"></span>
          </label>
        </div>
      </div>

      <div class="toggleRow">
        <div class="toggleLeft">
          <div class="toggleTitle">PWR_EN pin / PMIC enable input</div>
          <div class="toggleMeta">GPIO33 PWR_EN; optional for SPD/PMIC sideband reads</div>
        </div>
        <div class="toggleRight">
          <span id="pwrEnState" class="stateBadge stateOff">OFF</span>
          <label class="switch">
            <input type="checkbox" id="pwrEnToggle" onchange="togglePwrEn()">
            <span class="slider"></span>
          </label>
        </div>
      </div>

      <div class="row">
        <button class="btnAmber" id="dimmCycleBtn" onclick="cycleDimmPowerUi()">Cold-cycle VIN_BULK</button>
      </div>
      <div class="subtle" id="dimmControlNote"></div>
      <div class="subtle" id="pwrEnControlNote"></div>
    </div>
  </div>

  <div class="card hsa-card">
    <h3>HSA / Address Strap</h3>
    <div class="toggleWrap">
      <div class="toggleRow">
        <div class="toggleLeft">
          <div class="toggleTitle" id="hsaState">HSA config: --</div>
          <div class="toggleMeta" id="hsaObserved">Observed SPD/HUB address: --</div>
        </div>
      </div>
    </div>
    <div class="row">
      <button id="hsaReleaseBtn" onclick="hsaReleaseUi()">Release HSA GPIO</button>
      <button id="hsaGroundBtn" onclick="hsaGroundUi()">Drive HSA GND/OFFLINE</button>
    </div>
    <div class="subtle" id="hsaControlNote">HSA is sampled at cold power-up. Changing GPIO27 does not change effective HSA mode unless the DIMM is fully power-cycled.</div>
  </div>
</div>

<div class="card section">
  <div class="toolbar">
    <div>
      <h3>Tools</h3>
      <div class="muted">Read-only SPD/PMIC/I2C diagnostics. These tools do not write SPD, change straps, toggle PWR_EN, or modify saved references.</div>
    </div>
    <div class="row">
      <button class="btnBlue" onclick="sendCmdValue('scan')">Scan</button>
      <button class="btnBlue" onclick="sendCmdValue('autodetect')">Auto-detect Roles</button>
      <button class="btnBlue" onclick="sendCmdValue('status')">Status</button>
      <button class="btnBlue" onclick="sendCmdValue('map')">Map Current Mode</button>
    </div>
  </div>

  <div class="toolGrid">
    <div class="miniBlock">
      <div class="toolbar">
        <div class="sectionTitle">Quick Health Check</div>
        <span id="healthBadge" class="resultBadge idle">IDLE</span>
      </div>
      <button class="btnGreen" onclick="runDiagnosticCard('health','healthBadge','healthText','healthLog')">Run Quick Health Check</button>
      <div class="subtle" id="healthText">Fast read-only sanity test for scan, SPD header, MR11, PMIC ID, PWR_GOOD, diagnostic reference, and tweak checkpoint.</div>
      <details class="collapsedBlock">
        <summary>Show result log</summary>
        <div class="collapsedBlockBody"><pre id="healthLog" style="height:170px"></pre></div>
      </details>
    </div>

    <div class="miniBlock">
      <div class="toolbar">
        <div class="sectionTitle">Speed / Stability Test</div>
        <span id="speedBadge" class="resultBadge idle">IDLE</span>
      </div>
      <div class="subtle" id="speedText">Tests SPD/I2C/PMIC diagnostic read stability only. This does not test RAM bandwidth or memory-cell stability.</div>
      <div class="fieldGrid">
        <div class="fieldBox"><label>SPD addr</label><input id="speedAddr" type="text" value="auto"></div>
        <div class="fieldBox"><label>Read length</label><select id="speedLen"><option>16</option><option selected>32</option><option>64</option><option>256</option><option>1024</option></select></div>
        <div class="fieldBox"><label>Passes</label><input id="speedPasses" type="number" min="1" max="500" value="20"></div>
        <div class="fieldBox"><label>Delay ms</label><input id="speedDelay" type="number" min="0" max="10000" value="20"></div>
      </div>
      <div class="row" style="margin-top:8px">
        <label><input id="speedScan" type="checkbox" checked> Include scan consistency</label>
        <label><input id="speedPmic" type="checkbox" checked onchange="speedPmicTouched=true"> Include PMIC quick check</label>
      </div>
      <button class="btnGreen" style="margin-top:8px" onclick="runSpeedDiagnostic()">Run Speed / Stability Test</button>
      <table class="metricTable" id="speedMetrics"><tbody><tr><td>Status</td><td>No run yet.</td></tr></tbody></table>
      <details class="collapsedBlock">
        <summary>Show result log</summary>
        <div class="collapsedBlockBody"><pre id="speedLog" style="height:190px"></pre></div>
      </details>
    </div>

    <div class="miniBlock">
      <div class="toolbar">
        <div class="sectionTitle">Full System Diagnostic</div>
        <span id="fullDiagBadge" class="resultBadge idle">IDLE</span>
      </div>
      <button class="btnGreen" onclick="runDiagnosticCard('fulldiag','fullDiagBadge','fullDiagSummary','fullDiagLog')">Run Full System Diagnostic</button>
      <button onclick="copyDiagnosticReport()">Copy diagnostic report</button>
      <div class="subtle" id="fullDiagSummary">One-click report for SPD/HUB, PMIC, PWR_GOOD, saved references, compare status, and short stability test.</div>
      <details class="collapsedBlock">
        <summary>Show full log</summary>
        <div class="collapsedBlockBody"><pre id="fullDiagLog" style="height:240px"></pre></div>
      </details>
    </div>
  </div>

  <details class="collapsedBlock">
    <summary>Expert tools</summary>
    <div class="collapsedBlockBody">
      <div class="row">
        <button onclick="sendCmdValue('powerdiag')">powerdiag</button>
        <button onclick="sendCmdValue('timespd')">timespd</button>
        <button onclick="sendCmdValue('timereg')">timereg</button>
      </div>
      <div class="subtle">Expert timing tools print detailed serial-style logs. powerdiag repeatedly scans and samples power state but does not change PWR_EN, HSA, VIN_BULK, SPD, or saved references.</div>
    </div>
  </details>
</div>

<div class="card section">
  <div class="toolbar">
    <div class="muted">Terminal Log</div>
    <div class="row">
      <button onclick="sendCmdValue('help')">Help</button>
      <button onclick="sendCmdValue('status')">Status</button>
      <button onclick="clearLog()">Clear Log</button>
    </div>
  </div>

  <div class="row" style="margin:10px 0">
    <input id="cmd" type="text" placeholder="Type command e.g. read 0x50 0x0000 16" />
    <button onclick="sendCmd()">Run</button>
  </div>

  <pre id="term"></pre>
</div>

<div class="card section">
  <h3>Discovered Devices</h3>
  <div class="muted">Run Scan and this becomes the universal action area for each detected address.</div>
  <div id="devices" class="deviceList"></div>
</div>

<details class="advanced section" id="advancedSpd">
  <summary>
    <div class="advancedHeader">
      <div>
        <h3>Advanced SPD Editing</h3>
        <div class="muted">Timing/profile decode from the current SPD dump</div>
      </div>
      <div class="subtle" id="advancedSpdSummary">SPD addr: -- | table: waiting for dump</div>
    </div>
  </summary>
  <div class="advancedBody">
    <div class="danger-banner">
      <div class="danger-banner-title">EXTREME WARNING — EXPERIMENTAL SPD EDITING</div>
      <p>This feature directly edits DDR5 SPD profile bytes. Incorrect values can make the DIMM fail to boot, train incorrectly, corrupt timing profiles, or become difficult to recover.</p>
      <p>Manufacturer timings are programmed, validated, and protected for a reason. They are not arbitrary numbers. Changing them can create instability, data corruption, failed POST, or hardware stress.</p>
      <p>Use this only on sacrificial/test memory modules where failure is acceptable. Do not use this on production RAM, customer RAM, or memory containing data you care about.</p>
      <p><strong>Proceed entirely at your own risk.</strong></p>
    </div>

    <div class="miniBlock">
      <div class="sectionTitle">Lab SPD Write Mode</div>
      <div class="guardPanel" id="labGuardPanel">
        <div class="guardTitle" id="labGuardTitle">LAB SPD WRITE MODE: OFF</div>
        <div class="subtle" id="labGuardDetail">Required: boot-risk checkbox and LABMODE text confirmation.</div>
      </div>
      <div class="armControls">
        <label><input type="checkbox" id="labAck" onchange="updateLabArm()"> I understand this can make the DIMM fail to boot</label>
        <input id="labToken" type="text" placeholder="Type LABMODE" oninput="updateLabArm()" />
      </div>
    </div>

    <div class="miniBlock">
      <div class="statusStrip" id="spdEditStatusStrip">Current dump: none | Diagnostic ref: -- | Tweak checkpoint: -- | EXPO CRC: not checked | repair unavailable | Apply blocked</div>
      <div class="row" style="margin-top:10px">
        <button class="btnBlue" id="loadSpdBtn" onclick="loadSpdEditor()">Decode current SPD dump</button>
        <button onclick="previewSpdEdits()">Preview SPD edits</button>
        <button class="btnRed" id="applyEditBtn" onclick="applySpdEdits()" disabled>Apply SPD edits to module</button>
        <button onclick="dumpSpdAndShowTiming(currentSpdEditorAddr())">Re-dump and verify</button>
        <button class="btnGreen" onclick="confirmRun('backupspd ' + currentSpdEditorAddr(),'Save current SPD image as the tweak checkpoint? This becomes the rollback target for profile editing.')">Save tweak checkpoint</button>
        <button onclick="sendCmdValue('backupinfo')">Checkpoint info</button>
        <button class="btnAmber" title="CLI equivalent: restorelast [addr] YES_RESTORE_SPD_BACKUP" onclick="promptRestoreBackup('restorebackup ' + currentSpdEditorAddr())">Restore tweak checkpoint</button>
      </div>
      <div class="applyWarning">Writes are experimental. Preview and tweak checkpoint are required. Readback verification only confirms bytes changed; it does not prove the DIMM will boot or run safely.</div>
      <div class="subtle">Tweak checkpoint: save the current SPD image as the rollback target for profile editing. This can be the original SPD or a known-good tweaked profile.</div>
      <div class="subtle">Restore tweak checkpoint: writes the saved checkpoint back to the module and verifies readback.</div>
      <div class="subtle" id="editGuardStatus">Lab write mode is off. Apply is disabled.</div>
      <input id="spdEditorAddr" type="text" value="auto" style="display:none" />
      <div class="subtle" id="spdEditorState">No SPD profile loaded.</div>
      <div id="spdEditFields"></div>
      <div id="spdTimingTables">
        <div class="subtle">Decode current SPD dump to show timing profile tables.</div>
      </div>
      <div id="spdMetadataDetails">
        <details class="collapsedBlock">
          <summary>Current SPD metadata</summary>
          <div class="collapsedBlockBody"><div class="subtle">Decode current SPD dump to show metadata.</div></div>
        </details>
      </div>
      <details class="collapsedBlock" id="spdPreviewDetails">
        <summary>Preview diff / write log</summary>
        <div class="collapsedBlockBody"><pre id="spdEditDiff" style="height:150px"></pre></div>
      </details>
    </div>

    <div style="display:none">
      <button class="btnAmber" id="applySpdBtn" onclick="applySpdTweak()" disabled>Apply tweak</button>
      <pre id="spdDiff"></pre>
      <div id="writeGuardStatus">Writes blocked: no verified editable timing fields and/or CRC repair missing.</div>
    </div>
  </div>
</details>

<script>
const term = document.getElementById('term');
const cmdI = document.getElementById('cmd');
const devicesDiv = document.getElementById('devices');

const dimmToggle = document.getElementById('dimmToggle');
const pwrEnToggle = document.getElementById('pwrEnToggle');
const dimmState = document.getElementById('dimmState');
const pwrEnState = document.getElementById('pwrEnState');
const dimmCycleBtn = document.getElementById('dimmCycleBtn');
const dimmControlNote = document.getElementById('dimmControlNote');
const pwrEnControlNote = document.getElementById('pwrEnControlNote');
const hsaReleaseBtn = document.getElementById('hsaReleaseBtn');
const hsaGroundBtn = document.getElementById('hsaGroundBtn');
const hsaState = document.getElementById('hsaState');
const hsaObserved = document.getElementById('hsaObserved');
const hsaControlNote = document.getElementById('hsaControlNote');
const spdTimingTables = document.getElementById('spdTimingTables');
const spdDiff = document.getElementById('spdDiff');
const spdEditorState = document.getElementById('spdEditorState');
const applySpdBtn = document.getElementById('applySpdBtn');
const advancedSpdSummary = document.getElementById('advancedSpdSummary');
const writeGuardStatus = document.getElementById('writeGuardStatus');
const loadSpdBtn = document.getElementById('loadSpdBtn');
const labAck = document.getElementById('labAck');
const labToken = document.getElementById('labToken');
const applyEditBtn = document.getElementById('applyEditBtn');
const editGuardStatus = document.getElementById('editGuardStatus');
const spdEditFields = document.getElementById('spdEditFields');
const spdEditDiff = document.getElementById('spdEditDiff');

let lastDiagnosticReport = '';

function applyThemeMode(mode){
  const m = mode || 'auto';
  if(m === 'auto') document.documentElement.removeAttribute('data-theme');
  else document.documentElement.setAttribute('data-theme', m);
  ['Light','Dark','Auto'].forEach((name)=>{
    const el = document.getElementById('theme' + name);
    if(el) el.classList.toggle('active', name.toLowerCase() === m);
  });
}

function setThemeMode(mode){
  localStorage.setItem('spdToolTheme', mode);
  applyThemeMode(mode);
}

applyThemeMode(localStorage.getItem('spdToolTheme') || 'auto');
const spdPreviewDetails = document.getElementById('spdPreviewDetails');
const labGuardPanel = document.getElementById('labGuardPanel');
const labGuardTitle = document.getElementById('labGuardTitle');
const labGuardDetail = document.getElementById('labGuardDetail');
const spdEditStatusStrip = document.getElementById('spdEditStatusStrip');

let uiState = {
  pwr_good:false,
  pwr_en_released:false,
  dimm_power_on:false,
  hsa_low:false,
  hw_profile:'unknown',
  hw_hsa:'unknown',
  hw_vinbulk:'unknown',
  hw_pwren:'unknown',
  hw_pgood:'unknown',
  hw_i2c:'unknown',
  hw_pullups:'unknown',
  good_valid:false,
  spd_backup_valid:false,
  pmic_ref_valid:false,
  last_dump_valid:false,
  current_spd_valid:false,
  current_spd_addr:'0x50',
  current_spd_len:0,
  current_spd_crc:'0x0',
  current_spd_generation:0,
  current_spd_source:'',
  xmp_ddr_rate:'',
  expo_crc_status:'not checked',
  expo_crc_repair:'unavailable',
  expo_crc_apply:'blocked',
  scan_count:0,
  scan_addrs:[],
  roles:[]
};
let hwSaving = false;
let hwDirty = false;
let lastSpdPreview = 'no preview loaded';
let spdPreviewGenerated = false;
let spdPreviewHasChanges = false;
let spdPreviewCrcRepair = false;
let preserveDevicesDuringSpdLoad = false;
let spdLoadInProgress = false;
let cachedScanAddrs = [];
let cachedRoles = [];
let spdEditPreviewReady = false;
let spdEditChangedBytes = 0;
let spdEditCrcRepairReady = false;
let spdEditPreviewApplyAllowed = true;
let spdEditExpoSpeed = null;
let spdEditLastFieldsJson = null;
let speedPmicTouched = false;

const profileColumns = ['Profile','Data Rate / tCK','VDD','VDDQ','VPP','tAA / CL','tRCD','tRP','tRAS','tRC','tWR','tRFC1','tRFC2','tRFCsb','CAS/profile mask'];
const secondaryColumns = ['Profile','tWR','tRFC1','tRFC2','tRFCsb'];
const metadataColumns = ['Profile','Kind','Decoded value','Source','Raw byte(s)','Editable','Validation'];
const friendlyAliases = [
  {id:'friendly_expo_data_rate', label:'Data rate / tCK', rawFieldId:'expo_tck', confidence:'verified EXPO tCK / crc-covered', note:'Input is DDR MT/s, for example 5600. Raw bytes store derived tCK in picoseconds.'},
  {id:'friendly_expo_vdd', label:'VDD', rawFieldId:'expo_vdd', confidence:'verified EXPO voltage / crc-covered', note:'Input is volts. Verified encodings only: 1.10, 1.25, 1.35, 1.40.'},
  {id:'friendly_expo_vddq', label:'VDDQ', rawFieldId:'expo_vddq', confidence:'verified EXPO voltage / crc-covered', note:'Input is volts. Verified encodings only: 1.10, 1.25, 1.35, 1.40.'},
  {id:'friendly_expo_vpp', label:'VPP', readOnly:true, source:'unmapped', note:'read-only: VPP backing offset not confirmed.'},
  {id:'friendly_cl', label:'tAA / CL', rawFieldId:'expo_taa', confidence:'verified EXPO timing / crc-covered', note:'Input is cycles; raw byte uses 256 ps units.'},
  {id:'friendly_trcd', label:'tRCD', rawFieldId:'expo_trcd', confidence:'verified EXPO timing / crc-covered', note:'Input is cycles; raw byte uses 256 ps units.'},
  {id:'friendly_trp', label:'tRP', rawFieldId:'expo_trp', confidence:'verified EXPO timing / crc-covered', note:'Input is cycles; raw byte uses 256 ps units.'},
  {id:'friendly_tras', label:'tRAS', rawFieldId:'expo_tras', confidence:'verified EXPO timing / crc-covered', note:'Input is cycles; raw byte uses 256 ps units.'},
  {id:'friendly_trc', label:'tRC', rawFieldId:'expo_trc', confidence:'verified EXPO timing / crc-covered', note:'Input is cycles; raw byte uses 256 ps units.'},
  {id:'friendly_twr', label:'tWR', rawFieldId:'expo_twr', confidence:'verified EXPO timing / crc-covered', note:'Input is cycles; raw word stores ps.'},
  {id:'friendly_trfc1', label:'tRFC1', rawFieldId:'expo_trfc1', confidence:'verified EXPO timing / crc-covered', note:'Input is cycles; raw word stores ns.'},
  {id:'friendly_trfc2', label:'tRFC2', rawFieldId:'expo_trfc2', confidence:'verified EXPO timing / crc-covered', note:'Input is cycles; raw word stores ns.'},
  {id:'friendly_trfcsb', label:'tRFCsb', rawFieldId:'expo_trfcsb', confidence:'verified EXPO timing / crc-covered', note:'Input is cycles; raw byte stores ns.'}
];
const xmpFriendlyAliases = [
  {id:'friendly_xmp_data_rate', label:'Data rate / tCK', rawFieldId:'xmp_tck', confidence:'verified XMP tCK / crc-covered', note:'Input is DDR MT/s, for example 5600. Raw bytes store derived tCK in picoseconds.'},
  {id:'friendly_xmp_vdd', label:'VDD', rawFieldId:'xmp_vdd', confidence:'verified XMP voltage / crc-covered', note:'Input is volts. Verified encodings only: 1.10, 1.25, 1.35, 1.40.'},
  {id:'friendly_xmp_vddq', label:'VDDQ', rawFieldId:'xmp_vddq', confidence:'verified XMP voltage / crc-covered', note:'Input is volts. Verified encodings only: 1.10, 1.25, 1.35, 1.40.'},
  {id:'friendly_xmp_vpp', label:'VPP', readOnly:true, source:'unmapped', note:'read-only: VPP backing offset not confirmed.'},
  {id:'friendly_xmp_taa', label:'tAA / CL', rawFieldId:'xmp_taa', confidence:'verified XMP timing / crc-covered', note:'Input is cycles; raw byte uses 256 ps units.'},
  {id:'friendly_xmp_trcd', label:'tRCD', rawFieldId:'xmp_trcd', confidence:'verified XMP timing / crc-covered', note:'Input is cycles; raw byte uses 256 ps units.'},
  {id:'friendly_xmp_trp', label:'tRP', rawFieldId:'xmp_trp', confidence:'verified XMP timing / crc-covered', note:'Input is cycles; raw byte uses 256 ps units.'},
  {id:'friendly_xmp_tras', label:'tRAS', rawFieldId:'xmp_tras', confidence:'verified XMP timing / crc-covered', note:'Input is cycles; raw byte uses 256 ps units.'},
  {id:'friendly_xmp_trc', label:'tRC', rawFieldId:'xmp_trc', confidence:'verified XMP timing / crc-covered', note:'Input is cycles; raw byte uses 256 ps units.'},
  {id:'friendly_xmp_twr', label:'tWR', rawFieldId:'xmp_twr', confidence:'verified XMP timing / crc-covered', note:'Input is cycles; raw word stores ps.'},
  {id:'friendly_xmp_trfc1', label:'tRFC1', rawFieldId:'xmp_trfc1', confidence:'verified XMP timing / crc-covered', note:'Input is cycles; raw word stores ns.'},
  {id:'friendly_xmp_trfc2', label:'tRFC2', rawFieldId:'xmp_trfc2', confidence:'verified XMP timing / crc-covered', note:'Input is cycles; raw word stores ns.'},
  {id:'friendly_xmp_trfcsb', label:'tRFCsb', rawFieldId:'xmp_trfcsb', confidence:'verified XMP timing / crc-covered', note:'Input is cycles; raw byte stores ns.'}
];
let spdEditFieldMap = {};
let spdEditProfile = 'expo';

function hex2(v){
  return '0x' + Number(v).toString(16).toUpperCase().padStart(2,'0');
}

function setBadge(el, onText, offText, on){
  el.textContent = on ? onText : offText;
  el.className = 'stateBadge ' + (on ? 'stateOn' : 'stateOff');
}

function setPill(id, text, mode='neutral'){
  const el = document.getElementById(id);
  el.textContent = text;
  el.className = 'pill ' + mode;
}

function firstObservedSpdHub(j){
  const roles = j.roles || [];
  for(const r of roles){
    if(r.role === 'SPD_HUB') return hex2(r.addr);
  }
  const addrs = j.scan_addrs || [];
  for(const a of addrs){
    const n = Number(a);
    if(n >= 0x50 && n <= 0x57) return hex2(n);
  }
  return null;
}

function mergeStickyScanState(j){
  const addrs = j.scan_addrs || [];
  const roles = j.roles || [];
  if(addrs.length > 0){
    cachedScanAddrs = addrs.slice();
    cachedRoles = roles.slice();
    return j;
  }
  if(cachedScanAddrs.length > 0){
    j.scan_addrs = cachedScanAddrs.slice();
    j.roles = cachedRoles.slice();
    j.scan_count = cachedScanAddrs.length;
  }
  return j;
}

function currentSpdEditorAddr(){
  const el = document.getElementById('spdEditorAddr');
  const v = el ? el.value.trim() : '';
  if(v && v.toLowerCase() !== 'auto' && v.toLowerCase() !== 'current' && v.toLowerCase() !== 'observed') return v;
  return firstObservedSpdHub(uiState) || 'auto';
}

function hsaConfigLabel(mode){
  if(mode === 'manual_ground') return 'manual direct-ground strap';
  if(mode === 'manual_resistor_strap') return 'manual resistor-to-ground strap';
  if(mode === 'manual_floating_high') return 'manual floating/high diagnostic strap';
  if(mode === 'gpio_switchable') return 'ESP32/GPIO-controlled HSA';
  return 'unknown HSA hardware config';
}

function updatePills(j){
  if(j.hw_pgood === 'gpio_read'){
    const sidebandOk = !!(j.scan_ok || j.read_ok || j.dump_ok);
    const mode = j.pwr_good ? 'ok' : (sidebandOk ? 'neutral' : 'bad');
    const suffix = j.pwr_good ? 'READY' : (sidebandOk ? 'LOW (check PWR_GOOD wiring/config)' : 'LOW');
    setPill('pillPwrGood', 'PWR_GOOD: ' + suffix, mode);
  }else{
    setPill('pillPwrGood', 'PWR_GOOD: not authoritative', 'neutral');
  }

  if(j.hw_pwren === 'gpio_controlled'){
    setPill('pillPwrEn', 'PWR_EN pin: ' + (j.pwr_en_released ? 'ON' : 'OFF'), j.pwr_en_released ? 'ok' : 'bad');
  }else if(j.hw_pwren === 'pullup_only'){
    setPill('pillPwrEn', 'PWR_EN pin: pulled high externally', 'ok');
  }else{
    setPill('pillPwrEn', 'PWR_EN pin: not authoritative', 'neutral');
  }

  if(j.hw_vinbulk === 'gpio_switchable'){
    setPill('pillDimm', 'VIN_BULK: ' + (j.dimm_power_on ? 'ON' : 'OFF'), j.dimm_power_on ? 'ok' : 'bad');
  }else if(j.hw_vinbulk === 'external_locked_on'){
    setPill('pillDimm', 'VIN_BULK: external / locked ON', 'ok');
  }else{
    setPill('pillDimm', 'VIN_BULK: external/configured', 'neutral');
  }
  setPill('pillHsa', 'HSA config: ' + hsaConfigLabel(j.hw_hsa), j.hw_hsa === 'gpio_switchable' ? 'ok' : 'neutral');
  setPill('pillHw', 'Hardware config: ' + (j.hw_profile || 'unknown'), j.hw_profile && j.hw_profile !== 'unknown' ? 'ok' : 'neutral');
  setPill('pillGood', 'Diagnostic ref: ' + (j.good_valid ? `PRESENT crc32=${j.good_crc || '0x0'}` : 'MISSING'), j.good_valid ? 'ok' : 'neutral');
  setPill('pillSpdBak', 'Tweak checkpoint: ' + (j.spd_backup_valid ? `PRESENT addr=0x${Number(j.spd_backup_addr || 0).toString(16)} crc32=${j.spd_backup_crc || '0x0'}` : 'MISSING'), j.spd_backup_valid ? 'ok' : 'neutral');
  setPill('pillPmicRef', 'PMIC reference: ' + (j.pmic_ref_valid ? 'PRESENT' : 'MISSING'), j.pmic_ref_valid ? 'ok' : 'neutral');
  setPill('pillScan', 'SCAN: ' + (j.scan_count || 0) + ' device(s)', (j.scan_count || 0) > 0 ? 'ok' : 'neutral');
  updateAdvancedSpdSummary();
}

async function refreshDumpState(){
  try{
    const r = await fetch('/api/spd/dumpstate');
    const j = await r.json();
    if(!j.ok) return;
    uiState.current_spd_valid = !!j.valid;
    if(j.valid){
      uiState.current_spd_addr = j.addr || uiState.current_spd_addr;
      uiState.current_spd_len = j.len || 0;
      uiState.current_spd_crc = j.crc32 || '0x0';
      uiState.current_spd_generation = j.generation || 0;
      uiState.current_spd_source = j.source || '';
    }
    updateAdvancedSpdSummary();
  }catch(e){
    console.error('dumpstate fetch failed', e);
  }
}

function setSelectByText(id, value){
  const el = document.getElementById(id);
  if(!el) return;
  for(const opt of el.options){
    if(opt.textContent === value || opt.value === value){
      el.value = opt.value;
      return;
    }
  }
}

function updateHwPanel(j){
  if(hwSaving || hwDirty) return;
  setSelectByText('hwPreset', j.hw_profile || 'unknown');
  setSelectByText('hwHsa', j.hw_hsa || 'unknown');
  setSelectByText('hwVin', j.hw_vinbulk || 'unknown');
  setSelectByText('hwPwrEn', j.hw_pwren || 'unknown');
  setSelectByText('hwPGood', j.hw_pgood || 'unknown');
  setSelectByText('hwI2c', j.hw_i2c || 'unknown');
  setSelectByText('hwPullups', j.hw_pullups || 'unknown');

  const warn = j.hw_gpio_not_authoritative ? ' GPIO runtime state is not authoritative for at least one configured control.' : '';
  const pull = j.hw_pullups === 'esp32_internal_only' ? ' External 10k SDA/SCL pull-ups may be needed if reads are flaky.' : '';
  document.getElementById('hwSummary').textContent =
    `profile=${j.hw_profile || 'unknown'} | HSA=${j.hw_hsa || 'unknown'} | tested-observed address=${j.hw_expected_addr || 'runtime-dependent'} | VIN_BULK=${j.hw_vinbulk || 'unknown'} | I2C=${j.hw_i2c || 'unknown'}.${warn}${pull}`;
}

function updateToggleVisuals(j){
  const vinLocked = j.hw_vinbulk === 'external_locked_on';
  const pwrPullup = j.hw_pwren === 'pullup_only';
  const hsaGpio = j.hw_hsa === 'gpio_switchable';

  dimmToggle.disabled = vinLocked;
  dimmCycleBtn.disabled = vinLocked;
  pwrEnToggle.disabled = pwrPullup;
  hsaReleaseBtn.disabled = !hsaGpio;
  hsaGroundBtn.disabled = !hsaGpio;

  dimmToggle.checked = vinLocked ? true : !!j.dimm_power_on;
  pwrEnToggle.checked = pwrPullup ? true : !!j.pwr_en_released;

  if(vinLocked){
    dimmState.textContent = 'external / locked ON';
    dimmState.className = 'stateBadge stateOn';
    dimmControlNote.textContent = 'GPIO32 is not authoritative in this harness config.';
  }else{
    setBadge(dimmState, 'ON', 'OFF', !!j.dimm_power_on);
    dimmControlNote.textContent = '';
  }

  if(pwrPullup){
    pwrEnState.textContent = 'pulled high externally / GPIO33 high-Z';
    pwrEnState.className = 'stateBadge stateOn';
    pwrEnControlNote.textContent = 'GPIO33 control disabled by hardware config.';
  }else{
    setBadge(pwrEnState, 'ON', 'OFF', !!j.pwr_en_released);
    pwrEnControlNote.textContent = '';
  }

  hsaState.textContent = hsaConfigLabel(j.hw_hsa);
  hsaObserved.textContent = 'Observed SPD/HUB address: ' + (firstObservedSpdHub(j) || 'not observed yet');
  if(hsaGpio){
    hsaControlNote.textContent = 'GPIO27 control enabled by hardware config. HSA is sampled at cold power-up; cold-cycle VIN_BULK after changes. Observed address is evidence only.';
  }else if(j.hw_hsa === 'unknown'){
    hsaControlNote.textContent = 'GPIO27 is not authoritative until hardware config declares ESP32/GPIO-controlled HSA. Observed address is evidence only.';
  }else{
    hsaControlNote.textContent = 'GPIO27 control disabled by hardware config. GPIO27 is not authoritative; observed address is shown separately and does not prove HSA mode.';
  }

  updateSpdApplyGate();
}

function buildDeviceButtons(addrs){
  if((!addrs || addrs.length === 0) && preserveDevicesDuringSpdLoad && devicesDiv.children.length > 0){
    return;
  }

  devicesDiv.innerHTML = '';

  if(!addrs || addrs.length === 0){
    devicesDiv.innerHTML = '<div class="muted">(no scan results yet)</div>';
    return;
  }

  const roleByAddr = {};
  (uiState.roles || []).forEach((r)=>{ roleByAddr[Number(r.addr)] = r; });

  addrs.forEach((a)=>{
    const h = hex2(a);
    const r = roleByAddr[Number(a)] || inferRole(a);
    const wrap = document.createElement('div');
    wrap.className = 'deviceCard';
    const actions = buildRoleActions(h, r.role);

    wrap.innerHTML = `
      <div class="deviceHeader">
        <div>
          <div class="deviceAddr">${h} - ${r.label}</div>
          <div class="muted">${r.confidence ? 'confidence=' + r.confidence + '  ' : ''}${r.reason || ''}</div>
        </div>
      </div>

      <div class="deviceActions">
        ${actions}
      </div>
    `;

    devicesDiv.appendChild(wrap);
  });
}

function inferRole(a){
  const n = Number(a);
  if(n === 0x7E) return {role:'RESERVED', label:'Reserved/Unknown', confidence:'medium', reason:'reserved/broadcast style address'};
  if(n >= 0x50 && n <= 0x57) return {role:'SPD_HUB', label:'SPD/HUB', confidence:'low', reason:'address-range hint; run Auto-detect Roles'};
  if(n >= 0x48 && n <= 0x4F) return {role:'PMIC', label:'PMIC', confidence:'low', reason:'address-range hint; run Auto-detect Roles'};
  if((n >= 0x10 && n <= 0x17) || (n >= 0x30 && n <= 0x37) || n === 0x0C) {
    return {role:'TEMP_SENSOR_CANDIDATE', label:'Temp candidate', confidence:'low', reason:'address-range hint; run Auto-detect Roles'};
  }
  return {role:'UNKNOWN', label:'Unknown', confidence:'low', reason:'run Auto-detect Roles'};
}

function buildRoleActions(h, role){
  const reg = `<button onclick="sendCmdValue('reg ${h} 0x00 16')">Reg Read</button>`;
  if(role === 'SPD_HUB'){
    return `
      ${reg}
      <button onclick="sendCmdValue('read ${h} 0x0000 16')">SPD Read</button>
      <button onclick="dumpSpdAndShowTiming('${h}')">Dump 1024</button>
      <button class="btnGreen" onclick="confirmRun('capturegood ${h}','Save current SPD as the diagnostic reference? This overwrites the stored diagnostic SPD reference in ESP32 flash.')">Save diagnostic reference</button>
      <button onclick="sendCmdValue('compare ${h}')">Compare to diagnostic reference</button>
      <button class="btnRed" ${uiState.good_valid ? '' : 'disabled'} title="Writes the saved Diagnostic SPD Reference back to this SPD/HUB address. This does not use the tweak checkpoint." onclick="writeDiagnosticReferenceToSpd('${h}')">Write diagnostic reference to SPD</button>
    `;
  }
  if(role === 'PMIC'){
    return `
      ${reg}
      <button onclick="sendCmdValue('pmicid ${h}')">PMIC ID</button>
      <button onclick="sendCmdValue('pmicdumpat ${h} 0x00 0x80')">PMIC Dump</button>
      <button class="btnGreen" onclick="confirmRun('capturepmic ${h} 0x00 0x80','Save current PMIC registers as PMIC reference? This overwrites the stored PMIC reference in ESP32 flash.')">Save PMIC reference</button>
      <button onclick="sendCmdValue('comparepmic ${h}')">Compare PMIC reference</button>
    `;
  }
  if(role === 'RESERVED' || role === 'I3C_RESERVED_OR_BROADCAST'){
    return '';
  }
  return reg;
}

async function refreshStatus(){
  if(spdLoadInProgress) return;
  try{
    const r = await fetch('/api/status',{method:'POST'});
    const t = await r.text();

    try{
      const j = mergeStickyScanState(JSON.parse(t));
      const decodedSpdState = {
        base_ddr_rate: uiState.base_ddr_rate || '',
        expo_ddr_rate: uiState.expo_ddr_rate || '',
        xmp_ddr_rate: uiState.xmp_ddr_rate || '',
        expo_crc_status: uiState.expo_crc_status || 'not checked',
        expo_crc_repair: uiState.expo_crc_repair || 'unavailable',
        expo_crc_apply: uiState.expo_crc_apply || 'blocked'
      };
      uiState = j;
      Object.keys(decodedSpdState).forEach((k)=>{ if(uiState[k] == null || uiState[k] === '') uiState[k] = decodedSpdState[k]; });
      updatePills(j);
      updateToggleVisuals(j);
      updateHwPanel(j);
      buildDeviceButtons(j.scan_addrs || []);
      const speedPmic = document.getElementById('speedPmic');
      if(speedPmic && !speedPmicTouched){
        speedPmic.checked = (j.roles || []).some((r)=>r.role === 'PMIC');
      }
      refreshDumpState();
    }catch(e){
      console.error('status parse error', e, t);
    }
  }catch(e){
    console.error(e);
  }
}

async function refreshLog(){
  try{
    const r = await fetch('/api/log');
    const t = await r.text();
    term.textContent = t;
    term.scrollTop = term.scrollHeight;
  }catch(e){
    term.textContent = "ERR: " + e;
  }
}

async function clearLog(){
  term.textContent = '';
}

async function writeDiagnosticReferenceToSpd(addr){
  const phrase = 'WRITE DIAGNOSTIC REFERENCE';
  if(!uiState.good_valid){
    term.textContent += `\\nWrite diagnostic reference blocked: no Diagnostic SPD Reference is stored.\\n`;
    term.scrollTop = term.scrollHeight;
    return;
  }
  if(!(labAck.checked && labToken.value === 'LABMODE')){
    term.textContent += `\\nWrite diagnostic reference blocked: enable Lab SPD Write Mode, check the danger acknowledgement, and type LABMODE first.\\n`;
    term.scrollTop = term.scrollHeight;
    return;
  }
  const currentCrc = uiState.current_spd_crc || 'unavailable';
  const refCrc = uiState.good_crc || '0x0';
  const msg = `DANGEROUS SPD WRITE\\n\\nTarget SPD/HUB address: ${addr}\\nDiagnostic reference CRC: ${refCrc}\\nCurrent dump CRC: ${currentCrc}\\n\\nThis will overwrite the module SPD contents with the saved Diagnostic SPD Reference.\\nThis uses the diagnostic reference, not the Advanced SPD Editing tweak checkpoint.\\nThis can make the DIMM fail to boot.\\n\\nType exactly: ${phrase}`;
  const typed = prompt(msg, '');
  if(typed !== phrase){
    term.textContent += `\\nWrite diagnostic reference cancelled: confirmation phrase did not match.\\n`;
    term.scrollTop = term.scrollHeight;
    return;
  }
  try{
    const form = new URLSearchParams();
    form.set('addr', addr);
    form.set('labmode', labToken.value);
    form.set('laback', labAck.checked ? '1' : '0');
    form.set('confirm', typed);
    term.textContent += `\\n> Write diagnostic reference to SPD ${addr}\\nWriting...\\n`;
    term.scrollTop = term.scrollHeight;
    const r = await fetch('/api/spd/writegood',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:form.toString()
    });
    const j = await r.json();
    const first = j.first_mismatch ? ` first_mismatch=${j.first_mismatch}` : '';
    term.textContent += `${j.ok ? 'PASS' : 'FAIL'}: Diagnostic reference write/verify addr=${j.addr || addr} ref_crc=${j.reference_crc || refCrc} readback_crc=${j.readback_crc || 'unavailable'} mismatches=${j.mismatches || 0}${first}\\n`;
    if(j.error) term.textContent += `  ${j.error}\\n`;
    term.scrollTop = term.scrollHeight;
    await refreshStatus();
    await refreshLog();
  }catch(e){
    term.textContent += `\\nERR: ${e}\\n`;
    term.scrollTop = term.scrollHeight;
  }
}

async function run(line){
  try{
    const body = "line=" + encodeURIComponent(line);
    await fetch('/api/run',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body
    });
  }catch(e){
    term.textContent += "\\nERR: " + e + "\\n";
  }

  await refreshStatus();
  await refreshLog();
}

async function getLogText(){
  const r = await fetch('/api/log');
  return await r.text();
}

function setResultBadge(id, result){
  const el = document.getElementById(id);
  if(!el) return;
  const r = String(result || 'IDLE').toUpperCase();
  el.textContent = r;
  el.className = 'resultBadge ' + (r === 'PASS' ? 'pass' : r === 'WARN' ? 'warn' : r === 'FAIL' ? 'fail' : 'idle');
}

function parseDiagnosticResult(text){
  const m = String(text || '').match(/(?:QUICK HEALTH|SPEEDTEST|FULLDIAG) RESULT:\s*(PASS|WARN|FAIL)/);
  return m ? m[1] : 'WARN';
}

function summarizeDiagnostic(text){
  const lines = String(text || '').split('\n').map((s)=>s.trim()).filter(Boolean);
  const summary = lines.filter((s)=>/RESULT:|SPD\/HUB addr:|PMIC addr:|PWR_GOOD:|Diagnostic ref:|Tweak checkpoint:|SPD compare:|Speed\/stability:|completed=|time_us:|scan:|pmic_quick:|first_mismatch:/.test(s));
  return (summary.length ? summary : lines.slice(-8)).join(' | ');
}

function updateSpeedMetrics(text){
  const rows = [];
  const add = (k,v)=>rows.push(`<tr><td>${htmlEscape(k)}</td><td>${htmlEscape(v || '')}</td></tr>`);
  const pick = (re)=>{ const m = String(text || '').match(re); return m ? m[1].trim() : ''; };
  add('Result', parseDiagnosticResult(text));
  add('Completed / failures / mismatches', pick(/completed=([^\n]+)/));
  add('Read timing', pick(/time_us:\s*([^\n]+)/));
  add('First mismatch', pick(/first_mismatch:\s*([^\n]+)/) || 'none reported');
  add('CRC32', pick(/baseline_crc32=([^\n]+)/) || 'only shown for 1024-byte reads');
  add('Scan consistency', pick(/scan:\s*([^\n]+)/) || 'skipped');
  add('PMIC quick check', pick(/pmic_quick:\s*([^\n]+)/) || 'skipped');
  add('PWR_GOOD', pick(/PWR_GOOD low events=([^\n]+)/));
  const table = document.getElementById('speedMetrics');
  if(table) table.innerHTML = '<tbody>' + rows.join('') + '</tbody>';
}

async function runDiagnosticCard(cmd, badgeId, summaryId, logId){
  setResultBadge(badgeId, 'RUNNING');
  const before = await getLogText();
  try{
    await fetch('/api/run',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:'line=' + encodeURIComponent(cmd)
    });
    await refreshStatus();
    const after = await getLogText();
    const delta = after.startsWith(before) ? after.slice(before.length).trim() : after.trim();
    const result = parseDiagnosticResult(delta || after);
    setResultBadge(badgeId, result);
    const logEl = document.getElementById(logId);
    if(logEl) logEl.textContent = delta || after;
    const summaryEl = document.getElementById(summaryId);
    if(summaryEl) summaryEl.textContent = summarizeDiagnostic(delta || after);
    if(cmd === 'fulldiag') lastDiagnosticReport = delta || after;
    if(cmd === 'health') lastDiagnosticReport = delta || after;
    await refreshLog();
  }catch(e){
    setResultBadge(badgeId, 'FAIL');
    const logEl = document.getElementById(logId);
    if(logEl) logEl.textContent = 'ERR: ' + e;
  }
}

function speedAddrValue(){
  const v = (document.getElementById('speedAddr').value || '').trim();
  if(v && v.toLowerCase() !== 'auto') return v;
  return firstObservedSpdHub(uiState) || uiState.current_spd_addr || '0x50';
}

async function runSpeedDiagnostic(){
  const addr = speedAddrValue();
  const len = document.getElementById('speedLen').value || '32';
  const passes = document.getElementById('speedPasses').value || '20';
  const delayMs = document.getElementById('speedDelay').value || '20';
  const flags = [];
  if(!document.getElementById('speedScan').checked) flags.push('noscan');
  if(!document.getElementById('speedPmic').checked) flags.push('nopmic');
  const cmd = `speedtest ${addr} ${len} ${passes} ${delayMs}${flags.length ? ' ' + flags.join(' ') : ''}`;
  await runDiagnosticCard(cmd, 'speedBadge', 'speedText', 'speedLog');
  const logEl = document.getElementById('speedLog');
  updateSpeedMetrics(logEl ? logEl.textContent : '');
}

async function copyDiagnosticReport(){
  const report = lastDiagnosticReport || (document.getElementById('fullDiagLog') ? document.getElementById('fullDiagLog').textContent : '');
  if(!report) return;
  try{
    if(navigator.clipboard && window.isSecureContext){
      await navigator.clipboard.writeText(report);
    }else{
      const ta = document.createElement('textarea');
      ta.value = report;
      ta.style.position = 'fixed';
      ta.style.left = '-9999px';
      document.body.appendChild(ta);
      ta.focus();
      ta.select();
      document.execCommand('copy');
      document.body.removeChild(ta);
    }
    const el = document.getElementById('fullDiagSummary');
    if(el) el.textContent = 'Diagnostic report copied.';
  }catch(e){
    const el = document.getElementById('fullDiagSummary');
    if(el) el.textContent = 'Copy failed: ' + e;
  }
}

async function runTerminalOnly(line){
  cmdI.value = line;
  try{
    const body = "line=" + encodeURIComponent(line);
    await fetch('/api/run',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body
    });
  }catch(e){
    term.textContent += "\\nERR: " + e + "\\n";
  }
  await refreshLog();
}

function sendCmd(){
  const v = cmdI.value.trim();
  if(!v) return;
  run(v);
}

function sendCmdValue(v){
  cmdI.value = v;
  run(v);
}

function confirmRun(cmd, msg){
  if(!confirm(msg)) return;
  sendCmdValue(cmd);
}

function saveHwPreset(){
  const p = document.getElementById('hwPreset').value;
  hwDirty = false;
  sendCmdValue('hwconfig preset ' + p);
}

async function saveHwManual(){
  hwSaving = true;
  hwDirty = false;
  const form = new URLSearchParams();
  form.set('profile', document.getElementById('hwPreset').value);
  form.set('hsa', document.getElementById('hwHsa').value);
  form.set('vinbulk', document.getElementById('hwVin').value);
  form.set('pwren', document.getElementById('hwPwrEn').value);
  form.set('pgood', document.getElementById('hwPGood').value);
  form.set('i2c', document.getElementById('hwI2c').value);
  form.set('pullups', document.getElementById('hwPullups').value);

  try{
    const r = await fetch('/api/hwconfig/save',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:form.toString()
    });
    const t = await r.text();
    term.textContent += "\\n> hwconfig save\\n" + t + "\\n";
  }catch(e){
    term.textContent += "\\nERR: " + e + "\\n";
  }
  hwSaving = false;
  await refreshStatus();
  await refreshLog();
}

function htmlEscape(v){
  return String(v == null ? '' : v)
    .replace(/&/g,'&amp;')
    .replace(/</g,'&lt;')
    .replace(/>/g,'&gt;')
    .replace(/"/g,'&quot;');
}

function unsupportedCell(label){
  return {
    decoded:'not decoded yet',
    source:label + ': source offset not mapped',
    raw:'source offset not mapped',
    editable:false,
    validation:'source offset not mapped'
  };
}

function renderTrackCell(cell){
  const c = cell || unsupportedCell('field');
  return `<td>
    <div class="cellValue">value: ${htmlEscape(c.decoded)}</div>
    <div class="cellMeta">source: ${htmlEscape(c.source)}</div>
    <div class="cellMeta">raw: ${htmlEscape(c.raw || 'source offset not mapped')}</div>
    <div class="cellMeta">editable: ${c.editable ? 'yes' : 'no'} | status: ${htmlEscape(c.validation)}</div>
  </td>`;
}

function renderMatrix(title, columns, profiles){
  const head = columns.map((c)=>`<th>${htmlEscape(c)}</th>`).join('');
  const rows = profiles.map((profile)=>{
    const cells = columns.map((col)=>{
      if(col === 'Profile') return `<td><strong>${htmlEscape(profile.name)}</strong></td>`;
      return renderTrackCell(profile.fields[col] || unsupportedCell(`${profile.name} ${col}`));
    }).join('');
    return `<tr>${cells}</tr>`;
  }).join('');

  return `<div class="sectionTitle">${htmlEscape(title)}</div>
    <div class="tableScroll"><table class="timingTable"><thead><tr>${head}</tr></thead><tbody>${rows}</tbody></table></div>`;
}

function renderMetadataTable(fields){
  const rows = fields.map((f)=>`<tr>
    <td>${htmlEscape(f.profile)}</td>
    <td>${htmlEscape(f.kind)}</td>
    <td>${htmlEscape(f.decoded)}</td>
    <td>${htmlEscape(f.source)}</td>
    <td>${htmlEscape(f.raw || 'source offset not mapped')}</td>
    <td>${f.editable ? 'yes' : 'no'}</td>
    <td>${htmlEscape(f.validation)}</td>
  </tr>`).join('');
  return `<div class="sectionTitle">Voltage/profile metadata table</div>
    <div class="tableScroll"><table class="timingTable">
      <thead><tr>${metadataColumns.map((c)=>`<th>${c}</th>`).join('')}</tr></thead>
      <tbody>${rows}</tbody>
    </table></div>`;
}

function fieldByKey(fields, key){
  return (fields || []).find((f)=>f.key === key);
}

function cellFromField(f){
  if(!f) return null;
  return {
    decoded:f.value || 'not decoded yet',
    source:f.source || f.note || 'source offset not mapped',
    raw:f.raw || 'source offset not mapped',
    editable:!!f.editable,
    validation:f.status || (f.editable ? 'editable guarded' : 'decoded / read-only')
  };
}

function metadataFromDecoded(j){
  const fields = j.fields || [];
  const item = (key, profile, kind)=>{
    const f = fieldByKey(fields, key);
    return {
      profile,
      kind,
      decoded:f ? (f.value || '') : 'not decoded yet',
      source:f ? (f.source || f.note || 'source offset not mapped') : 'source offset not mapped',
      raw:f ? (f.raw || 'source offset not mapped') : 'source offset not mapped',
      editable:!!(f && f.editable),
      validation:f ? (f.status || (f.editable ? 'editable guarded' : 'decoded / read-only')) : 'source offset not mapped'
    };
  };
  return [
    item('header', 'Base/JEDEC', 'Header bytes'),
    item('generation', 'Base/JEDEC', 'DDR generation'),
    item('module_type', 'Base/JEDEC', 'Module form factor'),
    item('part_number', 'Base/JEDEC', 'Module part number'),
    item('manufacturer', 'Base/JEDEC', 'Manufacturer bytes'),
    item('base_tck', 'Base/JEDEC', 'Base tCK'),
    item('base_speed', 'Base/JEDEC', 'Base data rate'),
    item('expo_tck', 'EXPO', 'EXPO tCK'),
    item('expo_speed', 'EXPO', 'EXPO data rate'),
    item('voltage', 'All profiles', 'Voltage fields'),
    item('xmp', 'XMP', 'Profile presence/offsets'),
    item('expo', 'EXPO', 'Profile presence/offsets'),
    item('crc', 'Touched region', 'CRC/checksum repair')
  ];
}

function makeTimingProfile(name, columns){
  const fields = {};
  columns.forEach((col)=>{
    if(col !== 'Profile') fields[col] = unsupportedCell(`${name} ${col}`);
  });
  return {name, fields};
}

function applyKnownBaseCells(profile, fields){
  const map = {
    'Data Rate / tCK':'base_tck',
    'tAA / CL':'base_taa',
    'tRCD':'base_trcd',
    'tRP':'base_trp',
    'tRAS':'base_tras',
    'tRC':'base_trc',
    'tWR':'base_twr',
    'tRFC1':'base_trfc1',
    'tRFC2':'base_trfc2',
    'tRFCsb':'base_trfcsb'
  };
  Object.keys(map).forEach((col)=>{
    profile.fields[col] = cellFromField(fieldByKey(fields, map[col])) || unsupportedCell(`${profile.name} ${col}`);
  });
  profile.fields['VDD'] = unsupportedCell(`${profile.name} VDD`);
  profile.fields['VDDQ'] = unsupportedCell(`${profile.name} VDDQ`);
  profile.fields['VPP'] = unsupportedCell(`${profile.name} VPP`);
  profile.fields['CAS/profile mask'] = unsupportedCell(`${profile.name} CAS/profile mask`);
}

function applyProfileCells(profile, fields, prefix){
  const map = {
    'Data Rate / tCK':prefix + '_tck',
    'VDD':prefix + '_vdd',
    'VDDQ':prefix + '_vddq',
    'tAA / CL':prefix + '_taa',
    'tRCD':prefix + '_trcd',
    'tRP':prefix + '_trp',
    'tRAS':prefix + '_tras',
    'tRC':prefix + '_trc',
    'tWR':prefix + '_twr',
    'tRFC1':prefix + '_trfc1',
    'tRFC2':prefix + '_trfc2',
    'tRFCsb':prefix + '_trfcsb',
    'CAS/profile mask':prefix + '_cas_mask'
  };
  Object.keys(map).forEach((col)=>{
    profile.fields[col] = cellFromField(fieldByKey(fields, map[col])) || unsupportedCell(`${profile.name} ${col}`);
  });
}

function makeProfileEvidence(name, f){
  const p = makeTimingProfile(name, profileColumns);
  if(f){
    p.fields['Data Rate / tCK'] = cellFromField(f);
  }
  return p;
}

function renderTinySpdMetadata(j){
  const rows = (j.metadata || []).map((m)=>`<tr>
    <td>${htmlEscape(m.name)}</td>
    <td>${htmlEscape(m.value)}</td>
    <td>${htmlEscape(m.raw)}</td>
  </tr>`).join('');
  spdEditorState.textContent = `Decoded current SPD dump at ${j.dump.addr}; crc32 ${j.dump.crc32}; source ${j.dump.source}.`;
  spdTimingTables.innerHTML = `<details class="collapsedBlock">
    <summary>Current SPD dump metadata</summary>
    <div class="collapsedBlockBody">
      <div class="tableScroll"><table class="timingTable compact-table">
        <thead><tr><th>Field</th><th>Value</th><th>Raw source</th></tr></thead>
        <tbody>${rows || '<tr><td colspan="3">No metadata decoded.</td></tr>'}</tbody>
      </table></div>
    </div>
  </details>`;
  uiState.current_spd_valid = true;
  uiState.current_spd_addr = j.dump.addr || uiState.current_spd_addr;
  uiState.current_spd_len = j.dump.len || 0;
  uiState.current_spd_crc = j.dump.crc32 || uiState.current_spd_crc;
  uiState.current_spd_source = j.dump.source || uiState.current_spd_source;
  uiState.current_spd_generation = j.dump.generation || uiState.current_spd_generation;
  uiState.base_ddr_rate = j.dump.base_speed || uiState.base_ddr_rate;
  uiState.expo_ddr_rate = j.dump.expo_speed || uiState.expo_ddr_rate;
  updateAdvancedSpdSummary();
  loadSpdEditFields();
}

function renderCompactSpdSummary(j){
  const base = j.base || {};
  const expo = j.expo || {};
  const xmp = j.xmp || {};
  const fmtRate = (o, fallback)=>{
    if(o && Number(o.mtps) > 0) return `DDR5-${o.mtps} / MCLK ~${o.mclk || 0} MHz / tCK ${o.tck_ps || 0} ps`;
    return fallback;
  };
  const rows = [
    ['Header bytes', j.header || 'decode unavailable'],
    ['DDR generation', j.generation || 'decode unavailable'],
    ['Module form factor', j.module_type || 'decode unavailable'],
    ['Base tCK/data rate', fmtRate(base, 'decode unavailable')],
    ['EXPO', expo.detected ? fmtRate(expo, expo.status || 'EXPO detected, decode incomplete') : (expo.status || 'EXPO not detected')],
    ['XMP', xmp.detected ? fmtRate(xmp, xmp.status || 'XMP detected, decode incomplete') : (xmp.status || 'XMP not detected/unmapped')],
    ['Free heap', `${j.free_heap || '--'} bytes; min ${j.min_free_heap || '--'}; max alloc ${j.max_alloc_heap || '--'}`]
  ].map((r)=>`<tr><td>${htmlEscape(r[0])}</td><td>${htmlEscape(r[1])}</td></tr>`).join('');
  spdEditorState.textContent = `Compact SPD decode at ${j.addr || currentSpdEditorAddr()}. Loading staged timing sections.`;
  spdTimingTables.innerHTML = `<details class="collapsedBlock" id="spdSummarySection">
      <summary>Compact SPD Decode Summary</summary>
      <div class="collapsedBlockBody">
        <div class="tableScroll"><table class="timingTable compact-table">
          <thead><tr><th>Field</th><th>Value</th></tr></thead>
          <tbody>${rows}</tbody>
        </table></div>
      </div>
    </details>
    <details class="collapsedBlock" id="spdProfileReference">
      <summary>Decoded timing profiles / read-only reference</summary>
      <div class="collapsedBlockBody">
        <div id="spdBaseSection"><div class="subtle">Base decode pending.</div></div>
        <div id="spdExpoSection"><div class="subtle">EXPO decode pending.</div></div>
        <div id="spdXmpSection"><div class="subtle">XMP decode pending.</div></div>
      </div>
    </details>`;
  const metadataDetails = document.getElementById('spdMetadataDetails');
  if(metadataDetails){
    metadataDetails.innerHTML = `<details class="collapsedBlock">
      <summary>Current SPD metadata</summary>
      <div class="collapsedBlockBody"><div class="subtle">crc32 ${htmlEscape(j.dump_crc || '0x0')} | source ${htmlEscape(j.dump_source || 'unknown')} | generation ${htmlEscape(j.dump_generation || 0)}</div></div>
    </details>`;
  }
  spdEditFields.innerHTML = '<div class="subtle">Loading verified EXPO timing edit controls...</div>';
  spdEditPreviewReady = false;
  spdEditChangedBytes = 0;
  spdEditCrcRepairReady = false;
  spdEditPreviewApplyAllowed = true;
  uiState.current_spd_valid = true;
  uiState.current_spd_addr = j.addr || uiState.current_spd_addr;
  uiState.current_spd_crc = j.dump_crc || uiState.current_spd_crc;
  uiState.current_spd_source = j.dump_source || uiState.current_spd_source;
  uiState.current_spd_generation = j.dump_generation || uiState.current_spd_generation;
  uiState.base_ddr_rate = Number(base.mtps) > 0 ? `DDR5-${base.mtps}` : '';
  uiState.expo_ddr_rate = expo.detected && Number(expo.mtps) > 0 ? `DDR5-${expo.mtps}` : '';
  uiState.xmp_ddr_rate = xmp.detected && Number(xmp.mtps) > 0 ? `DDR5-${xmp.mtps}` : '';
  updateLabArm();
  updateAdvancedSpdSummary();
}

function renderTimingDecodeSection(targetId, j){
  const target = document.getElementById(targetId);
  if(!target) return;
  if(!j || !j.ok){
    target.innerHTML = `<div class="sectionTitle">${htmlEscape((j && j.title) || targetId)}</div><div class="subtle">${htmlEscape((j && (j.error || j.message)) || 'section decode failed')}</div>`;
    return;
  }
  const rows = (j.rows || []).map((r)=>`<tr class="${r.editable ? '' : 'readOnlyRow'}">
    <td>${htmlEscape(r.label)}</td>
    <td>${htmlEscape(r.current)}</td>
    <td>${htmlEscape(r.source)}</td>
    <td>${r.editable ? 'yes' : 'no'}</td>
    <td>${htmlEscape(r.status)}</td>
  </tr>`).join('');
  target.innerHTML = `<div class="sectionTitle">${htmlEscape(j.title || 'SPD timing section')}</div>
    <div class="tableScroll"><table class="timingTable compact-table">
      <thead><tr><th>Field</th><th>Current</th><th>Source</th><th>Editable</th><th>Status</th></tr></thead>
      <tbody>${rows || '<tr><td colspan="5">No rows decoded.</td></tr>'}</tbody>
    </table></div>`;
  if(targetId === 'spdExpoSection'){
    const crc = (j.rows || []).find((r)=>r.label === 'EXPO CRC');
    if(crc){
      uiState.expo_crc_status = String(crc.current || '').startsWith('PASS') ? 'PASS' : (String(crc.current || '').startsWith('FAIL') ? 'FAIL' : 'not checked');
      uiState.expo_crc_repair = 'unavailable';
      uiState.expo_crc_apply = 'blocked';
      updateSpdEditStatusStrip();
    }
  }
}

function updateSpdEditStatusStrip(){
  const dump = uiState.current_spd_valid
    ? `Current dump: addr ${uiState.current_spd_addr || 'unknown'} | crc32 ${uiState.current_spd_crc || '0x0'} | source ${uiState.current_spd_source || 'unknown'}`
    : 'Current dump: none';
  const base = uiState.base_ddr_rate ? ` | Base ${uiState.base_ddr_rate}` : '';
  const expo = uiState.expo_ddr_rate
    ? (String(uiState.expo_ddr_rate).startsWith('EXPO:') ? ` | ${uiState.expo_ddr_rate}` : ` | EXPO ${uiState.expo_ddr_rate}`)
    : ' | EXPO: not detected';
  const xmp = uiState.xmp_ddr_rate ? ` | XMP ${uiState.xmp_ddr_rate}` : ' | XMP: not detected';
  const diagRef = uiState.good_valid
    ? `Diagnostic ref: PRESENT crc32=${uiState.good_crc || '0x0'}`
    : 'Diagnostic ref: MISSING';
  const checkpoint = uiState.spd_backup_valid
    ? `Tweak checkpoint: PRESENT addr=0x${Number(uiState.spd_backup_addr || 0).toString(16)} crc32=${uiState.spd_backup_crc || '0x0'}`
    : 'Tweak checkpoint: MISSING';
  const crcLabel = spdEditProfile === 'xmp' ? 'XMP CRC' : 'EXPO CRC';
  spdEditStatusStrip.innerHTML = `${htmlEscape(dump + base + expo + xmp)} | ${htmlEscape(diagRef)} | ${htmlEscape(checkpoint)} | <strong>${crcLabel}</strong> ${htmlEscape(uiState.expo_crc_status || 'not checked')} | <strong>repair:</strong> ${htmlEscape(uiState.expo_crc_repair || 'unavailable')} | <strong>Apply:</strong> ${htmlEscape(uiState.expo_crc_apply || 'blocked')}`;
}

function updateLabArm(){
  const armed = !!(labAck.checked && labToken.value === 'LABMODE');
  const hasBackup = !!uiState.spd_backup_valid;
  const hasDump = !!uiState.current_spd_valid;
  const repairReady = !!spdEditCrcRepairReady;
  const previewApplyAllowed = !!spdEditPreviewApplyAllowed;
  applyEditBtn.disabled = !(armed && hasBackup && hasDump && spdEditPreviewReady && spdEditChangedBytes > 0 && repairReady && previewApplyAllowed);
  labGuardPanel.className = 'guardPanel ' + (armed ? 'armed' : '');
  labGuardTitle.textContent = armed ? 'LAB SPD WRITE MODE: ARMED' : 'LAB SPD WRITE MODE: OFF';
  if(!armed){
    labGuardDetail.textContent = 'Required: boot-risk checkbox and LABMODE text confirmation.';
    editGuardStatus.textContent = 'Apply blocked: Lab SPD Write Mode is off.';
  }else if(!hasBackup){
    labGuardDetail.textContent = 'Tweak checkpoint is required before writing.';
    editGuardStatus.textContent = 'Apply blocked: tweak checkpoint missing.';
  }else if(!hasDump){
    labGuardDetail.textContent = 'Current SPD dump is required before writing.';
    editGuardStatus.textContent = 'Apply blocked: no current dump.';
  }else if(!spdEditPreviewReady){
    labGuardDetail.textContent = 'Preview required before apply.';
    editGuardStatus.textContent = 'Apply blocked: preview required.';
  }else if(!repairReady){
    labGuardDetail.textContent = 'CRC/checksum repair is required before writing.';
    editGuardStatus.textContent = 'Apply blocked: CRC repair unavailable for edited region.';
  }else if(!previewApplyAllowed){
    labGuardDetail.textContent = 'One or more previewed fields are preview-only.';
    editGuardStatus.textContent = 'Apply blocked: preview-only field selected.';
  }else{
    labGuardDetail.textContent = 'Preview required before apply.';
    editGuardStatus.textContent = `Lab write mode armed. Preview has ${spdEditChangedBytes} changed byte(s) and CRC repair is generated.`;
  }
  uiState.expo_crc_apply = applyEditBtn.disabled ? (editGuardStatus.textContent || 'blocked') : 'allowed';
  updateSpdEditStatusStrip();
}

function collectEditChanges(){
  const form = new URLSearchParams();
  const inputs = spdEditFields.querySelectorAll('input[data-field-id]:not([disabled])');
  const byField = {};
  const conflicts = [];
  inputs.forEach((inp)=>{
    const v = inp.value.trim();
    const orig = inp.getAttribute('data-original') || '';
    if(v && v !== orig){
      const fieldId = inp.getAttribute('data-field-id');
      const source = inp.getAttribute('data-edit-source') || 'raw';
      if(byField[fieldId] && byField[fieldId].value !== v){
        conflicts.push(fieldId);
      }else if(!byField[fieldId] || source === 'friendly'){
        byField[fieldId] = {
          value:v,
          source,
          label:inp.getAttribute('data-friendly-label') || inp.getAttribute('data-raw-label') || fieldId
        };
      }
    }
  });
  if(conflicts.length){
    return {ok:false, error:'Conflict: friendly and raw editors both changed ' + conflicts.join(', ') + '. Clear one value before preview.'};
  }
  Object.keys(byField).forEach((fieldId)=>form.set('field_' + fieldId, byField[fieldId].value));
  return {ok:true, form, labels:byField};
}

function renderFriendlyEditor(){
  const xmpRepairAvailable = spdEditLastFieldsJson && spdEditLastFieldsJson.xmp_crc_discovery && spdEditLastFieldsJson.xmp_crc_discovery.repair === 'available';
  const aliases = spdEditProfile === 'xmp' ? xmpFriendlyAliases : friendlyAliases;
  const rows = aliases.map((a)=>{
    if(a.readOnly){
      return `<tr class="readOnlyRow" title="${htmlEscape(a.note || '')}">
        <td>${htmlEscape(a.label)}</td>
        <td>${htmlEscape(a.note || 'read-only')}</td>
        <td><input class="compact-input" type="text" disabled value="" placeholder="read-only"></td>
        <td>${htmlEscape(a.source || '')}</td>
        <td><span class="status-badge locked">read-only</span></td>
      </tr>`;
    }
    const f = spdEditFieldMap[a.rawFieldId];
    if(!f) return '';
    const disabled = f.editable ? '' : 'disabled';
    const placeholder = f.preview_only ? 'preview' : (f.editable ? 'new' : 'read-only');
    const badges = f.preview_only
      ? '<span class="status-badge warn">preview</span><span class="status-badge">CRC</span>'
      : (f.editable
      ? '<span class="status-badge">editable</span><span class="status-badge">CRC</span>'
      : '<span class="status-badge locked">read-only</span>');
    const note = `${a.confidence}. ${a.note}`;
    return `<tr class="${f.editable ? '' : 'readOnlyRow'}" title="${htmlEscape(note)}">
      <td>${htmlEscape(a.label)}</td>
      <td>${htmlEscape(f.current)}</td>
      <td><input class="compact-input" type="text" ${disabled} data-field-id="${htmlEscape(f.id)}" data-edit-source="friendly" data-friendly-label="${htmlEscape(a.label)}" data-original="" placeholder="${placeholder}" oninput="markEditChanged(this)"></td>
      <td>${htmlEscape(f.offset)} / ${htmlEscape(f.id)}</td>
      <td>${badges}</td>
    </tr>`;
  }).join('');
  const title = spdEditProfile === 'xmp' ? 'XMP Performance Profile Edits' : 'EXPO Profile 1 Edits';
  const scope = spdEditProfile === 'xmp'
    ? 'Editing scope: XMP Performance Profile speed, voltage, and timing fields only. Base JEDEC/default timings and EXPO are separate profile regions.'
    : 'Editing scope: EXPO Profile 1 speed, voltage, and timing fields only. Base JEDEC/default timings and XMP are read-only unless XMP profile is selected.';
  const xmpCrcNote = spdEditProfile === 'xmp' && spdEditLastFieldsJson && spdEditLastFieldsJson.xmp_crc_discovery
    ? `<div class="subtle">XMP CRC: ${htmlEscape(spdEditLastFieldsJson.xmp_crc_discovery.status || 'unknown')} | repair: ${htmlEscape(spdEditLastFieldsJson.xmp_crc_discovery.repair || 'unavailable')}</div>`
    : '';
  return `<div class="profileSelector"><span>Profile to edit:</span>
      <button class="${spdEditProfile === 'expo' ? 'btnBlue' : ''}" onclick="setSpdEditProfile('expo')">EXPO Profile 1</button>
      <button class="${spdEditProfile === 'xmp' ? 'btnBlue' : ''}" ${xmpRepairAvailable ? '' : 'disabled'} title="${xmpRepairAvailable ? 'XMP CRC repair available' : 'XMP checksum/CRC repair is not available'}" onclick="setSpdEditProfile('xmp')">XMP Performance Profile</button>
    </div>
    <div class="sectionTitle">${title}</div>
    <div class="subtle">${scope}</div>
    ${xmpCrcNote}
    <div class="tableScroll"><table class="timingTable compact-table">
      <thead><tr><th>Timing</th><th>Current</th><th>New</th><th>Source</th><th>Status</th></tr></thead>
      <tbody>${rows || '<tr><td colspan="5">No friendly aliases available.</td></tr>'}</tbody>
    </table></div>`;
}

function setSpdEditProfile(profile){
  spdEditProfile = profile === 'xmp' ? 'xmp' : 'expo';
  spdEditPreviewReady = false;
  spdEditChangedBytes = 0;
  spdEditCrcRepairReady = false;
  spdEditPreviewApplyAllowed = true;
  if(spdEditDiff) spdEditDiff.textContent = 'Preview invalidated: profile changed. Click Preview SPD edits to rebuild from the selected profile.';
  if(spdEditLastFieldsJson){
    if(spdEditProfile === 'xmp' && spdEditLastFieldsJson.xmp_crc_discovery){
      uiState.expo_crc_status = spdEditLastFieldsJson.xmp_crc_discovery.status || 'unknown';
      uiState.expo_crc_repair = spdEditLastFieldsJson.xmp_crc_discovery.repair || 'unavailable';
      uiState.expo_crc_apply = spdEditLastFieldsJson.xmp_crc_discovery.apply || 'blocked';
    }else if(spdEditLastFieldsJson.crc_status){
      uiState.expo_crc_status = spdEditLastFieldsJson.crc_status.status || 'not checked';
      uiState.expo_crc_repair = spdEditLastFieldsJson.crc_status.repair || 'unavailable';
      uiState.expo_crc_apply = spdEditLastFieldsJson.crc_status.apply || 'blocked';
    }
  }
  if(spdEditLastFieldsJson) renderSpdEditFields(spdEditLastFieldsJson);
  updateSpdEditStatusStrip();
}

function renderSpdEditFields(j){
  spdEditLastFieldsJson = j;
  spdEditFieldMap = {};
  spdEditExpoSpeed = j.expo_speed || null;
  (j.fields || []).forEach((f)=>{ spdEditFieldMap[f.id] = f; });
  const rows = (j.fields || []).map((f)=>{
    const input = f.editable
      ? `<input class="compact-input" type="text" data-field-id="${htmlEscape(f.id)}" data-edit-source="raw" data-raw-label="${htmlEscape(f.label)}" data-original="" min="${f.min}" max="${f.max}" placeholder="new" oninput="markEditChanged(this)">`
      : '<input class="compact-input" type="text" disabled value="" placeholder="read-only">';
    return `<tr class="${f.editable ? '' : 'readOnlyRow'}">
      <td>${htmlEscape(f.profile)}</td>
      <td>${htmlEscape(f.label)}</td>
      <td>${htmlEscape(f.current)}</td>
      <td>${input}</td>
      <td>${htmlEscape(f.offset)}</td>
      <td>${htmlEscape(f.raw)}</td>
      <td>${htmlEscape(f.status)}</td>
    </tr>`;
  }).join('');
  const xmpCrc = j.xmp_crc_discovery || {};
  const xmpRows = (xmpCrc.candidates || []).map((c)=>`<tr>
    <td>${htmlEscape(c.algorithm)}</td>
    <td>${htmlEscape(c.range)}</td>
    <td>${htmlEscape(c.calculated)}</td>
    <td>${htmlEscape(c.stored)}</td>
    <td>${c.match ? 'yes' : 'no'}</td>
  </tr>`).join('');
  const secondaryRows = (j.secondary_candidates || []).map((c)=>`<tr>
    <td>${htmlEscape(c.profile)}</td>
    <td>${htmlEscape(c.label)}</td>
    <td>${htmlEscape(c.source)}</td>
    <td>${htmlEscape(c.raw)}</td>
    <td>${htmlEscape(c.guess)}</td>
  </tr>`).join('');
  spdEditFields.innerHTML = renderFriendlyEditor() + `
  <details class="rawDetails">
    <summary>Raw EXPO byte-level editor <span class="subtle">Shows raw offsets and editable backing fields for the friendly table.</span></summary>
    <div class="rawDetailsBody">
      <div class="tableScroll"><table class="timingTable compact-table">
        <thead><tr><th>Profile</th><th>Field</th><th>Decoded / current value</th><th>New value</th><th>Raw offset</th><th>Raw bytes</th><th>Status</th></tr></thead>
        <tbody>${rows || '<tr><td colspan="7">No edit fields returned.</td></tr>'}</tbody>
      </table></div>
    </div>
  </details>
  <details class="rawDetails">
    <summary>Raw XMP byte-level viewer <span class="subtle">Read-only 0x2C0-0x2FF performance profile block.</span></summary>
    <div class="rawDetailsBody"><pre>${htmlEscape(j.raw_xmp || 'not loaded')}</pre></div>
  </details>
  <details class="rawDetails">
    <summary>XMP checksum/CRC discovery <span class="subtle">Stored ${htmlEscape(xmpCrc.stored || '--')}; repair ${htmlEscape(xmpCrc.repair || 'unavailable')}</span></summary>
    <div class="rawDetailsBody">
      <div class="tableScroll"><table class="timingTable compact-table">
        <thead><tr><th>Algorithm</th><th>Range</th><th>Calculated</th><th>Stored</th><th>Match</th></tr></thead>
        <tbody>${xmpRows || '<tr><td colspan="5">No XMP checksum candidates available.</td></tr>'}</tbody>
      </table></div>
      <div class="subtle">XMP apply remains blocked unless a candidate exactly matches and repair is implemented.</div>
    </div>
  </details>
  <details class="rawDetails">
    <summary>Secondary timing candidates</summary>
    <div class="rawDetailsBody">
      <div class="tableScroll"><table class="timingTable compact-table">
        <thead><tr><th>Profile</th><th>Timing</th><th>Source</th><th>Raw</th><th>Status</th></tr></thead>
        <tbody>${secondaryRows || '<tr><td colspan="5">No candidates available.</td></tr>'}</tbody>
      </table></div>
    </div>
  </details>`;
  spdEditPreviewReady = false;
  spdEditChangedBytes = 0;
  spdEditCrcRepairReady = false;
  spdEditPreviewApplyAllowed = true;
  if(spdEditProfile === 'xmp' && j.xmp_crc_discovery){
    uiState.expo_crc_status = j.xmp_crc_discovery.status || 'unknown';
    uiState.expo_crc_repair = j.xmp_crc_discovery.repair || 'unavailable';
    uiState.expo_crc_apply = j.xmp_crc_discovery.apply || 'blocked';
  }else if(j.crc_status){
    uiState.expo_crc_status = j.crc_status.status || 'not checked';
    uiState.expo_crc_repair = j.crc_status.repair || 'unavailable';
    uiState.expo_crc_apply = j.crc_status.apply || 'blocked';
  }
  updateLabArm();
}

function markEditChanged(inp){
  const tr = inp.parentElement.parentElement;
  if(inp.value.trim()) tr.classList.add('changedRow');
  else tr.classList.remove('changedRow');
  spdEditPreviewReady = false;
  spdEditChangedBytes = 0;
  spdEditCrcRepairReady = false;
  spdEditPreviewApplyAllowed = true;
  if(spdEditDiff) spdEditDiff.textContent = 'Preview invalidated: field input changed. Click Preview SPD edits to rebuild from the current SPD dump and current inputs.';
  updateLabArm();
}

async function loadSpdEditFields(){
  try{
    const r = await fetch('/api/spd/edit/fields');
    const j = await r.json();
    if(!j.ok){
      spdEditFields.innerHTML = '<div class="subtle">' + htmlEscape(j.error || 'No edit fields available.') + '</div>';
      return;
    }
    renderSpdEditFields(j);
  }catch(e){
    spdEditFields.innerHTML = '<div class="subtle">Edit field load failed: ' + htmlEscape(e) + '</div>';
  }
}

async function previewSpdEdits(){
  const edits = collectEditChanges();
  if(!edits.ok){
    spdEditDiff.textContent = 'FAIL: ' + edits.error;
    if(spdPreviewDetails) spdPreviewDetails.open = true;
    spdEditPreviewReady = false;
    spdEditChangedBytes = 0;
    spdEditCrcRepairReady = false;
    spdEditPreviewApplyAllowed = true;
    updateLabArm();
    return;
  }
  try{
    spdEditPreviewReady = false;
    spdEditChangedBytes = 0;
    spdEditCrcRepairReady = false;
    spdEditPreviewApplyAllowed = true;
    spdEditDiff.textContent = 'Previewing current inputs...';
    updateLabArm();
    const r = await fetch('/api/spd/edit/preview',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:edits.form.toString()
    });
    const j = await r.json();
    if(!j.ok){
      spdEditDiff.textContent = 'FAIL: ' + (j.error || 'preview failed');
      if(spdPreviewDetails) spdPreviewDetails.open = true;
      spdEditPreviewReady = false;
      spdEditChangedBytes = 0;
      spdEditCrcRepairReady = false;
      spdEditPreviewApplyAllowed = true;
      if(j.crc_repair_supported === false){
        uiState.expo_crc_repair = 'unavailable';
        uiState.expo_crc_apply = 'blocked until CRC repair is implemented';
      }
      updateLabArm();
      return;
    }
    const previewProfile = String(j.profile || spdEditProfile || 'EXPO').toUpperCase();
    const diffTitle = (profile, label)=>{
      const p = String(profile || previewProfile || 'EXPO').toUpperCase();
      const l = String(label || '');
      return l.toUpperCase().startsWith(p + ' ') ? l : `${p} ${l}`;
    };
    const lines = [`Automatic ${previewProfile} CRC repair generated.`, `Changed fields: ${j.changed_fields}`, `Requested fields: ${j.requested_fields || (j.diff || []).length}`];
    (j.warnings || []).forEach((w)=>lines.push(`WARN: ${w}`));
    (j.diff || []).forEach((d)=>{
      const meta = edits.labels[d.field] || {};
      const label = d.label || meta.label || d.field;
      if(d.field === 'expo_crc' || d.field === 'profile_crc'){
        lines.push(`${label}\n  ${d.offset} old ${d.old} new ${d.new}`);
      }else if(d.input){
        lines.push(`${diffTitle(d.profile, label)}\n  input: ${d.input}\n  old decoded: ${d.old_decoded || ''}\n  new decoded: ${d.new_decoded || ''}\n  source: ${d.offset} / ${d.field || ''}\n  raw old: ${d.old}\n  raw new: ${d.new}${d.preview_only ? '\n  preview-only: apply blocked until validated' : ''}${d.noop ? '\n  no-op: no byte changes' : ''}`);
      }else{
        lines.push(`${diffTitle(d.profile, label)}\n  input: ${d.input_cycles || ''}T\n  old decoded: ${d.old_cycles || ''}T\n  new decoded: ${d.new_cycles || ''}T\n  source: ${d.offset} / ${d.field || ''}\n  raw old: ${d.old}\n  raw new: ${d.new}${d.noop ? '\n  no-op: no byte changes' : ''}`);
      }
    });
    spdEditDiff.textContent = lines.join('\\n');
    if(spdPreviewDetails) spdPreviewDetails.open = true;
    spdEditPreviewReady = true;
    spdEditChangedBytes = Number(j.changed_fields || 0);
    spdEditCrcRepairReady = !!j.crc_repair_supported;
    spdEditPreviewApplyAllowed = j.apply_allowed !== false;
    if(j.crc_status){
      uiState.expo_crc_status = j.crc_status.status || uiState.expo_crc_status;
      uiState.expo_crc_repair = j.crc_repair_supported ? 'available' : 'unavailable';
    }
    if(j.apply_allowed === false){
      uiState.expo_crc_apply = 'blocked: preview-only field selected';
    }
    updateLabArm();
  }catch(e){
    spdEditDiff.textContent = 'ERR: ' + e;
    if(spdPreviewDetails) spdPreviewDetails.open = true;
  }
}

async function applySpdEdits(){
  const edits = collectEditChanges();
  if(!edits.ok){
    spdEditDiff.textContent = 'FAIL: ' + edits.error;
    if(spdPreviewDetails) spdPreviewDetails.open = true;
    spdEditPreviewReady = false;
    spdEditChangedBytes = 0;
    spdEditCrcRepairReady = false;
    spdEditPreviewApplyAllowed = true;
    updateLabArm();
    return;
  }
  const form = edits.form;
  form.set('labmode', labToken.value);
  form.set('laback', labAck.checked ? '1' : '0');
  try{
    const r = await fetch('/api/spd/edit/apply',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:form.toString()
    });
    const j = await r.json();
    spdEditDiff.textContent = j.ok ? `${j.message}\\ncrc32=${j.crc32} generation=${j.generation}` : 'FAIL: ' + (j.error || 'apply failed');
    if(spdPreviewDetails) spdPreviewDetails.open = true;
    if(j.ok){
      spdEditPreviewReady = false;
      spdEditCrcRepairReady = false;
      spdEditPreviewApplyAllowed = true;
      await refreshDumpState();
      await loadSpdEditor();
    }
    updateLabArm();
  }catch(e){
    spdEditDiff.textContent = 'ERR: ' + e;
    if(spdPreviewDetails) spdPreviewDetails.open = true;
  }
}

function renderSpdDecoded(j){
  if(j.compact_summary){
    renderCompactSpdSummary(j);
    return;
  }
  if(j.dump && j.metadata){
    renderTinySpdMetadata(j);
    return;
  }

  spdPreviewGenerated = false;
  spdPreviewHasChanges = false;
  spdPreviewCrcRepair = !!j.crc_repair_supported;
  updateSpdApplyGate();

  spdEditorState.textContent = `${j.summary || 'decoded'} at ${j.addr || document.getElementById('spdEditorAddr').value}. EXPO timing writes require preview with automatic CRC repair; XMP is read-only.`;

  const fields = j.fields || [];
  const xmpPresent = !!fieldByKey(fields, 'xmp_speed');
  const expoPresent = !!fieldByKey(fields, 'expo_speed');
  const base = makeTimingProfile('Base/JEDEC', profileColumns);
  applyKnownBaseCells(base, fields);
  let html = '';
  html += renderMatrix('Base / JEDEC Profile', profileColumns, [base]);
  if(expoPresent){
    const expoProfile = makeTimingProfile('EXPO Profile 1', profileColumns);
    applyProfileCells(expoProfile, fields, 'expo');
    html += renderMatrix('EXPO Profile 1 Timings', profileColumns, [expoProfile]);
  }
  if(xmpPresent){
    const xmpProfile = makeTimingProfile('XMP 3.0 Performance', profileColumns);
    applyProfileCells(xmpProfile, fields, 'xmp');
    html += renderMatrix('XMP 3.0 Performance Profile Timings', profileColumns, [xmpProfile]);
  }else{
    html += renderMatrix('XMP 3.0 Performance Profile Timings', profileColumns, [makeProfileEvidence('XMP not detected', fieldByKey(fields, 'xmp'))]);
  }
  html += renderMetadataTable(metadataFromDecoded(j));
  spdTimingTables.innerHTML = html;
  uiState.current_spd_valid = true;
  uiState.current_spd_addr = j.addr || uiState.current_spd_addr;
  uiState.current_spd_crc = j.dump_crc || uiState.current_spd_crc;
  uiState.current_spd_source = j.dump_source || uiState.current_spd_source;
  uiState.current_spd_generation = j.dump_generation || uiState.current_spd_generation;
  const baseSpeed = fieldByKey(fields, 'base_speed');
  const expoSpeed = fieldByKey(fields, 'expo_speed');
  const xmpSpeed = fieldByKey(fields, 'xmp_speed');
  if(baseSpeed && baseSpeed.value) uiState.base_ddr_rate = (baseSpeed.value.match(/DDR5-[0-9]+/) || [baseSpeed.value])[0];
  if(expoSpeed && expoSpeed.value) uiState.expo_ddr_rate = (expoSpeed.value.match(/DDR5-[0-9]+/) || [expoSpeed.value])[0];
  if(xmpSpeed && xmpSpeed.value) uiState.xmp_ddr_rate = (xmpSpeed.value.match(/DDR5-[0-9]+/) || [xmpSpeed.value])[0];
  loadSpdEditFields();
  updateAdvancedSpdSummary();
}

async function loadSpdEditor(){
  const addr = currentSpdEditorAddr();
  await decodeCurrentSpdDump(addr);
}

async function decodeCurrentSpdDump(addr){
  spdLoadInProgress = true;
  preserveDevicesDuringSpdLoad = true;
  loadSpdBtn.disabled = true;
  spdEditorState.textContent = `Decoding current SPD dump for ${addr}...`;
  try{
    const summaryResp = await fetch('/api/spd/decode/summary');
    const summary = await summaryResp.json();
    if(!summaryResp.ok || !summary.ok){
      spdEditorState.textContent = summary.error || 'SPD decode failed';
      const msg = summary.error || 'No 1024-byte SPD dump captured yet. Click Dump 1024 or Map Current Mode first.';
      spdTimingTables.innerHTML = '<div class="subtle">' + htmlEscape(msg) + '</div>';
      if(uiState.current_spd_valid) advancedSpdSummary.textContent = 'decode failed: ' + msg;
      else updateAdvancedSpdSummary();
      return;
    }
    renderCompactSpdSummary(summary);
    const sections = [
      {url:'/api/spd/decode/base', target:'spdBaseSection'},
      {url:'/api/spd/decode/expo', target:'spdExpoSection'},
      {url:'/api/spd/decode/xmp', target:'spdXmpSection'}
    ];
    for(const s of sections){
      try{
        const r = await fetch(s.url);
        const j = await r.json();
        renderTimingDecodeSection(s.target, j);
      }catch(e){
        renderTimingDecodeSection(s.target, {ok:false, title:s.target, error:String(e)});
      }
    }
    await loadSpdEditFields();
    spdEditorState.textContent = `Decoded current SPD dump for ${addr}. Sections loaded independently; verified EXPO timing edits are available through guarded preview.`;
  }catch(e){
    spdEditorState.textContent = 'ERR: ' + e;
    if(!uiState.current_spd_valid) spdTimingTables.innerHTML = '<div class="subtle">SPD decode failed before the table could be updated.</div>';
  }finally{
    loadSpdBtn.disabled = false;
    preserveDevicesDuringSpdLoad = false;
    spdLoadInProgress = false;
  }
}

async function dumpSpdAndShowTiming(addr){
  const adv = document.getElementById('advancedSpd');
  if(adv) adv.open = true;
  const addrInput = document.getElementById('spdEditorAddr');
  if(addrInput) addrInput.value = addr;

  spdLoadInProgress = true;
  preserveDevicesDuringSpdLoad = true;
  spdEditorState.textContent = `Dumping 1024 bytes from ${addr} to terminal...`;
  loadSpdBtn.disabled = true;

  try{
    await runTerminalOnly('dump ' + addr);
    spdEditorState.textContent = `Building timing table from current dump at ${addr}...`;
    await decodeCurrentSpdDump(addr);
  }catch(e){
    spdEditorState.textContent = 'ERR: ' + e;
    spdTimingTables.innerHTML = '<div class="subtle">Dump command failed before the table could be updated.</div>';
  }finally{
    loadSpdBtn.disabled = false;
    preserveDevicesDuringSpdLoad = false;
    spdLoadInProgress = false;
  }
}

async function previewSpdTweak(){
  const addr = currentSpdEditorAddr();
  try{
    const body = 'addr=' + encodeURIComponent(addr);
    const r = await fetch('/api/spdtweak/preview',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body
    });
    const j = await r.json();
    lastSpdPreview = `${j.message || ''}\\n${j.diff || ''}`;
    spdDiff.textContent = lastSpdPreview;
    spdPreviewGenerated = true;
    spdPreviewHasChanges = !!j.has_changes;
    spdPreviewCrcRepair = !!j.crc_repair_supported;
    spdEditorState.textContent = j.has_changes ? 'Preview contains byte changes.' : 'Preview has no byte changes; no editable SPD fields are enabled yet.';
    updateSpdApplyGate();
  }catch(e){
    spdDiff.textContent = 'ERR: ' + e;
    spdPreviewGenerated = false;
    spdPreviewHasChanges = false;
    updateSpdApplyGate();
  }
}

async function applySpdTweak(){
  updateSpdApplyGate();
  if(applySpdBtn.disabled){
    spdDiff.textContent = 'Writes blocked: no verified editable timing fields and/or CRC repair missing.';
    return;
  }
  const token = prompt('Type YES_WRITE_SPD_TWEAK to write supported SPD byte changes.');
  if(token !== 'YES_WRITE_SPD_TWEAK') return;
  const addr = currentSpdEditorAddr();
  try{
    const body = 'addr=' + encodeURIComponent(addr) + '&token=' + encodeURIComponent(token);
    const r = await fetch('/api/spdtweak/apply',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body
    });
    const j = await r.json();
    spdDiff.textContent = j.ok ? 'PASS' : 'FAIL: ' + (j.error || 'apply failed');
    await refreshStatus();
    await refreshLog();
  }catch(e){
    spdDiff.textContent = 'ERR: ' + e;
  }
}

function showSpdRawDiff(){
  spdDiff.textContent = lastSpdPreview || 'no preview loaded';
}

function updateSpdApplyGate(){
  const canApply = !!(uiState.spd_backup_valid && spdPreviewGenerated && spdPreviewHasChanges && spdPreviewCrcRepair);
  applySpdBtn.disabled = !canApply;
  applySpdBtn.title = canApply
    ? 'Ready to apply verified SPD byte changes.'
    : 'Writes blocked: no verified editable timing fields and/or CRC repair missing.';
  writeGuardStatus.textContent = canApply
    ? 'Writes guarded: tweak checkpoint present, preview diff generated, and CRC repair reported available.'
    : 'Writes blocked: no verified editable timing fields, tweak checkpoint, and/or CRC repair missing.';
  updateAdvancedSpdSummary();
}

function updateAdvancedSpdSummary(){
  if(!advancedSpdSummary) return;
  if(uiState.current_spd_valid){
    advancedSpdSummary.textContent =
      `table: current dump ready | addr ${uiState.current_spd_addr || 'unknown'} observed | crc32 ${uiState.current_spd_crc || '0x0'} | source ${uiState.current_spd_source || 'unknown'}`;
  }else{
    advancedSpdSummary.textContent = 'table: no current SPD dump';
  }
  if(spdEditStatusStrip) updateSpdEditStatusStrip();
}

['hwPreset','hwHsa','hwVin','hwPwrEn','hwPGood','hwI2c','hwPullups'].forEach((id)=>{
  const el = document.getElementById(id);
  if(el) el.addEventListener('change', ()=>{ hwDirty = true; });
});

function promptRestoreBackup(cmd){
  const token = prompt('This will overwrite current SPD contents from the saved tweak checkpoint and verify readback. CLI equivalent: restorelast [addr] YES_RESTORE_SPD_BACKUP. Type YES_RESTORE_SPD_BACKUP to continue.');
  if(token !== 'YES_RESTORE_SPD_BACKUP') return;
  sendCmdValue(cmd + ' YES_RESTORE_SPD_BACKUP');
}

function toggleDimmPower(){
  if(uiState.hw_vinbulk === 'external_locked_on'){
    dimmToggle.checked = true;
    spdDiff.textContent = 'VIN_BULK command not sent: GPIO32 is not authoritative in this harness config.';
    updateToggleVisuals(uiState);
    return;
  }
  const wantOn = dimmToggle.checked;
  sendCmdValue(wantOn ? 'dimmpwr on' : 'dimmpwr off');
}

function cycleDimmPowerUi(){
  if(uiState.hw_vinbulk === 'external_locked_on'){
    spdDiff.textContent = 'VIN_BULK cold-cycle command not sent: GPIO32 is not authoritative in this harness config.';
    updateToggleVisuals(uiState);
    return;
  }
  sendCmdValue('dimmpwr cycle');
}

function hsaReleaseUi(){
  if(uiState.hw_hsa !== 'gpio_switchable'){
    spdDiff.textContent = 'HSA command not sent: GPIO27 control disabled by hardware config.';
    updateToggleVisuals(uiState);
    return;
  }
  sendCmdValue('hsa release');
}

function hsaGroundUi(){
  if(uiState.hw_hsa !== 'gpio_switchable'){
    spdDiff.textContent = 'HSA command not sent: GPIO27 control disabled by hardware config.';
    updateToggleVisuals(uiState);
    return;
  }
  sendCmdValue('hsa ground');
}

function togglePwrEn(){
  if(uiState.hw_pwren === 'pullup_only'){
    pwrEnToggle.checked = true;
    spdDiff.textContent = 'PWR_EN command not sent: GPIO33 control disabled by hardware config.';
    updateToggleVisuals(uiState);
    return;
  }
  const wantOn = pwrEnToggle.checked;
  sendCmdValue(wantOn ? 'en on' : 'en off');
}

cmdI.addEventListener('keydown', (e)=>{
  if(e.key === 'Enter') sendCmd();
});

refreshStatus();
refreshDumpState();
refreshLog();
setInterval(refreshStatus, 2500);
</script>
</body>
</html>)HTML";

static void handleWebRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void webSetup() {
  server.on("/", HTTP_GET, handleWebRoot);

  server.on("/api/status", HTTP_POST, handleWebStatus);
  server.on("/api/run",    HTTP_POST, handleWebRun);
  server.on("/api/log",    HTTP_GET,  handleWebLog);
  server.on("/api/clearlog", HTTP_POST, handleWebClearLog);
  server.on("/api/hwconfig/save", HTTP_POST, handleWebHwConfigSave);
  server.on("/api/spdtweak/decode", HTTP_POST, handleWebSpdTweakDecode);
  server.on("/api/spdtweak/decode_current", HTTP_POST, handleWebSpdTweakDecodeCurrent);
  server.on("/api/spdtweak/dump_decode", HTTP_POST, handleWebDumpAndDecode);
  server.on("/api/spd/dumpstate", HTTP_GET, handleWebSpdDumpState);
  server.on("/api/spd/decode/current", HTTP_GET, handleWebSpdTweakDecodeCurrent);
  server.on("/api/spd/decode/summary", HTTP_GET, handleWebSpdDecodeSummary);
  server.on("/api/spd/decode/base", HTTP_GET, handleWebSpdDecodeBase);
  server.on("/api/spd/decode/expo", HTTP_GET, handleWebSpdDecodeExpo);
  server.on("/api/spd/decode/xmp", HTTP_GET, handleWebSpdDecodeXmp);
  server.on("/api/spd/edit/fields", HTTP_GET, handleWebSpdEditFields);
  server.on("/api/spd/edit/preview", HTTP_POST, handleWebSpdEditPreview);
  server.on("/api/spd/edit/apply", HTTP_POST, handleWebSpdEditApply);
  server.on("/api/spd/writegood", HTTP_POST, handleWebWriteDiagnosticReference);
  server.on("/api/spdtweak/preview", HTTP_POST, handleWebSpdTweakPreview);
  server.on("/api/spdtweak/apply", HTTP_POST, handleWebSpdTweakApply);

  server.on("/download/current.bin", HTTP_GET, handleDownloadCurrent);
  server.on("/download/good.bin",    HTTP_GET, handleDownloadGood);

  server.begin();
}

void webInit() {
  if (!WIFI_ENABLE) return;

  WiFi.mode(WIFI_AP);
  if (strlen(AP_PASS) >= 8) WiFi.softAP(AP_SSID, AP_PASS);
  else WiFi.softAP(AP_SSID);

  delay(50);
  webSetup();
}

void webPoll() {
  if (WIFI_ENABLE) server.handleClient();
}
