# Wiring Diagram

This diagram shows the current diagnostic harness model.

Key points:

- VIN_BULK can be powered through the optional ESP32-controlled MOSFET switch **or** directly/manual-switched from stable 5 V.
- HSA is preferably controlled by manual strap during bench testing.
- GPIO27 HSA control was only an optional experiment.
- PWR_EN is optional PMIC VR / DRAM rail enable, **not** SPD hub enable.
- PWR_GOOD is a readiness/wiring indicator.
- The minimum proven setup uses direct ESP32 GPIO21/GPIO22 wiring to HSDA/HSCL on a DDR5 adapter/breakout.
- PCA9306 level shifting is an optional conservative/alternate approach, not required for the proven basic direct-read setup.

## Main harness diagram

```mermaid
flowchart LR
    PSU5["Stable 5 V supply"] --> POWERCHOICE{"VIN_BULK power method"}

    POWERCHOICE --> MANUAL["Direct or manual-switched 5 V"]
    MANUAL --> VIN["DIMM VIN_BULK pins 1 / 145 / 146"]

    POWERCHOICE --> MOSFET["Optional MOSFET high-side switch"]
    GPIO32["ESP32 GPIO32 optional VIN_BULK switch control"] --> MOSFET
    MOSFET --> VIN

    ESP32GND["ESP32 GND"] --- GND["Common ground"]
    PSUGND["5 V supply GND"] --- GND
    DIMMGND["DIMM GND pins 8 / 10"] --- GND
    IFGND["optional interface/protection GND if used"] --- GND
    MOSGND["MOSFET control GND if used"] --- GND

    HSA["DIMM HSA pin 148"] --> HSASTRAP{"Manual HSA strap at power-up"}
    HSASTRAP --> HSALOW["Hard-low / GND observed 0x50"]
    HSASTRAP --> HSARES["Resistor / slot-style strap observed 0x53"]
    HSASTRAP --> HSAHIGH["Floating or high-ish older observed 0x57"]

    GPIO27["ESP32 GPIO27 optional HSA experiment"] -. "1k series" .-> HSA
    HSA -. "optional 100k pull-up" .-> V33["3.3 V"]

    GPIO33["ESP32 GPIO33 optional VR enable"] --> PWREN["DIMM PWR_EN pin 151"]
    PWREN -- "10k pull-up" --> V33
    PWREN --> VRNOTE["PWR_EN equals PMIC VR / DRAM rail enable"]

    PWRGOOD["DIMM PWR_GOOD pin 147"] --> GPIO34["ESP32 GPIO34 input"]
    PWRGOOD -- "10k pull-up" --> V33
    PWRGOOD --> PGNOTE["PWR_GOOD high before trusting SPD / PMIC reads"]

    GPIO21["ESP32 GPIO21 SDA"] --> HSDA["DIMM HSDA pin 5"]
    GPIO22["ESP32 GPIO22 SCL"] --> HSCL["DIMM HSCL pin 4"]
```

## Power-cycle rule

After changing HSA strap state:

```text
Remove VIN_BULK
Set HSA strap
Restore VIN_BULK
Wait/check PWR_GOOD
Scan/read
```

That VIN_BULK cold cycle can be done by:

- GPIO32-controlled MOSFET switch
- Manual inline switch
- Bench supply output toggle
- Physically removing/restoring the 5 V feed

PWR_EN alone is not enough to force the SPD hub to re-sample HSA.

## Minimal direct/manual bench wiring

```mermaid
flowchart LR
    PSU5["Stable 5 V supply"] --> SWITCH["Optional manual switch"]
    SWITCH --> VIN["DIMM VIN_BULK pins 1 / 145 / 146"]

    PSUGND["5 V supply GND"] --- GND["Common ground"]
    ESP32GND["ESP32 GND"] --- GND
    DIMMGND["DIMM GND pins 8 / 10"] --- GND

    HSA["DIMM HSA pin 148"] --> STRAP["Manual HSA strap"]

    PWRGOOD["DIMM PWR_GOOD pin 147"] --> GPIO34["ESP32 GPIO34"]
    PWRGOOD -- "10k pull-up" --> V33["3.3 V"]

    GPIO21["ESP32 GPIO21 SDA"] -. "direct 3.3 V open-drain" .-> HSDA["DIMM HSDA pin 5"]
    GPIO22["ESP32 GPIO22 SCL"] -. "direct 3.3 V open-drain" .-> HSCL["DIMM HSCL pin 4"]
```

## Optional conservative/alternate sideband wiring

```mermaid
flowchart LR
    GPIO21["ESP32 GPIO21 SDA"] --> PCA3V["PCA9306 3.3 V side"]
    GPIO22["ESP32 GPIO22 SCL"] --> PCA3V

    PCA3V --> PCADIMM["PCA9306 DIMM-side lower-voltage side"]
    PCADIMM --> HSDA["DIMM HSDA pin 5"]
    PCADIMM --> HSCL["DIMM HSCL pin 4"]

    V33["3.3 V pull-ups"] --> PCA3V
    VDIMM["DIMM-side pull-ups"] --> PCADIMM
    GND["Common ground"] --- PCA3V
    GND --- PCADIMM
```

This optional approach may help in another harness, but it was not needed for
the proven basic direct-read setup.

## Short version

```text
VIN_BULK:
  stable 5 V direct/manual OR optional GPIO32 MOSFET switch

HSA:
  manual strap preferred
  GPIO27 optional experiment only
  full VIN_BULK cold-cycle required after change

PWR_EN:
  optional PMIC VR / DRAM rail enable
  not SPD hub enable

PWR_GOOD:
  readiness/wiring indicator
  LOW = check harness first

I2C:
  direct ESP32 GPIO21/GPIO22 to HSDA/HSCL worked in this lab setup
  optional level shifting or extra pull-ups are troubleshooting/alternate choices
```
