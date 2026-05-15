# ESP32 SPD Tool Command Reference

[Back to README](../../README.md) | [Quick start](../quick-start.md) | [Safety](../safety.md)

This reference is based on the checked-in firmware command parser in `firmware/esp32-spd-tool/src/Cli.cpp`, plus the current SPD edit/tweak helpers. The same command surface is exposed through the serial fallback and the Web UI terminal/run actions.

Classifications:

- **Read-only**: reads state or prints metadata.
- **Control**: changes ESP32-controlled harness GPIO state only when the declared hardware config allows it.
- **Saved-reference write**: writes ESP32 flash/NVS reference data, not the DIMM.
- **DIMM write-capable**: can write SPD/hub/PMIC/module state.

Observed SPD/HUB address is evidence only. Declared hardware config matters, especially for HSA, VIN_BULK, PWR_EN, PWR_GOOD, and I2C wiring mode.

## Safe / Read-Only Basics

| Command | Aliases | Class | What it does |
| --- | --- | --- | --- |
| `help` | `h` | Read-only | Prints safe command help. |
| `advanced` | `danger`, `help danger`, `help advanced` | Read-only | Prints dangerous/write-capable command help. |
| `status` | | Read-only | Shows rails/HSA/MR11 and diagnostic reference/checkpoint state. |
| `hwconfig` | `hwcfg` | Read-only | Shows declared physical harness config. |
| `hwconfig help` | | Read-only | Prints hardware config presets and setters. |
| `scan` | `s` | Read-only | I2C scan. |
| `autodetect` | `detect`, `roles` | Read-only | Scans/probes and classifies SPD/PMIC/temp/unknown devices. |
| `mapall [spd] [pmic] [hub]` | `map`, `m` shortcut with defaults | Read-only | Best-effort current-mode map. Does not switch HSA or cold-cycle VIN_BULK. |
| `read [addr] [off] [n]` | `r` shortcut with defaults | Read-only | Reads SPD NVM bytes. Default is `0x50 0x0000 16`. |
| `dump [addr]` | `d` shortcut with default | Read-only | Dumps 1024 SPD bytes and stores current dump metadata in RAM. |
| `spddumpstate` | | Read-only | Shows current 1024-byte SPD dump metadata. |
| `reg <addr> <reg> [n]` | | Read-only | Generic register read. |
| `flash` | | Read-only | Shows ESP32 flash size info. |
| `clearlog` | Web UI Clear Log | UI/log action | Clears the ESP32 tool log buffer. |

Examples:

```text
scan
autodetect
map
dump 0x50
spddumpstate
reg 0x50 0x0B 1
clearlog
```

## Hardware State / Harness Controls

These commands only affect hardware when the declared hardware config says the ESP32 GPIO is authoritative. If the config says a signal is pull-up-only, external, locked-on, or physically strapped, the firmware warns or ignores the GPIO command.

| Command | Aliases | Class | What it does |
| --- | --- | --- | --- |
| `hwconfig preset full|minimum_direct|manual|unknown` | | Saved config | Saves a hardware preset in ESP32 NVS. |
| `hwconfig hsa gpio|ground|resistor|floating|unknown` | | Saved config | Declares physical HSA mode. |
| `hwconfig vinbulk gpio|locked|manual|unknown` | | Saved config | Declares VIN_BULK control mode. |
| `hwconfig pwren gpio|pullup|none|unknown` | | Saved config | Declares PWR_EN wiring/control mode. |
| `hwconfig pgood gpio|pullup|none|unknown` | | Saved config | Declares PWR_GOOD wiring/monitor mode. |
| `hwconfig i2c pca9306|direct|other|unknown` | | Saved config | Declares I2C wiring mode. |
| `hwconfig pullups external|internal|module|unknown` | | Saved config | Declares pull-up source. |
| `hwconfig clear` | | Saved config | Clears saved hardware config. |
| `en on|off` | | Control | Releases or pulls PWR_EN depending on hardware config. |
| `dimmpwr` | | Read-only | Shows VIN_BULK switch/raw state or declared external mode. |
| `dimmpwr on|off|cycle` | | Control | Controls/cycles optional ESP32-controlled VIN_BULK switch if allowed. |
| `hsa` | | Read-only | Shows declared HSA config and raw GPIO27 state. |
| `hsa release` | `float`, `floating`, `normal`, `high` | Control | Releases GPIO27 when `hwconfig hsa=gpio`. Cold-cycle VIN_BULK before treating as effective. |
| `hsa ground` | `gnd`, `write`, `low` | Control | Drives GPIO27 low when `hwconfig hsa=gpio`. Cold-cycle VIN_BULK before treating as effective. |

