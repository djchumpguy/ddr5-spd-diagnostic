# Active SPD/PMIC Tool Setup Guide

This is a practical setup route for recreating the active ESP32 SPD/PMIC tool
workflow. It documents one experimental path from this project, not a universal
DDR5 build manual.

## Minimum viable setup

The active SPD/PMIC setup does not need to copy the original prototype harness
exactly. A practical minimum is:

- ESP32 running the SPD tool firmware.
- Common ground.
- Safe I2C connection to DDR5 HSCL/HSDA, preferably through a PCA9306 or
  equivalent level shifter.
- Stable VIN_BULK / 5 V supply.
- Known HSA state.
- Read-only command workflow first.

GPIO32-controlled VIN_BULK switching is convenient, but not mandatory. A manual
stable 5 V VIN_BULK supply or physical power switch can be acceptable if the
setup is verified carefully.

GPIO27-controlled HSA is useful as an experiment, but not required. Manual HSA
switches or jumpers are often cleaner and easier to reason about.

PWR_EN is optional PMIC VR enable / DRAM rail enable control. It is not a
substitute for true VIN_BULK cold cycling.

## Cleaner recommended harness

A cleaner harness would separate the decisions physically and label them:

- HSA mode switch/jumper:
  - direct GND / hard-low
  - nominal 35.7 kΩ / ~36 kΩ HSA/HID resistor-selected strap
  - floating/high experimental
- VIN_BULK switch or ESP32-controlled MOSFET.
- Optional PWR_EN switch/GPIO control.
- PWR_GOOD indicator/input.
- Labeled HSCL/HSDA I2C level shifting.

Keep the active SPD/PMIC tool wiring separate from passive boot-sniffer wiring.
In the active harness, GPIO34 is PWR_GOOD. In the passive sniffer harness,
GPIO34 is HSCL sniff input.

## HSA behavior

| HSA condition | Practical wiring | Observed project behavior | Notes |
|---|---|---|---|
| Direct hard-low / ground | HSA tied directly to GND | SPD/HUB around `0x50` | Offline/direct-ground behavior |
| Resistor-selected strap | Nominal 35.7 kΩ / ~36 kΩ strap; measured ~34.4 kΩ in-circuit here | SPD/HUB around `0x53` | Normal tested harness behavior |
| Floating/high/released | HSA released/floating/high | SPD/HUB around `0x57` | Experimental high/floating behavior |

HSA is sampled at power-up. After any HSA change, perform a true VIN_BULK cold
cycle, then run `scan`, `autodetect`, and `mapall`. PWR_EN alone is not enough.

The nominal/reference HSA/HID strap value comes from the SPD hub reference
material tracked in [`../../sources/source-index.md`](../../sources/source-index.md).
The ~34.4 kΩ value was measured in-circuit on this project's adapter/harness.

## Read-only starting point

Start with read-only commands and record the HSA state, power-cycle method, and
observed addresses:

```text
status
scan
autodetect
mapall
powerdiag 20 100
timespd {spd_addr} 0x0000 32 50
timereg {spd_addr} 0x0B 16 50
pmicid {pmic_addr}
pmicdumpat {pmic_addr} 0x00 0x80
```

Use `autodetect` and `mapall` to resolve `{spd_addr}` and `{pmic_addr}` for the
current HSA and power state.

## Write-capable workflows

The project did use the active tool to write and verify a known-good SPD image
on a suspect module, but write-capable workflows are recovery work, not the
normal starting point. Save original dumps, confirm module match, record
HSA/address state, and assume a management-plane repair may still leave a
DRAM-side / training-path failure.

See [`11-spd-tool-advanced-workflows.md`](11-spd-tool-advanced-workflows.md).
