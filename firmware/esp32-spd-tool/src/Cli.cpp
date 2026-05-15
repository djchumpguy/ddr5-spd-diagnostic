#include "Cli.h"
#include "AppConfig.h"
#include "AppState.h"
#include "BoardControl.h"
#include "HardwareConfig.h"
#include "SpdOps.h"
#include "SpdEditMap.h"
#include "PowerDiag.h"
#include "TimeSpd.h"
#include "TimeReg.h"
#include "Log.h"
#include "MapOps.h"
#include "PmicRef.h"
#include "RoleDetect.h"
#include "BiosRead/BiosRead.h"
#include <string.h>

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

static bool isRestoreToken(const char* s) {
  if (!s) return false;
  return strcmp(s, "YES_RESTORE_SPD_BACKUP") == 0;
}

static uint8_t defaultMapHubAddr() {
  // Address choice for reads only. HSA mode must come from declared hardware config,
  // not from the observed address.
  return isHsaLow() ? 0x50 : 0x53;
}

static uint8_t defaultRestoreAddr() {
  if (gApp.currentSpdDump.valid) return gApp.currentSpdDump.addr;
  for (size_t i = 0; i < gApp.roleRecordCount; i++) {
    const DeviceRoleRecord& r = gApp.roleRecords[i];
    if (r.role == ROLE_SPD_HUB && r.probeOk) return r.address;
  }
  if (gApp.spdBackupValid) return gApp.spdBackupAddr;
  uint8_t expected = 0;
  const char* desc = nullptr;
  if (hwExpectedSpdAddr(expected, desc)) return expected;
  return defaultMapHubAddr();
}

static bool hsaGpioControlAllowed() {
  if (gApp.hwConfig.hsa == HW_HSA_GPIO_SWITCHABLE) return true;
  outPrintf("Ignored: HSA mode is %s; GPIO27 is not authoritative in this hardware config.\n",
            hwHsaModeName(gApp.hwConfig.hsa));
  return false;
}

