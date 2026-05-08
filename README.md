# DDR5 SPD Diagnostic Notes

A practical lab notebook for DDR5 UDIMM SPD hub, PMIC, sideband bus, and ESP32-based recovery/debug tooling.

This repository is intended to collect the useful hard-won information from the DDR5 diagnostic project in one place:

- ESP32 harness wiring
- DDR5 UDIMM management pins
- SPD hub addressing behavior
- PMIC bring-up and status behavior
- good-vs-bad module investigation notes
- safe workflows for reading, dumping, comparing, and eventually writing SPD-related state

> ⚠️ This is hardware-debug documentation, not a finished consumer repair guide. DDR5 modules can be damaged by incorrect voltage, wiring, write mode, or PMIC commands. Treat every write operation like a loaded potato cannon.

## Current project state

The current harness design uses an ESP32 to control:

| Function | ESP32 GPIO | Notes |
|---|---:|---|
| I2C SDA | 21 | 3.3 V side of PCA9306 |
| I2C SCL | 22 | 3.3 V side of PCA9306 |
| HSA mode strap | 27 | LOW = write/offline mode, released = normal/high |
| Full DIMM VIN_BULK switch | 32 | HIGH = DIMM off, LOW = DIMM on |
| PWR_EN | 33 | hub enable control |
| PWR_GOOD | 34 | input-only status read |
| Ready LED | 26 | idle/ready |
| Processing LED | 25 | active command |
| Success LED | 19 | last command success |
| Failure LED | 23 | failed/not ready/PWR_GOOD blink |
| Danger/write LED | 18 | active write-danger state |

The important architectural correction is that **HSA mode changes require a true DIMM VIN_BULK power cycle**, not just toggling PWR_EN.

## Repository layout

```text
docs/                  Main technical notes and workflows
hardware/              Harness wiring, pin references, CSV tables
firmware-notes/        ESP32 command surface and behavior notes
investigations/        Good/bad stick findings and hypotheses
logs/                  Place future captures here; raw logs are ignored by default
assets/                Diagrams/photos/screenshots; raw bulky assets ignored by default
sources/               Source reference index; do not commit copyrighted PDFs by default
scripts/               Helper scripts for validation/export
```

## Start here

Read these first:

1. [`docs/00-project-overview.md`](docs/00-project-overview.md)
2. [`docs/01-safety-boundaries.md`](docs/01-safety-boundaries.md)
3. [`hardware/harness-wiring.md`](hardware/harness-wiring.md)
4. [`docs/04-spd-hub-addressing.md`](docs/04-spd-hub-addressing.md)
5. [`investigations/good-vs-bad-stick.md`](investigations/good-vs-bad-stick.md)

## What is intentionally not included

The public repo should probably **not** include copied datasheet PDFs unless their licenses clearly allow redistribution. Keep PDFs locally and cite/link them in [`sources/source-index.md`](sources/source-index.md).

## Status

This is an information repo seed. Firmware source can be added later as a separate `firmware/` directory once the docs stabilize.
