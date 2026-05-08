# Register Watchlist

## SPD hub registers

| Register | Name/use | Why it matters |
|---|---|---|
| `0x0B` | MR11 | legacy addressing mode + page pointer |
| `0x0C` | MR12 | write protection bits for SPD NVM blocks 0-7 |
| `0x0D` | MR13 | write protection bits for SPD NVM blocks 8-15 |
| `0x30` | observed mode/state register | changed between normal/offline captures |
| `0x31` | observed mode/state register | captured during good/bad comparison |
| `0x32` | observed mode/state register | captured during good/bad comparison |

## PMIC registers

| Register | Bits | Meaning in project notes |
|---|---|---|
| `0x08` | status bits | power-good / output status area |
| `0x09` | status bits | warning/switchover/status area |
| `0x0A` | status bits | OV / PEC / parity status area |
| `0x0B` | status bits | UV/current limit status area |
| `0x14` | bit 0 | global status clear behavior in referenced PMIC docs |
| `0x2F` | config bits | output enables / write-protect related behavior depending PMIC |
| `0x32` | bit 7 | VR Enable command bit in referenced PMIC docs |
| `0x32` | bit 4 | Fail_n behavior in referenced PMIC docs |

## Capture template

```text
Date/time:
Stick ID:
Mode: normal | write/offline
HSA state before power-up:
Full DIMM power cycle performed: yes/no
PWR_EN state:
PWR_GOOD state:
Hub address used:
PMIC address used:
Command/tool version:

Registers:
  MR11 0x0B =
  MR12 0x0C =
  MR13 0x0D =
  MR30 0x30 =
  MR31 0x31 =
  MR32 0x32 =

Notes:
```