static void printSafeHelp() {
  outPrintln();
  outPrintln("ESP32 SPD Tool (Web-first, Serial fallback)");
  outPrintln("Safe commands:");
  outPrintln("  h | help                         = help");
  outPrintln("  advanced | danger | help danger  = show dangerous/write-capable commands");
  outPrintln("  status                           = show rails/HSA/MR11/diagnostic reference/checkpoint state");
  outPrintln("  hwconfig | hwcfg                 = show declared physical hardware harness config");
  outPrintln("  hwconfig help                    = hardware config presets/setters");
  outPrintln("  en on|off                        = PWR_EN pin / PMIC enable input; optional for SPD/PMIC reads");
  outPrintln("  dimmpwr                          = show VIN_BULK switch state");
  outPrintln("  dimmpwr on|off                   = turn optional VIN_BULK switch on/off");
  outPrintln("  dimmpwr cycle                    = cold-cycle VIN_BULK");
  outPrintln("  hsa                              = show HSA / Address Strap state");
  outPrintln("  hsa release|ground               = release GPIO27 or drive HSA low when hwconfig hsa=gpio");
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
  outPrintln("                                     observed SPD/HUB address is evidence only; hwconfig declares HSA mode");
  outPrintln("                                     does NOT switch HSA or cold-cycle VIN_BULK");
  outPrintln("  r | read [addr] [off] [n]        = read SPD NVM bytes (default 0x50 0x0000 16)");
  outPrintln("  d | dump [addr]                  = dump 1024 bytes (stores current dump buffer)");
  outPrintln("  spddumpstate                     = show current 1024-byte SPD dump metadata");
  outPrintln("  biosmr11 [addr]                  = read MR11 through BIOS helper path");
  outPrintln("                                     Auto-detect resolves active SPD/HUB and PMIC addresses");
  outPrintln("  biosread | bread [addr] [off]    = BIOS-style legacy 1-byte SPD read");
  outPrintln("                                     Auto-detect resolves active SPD/HUB and PMIC addresses");
  outPrintln("  biosdump | bdump [addr] [off] [n]= BIOS-style legacy dump");
  outPrintln("                                     Auto-detect resolves active SPD/HUB and PMIC addresses");
  outPrintln("  biosinteresting | bint [addr]    = read 0x0D7 0x0D9 0x0DA 0x0DB 0x0DC");
  outPrintln("                                     Auto-detect resolves active SPD/HUB and PMIC addresses");
  outPrintln();
  outPrintln("Diagnostic reference:");
  outPrintln("  capturegood | cg [addr]          = Save current SPD as diagnostic reference.");
  outPrintln("  compare | cmp [addr]             = Dump current SPD and compare to diagnostic reference.");
  outPrintln("  verifygood | vg [addr]           = Legacy alias/compatibility check against diagnostic reference.");
  outPrintln("  cleargood | clg                  = Erase diagnostic reference.");
  outPrintln();
  outPrintln("Advanced SPD editing:");
  outPrintln("  spdtweak help                    = Advanced SPD Editing command help");
  outPrintln("  spdtweak status                  = Show tweak/checkpoint status.");
  outPrintln("  spdtweak decode [summary|base|expo|xmp|all] = staged crash-safe decode");
  outPrintln("  spdedit fields                   = Show editable EXPO/XMP profile fields.");
  outPrintln("  spdedit preview field=value...   = Preview profile edits and CRC repair.");
  outPrintln("                                     write-capable edit/apply commands are in: help danger");
  outPrintln();
  outPrintln("Tools / diagnostics:");
  outPrintln("  health | diagquick               = Run quick read-only SPD/PMIC/harness health check.");
  outPrintln("  speedtest [addr] [len] [passes] [delay_ms]");
  outPrintln("                                     Run SPD/I2C repeat-read speed and stability test.");
  outPrintln("  fulldiag                         = Run full read-only system diagnostic report.");
  outPrintln();
  outPrintln("Expert timing tools:");
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
  outPrintln("  writegood | wg yes [addr]        = unlock + write diagnostic reference SPD + verify");
  outPrintln("                                     requires explicit 'yes'");
  outPrintln("  backupspd | spdbak | bakspd [addr]");
  outPrintln("                                     Save current SPD as tweak checkpoint.");
  outPrintln("  backupinfo | spdbakinfo          = show tweak checkpoint metadata");
  outPrintln("  restoreinfo                      = show edit rollback checkpoint metadata");
  outPrintln("  restorebackup <addr> YES_RESTORE_SPD_BACKUP");
  outPrintln("                                     Restore tweak checkpoint and readback verify.");
  outPrintln("  restorelast [addr] YES_RESTORE_SPD_BACKUP");
  outPrintln("                                     Restore tweak checkpoint using resolved/current SPD address.");
  outPrintln("                                     Restore does not change the diagnostic reference.");
  outPrintln("  clearbackup | clrbak             = erase only the tweak checkpoint slot");
  outPrintln("  spdedit apply LABMODE field=value...");
  outPrintln("                                     apply verified EXPO timing edits with CRC repair and readback verify");
  outPrintln("  spdtweak apply [addr] field=value ... YES_WRITE_SPD_TWEAK");
  outPrintln("                                     blocked until verified DDR5 SPD profile field map / CRC updater");
  outPrintln("  Validation flow: sacrificial module first; confirm CRC/checksum PASS and restorebackup PASS;");
  outPrintln("                   save diagnostic reference + tweak checkpoint before any write; JEDEC/base stays read-only.");
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
  if (cmd == "hwconfig" || cmd == "hwcfg") { cmdHardwareConfig(nt, tok); return; }

  if (cmd == "en") {
    if (nt < 2) { outPrintln("Usage: en on|off"); latchCommandResult(false); return; }
    String v = tok[1]; v.toLowerCase();
    if (v == "on") {
      warnIfPwrEnNotGpioControlled();
      setPwrEnEnabled(true);
      outPrintln("PWR_EN pin / PMIC enable input released ON. Optional for SPD/PMIC sideband reads.");
      latchCommandResult(true);
    } else if (v == "off") {
      warnIfPwrEnNotGpioControlled();
      setPwrEnEnabled(false);
      if (gApp.hwConfig.pwrEn == HW_PWREN_PULLUP_ONLY) {
        outPrintln("PWR_EN GPIO33 left high-Z because hardware config is pullup_only.");
      } else {
        outPrintln("PWR_EN pin / PMIC enable input pulled LOW/OFF.");
      }
      latchCommandResult(true);
    } else {
      outPrintln("Usage: en on|off");
      latchCommandResult(false);
    }
    return;
  }

  if (cmd == "dimmpwr") {
    if (nt == 1) {
      if (gApp.hwConfig.vinBulk == HW_VIN_GPIO_SWITCHABLE) {
        outPrintf("VIN_BULK switch: %s\n", isDimmPowerOn() ? "ON" : "OFF");
      } else {
        outPrintf("VIN_BULK physical config: %s; GPIO32 raw=%s is not authoritative for actual DIMM power.\n",
                  hwVinBulkModeName(gApp.hwConfig.vinBulk),
                  isDimmPowerOn() ? "ON" : "OFF");
      }
      latchCommandResult(true);
      return;
    }

    String v = tok[1];
    v.toLowerCase();

    if (v == "on") {
      if (gApp.hwConfig.vinBulk == HW_VIN_EXTERNAL_LOCKED_ON) {
        outPrintln("Ignored: VIN_BULK is external_locked_on; GPIO32 not authoritative.");
        latchCommandResult(false);
        return;
      }
      warnIfVinBulkNotSwitchable();
      setDimmPower(true);
      outPrintln("VIN_BULK switch set ON.");
      latchCommandResult(true);
      return;
    }
    if (v == "off") {
      if (gApp.hwConfig.vinBulk == HW_VIN_EXTERNAL_LOCKED_ON) {
        outPrintln("Ignored: VIN_BULK is external_locked_on; GPIO32 not authoritative.");
        latchCommandResult(false);
        return;
      }
      warnIfVinBulkNotSwitchable();
      setDimmPower(false);
      outPrintln("VIN_BULK switch set OFF.");
      latchCommandResult(true);
      return;
    }
    if (v == "cycle") {
      if (gApp.hwConfig.vinBulk == HW_VIN_EXTERNAL_LOCKED_ON) {
        outPrintln("Ignored: VIN_BULK is external_locked_on; GPIO32 not authoritative.");
        latchCommandResult(false);
        return;
      }
      warnIfVinBulkNotSwitchable();
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
      outPrintf("Declared HSA physical config: %s\n", hwHsaModeName(gApp.hwConfig.hsa));
      outPrintf("GPIO27 raw: %s", isHsaLow() ? "DRIVING GND/OFFLINE" : "RELEASED");
      if (gApp.hwConfig.hsa != HW_HSA_GPIO_SWITCHABLE) outPrintf(", ignored because HSA mode is %s", hwHsaModeName(gApp.hwConfig.hsa));
      outPrintln();
      outPrintf("Interpretation: %s\n", hwDeclaredHsaInterpretation());
      outPrintln("Observed SPD/HUB address comes from scan/autodetect only and does not prove HSA mode.");
      latchCommandResult(true);
      return;
    }

    String v = tok[1];
    v.toLowerCase();

    if (v == "normal" || v == "high" || v == "floating" || v == "float" || v == "release") {
      if (!hsaGpioControlAllowed()) { latchCommandResult(false); return; }
      setHsaLow(false);
      outPrintln("HSA GPIO released.");
      outPrintln("HSA is sampled at cold power-up. Cold-cycle VIN_BULK before treating this as effective.");
      outPrintln("Auto-detect determines the observed SPD/HUB address; address alone does not prove HSA mode.");
      latchCommandResult(true);
      return;
    }
    if (v == "write" || v == "low" || v == "ground" || v == "gnd") {
      if (!hsaGpioControlAllowed()) { latchCommandResult(false); return; }
      setHsaLow(true);
      outPrintln("HSA GPIO driving low.");
      outPrintln("HSA is sampled at cold power-up. Cold-cycle VIN_BULK before treating this as effective.");
      outPrintln("Auto-detect determines the observed SPD/HUB address; address alone does not prove HSA mode.");
      latchCommandResult(true);
      return;
    }

    outPrintln("Usage: hsa release|ground   (aliases: float, floating, normal, high, write, low, gnd)");
    latchCommandResult(false);
    return;
  }

  if (cmd == "scan") { cmdScan(); return; }
  if (cmd == "autodetect" || cmd == "detect" || cmd == "roles") { cmdAutoDetectRoles(); return; }

  if (cmd == "health" || cmd == "diagquick") {
    cmdQuickHealth();
    return;
  }

  if (cmd == "speedtest") {
    uint32_t addr = DEFAULT_SPD_ADDR;
    uint32_t len = 32;
    uint32_t passes = 20;
    uint32_t delayMs = 20;
    bool includeScan = true;
    bool includePmic = true;

    if (nt >= 2) parseU32(tok[1], addr);
    if (nt >= 3) parseU32(tok[2], len);
    if (nt >= 4) parseU32(tok[3], passes);
    if (nt >= 5) parseU32(tok[4], delayMs);
    for (int i = 5; i < nt; i++) {
      if (strcmp(tok[i], "noscan") == 0) includeScan = false;
      if (strcmp(tok[i], "nopmic") == 0) includePmic = false;
    }

    cmdSpeedTest((uint8_t)addr, (uint16_t)len, (uint16_t)passes, (uint16_t)delayMs, includeScan, includePmic);
    return;
  }

  if (cmd == "fulldiag") {
    cmdFullDiag();
    return;
  }

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

  if (cmd == "spddumpstate") {
    printCurrentSpdDumpState();
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

  if (cmd == "backupspd" || cmd == "spdbak" || cmd == "bakspd") {
    uint32_t addr = defaultMapHubAddr();
    if (nt >= 2) parseU32(tok[1], addr);
    cmdBackupSpd((uint8_t)addr);
    return;
  }

  if (cmd == "backupinfo" || cmd == "spdbakinfo" || cmd == "restoreinfo") {
    cmdBackupInfo();
    return;
  }

  if (cmd == "clearbackup" || cmd == "clrbak") {
    cmdClearBackup();
    return;
  }

  if (cmd == "restorebackup") {
    uint32_t addr = 0;
    if (nt != 3 || !parseU32(tok[1], addr) || addr > 0x7F || !isRestoreToken(tok[2])) {
      outPrintln("Usage: restorebackup <addr> YES_RESTORE_SPD_BACKUP");
      latchCommandResult(false);
      return;
    }
    cmdRestoreBackup((uint8_t)addr, true);
    return;
  }

  if (cmd == "restorelast" || cmd == "spdrestore" || cmd == "spdr") {
    uint32_t addr = defaultRestoreAddr();
    if (nt == 2) {
      if (!isRestoreToken(tok[1])) {
        outPrintln("Usage: restorelast [addr] YES_RESTORE_SPD_BACKUP");
        latchCommandResult(false);
        return;
      }
    } else if (nt == 3) {
      if (!parseU32(tok[1], addr) || addr > 0x7F || !isRestoreToken(tok[2])) {
        outPrintln("Usage: restorelast [addr] YES_RESTORE_SPD_BACKUP");
        latchCommandResult(false);
        return;
      }
    } else {
      outPrintln("Usage: restorelast [addr] YES_RESTORE_SPD_BACKUP");
      latchCommandResult(false);
      return;
    }
    outPrintf("RESTORELAST: using SPD/HUB address 0x%02X.\n", (unsigned)addr);
    cmdRestoreBackup((uint8_t)addr, true);
    return;
  }

  if (cmd == "spdtweak") {
    cmdSpdTweak(nt, tok);
    return;
  }

  if (cmd == "spdedit") {
    cmdSpdEdit(nt, tok);
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
