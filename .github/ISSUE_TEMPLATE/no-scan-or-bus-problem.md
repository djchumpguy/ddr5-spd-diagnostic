---
name: No scan or bus problem
about: The SPD Tool cannot find expected devices or bus reads fail
title: "[Bus] "
labels: bus, hardware
assignees: ""
---

## Firmware And Board

- Firmware package/version:
- ESP32 board:
- Web UI or serial command path:

## Harness And Power

- Wiring/harness type:
- DDR5 adapter/breakout used:
- DIMM tested:
- 5 V VIN_BULK power source:
- ESP32 power source:
- Are ESP32 ground and DIMM power ground shared?
- PWR_EN pull-up present?
- PWR_GOOD pull-up present and monitored on GPIO34?
- HSA strap/config:

## Command Output

Please paste output from:

```text
status
scan
autodetect
health
```

Add `mapall`, `powerdiag`, or screenshots if available.

## Screenshots

Attach Web UI screenshots or photos if they help.

## What You Expected

Describe what devices or addresses you expected.

## What Happened

Describe the actual scan/read behavior.
