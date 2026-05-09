#include "Cli.h"
#include "AppConfig.h"
#include "AppState.h"
#include "BoardControl.h"
#include "SpdOps.h"
#include "PowerDiag.h"
#include "TimeSpd.h"
#include "TimeReg.h"
#include "Log.h"
#include "MapOps.h"
#include "PmicRef.h"
#include "RoleDetect.h"
#include "BiosRead/BiosRead.h"

static int splitTokens(char* line, char* out[], int maxTokens) {
  int n = 0;
  while (*line && n < maxTokens) {
    while (*line == ' ' || *line == '\t') line++;
    if (!*line) break;
    out[n++] = line;
    while (*line && *line != ' ' && *line != '\t') line++;
    if (*line) *line++ = 0;
  }
  return n;
}

static bool parseU32(const char* s, uint32_t& v) {
  if (!s || !*s) return false;
  char* end = nullptr;
  v = strtoul(s, &end, 0);
  return end && *end == 0;
}

static bool isYes(const char* s) {
  if (!s) return false;
  String t = s;
  t.toLowerCase();
  return (t == "yes" || t == "y");
}

static uint8_t defaultMapHubAddr() {
  // Your bench mapping:
  //   normal/runtime strap -> 0x53
  //   write/offline direct GND -> 0x50
  return isHsaLow() ? 0x50 : 0x53;
}

static void printSafeHelp() {
  outPrintln();
  outPrintln("ESP32 SPD Tool (Web-first, Serial fallback)");
  outPrintln("Safe commands:");
  outPrintln("  h | help                         = help");
  outPrintln("  advanced | danger | help danger  = show dangerous/write-capable commands");
  outPrintln("  status                           = show rails/HSA/MR11/known-good reference");
  outPrintln("  en on|off                        = PWR_EN / VR enable; optional for SPD/PMIC reads");
  outPrintln("  dimmpwr                          = show VIN_BULK switch state");
  outPrintln("  dimmpwr on|off                   = turn optional VIN_BULK switch on/off");
  outPrintln("  dimmpwr cycle                    = cold-cycle VIN_BULK");
  outPrintln("  hsa                              = show HSA / Address Strap state");
  outPrintln("  hsa release|ground               = release GPIO27 or drive HSA hard-ground/offline");
  outPrintln("  hsa float|floating               = compatibility alias for release");
  outPrintln("  hsa normal|high                  = compatibility alias for release");
  outPrintln("  hsa write|low                    = compatibility alias for ground/offline");
  outPrintln("                                     released GPIO lets the external HSA strap decide");
  outPrintln("                                     Auto-detect determines the active SPD/HUB address");
  outPrintln("                                     cold-cycle VIN_BULK after HSA changes");
  outPrintln("  s | scan                         = I2C scan");
  outPrintln("  autodetect | detect | roles      = scan/probe and classify SPD/PMIC/temp/unknown devices");
  outPrintln("  mapall | map [spd] [pmic] [hub]  = read-only best-effort map using CURRENT mode only");
  outPrintln("                                     Auto-detect resolves active SPD/HUB and PMIC addresses");
  outPrintln("                                     observed hub examples: 0x50 direct-GND/offline, 0x53 resistor strap, 0x57 floating/high");
  outPrintln("                                     does NOT switch HSA or cold-cycle VIN_BULK");
  outPrintln("  r | read [addr] [off] [n]        = read SPD NVM bytes (default 0x50 0x0000 16)");
  outPrintln("  d | dump [addr]                  = dump 1024 bytes (stores current dump buffer)");
  outPrintln("  biosmr11 [addr]                  = read MR11 through BIOS helper path");
  outPrintln("                                     Auto-detect resolves active SPD/HUB and PMIC addresses");
  outPrintln("  biosread | bread [addr] [off]    = BIOS-style legacy 1-byte SPD read");
  outPrintln("                                     Auto-detect resolves active SPD/HUB and PMIC addresses");
  outPrintln("  biosdump | bdump [addr] [off] [n]= BIOS-style legacy dump");
  outPrintln("                                     Auto-detect resolves active SPD/HUB and PMIC addresses");
  outPrintln("  biosinteresting | bint [addr]    = read 0x0D7 0x0D9 0x0DA 0x0DB 0x0DC");
  outPrintln("                                     Auto-detect resolves active SPD/HUB and PMIC addresses");
  outPrintln("  compare | cmp [addr]             = dump+compare current vs known-good reference");
  outPrintln("  capturegood | cg [addr]          = dump current and save known-good reference");
  outPrintln("  cleargood | clg                  = erase known-good SPD reference from flash");
  outPrintln("  verifygood | vg [addr]           = verify stick vs known-good reference");
  outPrintln("  powerdiag | pdiag [passes] [ms]  = repeated power + scan stability test");
  outPrintln("                                     default: 12 passes, 50ms between passes");
  outPrintln("  timespd | tspd [addr] [off] [len] [passes] = repeat SPD reads and time/compare them");
  outPrintln("                                     default: 0x50 0x0000 32 20");
  outPrintln("  timereg | treg [addr] [reg] [len] [passes] = repeat register reads and time/compare them");
  outPrintln("                                     default: 0x50 0x00 16 20");
  outPrintln();
  outPrintln("Extras:");
  outPrintln("  pmicid [addr]                    = read PMIC id (default 0x48)");
  outPrintln("  pmicdump [start] [len]           = dump PMIC regs from 0x48 (default 0x00 0x80)");
  outPrintln("  pmicdumpat <addr> [start] [len]  = dump PMIC regs at any address");
  outPrintln("  capturepmic | cpmic [addr] [start] [len] = save PMIC register reference");
  outPrintln("  comparepmic | cmpmic [addr] [start] [len] = compare current PMIC regs to reference");
  outPrintln("  pmicref                         = show PMIC reference metadata");
  outPrintln("  clearpmic | clpmic              = erase PMIC reference");
  outPrintln("  reg <addr> <reg> [n]             = generic register read");
  outPrintln("  flash                            = show flash size info");
  outPrintln();
}

