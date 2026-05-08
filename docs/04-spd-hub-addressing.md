# SPD Hub Addressing Notes

## SPD5 hub basics

DDR5 SPD hubs expose EEPROM/register functions through an I2C/I3C-compatible sideband bus. The hub includes non-volatile SPD storage and volatile mode/status registers.

## HSA / HID concept

The HSA pin determines the hub's host ID / addressing context at power-up. Different HSA resistor/drive states map to different HID values.

Practical repo rule:

> Record the HSA state, power-cycle type, and scan result every time. Do not assume an address without logging the mode.

## Project-observed address contexts

| Context | Observed/project use | Notes |
|---|---|---|
| Offline/write direct-GND style | `0x50` | used in write/offline tester path |
| Normal/runtime | `0x53` / `0x57` in notes | depends on HSA strap/history and tool assumptions |
| PMIC local device through hub | `0x4F` / `0x4B` observed in notes | depends on local device translation and mode |

## Legacy access / MR11

MR11 (`0x0B`) is important because it controls the legacy addressing/page-pointer behavior:

| MR11 bits | Meaning |
|---|---|
| bit 3 | addressing mode: 0 = 1-byte, 1 = 2-byte |
| bits 2:0 | page pointer for 1-byte legacy addressing |

The current investigation notes showed both good and bad modules with MR11 = `0x00`, meaning the MR11 value itself was not the remaining difference.
