# ESP32 Firmware Command Surface Notes

This repo does not yet include the firmware source, but the docs should preserve the command intent.

## Command groups

### Power / mode control

| Command idea | Purpose |
|---|---|
| `pwr on` / `pwr off` | control GPIO32 full DIMM power switch |
| `hsa normal` | release HSA / normal mode selection before cold-cycle |
| `hsa write` | force HSA low / offline tester selection before cold-cycle |
| `pwr_en on/off` | control GPIO33 hub enable |
| `powerdiag` | report GPIO states and PWR_GOOD |

### Discovery / read-only

| Command idea | Purpose |
|---|---|
| `scan` | scan active bus state |
| `mapall` | current-mode read-only address mapping |
| `reg` | generic register read/write command, gated for writes |
| `biosread` | BIOS-style one-byte legacy read |
| `biosdump` | BIOS-style dump path |
| `pmicid` | PMIC identification/read sanity |
| `pmicdump` | PMIC register dump |
| `timespd` | SPD timing/stability test |
| `timereg` | register timing/stability test |

### Dangerous / write-capable

| Command idea | Purpose |
|---|---|
| `capturegood` | store known-good SPD to NVS |
| `compare` | compare current stick against stored good SPD |
| `writegood` | write stored good SPD payload to target stick |
| `verifygood` | verify target matches stored good SPD |
| `autofix` | should remain strongly gated; never casual |
| `protect clone` | future MR12/MR13 protection map clone command |

## Firmware naming rule

Avoid ambiguous labels like `power` when there are multiple power-ish controls. Prefer:

- `dimm_power` for GPIO32/VIN_BULK switch
- `hub_enable` for GPIO33/PWR_EN
- `hsa_mode` for GPIO27 strap state
- `pwr_good` for GPIO34 status
