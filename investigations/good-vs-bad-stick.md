# Good vs Bad DDR5 Stick Investigation

## Status

This page is a **historical investigation record**, not the current active diagnosis.

Earlier testing found a mismatch in SPD hub write-protection registers **MR12/MR13** between the known-good stick and the recovered bad stick. That mismatch was originally treated as a possible cause of motherboard initialization failure.

**Current project state:** MR12 and MR13 are no longer considered an active mismatch. Later testing brought the good and bad sticks into matching MR12/MR13 state. Do not treat the old MR12/MR13 mismatch as the current root cause unless new captures show it diverging again.

## Test context

The comparison was performed using the ESP32 DDR5 diagnostic harness with:

- ESP32 SPD/PMIC tool
- Manual HSA strap testing, with GPIO27 HSA control treated as historical/optional experiment
- VIN_BULK cold power cycling, either switched or manual depending harness setup
- `PWR_EN` treated as optional PMIC VR / DRAM rail enable, not SPD hub enable
- `PWR_GOOD` readiness/wiring status readback
- SPD hub register reads
- PMIC register reads
- `timereg` stability tests
- Normal mode and direct-GND/offline tester mode comparisons

## Current evidence table

| Area | Result |
|---|---|
| SPD payload | Matched known-good after recovery write |
| SPD read stability | Pass |
| Hub register reads | Pass |
| PMIC register reads | Pass |
| Mode switching | Pass |
| Address remap | Pass |
| MR11 | Matched: `0x00` |
| MR12/MR13 | Historical mismatch; later resolved / no longer active |

## Historical protection-register observation

The following values are preserved because they were useful during the investigation. They should not be read as the current state of the sticks.

### Good stick — historical capture

| Register | Value |
|---|---:|
| MR12 / `0x0C` | `0xFF` |
| MR13 / `0x0D` | `0x3C` |

Interpreted protection map:

| Block range | State |
|---|---|
| 0–7 | Protected |
| 8–9 | Writable |
| 10–13 | Protected |
| 14–15 | Writable |

### Bad stick — historical capture

> Historical capture only. Later testing resolved the MR12/MR13 mismatch, so these values are preserved as an earlier observation, not current state.

| Register | Value |
|---|---:|
| MR12 / `0x0C` | `0x00` |
| MR13 / `0x0D` | `0x00` |

Interpreted protection map at that time:

| Block range | State |
|---|---|
| 0–15 | Writable |

## Historical working hypothesis

At this stage of the investigation, the bad stick was suspected to fail motherboard initialization because its SPD hub write-protection/finalization state did not match the known-good module, even though the recovered SPD payload itself matched.

This hypothesis is now historical. Later project state says MR12/MR13 are no longer an active mismatch, so this should not be treated as the current root cause.

## Addressing notes

Observed address behavior evolved during the project. Keep these notes explicit so future debugging does not mix old assumptions with later findings.

| Address | Meaning / context |
|---|---|
| `0x50` | Direct-GND / hard-low / offline tester access observed during programming/recovery work |
| `0x53` | HSA resistor strap / HID-selected observed harness state with the actual harness mode wiring |
| `0x57` | HSA floating/high-ish observed behavior; do not assume this is the current default |

## MR11 note

MR11 did not appear to explain the failure in this investigation.

| Register | Observed value |
|---|---:|
| MR11 / `0x0B` | `0x00` |

Interpreted behavior:

- Legacy 1-byte addressing mode
- Page pointer set to page 0
- No observed good-vs-bad difference

## What this page is still useful for

This page is useful as:

- A record of the earlier protection-register mismatch
- A reminder that SPD payload matching alone may not be the whole story
- A warning that mode strap / HSA state changes require true power cycling
- A template for future good-vs-bad comparisons
- A breadcrumb trail showing one hypothesis that was later retired

## Do not forget

MR12/MR13 were later noted as no longer the active issue in project memory. Keep this page as historical investigation context, not as unquestioned current truth.

If new captures show MR12/MR13 diverging again, update this file with:

1. Capture date
2. Stick identity
3. Harness mode
4. HSA state
5. DIMM power-cycle method
6. Raw MR11/MR12/MR13 values
7. Whether the mismatch reproduces after a true cold power cycle
