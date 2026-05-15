# Flashing The ESP32 SPD Tool

[Back to README](../README.md) | [Quick start](quick-start.md) | [SPD tool wiring](hardware/spd-tool-wiring.md)

Most users should not need the full repository just to flash and use the ESP32 SPD Tool.

## Firmware-Only Install Path

Future beginner releases should provide a prepared firmware package through GitHub Releases or a release ZIP. Use that package if you only want to flash the ESP32 and use the Web UI/command surface.

Do not download random build artifacts from old logs or local build folders. A proper ESP32 flashing package may need bootloader, partition table, and application binaries or a prepared flashing script, depending on how the release is packaged. Do not assume a single `firmware.bin` is enough unless the release instructions explicitly say so.

## Source-Build Path

Use the full repository if you want to inspect, modify, or build the firmware yourself. The active SPD tool firmware is built from:

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
