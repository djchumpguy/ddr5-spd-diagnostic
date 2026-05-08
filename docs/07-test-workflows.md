# Test Workflows

## Read-only baseline workflow

1. Confirm wiring against `hardware/harness-wiring.md`.
2. Start in normal mode.
3. Cold-cycle VIN_BULK.
4. Confirm PWR_GOOD behavior.
5. Run bus scan.
6. Dump SPD hub registers.
7. Dump SPD EEPROM.
8. Dump PMIC registers.
9. Run timing/stability test.
10. Save logs under `logs/YYYY-MM-DD-stick-id/` locally.

## Good-stick capture workflow

1. Label stick physically.
2. Read-only normal-mode dump.
3. Read-only write/offline-mode dump.
4. Save all logs and EEPROM dumps.
5. Hash the dumps.
6. Do not write to the good stick.

## Bad-stick comparison workflow

1. Read-only normal-mode dump.
2. Read-only write/offline-mode dump.
3. Compare SPD EEPROM payload to known-good.
4. Compare SPD hub register watchlist.
5. Compare PMIC status/config dumps.
6. Note only persistent differences.

## Protection-register clone workflow draft

This is intentionally a draft and should not be run until firmware supports readback verification and safety gating.

1. Enter write/offline mode intentionally.
2. Read MR12/MR13 before write.
3. Confirm current SPD payload matches known-good.
4. Write MR12/MR13 to target protection map.
5. Power-cycle.
6. Read MR12/MR13 in offline mode.
7. Power-cycle normal mode.
8. Read MR12/MR13 normal mode.
9. Test motherboard boot.

Target map from current good-stick notes:

```text
MR12 = 0xFF
MR13 = 0x3C
```

Do not run this blindly. This changes protection state.