static void printDangerHelp() {
  outPrintln();
  outPrintln("DANGEROUS / WRITE-CAPABLE COMMANDS");
  outPrintln("  writegood | wg yes [addr]        = unlock + write known-good SPD + verify");
  outPrintln("                                     requires explicit 'yes'");
  outPrintln("  editreg yes <addr> <reg> <value> = write one register and verify readback");
  outPrintln("                                     requires explicit 'yes'");
  outPrintln("  AutoFix                          = disabled in this build");
  outPrintln();
}

void printHelp() {
  printSafeHelp();
}

static void handleLine(char* line) {
  while (*line == ' ' || *line == '\t') line++;
  if (!*line) return;

  if (line[1] == 0) {
    switch (line[0]) {
      case 'h': printHelp(); latchCommandResult(true); return;
      case 's': cmdScan(); return;
      case 'r': cmdRead(DEFAULT_SPD_ADDR, 0x0000, 16); return;
      case 'd': cmdDump(DEFAULT_SPD_ADDR); return;
      case 'm': cmdMapAll(DEFAULT_SPD_ADDR, DEFAULT_PMIC_ADDR, defaultMapHubAddr()); return;
      case 'v':
        gApp.verbose = !gApp.verbose;
        outPrintf("Verbose = %s\n", gApp.verbose ? "ON" : "OFF");
        latchCommandResult(true);
        return;
      default: break;
    }
  }

  char* tok[64] = {0};
  int nt = splitTokens(line, tok, 64);
  if (nt <= 0) return;

  String cmd = tok[0];
  cmd.toLowerCase();

  if (cmd == "help") {
    if (nt >= 2) {
      String topic = tok[1];
      topic.toLowerCase();
      if (topic == "danger" || topic == "advanced") {
        printDangerHelp();
        latchCommandResult(true);
        return;
      }
    }
    printHelp();
    latchCommandResult(true);
    return;
  }
  if (cmd == "advanced" || cmd == "danger") { printDangerHelp(); latchCommandResult(true); return; }
  if (cmd == "status") { cmdStatus(); return; }

  if (cmd == "en") {
    if (nt < 2) { outPrintln("Usage: en on|off"); latchCommandResult(false); return; }
    String v = tok[1]; v.toLowerCase();
    if (v == "on") {
      setPwrEnEnabled(true);
      outPrintln("PWR_EN / PMIC VR enable released ON. Optional for SPD/PMIC sideband reads.");
      latchCommandResult(true);
    } else if (v == "off") {
      setPwrEnEnabled(false);
      outPrintln("PWR_EN / PMIC VR enable pulled LOW/OFF.");
      latchCommandResult(true);
    } else {
      outPrintln("Usage: en on|off");
      latchCommandResult(false);
    }
    return;
  }

  if (cmd == "dimmpwr") {
    if (nt == 1) {
      outPrintf("VIN_BULK switch: %s\n", isDimmPowerOn() ? "ON" : "OFF");
      latchCommandResult(true);
      return;
    }

    String v = tok[1];
    v.toLowerCase();

    if (v == "on") {
      setDimmPower(true);
      outPrintln("VIN_BULK switch set ON.");
      latchCommandResult(true);
      return;
    }
    if (v == "off") {
      setDimmPower(false);
      outPrintln("VIN_BULK switch set OFF.");
      latchCommandResult(true);
      return;
    }
    if (v == "cycle") {
      outPrintln("Cold-cycling VIN_BULK...");
      cycleDimmPower(1000, 1000);
      outPrintln("VIN_BULK cold-cycle complete.");
      latchCommandResult(true);
      return;
    }

    outPrintln("Usage: dimmpwr on|off|cycle");
    latchCommandResult(false);
    return;
  }

  if (cmd == "hsa") {
    if (nt == 1) {
      outPrintf("HSA GPIO: %s\n", isHsaLow() ? "DRIVING GND/OFFLINE" : "RELEASED");
      outPrintln("GPIO27 control is experimental. Released GPIO lets the external HSA strap decide the address.");
      outPrintln("Auto-detect determines the active SPD/HUB address. Cold-cycle VIN_BULK after HSA changes.");
      latchCommandResult(true);
      return;
    }

    String v = tok[1];
    v.toLowerCase();

    if (v == "normal" || v == "high" || v == "floating" || v == "float" || v == "release") {
      setHsaLow(false);
      outPrintln("HSA GPIO released.");
      outPrintln("GPIO27 control is experimental. Released GPIO lets the external HSA strap decide the address.");
      outPrintln("Auto-detect determines the active SPD/HUB address. Cold-cycle VIN_BULK after HSA changes.");
      latchCommandResult(true);
      return;
    }
    if (v == "write" || v == "low" || v == "ground" || v == "gnd") {
      setHsaLow(true);
      outPrintln("HSA GPIO driving GND/OFFLINE.");
      outPrintln("GPIO27 control is experimental. Auto-detect determines the active SPD/HUB address.");
      outPrintln("Cold-cycle VIN_BULK after HSA changes.");
      latchCommandResult(true);
      return;
    }

    outPrintln("Usage: hsa release|ground   (aliases: float, floating, normal, high, write, low, gnd)");
    latchCommandResult(false);
    return;
  }

  if (cmd == "scan") { cmdScan(); return; }
  if (cmd == "autodetect" || cmd == "detect" || cmd == "roles") { cmdAutoDetectRoles(); return; }

  if (cmd == "powerdiag" || cmd == "pdiag") {
    uint32_t passes = 12;
    uint32_t intervalMs = 50;

    if (nt >= 2) parseU32(tok[1], passes);
    if (nt >= 3) parseU32(tok[2], intervalMs);

    cmdPowerDiag((uint16_t)passes, (uint16_t)intervalMs);
    return;
  }

  if (cmd == "timespd" || cmd == "tspd") {
    uint32_t addr = DEFAULT_SPD_ADDR;
    uint32_t off = 0x0000;
    uint32_t len = 32;
    uint32_t passes = 20;

    if (nt >= 2) parseU32(tok[1], addr);
    if (nt >= 3) parseU32(tok[2], off);
    if (nt >= 4) parseU32(tok[3], len);
    if (nt >= 5) parseU32(tok[4], passes);

    cmdTimeSpd((uint8_t)addr, (uint16_t)off, (uint16_t)len, (uint16_t)passes);
    return;
  }

  if (cmd == "timereg" || cmd == "treg") {
    uint32_t addr = DEFAULT_SPD_ADDR;
    uint32_t reg = 0x00;
    uint32_t len = 16;
    uint32_t passes = 20;

    if (nt >= 2) parseU32(tok[1], addr);
    if (nt >= 3) parseU32(tok[2], reg);
    if (nt >= 4) parseU32(tok[3], len);
    if (nt >= 5) parseU32(tok[4], passes);

    cmdTimeReg((uint8_t)addr, (uint8_t)reg, (uint16_t)len, (uint16_t)passes);
    return;
  }

  if (cmd == "read") {
    uint32_t addr = DEFAULT_SPD_ADDR, off = 0x0000, n = 16;
    if (nt >= 2) parseU32(tok[1], addr);
    if (nt >= 3) parseU32(tok[2], off);
    if (nt >= 4) parseU32(tok[3], n);
    cmdRead((uint8_t)addr, (uint16_t)off, (uint16_t)n);
    return;
  }

  if (cmd == "dump") {
    uint32_t addr = DEFAULT_SPD_ADDR;
    if (nt >= 2) parseU32(tok[1], addr);
    cmdDump((uint8_t)addr);
    return;
  }

  if (cmd == "biosmr11") {
    uint32_t addr = defaultMapHubAddr();
    if (nt >= 2) parseU32(tok[1], addr);
    cmdBiosMr11((uint8_t)addr);
    return;
  }

  if (cmd == "biosread" || cmd == "bread") {
    uint32_t addr = defaultMapHubAddr();
    uint32_t off = 0x0000;

    if (nt >= 2) parseU32(tok[1], addr);
    if (nt >= 3) parseU32(tok[2], off);

    cmdBiosRead((uint8_t)addr, (uint16_t)off);
    return;
  }

  if (cmd == "biosdump" || cmd == "bdump") {
    uint32_t addr = defaultMapHubAddr();
    uint32_t off = 0x0000;
    uint32_t len = 16;

    if (nt >= 2) parseU32(tok[1], addr);
    if (nt >= 3) parseU32(tok[2], off);
    if (nt >= 4) parseU32(tok[3], len);

    cmdBiosDump((uint8_t)addr, (uint16_t)off, (uint16_t)len);
    return;
  }

  if (cmd == "biosinteresting" || cmd == "bint") {
    uint32_t addr = defaultMapHubAddr();
    if (nt >= 2) parseU32(tok[1], addr);
    cmdBiosInteresting((uint8_t)addr);
    return;
  }

  if (cmd == "mapall" || cmd == "map") {
    uint32_t spdAddr  = DEFAULT_SPD_ADDR;
    uint32_t pmicAddr = DEFAULT_PMIC_ADDR;
    uint32_t hubAddr  = defaultMapHubAddr();

    if (nt >= 2) parseU32(tok[1], spdAddr);
    if (nt >= 3) parseU32(tok[2], pmicAddr);
    if (nt >= 4) parseU32(tok[3], hubAddr);

    cmdMapAll((uint8_t)spdAddr, (uint8_t)pmicAddr, (uint8_t)hubAddr);
    return;
  }

  if (cmd == "compare" || cmd == "cmp") {
    uint32_t addr = DEFAULT_SPD_ADDR;
    if (nt >= 2) parseU32(tok[1], addr);
    cmdCompare((uint8_t)addr);
    return;
  }

  if (cmd == "capturegood" || cmd == "cg") {
    uint32_t addr = DEFAULT_SPD_ADDR;
    if (nt >= 2) parseU32(tok[1], addr);
    cmdCaptureGood((uint8_t)addr);
    return;
  }

  if (cmd == "cleargood" || cmd == "clg") {
    cmdClearGood();
    return;
  }

  if (cmd == "writegood" || cmd == "wg") {
    bool confirmed = false;
    uint32_t addr = DEFAULT_SPD_ADDR;

    if (nt >= 2 && isYes(tok[1])) confirmed = true;
    if (nt >= 3) parseU32(tok[2], addr);

    cmdWriteGood((uint8_t)addr, confirmed);
    return;
  }

  if (cmd == "verifygood" || cmd == "vg") {
    uint32_t addr = DEFAULT_SPD_ADDR;
    if (nt >= 2) parseU32(tok[1], addr);
    cmdVerifyGood((uint8_t)addr);
    return;
  }

  if (cmd == "autofix" || cmd == "af") {
    outPrintln("AutoFix is disabled in this build. Use explicit read/compare/write workflows.");
    latchCommandResult(false);
    return;
  }

  if (cmd == "pmicid") {
    uint32_t a = DEFAULT_PMIC_ADDR;
    if (nt >= 2) parseU32(tok[1], a);
    cmdPmicId((uint8_t)a);
    return;
  }

  if (cmd == "pmicdump") {
    uint32_t start = 0x00, len = 0x80;
    if (nt >= 2) parseU32(tok[1], start);
    if (nt >= 3) parseU32(tok[2], len);
    cmdPmicDump(DEFAULT_PMIC_ADDR, (uint8_t)start, (uint16_t)len);
    return;
  }

  if (cmd == "pmicdumpat") {
    if (nt < 2) {
      outPrintln("Usage: pmicdumpat <addr> [start] [len]");
      latchCommandResult(false);
      return;
    }
    uint32_t addr = 0, start = 0x00, len = 0x80;
    if (!parseU32(tok[1], addr)) {
      outPrintln("pmicdumpat: bad addr");
      latchCommandResult(false);
      return;
    }
    if (nt >= 3) parseU32(tok[2], start);
    if (nt >= 4) parseU32(tok[3], len);
    cmdPmicDump((uint8_t)addr, (uint8_t)start, (uint16_t)len);
    return;
  }

  if (cmd == "capturepmic" || cmd == "cpmic") {
    uint32_t addr = defaultPmicRefAddr();
    uint32_t start = 0x00;
    uint32_t len = 0x80;

    if (nt >= 2) parseU32(tok[1], addr);
    if (nt >= 3) parseU32(tok[2], start);
    if (nt >= 4) parseU32(tok[3], len);

    cmdCapturePmic((uint8_t)addr, (uint8_t)start, (uint16_t)len);
    return;
  }

  if (cmd == "comparepmic" || cmd == "cmpmic") {
    uint32_t addr = gApp.pmicRefValid ? gApp.pmicRefAddr : defaultPmicRefAddr();
    uint32_t start = gApp.pmicRefValid ? gApp.pmicRefStart : 0x00;
    uint32_t len = gApp.pmicRefValid ? gApp.pmicRefLen : 0x80;

    if (nt >= 2) parseU32(tok[1], addr);
    if (nt >= 3) parseU32(tok[2], start);
    if (nt >= 4) parseU32(tok[3], len);

    cmdComparePmic((uint8_t)addr, (uint8_t)start, (uint16_t)len);
    return;
  }

  if (cmd == "clearpmic" || cmd == "clpmic") {
    cmdClearPmic();
    return;
  }

  if (cmd == "pmicref") {
    cmdPmicRef();
    return;
  }

  if (cmd == "reg") {
    if (nt < 3) {
      outPrintln("Usage: reg <addr> <reg> [n]");
      latchCommandResult(false);
      return;
    }

    uint32_t a = 0, r = 0, n = 1;
    if (!parseU32(tok[1], a) || !parseU32(tok[2], r)) {
      outPrintln("reg: bad addr/reg");
      latchCommandResult(false);
      return;
    }
    if (nt >= 4) parseU32(tok[3], n);
    cmdRegRead((uint8_t)a, (uint8_t)r, (uint16_t)n);
    return;
  }

  if (cmd == "editreg") {
    if (nt < 5 || !isYes(tok[1])) {
      outPrintln("EDITREG blocked: register writes are dangerous.");
      outPrintln("Usage: editreg yes <addr> <reg> <value>");
      latchCommandResult(false);
      return;
    }

    uint32_t a = 0, r = 0, v = 0;
    if (!parseU32(tok[2], a) || !parseU32(tok[3], r) || !parseU32(tok[4], v)) {
      outPrintln("editreg: bad addr/reg/value");
      latchCommandResult(false);
      return;
    }

    cmdEditReg((uint8_t)a, (uint8_t)r, (uint8_t)v);
    return;
  }

  if (cmd == "flash") {
    cmdFlashInfo();
    return;
  }

  if (cmd == "clearlog") {
    clearLog();
    outPrintln("OK: log cleared.");
    latchCommandResult(true);
    return;
  }

  outPrintf("Unknown command: %s\n", tok[0]);
  outPrintln("Type 'help' or 'h'.");
  latchCommandResult(false);
}

void execCommandLine(const String& lineIn) {
  char buf[CLI_LINE_CAP];
  String s = lineIn;
  s.trim();
  if (s.length() == 0) return;

  outPrintf("\n> %s\n", s.c_str());

  memset(buf, 0, sizeof(buf));
  s.toCharArray(buf, sizeof(buf));
  handleLine(buf);
}

static int readAnyChar() {
  if (Serial.available()) return Serial.read();
  return -1;
}

void cliPollSerial() {
  static char line[CLI_LINE_CAP];
  static uint8_t idx = 0;

  for (int k = 0; k < 180; k++) {
    int r = readAnyChar();
    if (r < 0) break;

    char c = (char)r;
    if (c == '\r') continue;

    if (c == '\n') {
      line[idx] = 0;
      idx = 0;
      execCommandLine(String(line));
      continue;
    }

    if (c == 0x08 || c == 0x7F) {
      if (idx > 0) idx--;
      continue;
    }

    if ((uint8_t)c < 0x20) continue;

    if (idx < sizeof(line) - 1) line[idx++] = c;
  }
}
