# Passive Boot Sniffer Quick Start

[Back to README](../../README.md) | [Sniffer wiring](../hardware/sniffer-wiring.md) | [Boot sniffer deep dive](10-boot-sniffer.md)

The passive boot sniffer is a secondary tool. Use it when you want to observe motherboard-driven DDR5 sideband traffic during boot. It is not the active ESP32 SPD Tool and it does not power or query a DIMM by itself.

## 1. Download The Sniffer Firmware

- [esp32-ddr5-sniffer-v0.1.0.zip](https://github.com/djchumpguy/ddr5-spd-diagnostic/releases/download/v0.1.0/esp32-ddr5-sniffer-v0.1.0.zip)
- [Release page](https://github.com/djchumpguy/ddr5-spd-diagnostic/releases/tag/v0.1.0)

## 2. Flash The ESP32

Extract the ZIP and read its `README-FLASHING.txt`. Flashing is separate from DDR5 wiring; do not connect sniffer probes just to flash the ESP32.

## 3. Wire The Passive Tap

This is the setup that uses piggyback/tap wiring. The sniffer observes motherboard-driven lines during boot and must not drive the bus.

Typical tapped lines:

- DDR5 sideband SDA / HSDA.
- DDR5 sideband SCL / HSCL.
- Common ground reference.

See [Sniffer wiring](../hardware/sniffer-wiring.md) before connecting probes.

## What It Captures

The sniffer captures early boot-sideband behavior so you can compare known-good and suspect modules. It can show whether the motherboard reaches SPD hub and PMIC traffic before behavior diverges.

## What It Does Not Do

- It does not read SPD by itself like the active SPD Tool.
- It does not write SPD, PMIC, or hub state.
- It does not power the DIMM.
- It does not prove DRAM cell health or memory-controller training stability.

For deeper capture workflow details, see [Boot sniffer deep dive](10-boot-sniffer.md).
