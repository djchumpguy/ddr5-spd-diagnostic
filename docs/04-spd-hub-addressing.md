# SPD Hub Addressing Notes

## Summary

DDR5 SPD hub addressing depends on the HSA condition sampled at power-up.

In this project, the same stick appeared at different hub addresses depending on whether HSA was hard-low, resistor-strapped, floating, or high-ish during a true VIN_BULK cold power-up.

Do not compare scan results unless the HSA strap condition and power-cycle method are recorded.

## Critical rule

HSA is sampled at power-up.

Changing the HSA strap while the DIMM is already powered does not reliably change the active hub address/state.

After changing HSA:

```text
Set HSA strap
→ Turn DIMM VIN_BULK off
→ Wait for full power-down
→ Turn DIMM VIN_BULK on
→ Scan / read
```

PWR_EN alone is not enough. Use a real VIN_BULK cold-cycle through the DIMM power switch.

## Observed project address behavior

| HSA condition at power-up | Observed hub address / behavior | Project interpretation |
|---|---|---|
| Direct hard-low / tied to GND | `0x50` | Direct-GND / hard-low / offline tester behavior; write-protect override path |
| Resistor-selected low strap / slot-ID style strap | `0x53` | HSA resistor strap / HID-selected observed harness state |
| Floating or high-ish HSA | `0x57` | HSA floating/high-ish observed behavior |

## Current preferred bench workflow

The practical bench method is manual HSA strapping, not GPIO-driven HSA switching.

```text
1. Power DIMM off with GPIO32
2. Set HSA strap manually
3. Power DIMM on with GPIO32
4. Wait for PWR_GOOD / bus readiness
5. Scan the sideband bus
6. Record observed hub address
```

## Optional GPIO27 HSA experiment

GPIO27 was tested as an ESP32-controlled HSA method, but it is not required for normal bench testing.

Experimental wiring:

```text
Pin 148 HSA ---- 1k ---- ESP32 GPIO27
Pin 148 HSA ---- 100k ---- 3.3V
```

| GPIO27 state | HSA result | Intended test mode |
|---|---|---|
| LOW / output-low | HSA forced low | Direct-GND / offline tester test |
| INPUT / released | Pulled high by 100k | Normal / high-HSA test |

Because HSA must be re-sampled at power-up, GPIO27 control still requires a full VIN_BULK power cycle after each state change.

## Address notes

### `0x50`

`0x50` was associated with the direct-GND / offline tester style HSA condition.

This mode is useful for recovery/programming style work, but it should not be confused with normal motherboard-runtime behavior.

### `0x53`

`0x53` became the HSA resistor strap / HID-selected observed harness state when using the practical HSA strap behavior.

Treat `0x53` as the current expected hub address for the documented normal test setup unless a new capture shows otherwise.

### `0x57`

`0x57` was observed earlier with floating/high-ish HSA behavior.

Keep it as historical context, but do not assume `0x57` is the current normal default.

## Why this matters

Without recording HSA state, bus scans can look contradictory:

```text
Same stick
Different HSA strap at power-up
Different address
```

That can make a working hub look like it disappeared or moved. The address did not randomly change; the power-up strap condition changed.

## What to record with every capture

For every scan, dump, PMIC read, or recovery attempt, record:

- Stick identity
- HSA condition
- Whether HSA was manually strapped or GPIO-controlled
- Whether VIN_BULK was fully cold-cycled
- PWR_EN state
- PWR_GOOD state
- Observed hub address
- Observed PMIC/local-device address
- Whether the scan was normal mode or direct-GND/offline tester mode

## Local device / PMIC note

The PMIC is accessed through the SPD hub sideband path and may appear at different host-side addresses depending on hub/HSA routing behavior.

During this project, PMIC-related addresses observed included:

| Address | Context |
|---|---|
| `0x4F` | Common PMIC/local-device style address seen during testing |
| `0x4B` | Later observed PMIC address in normal-mode scans |

Do not treat PMIC address differences as standalone proof of failure without recording the HSA strap and hub address first.

## MR11 page pointer note

MR11 / register `0x0B` controls legacy SPD addressing mode and page pointer behavior.

Historical comparison showed MR11 matched between good and bad sticks:

| Register | Observed value |
|---|---:|
| MR11 / `0x0B` | `0x00` |

Interpretation used during the project:

- 1-byte legacy addressing mode
- Page pointer set to page 0
- No observed good-vs-bad difference

MR11 was not considered the active failure cause.

## Short version for future debugging

```text
If the hub address looks wrong:
1. Stop.
2. Record HSA strap condition.
3. Fully power-cycle VIN_BULK.
4. Scan again.
5. Only then compare results.
```

The address is part of the experiment state, not just a fixed property of the stick.
