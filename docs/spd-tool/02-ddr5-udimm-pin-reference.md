# DDR5 UDIMM Management Pin Reference

This page only tracks the management/debug pins used by the current harness. It is not a full 288-pin DDR5 routing guide.

## Pin summary

| DIMM pin | Signal | Current harness use |
|---:|---|---|
| 1 | VIN_BULK | 5 V module input; can be GPIO32-switched, manually switched, or fed from stable 5 V |
| 4 | HSCL | SPD hub / PMIC sideband clock |
| 5 | HSDA | SPD hub / PMIC sideband data |
| 8 | VSS / GND | Common ground |
| 10 | VSS / GND | Common ground |
| 145 | VIN_BULK | 5 V module input; tied with pins 1 and 146 |
| 146 | VIN_BULK | 5 V module input; tied with pins 1 and 145 |
| 147 | PWR_GOOD | Readiness/wiring indicator to ESP32 GPIO34 |
| 148 | HSA | Manual strap / address-mode selector sampled at power-up |
| 151 | PWR_EN | Optional PMIC VR enable / DRAM rail enable via ESP32 GPIO33 |

## VIN_BULK — pins 1, 145, 146

VIN_BULK is the DIMM's 5 V input rail.

In the automated lab harness, VIN_BULK was routed through a MOSFET high-side switch controlled by ESP32 GPIO32:

| GPIO32 state | VIN_BULK result |
|---|---|
| HIGH | DIMM off |
| LOW | DIMM on |

GPIO32 switching is convenient, but not mandatory.

VIN_BULK may also be supplied by:

- Stable 5 V feed
- Manual inline switch
- Bench supply output toggle
- ESP32-controlled MOSFET switch

The important requirement is that HSA strap changes need a real VIN_BULK cold power cycle.

## HSCL / HSDA — pins 4 and 5

| DIMM pin | Signal | ESP32 side |
|---:|---|---|
| 4 | HSCL | GPIO22 SCL |
| 5 | HSDA | GPIO21 SDA |

The conservative reference design uses a PCA9306 or equivalent level shifter between the ESP32 3.3 V I2C bus and the DIMM sideband bus.

Actual lab result:

```text
Direct ESP32 3.3 V open-drain I2C to DIMM HSCL/HSDA worked in this harness.
```

Direct wiring should be treated as a lab-proven shortcut for this setup, not a universal DDR5 rule.

SDA/SCL must be treated as open-drain I2C lines. Do not actively drive either line high as push-pull GPIO.

## PWR_GOOD — pin 147

PWR_GOOD goes to ESP32 GPIO34.

GPIO34 is input-only, which is a good fit for this status signal.

PWR_GOOD is a readiness/wiring indicator before trusting SPD/PMIC communication.

Project observation:

```text
Every time PWR_GOOD was pulled LOW in this lab setup, the cause was a wiring/readiness issue.
```

| PWR_GOOD state | Meaning in this harness |
|---|---|
| HIGH | Harness/DIMM management state appears ready enough to attempt SPD/PMIC access |
| LOW | Stop and check wiring/readiness before trusting bus results |

If PWR_GOOD is LOW, do not treat failed SPD/PMIC reads as proof that the SPD hub or PMIC is dead.

Check:

- VIN_BULK supply or switch state
- Ground continuity
- PWR_GOOD pull-up
- Sideband pull-ups
- HSA strap state
- DIMM seating / edge connector contact
- MOSFET switch wiring, if used
- Accidental shorts
- Missing common ground

## HSA — pin 148

HSA is sampled by the SPD hub at power-up and affects hub address/mode behavior.

The practical bench-test method is manual HSA strapping.

GPIO27-based HSA control was tested experimentally, but it is not the default/recommended workflow for basic bench testing.

### Required HSA workflow

```text
Remove VIN_BULK
Set HSA strap manually
Restore VIN_BULK
Wait/check PWR_GOOD
Scan/read
Record observed address
```

PWR_EN alone is not enough to force a new HSA sample.

### Observed HSA/address behavior

| HSA mode | Practical wiring | Observed behavior in this project | Use |
|---|---|---|---|
| Direct hard-low / ground | HSA tied directly to ground | SPD/HUB observed at `0x50` | Direct-ground / hard-low / offline-style testing |
| Resistor-selected strap | HSA through the nominal 35.7 kΩ / ~36 kΩ class HSA/HID strap; measured ~34.4 kΩ in-circuit on this adapter/harness | SPD/HUB observed around `0x53` | Normal tested harness behavior |
| Floating/high | HSA released/floating/high | SPD/HUB observed around `0x57` | Experimental high/floating behavior |

The 35.7 kΩ / ~36 kΩ class value is the nominal/reference HSA/HID strap value
from the SPD hub reference material tracked in `sources/source-index.md`. The
~34.4 kΩ value was measured in-circuit on this project's adapter/harness.
Verify the actual strap resistance and address behavior on your own setup with
`scan`, `autodetect`, and `mapall`.

## Optional GPIO27 HSA experiment

If using ESP32-controlled HSA experimentally:

```text
Pin 148 HSA ---- 1k ---- ESP32 GPIO27
Pin 148 HSA ---- 100k ---- 3.3V
```

| GPIO27 state | HSA result | Intended test mode |
|---|---|---|
| LOW / output-low | HSA forced low | Direct-GND / offline tester test |
| INPUT / released | Pulled high by 100k | Normal / high-HSA test |

Even with GPIO27 control, HSA changes still require a true VIN_BULK cold power cycle.

## PWR_EN — pin 151

PWR_EN goes to ESP32 GPIO33 in the lab harness.

Earlier notes called this “hub enable,” but that wording is misleading.

For this project, PWR_EN should be treated as:

```text
PMIC VR enable / DRAM rail enable
```

not:

```text
SPD hub enable
```

| GPIO33 state | Result |
|---|---|
| LOW / output-low | PMIC switching regulators / DRAM rails disabled |
| HIGH / released | PMIC VR enable allowed through pull-up |

PWR_EN is optional for basic SPD hub / PMIC sideband communication in this diagnostic setup.

PWR_EN is useful when intentionally observing PMIC regulator / DRAM rail behavior, but it is not required for normal SPD/PMIC reads and it is not a replacement for VIN_BULK cold cycling.

## Important distinctions

| Thing | What it actually does |
|---|---|
| VIN_BULK | Supplies 5 V module input power |
| VIN_BULK cold cycle | Forces the hub to re-sample HSA at power-up |
| HSA | Selects address/mode behavior at power-up |
| PWR_EN | Optional PMIC VR / DRAM rail enable |
| PWR_GOOD | Readiness/wiring indicator before trusting bus results |
| HSCL/HSDA | Sideband I2C access to SPD hub / PMIC path |

## Short version

```text
Power VIN_BULK from stable 5 V.
Set HSA manually.
Cold-cycle VIN_BULK after HSA changes.
Wait/check PWR_GOOD.
Use HSCL/HSDA for SPD/PMIC sideband access.
Treat PWR_EN as optional VR/DRAM rail enable, not hub enable.
```
