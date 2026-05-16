# Flashing Firmware

[Back to README](../README.md) | [Quick start](quick-start.md) | [SPD Tool wiring](hardware/spd-tool-wiring.md)

Firmware release v0.1.0 exists. Normal users do not need the full repository just to flash and use the ESP32 SPD Tool or passive sniffer.

## Release Downloads

- [Release page v0.1.0](https://github.com/djchumpguy/ddr5-spd-diagnostic/releases/tag/v0.1.0)
- [ESP32 SPD Tool firmware ZIP](https://github.com/djchumpguy/ddr5-spd-diagnostic/releases/download/v0.1.0/esp32-spd-tool-v0.1.0.zip)
- [ESP32 DDR5 sniffer firmware ZIP](https://github.com/djchumpguy/ddr5-spd-diagnostic/releases/download/v0.1.0/esp32-ddr5-sniffer-v0.1.0.zip)

Use the release ZIP if you only want to flash and use the tool. Clone the repo if you want source code, documentation, examples, captures, or to modify/build firmware yourself.

## Firmware-Only Install Path

Download the correct ZIP, extract it, and read its `README-FLASHING.txt`. Flashing is separate from DDR5 wiring. Select the correct ESP32 serial port before flashing.

Do not download random build artifacts from old logs or local build folders. A proper ESP32 flashing package may need bootloader, partition table, and application binaries or a prepared flashing script, depending on how the release is packaged. Do not assume a single `firmware.bin` is enough unless the release instructions explicitly say so.

## Source-Build Path

Use the full repository if you want to inspect, modify, or build the firmware yourself. The active SPD Tool firmware is built from:

```text
firmware/esp32-spd-tool
```

Build with PlatformIO:

```bash
pio run -d firmware/esp32-spd-tool
```

Do not commit `.pio` build output or other generated PlatformIO artifacts to the repo.

## Upload Command

The upload command is separate from the build command. Run it only when you intentionally want to flash a connected ESP32:

```bash
pio run -d firmware/esp32-spd-tool -t upload
```

This documentation does not require running upload. Follow the release or PlatformIO instructions for your operating system, USB adapter, board, and port.
