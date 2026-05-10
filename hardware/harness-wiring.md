# ESP32 DDR5 Harness Wiring

## Power rails

### VIN_BULK — 5 V domain

DDR5 UDIMM VIN_BULK input pins:

| DIMM pin | Wire color | Function |
|---:|---|---|
| 1 | Red | VIN_BULK |
| 145 | Red | VIN_BULK |
| 146 | Red | VIN_BULK |

These three red wires are tied together as the module's VIN_BULK rail.

## DIMM power options

The lab harness used ESP32-controlled full DIMM power switching, but this is a convenience feature, not the only valid way to power the module.

### Option A — ESP32-controlled VIN_BULK switch

This is the documented/automated harness setup:

```text
Breadboard PSU +5V
   → MOSFET high-side switch input
   → MOSFET switched +5V output
   → DIMM pin 1 / pin 145 / pin 146
```

ESP32 control:

| GPIO | Behavior |
|---|---|
| GPIO32 HIGH | DIMM off |
| GPIO32 LOW | DIMM on |

This setup is useful because the firmware can cold-cycle VIN_BULK for repeat testing and HSA strap changes.

### Option B — direct 5 V / manual power feed

The DIMM VIN_BULK rail can also be powered directly from a stable 5 V source or through a manual switch:

```text
Stable +5V supply
   → optional manual switch
   → DIMM pin 1 / pin 145 / pin 146
```

This is valid for basic bench testing as long as:

- The 5 V source is stable.
- Grounds are common.
- The DIMM is fully power-cycled after changing HSA strap state.
- The user waits/checks PWR_GOOD before trusting bus results.

In this direct/manual setup, GPIO32 is not required. The operator performs the cold power cycle manually instead of the ESP32.

## HSA power-cycle rule

Changing HSA without a true VIN_BULK power cycle may not take effect, because the SPD hub samples HSA during power-up.

The required behavior is:

```text
Change HSA strap
→ fully remove VIN_BULK
→ restore VIN_BULK
→ wait/check PWR_GOOD
→ scan/read
```

That full VIN_BULK reset can be done either by:

- ESP32-controlled GPIO32 MOSFET switch
- Manual inline switch
- Turning the 5 V bench supply output off/on
- Physically removing/restoring the VIN_BULK feed

PWR_EN alone is not enough to force a new HSA sample.

## Ground

Known harness ground pins:

| DIMM pin | Wire color | Function |
|---:|---|---|
| 8 | Black | GND |
| 10 | Black | GND |

All grounds must be common:

- DIMM GND
- Breadboard PSU / 5 V supply GND
- ESP32 GND
- PCA9306 GND, when used
- MOSFET control circuit ground, when used

## Control and status pins

### HSA — pin 148

HSA was tested experimentally in two ways:

1. ESP32-controlled through GPIO27.
2. Manual strap control during bench testing.

The manual strap method was easier and more reliable for testing because the SPD hub samples HSA at power-up anyway. Changing the HSA condition while the DIMM is already powered does not reliably change the hub's active address/state. A true full VIN_BULK power reset is required after changing HSA.

#### Current preferred bench-test method

Use manual HSA strap control, then cold-cycle VIN_BULK.

```text
Set HSA strap manually
→ Fully remove VIN_BULK
→ Restore VIN_BULK
→ Wait/check PWR_GOOD
→ Scan bus / read hub address
```

The VIN_BULK cycle can be done by GPIO32-controlled MOSFET switching or manually with a 5 V supply/switch.

#### Optional ESP32-controlled HSA experiment

GPIO27 was used experimentally to control HSA:

```text
Pin 148 HSA ---- 1k ---- ESP32 GPIO27
Pin 148 HSA ---- 100k ---- 3.3V
```

| GPIO27 state | HSA result | Intended test mode |
|---|---|---|
| LOW / output-low | HSA forced low | Direct-GND / offline tester test |
| INPUT / released | Pulled high by 100k | Normal / high-HSA test |

This GPIO27 method worked as a control experiment, but it is not required for basic bench testing. Manual HSA strapping plus a full VIN_BULK reset is simpler and avoids confusion.

### HSA strap and address behavior

The SPD hub address seen on the bus changed depending on the HSA condition present at power-up.

| HSA condition at power-up | Observed address / behavior | Notes |
|---|---|---|
| Direct hard-low / tied to GND | `0x50` | Direct-GND / hard-low / offline tester behavior; write-protect override path |
| Resistor-selected low strap / slot-ID style strap | `0x53` observed in later normal runtime setup | HSA resistor strap / HID-selected observed harness state |
| Floating or high-ish HSA | `0x57` observed earlier | HSA floating/high-ish observed behavior |

Important notes:

- HSA is sampled at power-up.
- A full VIN_BULK cold power cycle is required after changing HSA.
- PWR_EN alone is not enough to force a new HSA sample.
- Do not compare address scans unless the HSA strap condition and power-cycle method are recorded.

In short: the same stick can appear at different addresses depending on whether HSA was hard-low, resistor-strapped, floating, or high at power-up.

### PWR_EN — pin 151