The `minimum_direct` preset describes the simple direct adapter setup: HSA tied GND, VIN_BULK externally on, PWR_EN pull-up, direct ESP32 3.3 V I2C. The tested observed address was `0x50`, but address is evidence only.

## BIOS-Style / Legacy SPD Reads

| Command | Aliases | Class | What it does |
| --- | --- | --- | --- |
| `biosmr11 [addr]` | | Read-only | Reads MR11 through the BIOS helper path. |
| `biosread [addr] [off]` | `bread` | Read-only | BIOS-style legacy 1-byte SPD read. |
| `biosdump [addr] [off] [n]` | `bdump` | Read-only | BIOS-style legacy dump. |
| `biosinteresting [addr]` | `bint` | Read-only | Reads `0x0D7`, `0x0D9`, `0x0DA`, `0x0DB`, `0x0DC`. |

MR11 is a volatile legacy page/pointer register and may change depending on the access path and previous reads/dumps. Do not treat an MR11 value alone as a permanent module diagnosis.

## Diagnostic SPD Reference

The diagnostic SPD reference is a saved known-good/original comparison target in ESP32 flash/NVS. It is different from the tweak checkpoint.

| Command | Aliases | Class | What it does |
| --- | --- | --- | --- |
| `capturegood [addr]` | `cg` | Saved-reference write | Dumps current SPD and saves it as the diagnostic reference. |
| `compare [addr]` | `cmp` | Read-only | Dumps current SPD and compares it to the diagnostic reference. |
| `verifygood [addr]` | `vg` | Read-only | Legacy compatibility check against the diagnostic reference. |
| `cleargood` | `clg` | Saved-reference write | Erases the diagnostic SPD reference from ESP32 flash. |
| `writegood yes [addr]` | `wg yes [addr]` | DIMM write-capable | Unlocks, writes the diagnostic reference SPD image, then verifies readback. |

Example:

```text
capturegood 0x50
compare 0x50
wg yes 0x50
```

`writegood` is dangerous. It can write the wrong payload to the wrong module/address if your reference or address is wrong.

## Tweak Checkpoint / Backup / Restore

The tweak checkpoint is a rollback/checkpoint image for experimental SPD edits. It is not the diagnostic SPD reference.

| Command | Aliases | Class | What it does |
| --- | --- | --- | --- |
| `backupspd [addr]` | `spdbak`, `bakspd` | Saved-reference write | Saves current SPD as the tweak checkpoint. |
| `backupinfo` | `spdbakinfo`, `restoreinfo` | Read-only | Shows tweak checkpoint metadata. |
| `restorebackup <addr> YES_RESTORE_SPD_BACKUP` | | DIMM write-capable | Restores the tweak checkpoint to the explicit address and verifies readback. |
| `restorelast [addr] YES_RESTORE_SPD_BACKUP` | `spdrestore`, `spdr` | DIMM write-capable | Restores the tweak checkpoint using the current/resolved or supplied address and verifies readback. |
| `clearbackup` | `clrbak` | Saved-reference write | Erases only the tweak checkpoint slot. |

Example:

```text
backupspd 0x50
backupinfo
restorelast 0x50 YES_RESTORE_SPD_BACKUP
```

Restore does not change the diagnostic reference. Readback verification confirms what was written, not that the DIMM will boot.

## Advanced SPD Editing

Advanced SPD editing is experimental. It is not a beginner workflow. DDR5-5600 EXPO/XMP editing has proven preview/write/readback/CRC behavior only, not BIOS/POST/memory stability.

| Command | Aliases | Class | What it does |
| --- | --- | --- | --- |
| `spdtweak help` | | Read-only | Prints advanced SPD editing help. |
| `spdtweak status` | | Read-only | Compact-decodes the current 1024-byte SPD dump. |
| `spdtweak decode [summary|base|expo|xmp|all]` | | Read-only | Staged crash-safe decode of current SPD dump sections. |
| `spdtweak selftest` | | Read-only | Runs compact decoder safety checks. |
| `spdtweak preview [addr] field=value ...` | | Read-only preview | Dumps SPD if needed and previews supported byte changes. |
| `spdtweak apply [addr] field=value ... YES_WRITE_SPD_TWEAK` | | DIMM write-capable | Saves checkpoint, writes supported patched bytes, re-reads, and verifies. |
| `spdedit fields` | | Read-only | Lists editable/read-only EXPO/XMP profile fields. |
| `spdedit preview field=value...` | | Read-only preview | Previews profile edits and CRC/checksum repair. |
| `spdedit apply LABMODE field=value...` | | DIMM write-capable | Applies verified profile edits with CRC repair and readback verification. |

