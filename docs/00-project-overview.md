# Project Overview

## Goal

Build a reliable ESP32-assisted DDR5 UDIMM diagnostic and recovery setup for:

- Reading SPD hub state
- Reading SPD EEPROM data
- Writing SPD EEPROM data only when appropriate
- Observing PMIC behavior
- Comparing a known-good module against a suspect module
- Capturing practical bench notes so future debugging does not restart from zero

This is a hardware-debug notebook and workflow repo, not a polished consumer repair guide.

## Core components

| Component | Role | Status |
|---|---|---|
| DDR5 UDIMM test harness | Breaks out management pins and VIN_BULK power | Core hardware |
| ESP32 controller | I2C master, optional VIN_BULK switch control, status readback, firmware test platform | Core hardware |
| VIN_BULK power feed | Supplies DIMM pins 1, 145, and 146 | Required |
| MOSFET high-side switch | Optional automated VIN_BULK cold power cycling | Recommended convenience |
| Manual HSA strap | Practical bench method for selecting HSA state before power-up | Preferred for testing |
| PCA9306 level shifter | Conservative 3.3 V ↔ lower-voltage sideband interface | Safer reference design |
| Direct ESP32 I2C wiring | 3.3 V ESP32 SDA/SCL directly to HSDA/HSCL | Worked in this lab setup |
| Passive sniffer | Captures motherboard boot sideband traffic | Separate investigation tool |

## Working mental model

DDR5 sideband debug has several separate control layers that are easy to accidentally blur together:

1. **VIN_BULK power** — 5 V feed to module pins 1, 145, and 146.
2. **Full VIN_BULK cold cycling** — required after changing HSA strap state.
3. **PWR_EN / VR enable** — PMIC regulator / DRAM rail enable; not SPD hub enable.
4. **PWR_GOOD** — useful readiness/wiring indicator before trusting bus results.
5. **HSA strap state** — sampled by the SPD hub at power-up.
6. **Sideband bus access** — HSDA/HSCL through either level-shifted or direct open-drain wiring.
7. **Local devices behind the hub** — PMIC and other sideband clients reached through hub behavior.

The harness treats those separately.

The biggest practical rule:

> Changing HSA after the hub has already sampled it does not reliably change the active mode or address. Change the HSA strap, then perform a real VIN_BULK cold power cycle.

That cold cycle can be done by GPIO32-controlled switching, a manual inline switch, a bench supply toggle, or removing/restoring the 5 V feed.

PWR_EN is useful for PMIC regulator / DRAM rail experiments, but PWR_EN is not a substitute for removing/restoring VIN_BULK.

## HSA testing model

The project tested HSA control through ESP32 GPIO27, but manual HSA strapping became the preferred bench workflow.

| HSA method | Project status | Notes |
|---|---|---|
| Manual strap | Preferred | Easier to reason about during bench tests. Set strap, cold-cycle VIN_BULK, then scan. |
| ESP32 GPIO27 control | Optional experiment | Useful as a control experiment, but not required for normal bench testing. |

Manual HSA workflow:

```text
Remove VIN_BULK
→ Set HSA strap manually
→ Restore VIN_BULK
→ Wait/check PWR_GOOD
→ Scan bus
→ Record observed address
```

## Known addresses / modes from project notes

Address behavior is empirical harness state. Do not treat these as universal DDR5 constants.

| HSA condition at power-up | Observed hub address / behavior | Notes |
|---|---|---|
| Direct hard-low / tied to GND | `0x50` | Offline / write-programmer style behavior; write-protect override path |
| Resistor-selected low strap / slot-ID style strap | `0x53` | Later/current normal-runtime observation for this harness |
| Floating or high-ish HSA | `0x57` | Older observation; useful context, not the current default assumption |

Project notes also include PMIC/local-device addresses such as `0x4F` and later `0x4B`. Do not interpret PMIC address changes without also recording the HSA strap, hub address, and power-cycle method.

## I2C sideband wiring model

The technically conservative setup uses a PCA9306 or equivalent level shifter between ESP32 3.3 V I2C and the DIMM sideband pins.

However, the actual lab setup also worked with direct ESP32 3.3 V open-drain I2C wiring:

| ESP32 | DIMM |
|---|---|
| GPIO21 SDA | HSDA / pin 5 |
| GPIO22 SCL | HSCL / pin 4 |

Important distinction:

- Direct 3.3 V sideband wiring worked in this harness.
- PCA9306 level shifting remains the safer reference design.
- Direct 3.3 V wiring is a lab-proven shortcut for this setup, not a blanket DDR5 rule.
- SDA/SCL must be treated as open-drain I2C lines, not push-pull GPIO outputs.

## Current investigation status

The old MR12/MR13 protection-register mismatch is historical context, not the current active root-cause hypothesis.

Current project memory says:

- MR12 and MR13 now match between good and bad sticks.
- The old mismatch should not be treated as active unless new captures show it diverging again.
- The good-vs-bad investigation page is preserved as a lab history record.
- The final/current diagnosis is likely DRAM-side failure inferred from good-vs-bad motherboard boot sniffer divergence after SPD/PMIC communication appeared normal.

## Guiding rules

1. Prefer read-only discovery first.
2. Separate VIN_BULK power cycling from PWR_EN / VR enable toggling.
3. Record the HSA strap condition before trusting address scans.
4. After changing HSA, perform a real VIN_BULK cold power cycle.
5. Wait/check PWR_GOOD before trusting scan/read results.
6. If PWR_GOOD is LOW, check wiring/readiness before blaming silicon.
7. Never write until the known-good reference has been captured and backed up.
8. Log exact mode, HSA state, power-cycle sequence, address, register, and value for every test.
9. Do not treat one successful read as proof of electrical robustness; run repeat/timing tests.
10. Treat every write operation like a loaded potato cannon.
