# Hardware Index

## Active SPD/PMIC diagnostic harness

- [`harness-wiring.md`](harness-wiring.md)
- [`data/udimm-management-pins.csv`](data/udimm-management-pins.csv)

## Passive boot sniffer harness

- [`passive-boot-sniffer-wiring.md`](passive-boot-sniffer-wiring.md)

> [!CAUTION]
> The active harness and passive sniffer reuse some DDR5 sideband concepts but are not the same wiring. GPIO34 is PWR_GOOD in the active harness and HSCL sniff input in the passive sniffer harness.
