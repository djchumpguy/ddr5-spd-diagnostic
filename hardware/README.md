# Hardware Index

## Active SPD/PMIC diagnostic harness

- Active harness wiring: [`spd-tool/harness-wiring.md`](spd-tool/harness-wiring.md)
- Active wiring diagram: [`spd-tool/wiring-diagram.md`](spd-tool/wiring-diagram.md)
- UDIMM management pin data: [`spd-tool/data/udimm-management-pins.csv`](spd-tool/data/udimm-management-pins.csv)

## Passive boot sniffer harness

- Passive boot sniffer wiring: [`sniffer/passive-boot-sniffer-wiring.md`](sniffer/passive-boot-sniffer-wiring.md)

> [!CAUTION]
> The active SPD/PMIC diagnostic harness and the passive boot sniffer harness are separate setups. Do not merge the wiring assumptions casually. GPIO34 is PWR_GOOD in the active SPD/PMIC harness, but HSCL sniff input in the passive sniffer harness.
