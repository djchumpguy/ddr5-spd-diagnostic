# Passive Boot Sniffer Wiring

[Back to README](../../README.md) | [Safety](../safety.md) | [Examples](../examples/README.md)

The passive sniffer observes motherboard-driven DDR5 sideband traffic during boot. It is a different setup from the active SPD/PMIC tool.

![Sniffer ESP32](../images/sniffer/esp32-wroom.jpg)

## What It Is For

Use the sniffer to compare boot-time sideband behavior between a known-good DIMM and a suspect DIMM. It can show whether the motherboard reaches SPD hub and PMIC traffic before diverging.

It does not prove DRAM cell health by itself, and poor probe wiring can change the bus.

## Wiring Notes

- Keep SDA/SCL pickup wiring short.
- Maintain a reliable ground reference.
- Add strain relief to piggyback solder joints.
- Do not let sniffer wiring short adjacent pins.
- Avoid loading the bus.

![SDA/SCL piggyback example](../images/sniffer/sda-scl-piggyback-example.jpg)

![Piggyback pin detail](../images/sniffer/piggyback-pin.jpg)

## Interpreting Captures

The current best conclusion from this project is that the bad stick reaches useful SPD/HUB and PMIC sideband activity, then diverges during boot. That evidence points toward DRAM-side failure, not an active SPD hub MR12/MR13 mismatch.
