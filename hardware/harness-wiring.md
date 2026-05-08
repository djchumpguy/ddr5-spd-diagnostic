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

### Full DIMM power switching

```text
Breadboard PSU +5V
   → MOSFET high-side switch input
   → MOSFET switched +5V output
   → DIMM pin 1 / pin 145 / pin 146
```

ESP32 control:

| GPIO | Behavior |
|---:|---|
| GPIO32 HIGH | DIMM off |
| GPIO32 LOW | DIMM on |

This is the real cold-power control used when changing HSA mode.

### Ground

Known harness ground pins:

| DIMM pin | Wire color | Function |
|---:|---|---|
| 8 | Black | GND |
| 10 | Black | GND |

All grounds must be common:

- DIMM GND
- breadboard PSU GND
- ESP32 GND
- PCA9306 GND
- LED grounds
- MOSFET control circuit ground

## Control and status pins

### HSA — pin 148

```text
Pin 148 HSA ---- 1k ---- ESP32 GPIO27
Pin 148 HSA ---- 100k ---- 3.3V
```

| GPIO27 state | HSA result | Mode intent |
|---|---|---|
| LOW/output-low | HSA forced low | write/offline tester mode |
| INPUT/released | pulled high by 100k | normal mode |

HSA is sampled at power-up. Changing GPIO27 alone is not enough. Use GPIO32 to cold-cycle VIN_BULK after changing HSA state.

### PWR_EN — pin 151

```text
3.3V ---- 10k ----+---- pin 151 PWR_EN
                  |
                  +---- ESP32 GPIO33
```

| GPIO33 state | Result |
|---|---|
| LOW/output-low | hub disabled |
| HIGH/released | hub enabled via pull-up |

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
| HIGH | hub powered / internal rails likely stable |
| LOW | disabled / not ready / fault |

## I2C / sideband bus

DIMM side:

| DIMM pin | Signal | Wire color | PCA9306 side |
|---:|---|---|---|
| 4 | HSCL | Yellow | SCL2 / 1.8 V side |
| 5 | HSDA | Green | SDA2 / 1.8 V side |

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

## LED status system

| LED | GPIO | Meaning |
|---|---:|---|
| White | 26 | ready/idle |
| Yellow | 25 | processing/pulse |
| Green | 19 | last command success |
| Red | 23 | failure/not ready/PWR_GOOD blink |
| Blue | 18 | danger/write active |

## Quick schematic

```text
HSA
Pin 148 ---- 1k ---- GPIO27
Pin 148 ---- 100k ---- 3.3V

PWR_EN
Pin 151 ---- GPIO33
Pin 151 ---- 10k ---- 3.3V

PWR_GOOD
Pin 147 ---- GPIO34
Pin 147 ---- 10k ---- 3.3V

Full DIMM power
PSU +5V ---- MOSFET high-side switch ---- Pin 1 / 145 / 146
GPIO32 ---- MOSFET control circuit

I2C
ESP32 GPIO21 SDA ---- PCA9306 ---- HSDA pin 5
ESP32 GPIO22 SCL ---- PCA9306 ---- HSCL pin 4
```
