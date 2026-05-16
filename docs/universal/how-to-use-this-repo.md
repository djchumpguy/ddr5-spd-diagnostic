# How to Use This Repo

This guide gives a practical route through the DDR5 SPD/PMIC diagnostic project
without pretending the repo is a polished universal build manual.

## What this repo is

This is a DIY DDR5 SPD/PMIC diagnostic and passive boot-sniffer project. It is
built around ESP32 firmware, real wiring experiments, captured logs, and
good-vs-bad evidence from an actual failed DDR5 module.

The repo documents one working experimental path and the lessons learned from
it. The wiring, firmware, and workflow are starting points for careful lab work,
not the only possible implementation.

## What this repo is not

- Not a professional DDR5 analyzer.
- Not a universal safe repair guide.
- Not vendor documentation.
- Not a guarantee that a repaired SPD means a working DIMM.

## Choose your path

| Goal | Start here |
|---|---|
| Understand the project | [`../universal/00-project-overview.md`](../universal/00-project-overview.md) |
| Build/use the active SPD/PMIC tool | [`../spd-tool/setup-guide.md`](../spd-tool/setup-guide.md) |
| Wire the active SPD/PMIC harness | [`../../hardware/spd-tool/harness-wiring.md`](../../hardware/spd-tool/harness-wiring.md) |
| Use the passive boot sniffer | [`../sniffer/10-boot-sniffer.md`](../sniffer/10-boot-sniffer.md) |
| Wire the passive sniffer | [`../../hardware/sniffer/passive-boot-sniffer-wiring.md`](../../hardware/sniffer/passive-boot-sniffer-wiring.md) |
| Review evidence examples | [`../examples/README.md`](../examples/README.md) |
| Review the final diagnosis | [`../../investigations/final-diagnosis-dram-failure.md`](../../investigations/final-diagnosis-dram-failure.md) |

## Minimum practical active SPD/PMIC setup

A minimal setup does not need to copy the original prototype harness exactly.
For active SPD/PMIC work, the practical minimum is:

- ESP32 running the SPD tool firmware.
- Common ground between the ESP32/tooling and the DDR5 module setup.
- Direct adapter/breakout wiring from ESP32 GPIO21/GPIO22 to DDR5 HSDA/HSCL.
- 10 kOhm pull-ups to 3.3 V for PWR_EN and PWR_GOOD.
- Stable VIN_BULK / 5 V supply.
- Known HSA state.
- Read-only command workflow first.

The proven basic direct-read setup did not require a PCA9306 or external
SDA/SCL pull-ups. Treat those as optional troubleshooting or conservative-design
choices for other harnesses.

GPIO32-controlled VIN_BULK switching is convenient but not mandatory. Manual
stable 5 V VIN_BULK or a physical switch can be acceptable if the setup is
verified carefully. GPIO27-controlled HSA is convenient/experimental but not
mandatory. Manual switches or jumpers can be cleaner and easier to reason
about.

## Cleaner recommended harness

A cleaner harness would use:

- HSA mode switch/jumper:
  - direct GND / hard-low
  - nominal 35.7 kΩ / ~36 kΩ resistor-selected strap
  - floating/high experimental
- VIN_BULK switch or ESP32-controlled MOSFET.
- Required PWR_EN pull-up plus optional GPIO33 switch/control.
- PWR_GOOD pull-up and GPIO34 indicator/input.
- Optional labeled I2C interface/protection if your harness needs it.
- Clear separation between active tool wiring and passive sniffer wiring.

## HSA and address behavior

| HSA condition | Practical wiring | Observed project behavior | Notes |
|---|---|---|---|
| Direct hard-low / ground | HSA tied directly to GND | SPD/HUB around `0x50` | Offline/direct-ground behavior |
| Resistor-selected strap | Nominal 35.7 kΩ / ~36 kΩ strap; measured ~34.4 kΩ in-circuit here | SPD/HUB around `0x53` | Normal tested harness behavior |
| Floating/high/released | HSA released/floating/high | SPD/HUB around `0x57` | Experimental high/floating behavior |

Do not assume addresses blindly. Always cold-cycle VIN_BULK after HSA changes,
then run `scan`, `autodetect`, and `mapall`.

PWR_EN is not a substitute for a true VIN_BULK cold cycle.

## Read-only first workflow

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

Use autodetect and mapall to resolve {spd_addr} and {pmic_addr} for the current
HSA and power state.

## Recovery/write workflow caution

This project did recover corrupted SPD contents by writing a known-good SPD
image to the suspect module, then correcting MR12/MR13 protection state. Write
workflows are not the starting point.

Before writing:

- Save the original dump if possible.
- Confirm the module match.
- Capture a known-good reference.
- Confirm address/HSA state.
- Understand that a management-plane repair may not fix DRAM-side failure.

Advanced write-capable workflows are documented in
[`../spd-tool/11-spd-tool-advanced-workflows.md`](../spd-tool/11-spd-tool-advanced-workflows.md).
MR12/MR13 remains historical/secondary context, not the current root cause.

## Passive sniffer workflow

The passive boot sniffer uses separate firmware and a separate harness from the
active SPD/PMIC tool. In this setup, GPIO34/GPIO35 are passive HSCL/HSDA inputs,
not active-tool PWR_GOOD wiring.

Use a shared ground, power the ESP32 separately if a volatile RAM-only capture
must survive PC power changes, arm before motherboard power-on, and dump over
Bluetooth/USB before power loss or reset. Sniffer captures do not survive ESP32
reset or power loss.

This is different from active-tool reference captures such as `capturegood` and
`capturepmic`, which are stored in ESP32 flash/NVS and persist until cleared or
overwritten.

See [`../sniffer/10-boot-sniffer.md`](../sniffer/10-boot-sniffer.md).

## Evidence interpretation

The repo evidence supports this sequence:

- The SPD/HUB/PMIC management side was corrupted or mismatched initially.
- Repair/validation made the management side stable.
- Boot still failed.
- Good-vs-bad sniffer divergence remained.
- The current conclusion is likely DRAM-side / training-path failure after
  SPD/HUB/PMIC management-plane repair and validation.

Evidence and diagnosis links:

- [`../examples/README.md`](../examples/README.md)
- [`../../investigations/final-diagnosis-dram-failure.md`](../../investigations/final-diagnosis-dram-failure.md)
