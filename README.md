# DDR5 SPD Diagnostic Notes

A practical lab notebook for DDR5 UDIMM SPD hub, PMIC, sideband bus, and ESP32-based recovery/debug tooling.

This repository collects the useful hard-won information from the DDR5 diagnostic project in one place:

- ESP32 harness wiring
- DDR5 UDIMM management pins
- SPD hub addressing behavior
- HSA strap behavior and address changes
- PMIC bring-up and status behavior
- Good-vs-bad module investigation notes
- Safe workflows for reading, dumping, comparing, and eventually writing SPD-related state

> ⚠️ This is hardware-debug documentation, not a finished consumer repair guide. DDR5 modules can be damaged by incorrect voltage, wiring, write mode, or PMIC commands. Treat every write operation like a loaded potato cannon.

## Current project state

The current diagnostic setup is centered around:

- ESP32 sideband access
- VIN_BULK power control or manual 5 V power cycling
- Manual HSA strap testing
- PWR_GOOD readiness checking
- Read/dump/compare-first diagnostic workflow

### Required / core signals

| Function | ESP32 GPIO | Status | Notes |
|---|---:|---|---|
| I2C SDA | 21 | Required | DDR5 sideband SDA. Direct 3.3 V lab wiring worked; PCA9306 level shifting remains the safer reference design. |
| I2C SCL | 22 | Required | DDR5 sideband SCL. Direct 3.3 V lab wiring worked; PCA9306 level shifting remains the safer reference design. |
| VIN_BULK control | 32 | Recommended convenience | Controls optional MOSFET high-side VIN_BULK switch. HIGH = DIMM off, LOW = DIMM on. Direct stable 5 V or a manual switch can also be used. |
| PWR_EN / VR enable | 33 | Optional | PMIC VR enable / DRAM rail enable. Not required for basic SPD/PMIC sideband access. |
| PWR_GOOD | 34 | Recommended readiness indicator | Useful readiness/wiring signal. If LOW, check harness/power/readiness before trusting bus results. |

### HSA strap testing

HSA was tested experimentally through ESP32 GPIO27, but the practical bench-test method became **manual HSA strapping**.

| HSA method | Status | Notes |
|---|---|---|
| Manual strap | Preferred for bench testing | Easier to reason about. Change strap, cold-cycle VIN_BULK, then scan/read. |
| ESP32 GPIO27 control | Optional experiment | Useful during testing, but not required for the basic harness workflow. |

Important HSA rule:

> HSA is sampled at power-up. Changing HSA requires a true DIMM VIN_BULK power cycle. Toggling PWR_EN alone is not enough.

Observed address behavior changed depending on HSA state at power-up:

| HSA condition at power-up | Observed address / behavior | Notes |
|---|---|---|
| Direct hard-low / tied to GND | `0x50` | Direct-GND / hard-low / offline tester behavior. |
| Resistor-selected low strap / slot-ID style strap | `0x53` | HSA resistor strap / HID-selected observed harness state. |
| Floating or high-ish HSA | `0x57` | HSA floating/high-ish observed behavior. |

## Key project corrections

- The technically conservative I2C wiring uses a PCA9306 or equivalent level shifter.
- The actual lab setup also worked with ESP32 3.3 V open-drain I2C connected directly to DIMM HSDA/HSCL.
- Direct 3.3 V sideband wiring is documented as a lab-proven shortcut for this setup, not a universal DDR5 rule.
- GPIO32 VIN_BULK switching is convenient, not mandatory. Direct stable 5 V or a manual switch can power VIN_BULK.
- PWR_EN should be treated as optional PMIC VR enable / DRAM rail enable, not SPD hub enable.
- PWR_GOOD LOW was observed as a wiring/readiness issue in this lab. Do not treat it as immediate proof of bad SPD/PMIC/DRAM.
- HSA behavior must be recorded with the strap condition and whether a full VIN_BULK cold reset was performed.
- MR12/MR13 mismatch is historical investigation context, not the current active root cause.
- Final diagnosis is likely DRAM-side failure inferred from good-vs-bad motherboard boot sniffer divergence after SPD/PMIC communication appeared normal.

## Repository layout

```text
docs/                  Main technical notes and workflows
hardware/              Harness wiring, pin references, CSV tables
firmware-notes/        ESP32 command surface and behavior notes
investigations/        Good/bad stick findings and historical hypotheses
logs/                  Place future sanitized captures here; raw logs are ignored by default
assets/                Diagrams/photos/screenshots; raw bulky assets ignored by default
sources/               Source reference index; do not commit copyrighted PDFs by default
scripts/               Helper scripts for validation/export
```

## Start here

Read these first:

- `docs/00-project-overview.md`
- `docs/01-safety-boundaries.md`
- `hardware/harness-wiring.md`
- `docs/04-spd-hub-addressing.md`
- `investigations/final-diagnosis-dram-failure.md`
- `investigations/good-vs-bad-stick.md`

## What is intentionally not included

The public repo should probably not include copied datasheet PDFs unless their licenses clearly allow redistribution.

Keep PDFs locally and cite/link them in `sources/source-index.md`.

## Status

This is an information repo seed. Firmware source can be added later as a separate `firmware/` directory once the docs stabilize.
