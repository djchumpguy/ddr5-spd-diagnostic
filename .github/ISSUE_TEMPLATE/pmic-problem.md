---
name: PMIC problem
about: PMIC ID, dump, compare, or power/readiness behavior looks wrong
title: "[PMIC] "
labels: pmic, diagnostics
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
pmicid
pmicdump
comparepmic
```

Include `powerdiag` output if this is a readiness or PWR_GOOD problem.

## Screenshots

Attach Web UI screenshots or photos if they help.

## What You Expected

Describe the expected PMIC behavior.

## What Happened

Describe the actual PMIC ID, dump, compare, or readiness behavior.
