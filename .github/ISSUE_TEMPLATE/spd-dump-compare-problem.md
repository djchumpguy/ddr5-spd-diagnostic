---
name: SPD dump or compare problem
about: SPD dump, reference, compare, or readback results look wrong
title: "[SPD] "
labels: spd, diagnostics
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
dump
spddumpstate
compare
```

If this involves a diagnostic reference or tweak checkpoint, include the exact
reference/checkpoint command output.

## Screenshots

Attach Web UI screenshots or photos if they help.

## What You Expected

Describe the expected dump, compare, or reference behavior.

## What Happened

Describe the mismatch, failure, or confusing output.