The Web UI exposes friendly and raw edit fields, profile selection, preview details, guarded apply controls, EXPO/XMP checksum/CRC repair status, and readback verification results.

Data-rate/tCK note:

```text
tCK_ps = 2000000 / DDR data rate MT/s
```

Integer picosecond storage can make DDR5-5600 display as a derived rate around DDR5-5602. Voltage editing uses verified encodings only.

## PMIC Tools

| Command | Aliases | Class | What it does |
| --- | --- | --- | --- |
| `pmicid [addr]` | | Read-only | Reads PMIC ID. Default address is `0x48`. |
| `pmicdump [start] [len]` | | Read-only | Dumps PMIC registers from default `0x48`. Default range is `0x00 0x80`. |
| `pmicdumpat <addr> [start] [len]` | | Read-only | Dumps PMIC registers at any address. |
| `capturepmic [addr] [start] [len]` | `cpmic` | Saved-reference write | Saves current PMIC registers as PMIC reference. |
| `comparepmic [addr] [start] [len]` | `cmpmic` | Read-only | Compares current PMIC registers to saved PMIC reference. |
| `pmicref` | | Read-only | Shows PMIC reference metadata. |
| `clearpmic` | `clpmic` | Saved-reference write | Erases PMIC reference. |

## Stability / Timing Diagnostics

These commands test management-plane bus-read stability only. They do not test DRAM cells, RAM bandwidth, or memory-controller training stability.

| Command | Aliases | Class | What it does |
| --- | --- | --- | --- |
| `health` | `diagquick` | Read-only | Quick sanity test for scan, SPD header, MR11, PMIC ID, PWR_GOOD, diagnostic reference, and tweak checkpoint. |
| `speedtest [addr] [len] [passes] [delay_ms] [noscan] [nopmic]` | | Read-only | Repeat-read speed/stability test, with optional scan consistency and PMIC quick check. |
| `fulldiag` | | Read-only | Full management-plane diagnostic report. |
| `powerdiag [passes] [ms]` | `pdiag` | Read-only | Repeated power/status/scan stability sampling; default `12 50`. |
| `timespd [addr] [off] [len] [passes]` | `tspd` | Read-only | Repeats SPD NVM reads and timing/compare; default `0x50 0x0000 32 20`. |
| `timereg [addr] [reg] [len] [passes]` | `treg` | Read-only | Repeats register reads and timing/compare; default `0x50 0x00 16 20`. |

The Web UI includes Quick Health Check, Speed / Stability Test, and Full System Diagnostic cards. The Speed / Stability Test has scan and PMIC toggles that map to `noscan` and `nopmic`.

## Dangerous Register Writes

| Command | Class | Why it is dangerous |
| --- | --- | --- |
| `editreg yes <addr> <reg> <value>` | DIMM/PMIC/hub write-capable | Writes one register and verifies readback. Can alter PMIC, hub, or other device state. |
| `writegood yes [addr]` / `wg yes [addr]` | DIMM write-capable | Writes the diagnostic SPD reference to the module. |
| `restorebackup <addr> YES_RESTORE_SPD_BACKUP` | DIMM write-capable | Writes tweak checkpoint bytes back to SPD. |
| `restorelast [addr] YES_RESTORE_SPD_BACKUP` | DIMM write-capable | Writes tweak checkpoint bytes back to resolved/current SPD address. |
| `spdedit apply LABMODE field=value...` | DIMM write-capable | Writes experimental profile edits. |
| `spdtweak apply [addr] field=value ... YES_WRITE_SPD_TWEAK` | DIMM write-capable | Writes experimental supported byte changes. |

Use write-capable commands only on hardware you can afford to lose. Save diagnostic references and tweak checkpoints before writing. CRC/checksum repair and readback verification confirm bytes and payload math, not bootability.

## Disabled / Compatibility Commands

| Command | Aliases | Class | What it does |
| --- | --- | --- | --- |
| `autofix` | `af` | Disabled | Prints that AutoFix is disabled in this build. |
| `v` | | Runtime toggle | Single-letter shortcut toggles verbose logging. |