```text
3.3V ---- 10k ----+---- pin 151 PWR_EN
                  |
                  +---- ESP32 GPIO33
```

| GPIO33 state | Result |
|---|---|
| LOW / output-low | PMIC switching regulators / DRAM rails disabled |
| HIGH / released | PMIC VR enable allowed through pull-up |

PWR_EN should be treated as optional PMIC VR enable / DRAM rail enable.

It is not required for basic SPD hub / PMIC sideband communication in this diagnostic setup, and it is not a substitute for full DIMM power removal.

### PWR_GOOD — pin 147

```text
3.3V ---- 10k ----+---- pin 147 PWR_GOOD
                  |
                  +---- ESP32 GPIO34
```

GPIO34 is input-only, which makes it a good fit for PWR_GOOD.

PWR_GOOD is useful as a readiness/wiring indicator before trusting SPD/PMIC communication.

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
- Any accidental shorts or missing common ground

## I2C / sideband bus

The DDR5 sideband bus uses:

| DIMM pin | Signal | Wire color |
|---:|---|---|
| 4 | HSCL | Yellow |
| 5 | HSDA | Green |

ESP32 side:

| ESP32 GPIO | Signal |
|---:|---|
| 22 | SCL |
| 21 | SDA |

## I2C voltage-level note

The technically conservative design uses a PCA9306 or equivalent level shifter between the ESP32 3.3 V I2C bus and the DIMM sideband bus.

### Conservative / recommended wiring

DIMM side:

| DIMM pin | Signal | PCA9306 side |
|---:|---|---|
| 4 | HSCL | SCL2 / 1.8 V side |
| 5 | HSDA | SDA2 / 1.8 V side |

ESP32 side:

| ESP32 GPIO | Signal | PCA9306 side |
|---:|---|---|
| 22 | SCL | SCL1 / 3.3 V side |
| 21 | SDA | SDA1 / 3.3 V side |

Pull-ups:

| Side | Pull-up |
|---|---|
| DIMM / 1.8 V side | 5.1k to 1.8 V |
| ESP32 / 3.3 V side | 5.1k to 3.3 V |

This is the safer documented wiring because it respects the expected lower-voltage sideband domain.

### Actual lab wiring that worked

The actual harness also worked with the ESP32 I2C pins connected directly to the DIMM sideband pins:

| ESP32 | DIMM |
|---|---|
| GPIO21 SDA | HSDA / pin 5 |
| GPIO22 SCL | HSCL / pin 4 |

In this setup, SDA and SCL must be treated as open-drain I2C lines.

Do not actively drive SDA or SCL high as push-pull GPIO. The bus should only be pulled high through pull-ups and pulled low by devices.

Observed result:

- Direct ESP32 3.3 V I2C to DIMM HSDA/HSCL worked in this harness.
- The PCA9306 level-shifted version remains the safer design.
- Direct 3.3 V wiring should be treated as a lab-proven shortcut for this specific setup, not a universal DDR5 DIMM rule.

Do not confuse the I2C shortcut with the power/control pins. VIN_BULK, PWR_EN, PWR_GOOD, HSA, and PMIC behavior still need to be handled according to their own voltage and mode requirements.

## Passive boot sniffer wiring

The passive boot sniffer is a separate harness from the active SPD/PMIC tool.
Its wiring is documented here:

[Passive Boot Sniffer Wiring](passive-boot-sniffer-wiring.md)

Do not mix the active SPD/PMIC harness wiring with the passive sniffer wiring.
GPIO34 is PWR_GOOD in the active harness but HSCL sniff input in the passive
sniffer harness.

## Quick schematic

### VIN_BULK — optional ESP32-controlled switch

```text
PSU +5V ---- MOSFET high-side switch ---- Pin 1 / 145 / 146
GPIO32 ---- MOSFET control circuit
```

### VIN_BULK — direct/manual feed

```text
Stable +5V ---- optional manual switch ---- Pin 1 / 145 / 146
```

### Manual HSA strap

```text
Pin 148 HSA ---- manual strap / resistor / high / floating test condition
```

After changing this strap, cold-cycle VIN_BULK.

### Optional GPIO27 HSA experiment

```text
Pin 148 ---- 1k ---- GPIO27
Pin 148 ---- 100k ---- 3.3V
```

### PWR_EN / optional VR enable

```text
Pin 151 ---- GPIO33
Pin 151 ---- 10k ---- 3.3V
```

### PWR_GOOD / readiness wiring indicator

```text
Pin 147 ---- GPIO34
Pin 147 ---- 10k ---- 3.3V
```

### I2C — conservative level-shifted wiring

```text
ESP32 GPIO21 SDA ---- PCA9306 ---- HSDA pin 5
ESP32 GPIO22 SCL ---- PCA9306 ---- HSCL pin 4
```

### I2C — observed direct lab wiring

```text
ESP32 GPIO21 SDA ---- HSDA pin 5
ESP32 GPIO22 SCL ---- HSCL pin 4
```

Direct wiring worked in this lab setup, but the level-shifted version remains the safer reference design.
