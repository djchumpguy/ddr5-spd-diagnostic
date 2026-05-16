# DDR5 SPD Diagnostic / ESP32 SPD Tool

The main project here is the **ESP32 SPD Tool**: a Web UI plus serial fallback tool for
reading DDR5 SPD, SPD hub, and PMIC management-plane state from a simple active
adapter/breakout harness. A passive DDR5 boot sniffer is also included as a secondary
companion tool for motherboard boot-sideband captures.

> [!WARNING]
> This is lab-proven DIY hardware, not a universal consumer-safe DDR5 repair kit. Wiring
> mistakes, wrong voltage rails, bad grounds, probing mistakes, power sequencing errors,
> or write commands can damage DIMMs, ESP32 boards, motherboards, power supplies, or USB
> ports.

Most users do not need to download the whole repository. If you only want to flash and
use the ESP32 SPD Tool, download the firmware ZIP from Releases. Clone the full repo
only if you want source code, docs, examples, captures, or to modify/build the firmware
yourself.

## Start Here: ESP32 SPD Tool

1. **Download firmware**
   - [ESP32 SPD Tool v0.1.0 ZIP](https://github.com/djchumpguy/ddr5-spd-diagnostic/releases/download/v0.1.0/esp32-spd-tool-v0.1.0.zip)
   - [All releases](https://github.com/djchumpguy/ddr5-spd-diagnostic/releases)

2. **Flash and wire**
   - [Flashing guide](docs/flashing.md)
   - [Quick start](docs/quick-start.md)
   - [SPD Tool wiring](docs/hardware/spd-tool-wiring.md)

3. **Run safe read-only diagnostics**
   - `scan`
   - `autodetect`
   - `dump`
   - diagnostic reference / compare workflows
   - speed/stability tests as management-plane read stability only

## Compact Navigation

| Main path | Link |
| --- | --- |
| Download SPD Tool firmware | [esp32-spd-tool-v0.1.0.zip](https://github.com/djchumpguy/ddr5-spd-diagnostic/releases/download/v0.1.0/esp32-spd-tool-v0.1.0.zip) |
| Flashing guide | [docs/flashing.md](docs/flashing.md) |
| Quick start | [docs/quick-start.md](docs/quick-start.md) |
| SPD Tool wiring | [docs/hardware/spd-tool-wiring.md](docs/hardware/spd-tool-wiring.md) |
| Minimum wiring diagram | [docs/hardware/spd-tool-wiring.md#minimum-wiring-diagram](docs/hardware/spd-tool-wiring.md#minimum-wiring-diagram) |
| Command reference | [docs/spd-tool/command-reference.md](docs/spd-tool/command-reference.md) |

| Advanced | Link |
| --- | --- |
| Advanced SPD editing | [docs/advanced-spd-editing.md](docs/advanced-spd-editing.md) |
| Examples and comparisons | [docs/examples/comparisons/README.md](docs/examples/comparisons/README.md) |
| Troubleshooting | [docs/troubleshooting.md](docs/troubleshooting.md) |
| Full docs index | [docs/README.md](docs/README.md) |

| Sniffer | Link |
| --- | --- |
| Download sniffer firmware | [esp32-ddr5-sniffer-v0.1.0.zip](https://github.com/djchumpguy/ddr5-spd-diagnostic/releases/download/v0.1.0/esp32-ddr5-sniffer-v0.1.0.zip) |
| Sniffer quick start | [docs/sniffer/quick-start.md](docs/sniffer/quick-start.md) |
| Sniffer wiring | [docs/hardware/sniffer-wiring.md](docs/hardware/sniffer-wiring.md) |
| Boot sniffer deep dive | [docs/sniffer/10-boot-sniffer.md](docs/sniffer/10-boot-sniffer.md) |

## Minimum Active SPD Tool Hardware

The proven basic direct-read adapter harness used:

- ESP32 development board.
- DDR5 UDIMM extension adapter / breakout.
- Two 10 kΩ resistors:
  - PWR_EN pull-up to 3.3 V. This pull-up is required in the documented basic harness.
  - PWR_GOOD pull-up to 3.3 V. This pull-up is required/recommended and is monitored by GPIO34.
- Stable 5 V source for DIMM VIN_BULK.
- ESP32 USB power or equivalent.
- Shared ground between ESP32 and DIMM power source.
- Wire/soldering tools and strain relief.
- Multimeter; current-limited supply preferred.

ESP32 SDA/SCL connected directly to DIMM HSDA/HSCL worked for the proven basic
direct-read adapter harness. No PCA9306 level shifter and no external SDA/SCL pull-ups
were required for that setup. External SDA/SCL pull-ups or level shifting are optional
troubleshooting/alternate-harness items, not the beginner minimum.

The active SPD Tool should use an adapter/breakout connection. Piggyback/tap wiring
belongs to the passive sniffer setup, not the normal active-tool harness.

## Basic Active Wiring

| Connection | Minimum direct-read wiring |
| --- | --- |
| ESP32 GPIO21 | DIMM/adapter HSDA/SDA |
| ESP32 GPIO22 | DIMM/adapter HSCL/SCL |
| PWR_EN | 10 kΩ pull-up to 3.3 V required; ESP32 GPIO33 control optional |
| PWR_GOOD | 10 kΩ pull-up to 3.3 V required/recommended; monitored by ESP32 GPIO34 input |
| DIMM VIN_BULK | stable 5 V source |
| ESP32 power | USB or other ESP32-safe source |
| Grounds | ESP32 and DIMM supply grounds shared |

![Active ESP32 SPD tool wiring](docs/images/spd-tool/esp32-spd-tool-basic-wiring-overview.jpg)

## What Success Looks Like

For read-only diagnostics, useful success means:

- the bus scans consistently,
- the SPD hub responds,
- PMIC reads work when expected,
- a 1024-byte SPD dump is repeatable,
- CRC/checksum and compare results make sense,
- speed/stability tests repeat management-plane reads reliably.

That is not the same as proving the DIMM will boot. These tests are I2C/SPD/PMIC
management-plane checks, not DRAM cell tests or memory-controller training tests.

## What This Does Not Prove

This project does not prove that a DIMM is electrically healthy, that DRAM cells are
good, that a BIOS will train the module, or that an SPD write made the stick bootable.

The documented bad-stick case is management-plane evidence only. Cloning/restoring a
known-good SPD payload to the corrupt DIMM is mostly proven at the SPD management-plane
level: write/readback/compare/CRC, hub access, PMIC access, and read stability checks
can pass. The bad stick still did not work.

Current best conclusion: evidence points toward DRAM-side/training-path failure, not an
active SPD hub MR12/MR13 mismatch. MR12/MR13 protected/unprotected differences are
historical diagnostic context only.

Before/after comparisons: see
[docs/examples/comparisons](docs/examples/comparisons/README.md).

## Advanced SPD Editing

Advanced SPD editing is experimental and is not a beginner workflow. The DDR5-5600
EXPO/XMP edit path only proves preview/write/readback/CRC behavior. It does not prove
BIOS/POST/memory stability.

See [Advanced SPD editing](docs/advanced-spd-editing.md).

## Passive Boot Sniffer

The passive boot sniffer is separate from the active SPD Tool. Use it when you want to
observe motherboard-driven DDR5 sideband traffic during boot. This is the setup that
uses piggyback/tap wiring, and it is more advanced than the active SPD Tool read-only
workflow.

- [Download sniffer firmware v0.1.0](https://github.com/djchumpguy/ddr5-spd-diagnostic/releases/download/v0.1.0/esp32-ddr5-sniffer-v0.1.0.zip)
- [Sniffer quick start](docs/sniffer/quick-start.md)
- [Sniffer easy wiring guide](docs/hardware/sniffer-wiring.md)
- [Deeper sniffer docs](docs/sniffer/10-boot-sniffer.md)

## Repository Layout

| Path | Contents |
| --- | --- |
| `firmware/esp32-spd-tool/` | Active ESP32 SPD/PMIC PlatformIO firmware source |
| `docs/` | Beginner and technical documentation |
| `docs/images/spd-tool/` | Active tool hardware photos |
| `docs/images/sniffer/` | Passive sniffer hardware photos |
| `docs/images/ui/` | Web UI screenshots |
| `docs/examples/` | Curated dumps, comparisons, and repair-case evidence |
| `hardware/` | Older/deeper hardware notes and references |
| `investigations/` | Failure-analysis notes |

## Maintenance Note

Unless this project gains serious traction or more bad DDR5 modules become available for
testing, updates may slow down. The current repo is intended to preserve the working
tool, wiring notes, captures, and investigation results.
