# ESP32 SPD Tool Firmware

Prototype PlatformIO/Arduino ESP32 firmware for the DDR5 SPD/PMIC diagnostic
tool.

This firmware is the working implementation for the Web UI and serial fallback
command interface described by the repository notes. The docs remain the current
interpretation of the lab setup; this firmware is the code that exercises that
setup on the bench.

## Status

This is prototype lab firmware, not a finished consumer repair product. It is
intended for controlled DDR5 SPD/PMIC sideband investigation with a known
harness, careful power handling, and read/compare-first workflows.

Dangerous write-capable commands are intentionally hidden behind advanced/danger
help and explicit confirmation gates. Keep that behavior intact unless you are
doing a deliberate lab-only experiment.

## Features

- Web UI over ESP32 SoftAP
- Serial fallback command interface at `115200`
- SPD/HUB scan, read, dump, compare, verify, and known-good reference capture
- PMIC ID, register dump, reference save, compare, and clear workflows
- Read-only auto-detection of SPD/HUB, PMIC, temperature, reserved, and unknown
  device roles
- VIN_BULK switch control support
- PWR_GOOD readiness/status reporting
- HSA GPIO release/ground experimental control
- Diagnostic timing and power-cycle helpers for repeatability checks

## Safety notes

- Treat every write-capable SPD or PMIC command as hazardous.
- Keep write commands hidden/gated and require explicit confirmation.
- Prefer read-only scan, dump, compare, and reference workflows.
- Verify DIMM power, wiring, HSA state, and `PWR_GOOD` before trusting bus results.
- HSA is sampled at power-up; change the strap, then cold-cycle VIN_BULK.
- Do not commit binaries, logs, flash dumps, private captures, or device-specific secrets.

## Build

From this directory:

```bash
pio run
```

## Upload

Connect the ESP32 target and run:

```bash
pio run -t upload
```

To open the serial monitor:

```bash
pio device monitor
```

## Web UI

The firmware starts an ESP32 SoftAP when Wi-Fi is enabled in `src/AppConfig.h`.

Default access point settings:

- SSID: `ESP32_SPD_TOOL`
- Password: `spdtool123`
- Web UI: `http://192.168.4.1/`

The Web UI exposes the same lab-oriented diagnostic surface as the serial
fallback: status, scan/detect, SPD/HUB dump and compare workflows, PMIC
reference workflows, power controls, and log output.

## Notes on included/excluded files

Included here:

- PlatformIO project configuration
- Arduino ESP32 firmware source
- Small helper scripts and standard PlatformIO placeholder directories

Excluded by policy:

- Firmware build outputs
- Captured SPD/PMIC dumps
- Serial logs and private lab captures
- Device-specific binaries or flash images
- Redistributed copyrighted datasheets
