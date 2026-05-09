# Register Watchlist

This page tracks registers that were useful during the DDR5 diagnostic project.

It is not a complete SPD hub or PMIC register map. It is a practical watchlist for repeatable lab notes, good-vs-bad comparisons, and recovery experiments.

## Current diagnosis context

The old MR12/MR13 protection-register mismatch is historical context, not the current active root-cause path.

Current project conclusion:

```text
Likely DRAM-side failure inferred from good-vs-bad motherboard boot sniffer divergence after SPD/PMIC communication appeared normal.
```

So this watchlist should be used to document and reproduce observations, not to imply that these registers are still suspected as the final cause.

## SPD hub registers

| Register | Name / use | Why it matters | Current interpretation |
|---:|---|---|---|
| `0x0B` | MR11 | Legacy addressing mode + page pointer | Useful context; matched in good/bad comparison; not active failure cause |
| `0x0C` | MR12 | Write-protection bits for SPD NVM blocks 0–7 | Historical mismatch only; later resolved / no longer active |
| `0x0D` | MR13 | Write-protection bits for SPD NVM blocks 8–15 | Historical mismatch only; later resolved / no longer active |
| `0x30` | Observed mode/state register | Changed between normal/offline captures | Useful for mode/context comparison |
| `0x31` | Observed mode/state register | Captured during good/bad comparison | Useful for mode/context comparison |
| `0x32` | Observed mode/state register | Captured during good/bad comparison | Useful for mode/context comparison |

## MR11 note

MR11 / register `0x0B` was useful for understanding legacy addressing/page state.

Historical observation:

| Register | Observed value |
|---|---:|
| MR11 / `0x0B` | `0x00` |

Project interpretation:

- 1-byte legacy addressing mode
- Page pointer set to page 0
- No observed good-vs-bad difference
- Not the active failure cause

## MR12/MR13 note

MR12/MR13 were important during the investigation because an early good-vs-bad comparison showed different write-protection state.

That hypothesis is now historical.

Current handling:

- Keep MR12/MR13 in the watchlist for documentation.
- Do not treat MR12/MR13 as the current root cause.
- Do not present MR12/MR13 cloning as the normal fix path.
- Only revisit MR12/MR13 if new captures show a fresh mismatch after a true VIN_BULK cold cycle.

## PMIC registers

| Register | Bits / area | Meaning in project notes | Current interpretation |
|---:|---|---|---|
| `0x08` | Status bits | Power-good / output status area | Read-only observation / fault context |
| `0x09` | Status bits | Warning / switchover / status area | Read-only observation / fault context |
| `0x0A` | Status bits | OV / PEC / parity status area | Read-only observation / fault context |
| `0x0B` | Status bits | UV / current-limit status area | Read-only observation / fault context |
| `0x14` | Bit 0 | Global status clear behavior in referenced PMIC docs | Write-capable; use caution |
| `0x2F` | Config bits | Output enables / write-protect related behavior depending on PMIC | Advanced; do not casually write |
| `0x32` | Bit 7 | VR Enable command bit in referenced PMIC docs | Optional PMIC VR / DRAM rail experiment, not normal SPD read path |
| `0x32` | Bit 4 | Fail_n behavior in referenced PMIC docs | Advanced PMIC behavior; use caution |

## PMIC VR enable note

PMIC register `0x32[7]` should be treated as:

```text
PMIC VR enable / DRAM rail enable
```

not:

```text
SPD hub enable
```

This is not required for basic SPD hub / PMIC sideband reads.

Use VR enable only when intentionally observing PMIC regulator / DRAM rail behavior.

Recommended sequence before VR enable:

```text
Apply/cycle VIN_BULK
Wait/check PWR_GOOD
Scan sideband bus
Read PMIC ID/status/registers
Only then consider VR enable
```

## PWR_GOOD context for register captures

PWR_GOOD is a readiness/wiring indicator in this harness.

Project observation:

```text
Every time PWR_GOOD was pulled LOW in this lab setup, the cause was a wiring/readiness issue.
```

If PWR_GOOD is LOW:

- Do not trust failed register reads.
- Do not conclude the SPD hub or PMIC is dead.
- Check wiring/readiness first.
- Record that PWR_GOOD was LOW in the capture.

## HSA/address context for register captures

Register captures are only meaningful if the HSA/power state is recorded.

Observed project address behavior:

| HSA condition at power-up | Observed hub address / behavior |
|---|---|
| Direct hard-low / tied to GND | `0x50` | Direct-GND / hard-low / offline tester behavior |
| Resistor-selected low strap / slot-ID style strap | `0x53` | HSA resistor strap / HID-selected observed harness state |
| Floating or high-ish HSA | `0x57` | HSA floating/high-ish observed behavior |

HSA is sampled at power-up. After changing HSA, perform a true VIN_BULK cold power cycle before capturing registers.

## Capture template

```text
Date/time:
Stick ID:
Tool / firmware version:

Mode:
  normal-read | hsa-address-test | direct-gnd-offline-tester | pmic-vr-experiment

VIN_BULK method:
  GPIO32 switch | manual switch | bench supply toggle | direct 5V feed | other

VIN_BULK cold cycle performed:
  yes/no

HSA state before power-up:
  hard-low | resistor strap | high/pull-up | floating | GPIO27 experiment | unknown

PWR_GOOD state:
  high/low/unknown

PWR_EN / VR enable state:
  low/off | high/released | not used | unknown

Hub address used:
PMIC/local-device address used:

Commands run:

SPD hub registers:
  MR11 0x0B =
  MR12 0x0C =
  MR13 0x0D =
  MR30 0x30 =
  MR31 0x31 =
  MR32 0x32 =

PMIC registers:
  0x08 =
  0x09 =
  0x0A =
  0x0B =
  0x14 =
  0x2F =
  0x32 =

Direct observations:

Inferences:

Notes:
```

## Write caution

Any write-capable register command should be gated.

Dangerous categories:

- SPD EEPROM writes
- SPD hub protection state changes
- MR12/MR13 protection cloning
- PMIC configuration writes
- PMIC VR enable / DRAM rail experiments

Suggested confirmation phrase:

```text
I UNDERSTAND THIS CAN DAMAGE THE MODULE
```

## Short version

```text
MR11 = useful page/address context; not active failure.
MR12/MR13 = historical mismatch; not current root cause.
PMIC 0x32[7] = optional VR/DRAM rail enable; not normal SPD read path.
PWR_GOOD LOW = check wiring/readiness before trusting registers.
Always record HSA + VIN_BULK cycle state with register captures.
Final diagnosis lives in boot-sniffer divergence, not this watchlist alone.
```
