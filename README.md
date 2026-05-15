# DDR5 SPD Diagnostic Tooling

This repo documents a DIY ESP32 tool for reading and comparing DDR5 SPD hub and PMIC management-plane state, plus a separate passive boot sniffer used during a failed-DIMM investigation.

The beginner path is read-only: wire a DDR5 extension adapter/breakout to an ESP32, power the DIMM sideband path, scan the bus, dump SPD, and compare results. The advanced write/edit features are documented, but they are not where a normal user should start.

> [!WARNING]
> This is lab-proven for this project, not a universal consumer-safe DDR5 repair kit. Wiring mistakes, wrong voltage rails, bad grounds, power sequencing errors, probing mistakes, or write commands can damage DIMMs, ESP32 boards, motherboards, power supplies, or USB ports.

Most users do not need to download the whole repository. Once release packages are published, users who only want to flash and use the ESP32 SPD Tool should download the latest firmware package from Releases. Clone or download the full repo only if you want the source code, documentation, examples, or to modify/build the firmware yourself.

## Quick Navigation

| Need | Link |
| --- | --- |
| First read-only walkthrough | [Quick start](docs/quick-start.md) |
| Flashing/install paths | [Flashing](docs/flashing.md) |
| ESP32 SPD Tool commands | [Command reference](docs/spd-tool/command-reference.md) |
| Safety boundaries | [Safety](docs/safety.md) |
| Active ESP32 SPD tool wiring | [SPD tool wiring](docs/hardware/spd-tool-wiring.md) |
| Passive boot sniffer wiring | [Sniffer wiring](docs/hardware/sniffer-wiring.md) |
| Reference vs checkpoint | [Diagnostic reference vs tweak checkpoint](docs/reference-vs-checkpoint.md) |
| Experimental SPD editing | [Advanced SPD editing](docs/advanced-spd-editing.md) |
| Problems and interpretation | [Troubleshooting](docs/troubleshooting.md) |
| Example evidence | [Examples](docs/examples/README.md) |
| Full docs index | [Docs](docs/README.md) |

## The Two Setups

There are two separate harnesses:

| Setup | What it does | Wiring style |
| --- | --- | --- |
| Active ESP32 SPD/PMIC tool | Reads SPD, SPD hub, and PMIC state through the ESP32 Web UI/serial command surface | Direct adapter/breakout wiring to the DIMM sideband pins |
| Passive boot sniffer | Watches motherboard-driven SDA/SCL traffic during boot | Taps the motherboard/DIMM sideband lines without driving them |

Do not mix the wiring assumptions. The active SPD tool is **not** piggybacking off a motherboard.

## Minimum Hardware For The Active SPD Tool

The proven basic direct-read setup in this project used:

- ESP32 development board,
- DDR5 UDIMM extension adapter or breakout,
- two 10 kΩ resistors for PWR_EN and PWR_GOOD pull-ups to 3.3 V,
- stable 5 V source for DIMM VIN_BULK,
- ESP32 USB power or equivalent,
- shared ground between the ESP32 and DIMM power source,
- wire/soldering tools and strain relief,
- multimeter, preferably with a current-limited supply.

No PCA9306 level shifter and no external SDA/SCL pull-ups were needed for the proven basic direct-read setup. The ESP32 internal pull-ups worked for direct SPD/PMIC communication in this lab harness.

The active SPD tool should use an adapter/breakout connection. Piggyback/tap wiring belongs to the passive sniffer setup, not the normal active-tool harness. For repeated use, build or buy a proper adapter PCB.

## Basic Wiring

For the active tool:

| Connection | Minimum direct-read wiring |
| --- | --- |
| ESP32 GPIO21 | DIMM/adapter HSDA/SDA |
| ESP32 GPIO22 | DIMM/adapter HSCL/SCL |
| PWR_EN | 10 kOhm pull-up to 3.3 V |
| PWR_GOOD | 10 kOhm pull-up to 3.3 V, and ESP32 input if monitored |
| DIMM VIN_BULK | 5 V source |
| ESP32 power | USB or other ESP32-safe 5 V source |
| Grounds | DIMM power ground and ESP32 ground shared |

![Active ESP32 SPD tool wiring](docs/images/spd-tool/esp32-spd-tool-basic-wiring-overview.jpg)

See [SPD tool wiring](docs/hardware/spd-tool-wiring.md) for the full wiring page.

## Safe Read-Only First Commands

After wiring and verifying rails:

1. Open the ESP32 SPD tool Web UI.
2. Check hardware/status.
3. Run `scan`.
4. Run `autodetect`.
5. Run `health` or `diagquick`.
6. Run `dump`.
7. Save references only after you know which module and state you are saving.

See the [ESP32 SPD Tool command reference](docs/spd-tool/command-reference.md) for syntax, aliases, safety classification, and examples.

![Terminal showing discovered devices](docs/images/ui/esp32-spd-tool-terminal-discovered-devices-dark.png)

## What Success Looks Like

For read-only diagnostics, useful success means:

- the bus scans consistently,
- the SPD hub responds,
- PMIC reads work when expected,
- a 1024-byte SPD dump is repeatable,
- CRC/checksum and compare results make sense,
- speed/stability tests repeat management-plane reads reliably.

That is not the same as proving the DIMM will boot. These tests are I2C/SPD/PMIC management-plane checks, not DRAM cell tests or memory-controller training tests.

## What This Does Not Prove

This project does not prove that a DIMM is electrically healthy, that DRAM cells are good, that a BIOS will train the module, or that an SPD write made the stick bootable.

The documented bad-stick case is management-plane evidence only. Cloning/restoring a known-good SPD payload to the corrupt DIMM is mostly proven at the SPD management-plane level: write/readback/compare/CRC, hub access, PMIC access, and read stability checks can pass. The repaired bad stick still did not work.

Current best conclusion: evidence points toward DRAM-side/training-path failure, not an active SPD hub MR12/MR13 mismatch. MR12/MR13 protected/unprotected differences are historical diagnostic context only.

Before/after comparisons: see [docs/examples/comparisons](docs/examples/comparisons/README.md).

## Advanced SPD Editing

Advanced SPD editing is experimental and is not a beginner workflow. The DDR5-5600 EXPO/XMP edit path only proves preview/write/readback/CRC behavior. It does not prove BIOS/POST/memory stability.

![Advanced SPD editing warning](docs/images/ui/esp32-spd-tool-advanced-spd-editing-warning-dark.png)

See [Advanced SPD editing](docs/advanced-spd-editing.md).

## Repository Layout

| Path | Contents |
| --- | --- |
| `firmware/esp32-spd-tool/` | Active ESP32 SPD/PMIC PlatformIO firmware |
| `docs/` | Beginner and technical documentation |
| `docs/images/spd-tool/` | Active tool hardware photos |
| `docs/images/sniffer/` | Passive sniffer hardware photos |
| `docs/images/ui/` | Web UI screenshots |
| `docs/examples/` | Curated dumps, hub-register examples, and repair-case evidence |
| `hardware/` | Older/deeper hardware notes and references |
| `investigations/` | Failure-analysis notes |

## Maintenance Note

Unless this project gains serious traction or more bad DDR5 modules become available for testing, updates may slow down. The current repo is intended to preserve the working tool, wiring notes, captures, and investigation results.
