# DDR5 UDIMM Management Pin Reference

This page only tracks the management/debug pins used by the current harness. It is not a full 288-pin DDR5 routing guide.

| DIMM pin | Signal | Current harness use |
|---:|---|---|
| 1 | VIN_BULK | switched 5 V module input |
| 4 | HSCL | SPD/PMIC sideband clock |
| 5 | HSDA | SPD/PMIC sideband data |
| 8 | VSS/GND | common ground |
| 10 | VSS/GND | common ground |
| 145 | VIN_BULK | switched 5 V module input |
| 146 | VIN_BULK | switched 5 V module input |
| 147 | PWR_GOOD | status line to ESP32 GPIO34 |
| 148 | HSA | ESP32-controlled mode/address strap |
| 151 | PWR_EN | ESP32-controlled hub enable |

## Important distinction

- **VIN_BULK switching** changes the actual power state of the module.
- **PWR_EN** controls the hub/PMIC enable path.
- **HSA** is sampled by the SPD hub at power-up and affects addressing/mode behavior.

For mode changes, set HSA first, then cold-cycle VIN_BULK.
