# Prototype Harness and Future Board Target

## Purpose

This project was validated using a hand-wired ESP32 DDR5 lab harness.

The prototype proves that the active SPD/PMIC tool and passive boot-sniffer
approach can work, but the current wiring should be treated as a lab prototype
only.

It is not the recommended final hardware implementation.

## Validated Prototype Hardware

The validated lab setup used:

- ESP32 WROOM-class development board
- ESP32 screw-terminal adapter / breakout board
- breadboard
- PCA9306 I2C level-shifter module
- DDR5 extension / adapter card
- hand-soldered jumper wiring
- external DIMM VIN_BULK power switching
- external HSA, PWR_EN, and PWR_GOOD wiring

The DDR5 extension adapter was important because it provided accessible solder
points for the DDR5 edge-connector pins. This avoided soldering directly to the
DDR5 DIMM.

## Prototype Limitations

The prototype wiring is intentionally documented as a working lab setup, not as
a finished hardware design.

Known limitations:

- fragile jumper wiring
- easy-to-misconnect breadboard layout
- no keyed connectors for critical signals
- no integrated current limiting or eFuse protection
- no integrated DIMM socket
- no integrated power sequencing
- no integrated test-point labeling beyond the adapter/breakout boards
- PCA9306 level shifting was used for validation but is not recommended as the
  final board-level solution without further electrical review
- manual wiring creates avoidable risk when switching between normal,
  hard-low/offline-style, and floating/high HSA behavior

The prototype is useful because it proves the control architecture. It should
not be copied blindly as a final hardware product.

## Recommended Future Hardware

The ideal long-term hardware target is a dedicated ESP32 DDR5 SPD/PMIC
diagnostic HAT, shield, or standalone board.

A future board should include:

- onboard ESP32 module or socketed ESP32 dev-board footprint
- onboard DDR5 UDIMM socket
- USB-C input power
- protected switched DIMM VIN_BULK rail
- independent ESP32 power rail that stays alive while DIMM power cycles
- onboard 3.3 V regulation
- onboard sideband-bus level shifting / interface circuitry
- selectable or firmware-controlled HSA strap network
- PWR_EN control
- PWR_GOOD monitoring
- current limiting or eFuse protection for the DIMM rail
- status LEDs for ready, busy, success, failure, power, and write-danger states
- labeled test points for HSCL, HSDA, HSA, PWR_EN, PWR_GOOD, VIN_BULK, 3.3 V,
  and GND
- optional passive-sniffer breakout header
- optional logic-analyzer header
- clear silkscreen labels for normal, direct-ground/hard-low/offline-style, and
  floating/high HSA configurations

## Power Architecture Target

Recommended high-level power structure:

```text
USB-C 5 V input
   |-- 3.3 V regulator -> ESP32 and control logic
   `-- protected switched 5 V path -> DDR5 VIN_BULK pins
```

The ESP32 must not be powered through the switched DIMM VIN_BULK rail.

The ESP32 should remain alive while the DIMM rail is fully removed and restored.
This is necessary for controlled cold power cycling and HSA strap testing.

## Control Separation

The future board must preserve the distinction between full DIMM power control
and PWR_EN control:

```text
DIMM_PWR / VIN_BULK switch = true DIMM cold power cycle
PWR_EN = hub / PMIC enable control
```

Changing HSA state requires a true DIMM power cycle. Toggling PWR_EN alone is
not a substitute for removing and restoring DIMM VIN_BULK.

## HSA Modes to Support

The board should make HSA behavior explicit and hard to confuse:

| Observed address | HSA condition | Meaning |
|---|---|---|
| `0x53` | ~36 kΩ class resistor strap to GND | normal resistor-selected strap behavior |
| `0x50` | HSA tied directly to GND | direct-ground / hard-low / offline-style behavior |
| `0x57` | HSA floating or pulled high experimentally | floating/high experimental behavior |

Do not describe `0x50` as generic "write mode." In this project it corresponds
to direct-ground / hard-low / offline-style HSA behavior.

## Suggested Board Goals

A dedicated board should make the tool safer, easier to reproduce, and easier
to document.

Primary goals:

- eliminate breadboard wiring errors
- avoid soldering directly to DIMMs
- provide repeatable HSA mode selection
- provide safe DIMM power cycling
- expose useful test points
- support both active SPD/PMIC access and passive sideband sniffing where
  practical
- make the validated lab workflow easier for other people to reproduce

The current breadboard harness should be treated as proof of concept. The proper
next evolution is a dedicated diagnostic board.
