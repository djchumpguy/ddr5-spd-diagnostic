# Start Here

This repo is a lab notebook for DDR5 SPD/PMIC sideband investigation using two
related ESP32 tools: an active SPD/PMIC diagnostic tool and a passive boot
sniffer.

## What this project is

- A documentation and prototype firmware repo for DDR5 sideband debug.
- A record of the harness wiring, safety boundaries, captures, and investigation
  conclusions from this bench project.
- A read-first workflow for comparing known-good and suspect modules.

## What this project is not

- Not a polished repair guide or consumer-safe procedure.
- Not a universal DDR5 wiring standard.
- Not proof that every failed module can be fixed through SPD or PMIC edits.

## Low-cost / phone-friendly diagnostic goal

This project explores a low-cost route using inexpensive, widely available ESP32
boards instead of depending only on dedicated paid hardware. The active
SPD/PMIC tool can be interacted with over Wi-Fi from a phone, tablet, or
computer. The passive boot sniffer can be controlled and dumped over Bluetooth
serial from a phone.

That phone-friendly workflow is useful for limited-tool diagnostics, cramped
bench setups, or RAM-only captures where moving cables or power can erase the
data. It is still experimental DIY bench tooling, not a consumer-safe product or
a replacement for professional analyzers.

## Choose your tool

### Active SPD/PMIC tool

Use this when you want controlled reads, dumps, comparisons, and carefully gated
writes through the ESP32 SPD/PMIC diagnostic harness.

- Tool docs: [`../spd-tool/`](../spd-tool/)
- Active harness wiring: [`../../hardware/spd-tool/harness-wiring.md`](../../hardware/spd-tool/harness-wiring.md)
- Advanced workflows: [`../spd-tool/11-spd-tool-advanced-workflows.md`](../spd-tool/11-spd-tool-advanced-workflows.md)

### Passive boot sniffer

Use this when you want to observe motherboard-driven boot sideband traffic
without the ESP32 acting as the bus master.

- Capture workflow: [`../sniffer/10-boot-sniffer.md`](../sniffer/10-boot-sniffer.md)
- Passive wiring: [`../../hardware/sniffer/passive-boot-sniffer-wiring.md`](../../hardware/sniffer/passive-boot-sniffer-wiring.md)
- Good-vs-bad analysis: [`../../investigations/good-vs-bad-boot-sniffer-divergence.md`](../../investigations/good-vs-bad-boot-sniffer-divergence.md)

## Safe read-only path first

Start with the safety notes and read-only workflows before any write-capable
command:

- Safety boundaries: [`01-safety-boundaries.md`](01-safety-boundaries.md)
- Operating modes: [`../spd-tool/03-operating-modes.md`](../spd-tool/03-operating-modes.md)
- Test workflows: [`../spd-tool/07-test-workflows.md`](../spd-tool/07-test-workflows.md)

## Dangerous write/recovery workflows later

Only move to write-capable workflows after you have known-good backups, mode/HSA
state recorded, and a clear recovery goal:

- Advanced SPD tool workflows: [`../spd-tool/11-spd-tool-advanced-workflows.md`](../spd-tool/11-spd-tool-advanced-workflows.md)
- Register watchlist: [`../spd-tool/06-register-watchlist.md`](../spd-tool/06-register-watchlist.md)

## Current final diagnosis

The current strongest finding is likely DRAM-side / training-path failure,
inferred from good-vs-bad boot sniffer divergence after SPD/HUB and PMIC
sideband communication appeared functional.

- Final diagnosis: [`../../investigations/final-diagnosis-dram-failure.md`](../../investigations/final-diagnosis-dram-failure.md)
- Boot sniffer divergence note: [`../../investigations/good-vs-bad-boot-sniffer-divergence.md`](../../investigations/good-vs-bad-boot-sniffer-divergence.md)

MR12/MR13 remains historical/secondary context only, not the current root cause.

## Wiring docs

- Hardware index: [`../../hardware/README.md`](../../hardware/README.md)
- Active SPD/PMIC harness wiring: [`../../hardware/spd-tool/harness-wiring.md`](../../hardware/spd-tool/harness-wiring.md)
- Passive boot sniffer wiring: [`../../hardware/sniffer/passive-boot-sniffer-wiring.md`](../../hardware/sniffer/passive-boot-sniffer-wiring.md)
