# Documentation Index

## Universal project docs

- Firmware-only users should start with [`flashing.md`](flashing.md) and the read-only
  flow in [`quick-start.md`](quick-start.md). The full repo is not required for basic
  flashing/use.
- Repo users can browse docs, examples, firmware source, and hardware notes here.

- Beginner quick start: [`quick-start.md`](quick-start.md)
- Flashing/install paths: [`flashing.md`](flashing.md)
- Safety: [`safety.md`](safety.md)
- ESP32 SPD Tool command reference: [`spd-tool/command-reference.md`](spd-tool/command-reference.md)
- Diagnostic reference vs tweak checkpoint: [`reference-vs-checkpoint.md`](reference-vs-checkpoint.md)
- Advanced SPD editing: [`advanced-spd-editing.md`](advanced-spd-editing.md)
- Troubleshooting: [`troubleshooting.md`](troubleshooting.md)
- Examples and evidence: [`examples/README.md`](examples/README.md)
- Start here: [`universal/start-here.md`](universal/start-here.md)
- How to use this repo: [`universal/how-to-use-this-repo.md`](universal/how-to-use-this-repo.md)
- Project overview: [`universal/00-project-overview.md`](universal/00-project-overview.md)
- Safety boundaries: [`universal/01-safety-boundaries.md`](universal/01-safety-boundaries.md)

## Active ESP32 SPD/PMIC diagnostic tool

- Setup guide: [`spd-tool/setup-guide.md`](spd-tool/setup-guide.md)
- Command reference: [`spd-tool/command-reference.md`](spd-tool/command-reference.md)
- DDR5 UDIMM pin reference: [`spd-tool/02-ddr5-udimm-pin-reference.md`](spd-tool/02-ddr5-udimm-pin-reference.md)
- Operating modes: [`spd-tool/03-operating-modes.md`](spd-tool/03-operating-modes.md)
- SPD hub addressing: [`spd-tool/04-spd-hub-addressing.md`](spd-tool/04-spd-hub-addressing.md)
- PMIC power sequencing: [`spd-tool/05-pmic-power-sequencing.md`](spd-tool/05-pmic-power-sequencing.md)
- Register watchlist: [`spd-tool/06-register-watchlist.md`](spd-tool/06-register-watchlist.md)
- Test workflows: [`spd-tool/07-test-workflows.md`](spd-tool/07-test-workflows.md)
- Advanced SPD tool workflows: [`spd-tool/11-spd-tool-advanced-workflows.md`](spd-tool/11-spd-tool-advanced-workflows.md)
- Active harness wiring: [`../hardware/spd-tool/harness-wiring.md`](../hardware/spd-tool/harness-wiring.md)
- Active wiring diagram: [`../hardware/spd-tool/wiring-diagram.md`](../hardware/spd-tool/wiring-diagram.md)

## Passive ESP32 boot sniffer

- Sniffer quick start: [`sniffer/quick-start.md`](sniffer/quick-start.md)
- Beginner sniffer wiring: [`hardware/sniffer-wiring.md`](hardware/sniffer-wiring.md)
- Boot sniffer capture workflow: [`sniffer/10-boot-sniffer.md`](sniffer/10-boot-sniffer.md)
- Passive sniffer wiring: [`../hardware/sniffer/passive-boot-sniffer-wiring.md`](../hardware/sniffer/passive-boot-sniffer-wiring.md)
- Good-vs-bad sniffer analysis: [`../investigations/good-vs-bad-boot-sniffer-divergence.md`](../investigations/good-vs-bad-boot-sniffer-divergence.md)

## Investigations and conclusions

- Final DRAM-side failure diagnosis: [`../investigations/final-diagnosis-dram-failure.md`](../investigations/final-diagnosis-dram-failure.md)
- Good-vs-bad stick notes: [`../investigations/good-vs-bad-stick.md`](../investigations/good-vs-bad-stick.md)
- Good-vs-bad boot sniffer divergence: [`../investigations/good-vs-bad-boot-sniffer-divergence.md`](../investigations/good-vs-bad-boot-sniffer-divergence.md)

## Project tracking and sources

- TODO: [`../TODO.md`](../TODO.md)
- Source index: [`../sources/source-index.md`](../sources/source-index.md)

## Maintenance note

Unless this project gains serious traction or more bad DDR5 modules become available for
testing, updates may slow down. The current repo is intended to preserve the working
tool, wiring notes, captures, and investigation results.
