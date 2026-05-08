# Good vs Bad Stick Investigation

## Summary

The current known persistent difference was captured in SPD hub write-protection registers, not in basic bus stability or SPD payload after recovery.

## Test context

- ESP32 SPD tool
- HSA strap control
- DIMM_PWR cycling
- timereg stability tests
- both normal and write/offline modes

## Current evidence table

| Area | Result |
|---|---|
| SPD payload | matched known-good after write |
| SPD read stability | pass |
| Hub register reads | pass |
| PMIC register reads | pass |
| Mode switching | pass |
| Address remap | pass |
| MR11 | matched: `0x00` |
| MR12/MR13 | differed between sticks |

## Protection map comparison

### Good stick

| Register | Value |
|---|---:|
| MR12 / `0x0C` | `0xFF` |
| MR13 / `0x0D` | `0x3C` |

Interpreted protection:

| Block range | State |
|---|---|
| 0-7 | protected |
| 8-9 | writable |
| 10-13 | protected |
| 14-15 | writable |

### Bad stick

| Register | Value |
|---|---:|
| MR12 / `0x0C` | `0x00` |
| MR13 / `0x0D` | `0x00` |

Interpreted protection:

| Block range | State |
|---|---|
| 0-15 | writable |

## Working hypothesis

The bad stick may still fail motherboard initialization because the SPD protection/finalization state does not match the expected factory state, even though the recovered SPD payload itself matches.

## Do not forget

MR12/MR13 were later noted as no longer the active issue in project memory. Keep this page as historical investigation context, not as unquestioned current truth. If new captures show MR12/MR13 now match, update this file and move the hypothesis to `resolved/`.
