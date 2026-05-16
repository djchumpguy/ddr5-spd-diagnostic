# Active ESP32 SPD Tool Wiring

[Back to README](../../README.md) | [Quick start](../quick-start.md) | [Safety](../safety.md)

This page describes the active ESP32 SPD/PMIC tool. It uses a DDR5 extension adapter or
breakout and connects the ESP32 directly to the DIMM/adapter sideband pins. It is not a
motherboard tap or sniffer harness.

![Active SPD tool wiring overview](../images/spd-tool/esp32-spd-tool-basic-wiring-overview.jpg)

## Minimum Proven Direct-Read Setup

This is the lab-proven minimum setup for this project:

| Signal | Connection |
| --- | --- |
| ESP32 GPIO21 | DIMM/adapter HSDA/SDA |
| ESP32 GPIO22 | DIMM/adapter HSCL/SCL |
| PWR_EN | 10 kOhm pull-up to 3.3 V required; ESP32 GPIO33 control optional |
| PWR_GOOD | 10 kOhm pull-up to 3.3 V required/recommended; monitored by ESP32 GPIO34 input |
| DIMM VIN_BULK | 5 V source to the DIMM VIN_BULK pins |
| ESP32 power | USB or another ESP32-safe power source |
| Ground | DIMM power ground and ESP32 ground must be shared |

The ESP32 internal SDA/SCL pull-ups worked for direct SPD/PMIC communication in this lab
setup. No PCA9306 level shifter and no external SDA/SCL pull-ups were needed for this
proven basic direct-read path.

![PWR_EN and PWR_GOOD pull-up example](../images/spd-tool/basic-spd-tool-wiring-2x-10k-pwren-pwrgood-pullups.jpg)

## Minimum Wiring Diagram

```text
ESP32 GPIO21  -> DDR5 HSDA/SDA
ESP32 GPIO22  -> DDR5 HSCL/SCL
3.3 V --10k--> PWR_EN
3.3 V --10k--> PWR_GOOD
5 V ---------> VIN_BULK
GND ---------> shared ground
```

For the proven basic direct-read setup, no PCA9306 level shifter was needed and no
external SDA/SCL pull-ups were needed. The PWR_EN and PWR_GOOD 10k pull-ups are part of
the documented harness. Shared ground between ESP32 and DIMM power is mandatory.

## Practical Hardware List

- DDR5 extension adapter or breakout,
- ESP32 dev board,
- two 10 kOhm resistors for PWR_EN and PWR_GOOD pull-ups,
- 5 V source for DIMM VIN_BULK,
- USB or other ESP32 power,
- shared ground between the ESP32 and DIMM supply,
- soldering iron and wire for the adapter pins.

![DDR5 adapter pin reference](../images/spd-tool/ddr5-extension-adapter-pin-reference.jpg)

## Power Notes

Verify rails before connecting a DIMM. The DIMM VIN_BULK supply and ESP32 must share
ground or reads will fail. USB power for the ESP32 is fine as long as ground is shared
with the DIMM supply.

![Bench power module example](../images/spd-tool/bench-3v3-5v-power-module-example.jpg)

PWR_EN must be pulled to 3.3 V through 10 kOhm in the documented simple harness; without
that pull-up, the PMIC/VR enable path may stay disabled and the module can appear dead.
Connecting ESP32 GPIO33 to PWR_EN is optional and only needed when you want
firmware-controlled regulator disable.

PWR_GOOD is pulled to 3.3 V through 10 kOhm and monitored by ESP32 GPIO34 in the basic
setup. GPIO34 is input-only. Treat PWR_GOOD as a readiness/wiring indicator, not an
enable control.

## Optional/Alternate Wiring

Other harnesses may need extra help. External SDA/SCL pull-ups or level shifting can be
useful troubleshooting or conservative-design options, but they are not part of the
minimum proven direct-read setup described above.

A proper adapter PCB, cleaner protection, current limiting, and strain relief are better
for repeated use.

## HSA Address Behavior

Observed project behavior:

- direct-ground/offline-style HSA behavior could expose SPD/HUB around `0x50`,
- resistor/normal harness behavior was observed around `0x53`,
- floating/high behavior was observed around `0x57`.

This is harness/address behavior, not a universal DDR5 truth. HSA is sampled during
power-up, so change the physical HSA state, cold-cycle VIN_BULK, then scan again.

## Prototype Warning

The photos show prototype lab wiring. Loose jumpers and hand-soldered adapter wires
should have strain relief. Wiring mistakes can damage DIMMs, ESP32 boards, motherboards,
or power supplies.
