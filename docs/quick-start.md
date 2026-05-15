# Quick Start: Read-Only DDR5 SPD/PMIC Diagnostics

[Back to README](../README.md) | [Safety](safety.md) | [SPD tool wiring](hardware/spd-tool-wiring.md)

This is the safest path through the project. It is for learning what the DDR5 SPD hub and PMIC management plane report without writing anything to the DIMM.

## Before Power

- Read [Safety](safety.md).
- Build the active ESP32 SPD/PMIC tool wiring from [SPD tool wiring](hardware/spd-tool-wiring.md).
- Verify 3.3 V, 5 V, ground, and pull-up behavior with a meter before inserting a DIMM.
- Use strain relief. Temporary wires and piggyback solder joints can break or short.
- Confirm whether your harness physically straps HSA. The ESP32 GPIO runtime state may not describe the real HSA strap.

## First Web UI Session

1. Power the ESP32 and DIMM harness.
2. Open the ESP32 SPD tool Web UI.
3. Check status and hardware config.
4. Run scan or auto-detect.
5. Run current-mode map.
6. Dump the 1024-byte SPD payload.
7. Save a diagnostic SPD reference only if you are confident it is the known-good/original payload.
8. Capture a PMIC reference only when the PMIC state is understood.

![Dashboard and hardware tools](images/ui/esp32-spd-tool-dashboard-hardware-tools-dark.png)

## Read-Only Commands To Prefer

- `status`
- `scan`
- `autodetect`
- `mapall`
- `dump`
- `compare`
- `verifygood`
- `capturegood` when saving a known-good/original reference
- `capturepmic` when saving a PMIC comparison reference
- `speedtest` for I2C/SPD/PMIC read repeatability only
- `fulldiag` for management-plane diagnostics only

Speed/stability tests are bus-read stability tests. They do not prove DRAM cell health or memory-controller training stability.

![Terminal discovered devices](images/ui/esp32-spd-tool-terminal-discovered-devices-dark.png)

## What A Clean Read Means

A clean SPD dump, PMIC read, CRC/checksum, and repeated read stability are useful management-plane evidence. They do not prove the DIMM will POST, train memory, or survive a memory test.

For the documented bad stick, management-plane evidence became clean after restoring SPD data, but the stick still did not boot. Boot-time sniffer behavior points toward DRAM-side failure rather than an active SPD hub MR12/MR13 mismatch.
