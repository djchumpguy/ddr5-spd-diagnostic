# Project Overview

## Goal

Build a reliable ESP32-based DDR5 UDIMM diagnostic and recovery setup for reading SPD hub state, reading/writing SPD EEPROM data when appropriate, observing PMIC behavior, and comparing a known-good module against a suspect module.

## Core components

- DDR5 UDIMM test harness
- ESP32 controller
- PCA9306 level shifter for 3.3 V ↔ 1.8 V I2C sideband access
- MOSFET high-side switch for full DIMM VIN_BULK control
- LED status panel
- Optional passive sniffer captures for motherboard boot sequences

## Working mental model

DDR5 sideband debug has three separate control layers that are easy to accidentally blur together:

1. **Full DIMM power** — VIN_BULK to module pins 1, 145, and 146.
2. **SPD hub enable/status** — PWR_EN and PWR_GOOD.
3. **HSA mode/address strap** — sampled by the SPD hub at power-up.

The harness now treats those separately. That matters because changing HSA after the hub has already sampled it does not change the active mode until the DIMM gets a real cold power cycle.

## Known addresses / modes from project notes

| Mode | HSA behavior | Expected hub address context |
|---|---|---|
| Normal/runtime | HSA released/high or resistor-selected | Runtime addresses depend on HID strap |
| Write/offline tester | HSA forced low at power-up | Offline direct-GND behavior, used for programming/recovery |

Project notes include previous observations of normal-mode hub activity around `0x57`/`0x53` depending on HSA/strap assumptions, and offline/write-mode access at `0x50`. Address notes should be treated as empirical harness state, not universal DDR5 law.

## Guiding rules

- Prefer read-only discovery first.
- Separate power cycling from hub enable toggling.
- Never write until the known-good reference has been captured and backed up.
- Log exact mode, HSA state, power-cycle sequence, address, register, and value for every test.
- Do not treat one successful read as proof of electrical robustness; run repeat/timing tests.
