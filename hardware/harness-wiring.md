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

## Full DIMM power switching

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

This is the real cold-power control used when changing HSA strap state.

Changing HSA without a true VIN_BULK power cycle may not take effect, because the SPD hub samples HSA during power-up.

## Ground

Known harness ground pins:

| DIMM pin | Wire color | Function |
|---:|---|---|
| 8 | Black | GND |
| 10 | Black | GND |

All grounds must be common:

- DIMM GND
- Breadboard PSU GND
- ESP32 GND
- PCA9306 GND, when used
- MOSFET control circuit ground
- Optional LED grounds, if debug LEDs are installed

## Control and status pins

### HSA — pin 148

HSA was tested experimentally in two ways:

1. ESP32-controlled through GPIO27.
2. Manual strap control during bench testing.

The manual strap method was easier and more reliable for testing because the SPD hub samples HSA at power-up anyway. Changing the HSA condition while the DIMM is already powered does not reliably change the hub's active address/state. A true full VIN_BULK power reset is required after changing HSA.

#### Current preferred bench-test method

Use manual HSA strap control, then cold-cycle VIN_BULK with GPIO32.

```text
Set HSA strap manually
→ Turn DIMM off with GPIO32
→ Wait for full power-down
→ Turn DIMM on with GPIO32
→ Scan bus / read hub address
```

#### Optional ESP32-controlled HSA experiment

GPIO27 was used experimentally to control HSA:

```text
Pin 148 HSA ---- 1k ---- ESP32 GPIO27
Pin 148 HSA ---- 100k ---- 3.3V
```

| GPIO27 state | HSA result | Intended test mode |
|---|---|---|
| LOW / output-low | HSA forced low | Write / offline tester style test |
| INPUT / released | Pulled high by 100k | Normal / high-HSA test |

This GPIO27 method worked as a control experiment, but it is not required for basic bench testing. Manual HSA strapping plus a full VIN_BULK reset is simpler and avoids confusion.

### HSA strap and address behavior

The SPD hub address seen on the bus changed depending on the HSA condition present at power-up.

| HSA condition at power-up | Observed address / behavior | Notes |
|---|---|---|
| Direct hard-low / tied to GND | `0x50` | Offline / write-programmer style behavior; write-protect override path |
| Resistor-selected low strap / slot-ID style strap | `0x53` observed in later normal runtime setup | Treat this as the later/current normal-mode observation for this harness |
| Floating or high-ish HSA | `0x57` observed earlier | Older observation; useful context, not the current default assumption |

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
| LOW / output-low | Hub disabled |
| HIGH / released | Hub enabled via pull-up |

PWR_EN is hub-enable control. It is not a substitute for full DIMM power removal.

### PWR_GOOD — pin 147

```text
3.3V ---- 10k ----+---- pin 147 PWR_GOOD
                  |
                  +---- ESP32 GPIO34
```

GPIO34 is input-only, which makes it a good fit for PWR_GOOD.

| PWR_GOOD state | Meaning |
|---|---|
| HIGH | Hub powered / internal rails likely stable |
| LOW | Disabled / not ready / fault |

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

## Optional debug LEDs

The LEDs were used as temporary diagnostic indicators during wiring bring-up, connection testing, and software development.

They are not required for DDR5 SPD/PMIC communication, HSA testing, power cycling, or normal operation of the harness.

| LED | GPIO | Original debug meaning |
|---|---:|---|
| White | 26 | Ready / idle |
| Yellow | 25 | Processing / pulse |
| Green | 19 | Last command success |
| Red | 23 | Failure / not ready / PWR_GOOD blink |
| Blue | 18 | Danger / write active |

These GPIOs can be omitted or repurposed if the firmware is adjusted.

## Quick schematic

### Manual HSA strap

```text
Pin 148 HSA ---- manual strap / resistor / high / floating test condition
```

After changing this strap, cold-cycle VIN_BULK with GPIO32.

### Optional GPIO27 HSA experiment

```text
Pin 148 ---- 1k ---- GPIO27
Pin 148 ---- 100k ---- 3.3V
```

### PWR_EN

```text
Pin 151 ---- GPIO33
Pin 151 ---- 10k ---- 3.3V
```

### PWR_GOOD

```text
Pin 147 ---- GPIO34
Pin 147 ---- 10k ---- 3.3V
```

### Full DIMM power

```text
PSU +5V ---- MOSFET high-side switch ---- Pin 1 / 145 / 146
GPIO32 ---- MOSFET control circuit
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
