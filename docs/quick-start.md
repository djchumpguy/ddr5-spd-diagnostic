# Quick Start: Read-Only DDR5 SPD/PMIC Diagnostics

[Back to README](../README.md) | [Flashing](flashing.md) | [Safety](safety.md) | [SPD tool wiring](hardware/spd-tool-wiring.md) | [Command reference](spd-tool/command-reference.md)

This is the simple read-only path for the active ESP32 SPD/PMIC tool. It does not start with SPD editing.

## Do I Need The Whole Repo?

Normal users should not need the full repository just to flash and use the ESP32 SPD Tool. Once release packages are published, use the firmware release/package for basic flashing and use.

Clone the repo if you are a builder/developer who wants to modify or build the firmware with PlatformIO. Clone it if you are doing research/debugging and want the docs, examples, captures, source code, or investigation notes.

Do not download random build artifacts from old logs. Do not commit `.pio` build output or other generated PlatformIO build folders.

For install options, see [Flashing](flashing.md).

## Build The Minimum Harness

1. Use a DDR5 extension adapter or breakout.
2. Connect ESP32 GPIO21 to DIMM/adapter HSDA/SDA.
3. Connect ESP32 GPIO22 to DIMM/adapter HSCL/SCL.
4. Pull PWR_EN to 3.3 V with 10 kOhm.
5. Pull PWR_GOOD to 3.3 V with 10 kOhm, and connect it to the ESP32 input if you want to monitor it.
6. Provide 5 V to the DIMM VIN_BULK pins.
7. Power the ESP32, USB is fine.
8. Share ground between the DIMM power supply and ESP32.

The proven basic direct-read setup did not need a PCA9306 level shifter or external SDA/SCL pull-ups. Those can be troubleshooting or alternate-harness options later.

## Before Power

- Read [Safety](safety.md).
- Verify 3.3 V, 5 V, PWR_EN, PWR_GOOD, and ground with a meter.
- Use strain relief on adapter wires.
- Confirm whether your harness physically straps HSA. The ESP32 GPIO runtime state may not describe the real HSA strap.

![Dashboard and hardware tools](images/ui/esp32-spd-tool-dashboard-hardware-tools-dark.png)

## First Commands

Use read-only commands first:

```text
status
scan
autodetect
health
dump
```

Useful next commands:

```text
mapall
diagquick
compare
verifygood
speedtest
fulldiag
```

Save a diagnostic SPD reference only when you are confident the dump is the known-good/original payload. Capture a PMIC reference only when the PMIC state is understood.

For command syntax, aliases, and safety classification, see the [command reference](spd-tool/command-reference.md).

![Terminal discovered devices](images/ui/esp32-spd-tool-terminal-discovered-devices-dark.png)

## What A Clean Read Means

A clean SPD dump, PMIC read, CRC/checksum, and repeated read stability are useful management-plane evidence. They do not prove the DIMM will POST, train memory, or survive a memory test.

For the documented bad stick, management-plane evidence became clean after restoring SPD data, but the stick still did not boot. Boot-time sniffer behavior points toward DRAM-side/training-path failure rather than an active SPD hub MR12/MR13 mismatch.
