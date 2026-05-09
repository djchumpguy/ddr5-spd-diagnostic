# TODO

Current priority: keep the diagnostic-tool documentation internally consistent before adding more raw data, firmware, or sniffer analysis.

## High priority — diagnostic repo cleanup

- [ ] Add/verify `.gitignore` blocks raw logs, private PDFs, binary dumps, and bulky media.
- [ ] Search repo for stale wording:
  - `hub enable`
  - `hub_enable`
- [ ] Confirm all docs describe `PWR_EN` as optional PMIC VR / DRAM rail enable, not SPD hub enable.
- [ ] Confirm all docs describe `PWR_GOOD LOW` as a wiring/readiness issue observed in this lab, not immediate proof of bad silicon.
- [ ] Confirm all docs say HSA changes require a true VIN_BULK cold power cycle.
- [ ] Confirm all docs say GPIO32 VIN_BULK switching is convenient, not mandatory.
- [ ] Confirm all docs allow direct/manual stable 5 V VIN_BULK power.
- [ ] Confirm all docs say manual HSA strapping is the preferred bench-test method.
- [ ] Confirm all docs say GPIO27 HSA control was optional/experimental only.
- [ ] Confirm all docs say direct ESP32 3.3 V open-drain I2C worked in this lab setup, while PCA9306 remains the safer reference design.

## High priority — diagnostic tool notes

- [ ] Add command examples from the actual ESP32 tool.
- [ ] Add safe read-only quickstart.
- [ ] Add known-good dump hashing workflow.
- [ ] Add exact PMIC register dump comparison table.
- [ ] Add cleaned example output for:
  - `scan`
  - `mapall`
  - `spd dump`
  - `pmicdump`
  - `timespd`
  - `timereg`
  - `powerdiag`
- [ ] Add a table showing expected tool behavior when `PWR_GOOD` is LOW.
- [ ] Add a table showing expected hub address behavior for HSA hard-low / resistor strap / floating-high cases.

## Investigation status

- [x] Mark MR12/MR13 mismatch as historical, not current active diagnosis.
- [x] Add final diagnosis page for likely DRAM-side failure inferred from boot sniffer divergence.
- [ ] Add sanitized boot-sniffer summary later.
- [ ] Add good-vs-bad boot-sniffer divergence timeline later.
- [ ] Preserve the distinction between directly observed sniffer events and inferred DRAM-side training failure.

## Hardware documentation

- [ ] Add photos of the actual harness.
- [ ] Add cleaned diagrams of:
  - minimal direct/manual 5 V setup
  - optional GPIO32-switched VIN_BULK setup
  - conservative PCA9306 level-shifted I2C setup
  - direct 3.3 V open-drain I2C lab setup
- [ ] Add connector orientation warning / pin-1 sanity check.
- [ ] Add a short harness bring-up checklist.

## Firmware / source

- [ ] Add firmware source or link to the firmware repo once docs stabilize.
- [ ] Add firmware build notes.
- [ ] Add command reference generated from the firmware help text.
- [ ] Add Web UI screenshots or screen descriptions after the command model is stable.

## Logs and captures

- [ ] Add sanitized example serial logs.
- [ ] Add sanitized example `powerdiag` output.
- [ ] Add sanitized example read-only baseline capture.
- [ ] Keep raw logs local unless explicitly cleaned.
- [ ] Do not commit unrelated system paths or private serial chatter.

## Nice to have

- [ ] Add troubleshooting flowchart.
- [ ] Add “known bad patterns” section.
- [ ] Add glossary:
  - VIN_BULK
  - VIN_MGMT
  - HSA
  - PWR_EN
  - PWR_GOOD
  - HSDA/HSCL
  - SPD hub
  - PMIC
  - CAMP
  - GSI_n
- [ ] Add a one-page “future me start here” checklist.

## Current repo truth

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
  LOW = check harness/readiness first

I2C:
  PCA9306 safer reference
  direct ESP32 3.3 V open-drain worked in this lab setup

MR12/MR13:
  historical mismatch only
  not current active diagnosis

Final diagnosis:
  likely DRAM-side failure inferred from good-vs-bad motherboard boot sniffer divergence after SPD/PMIC communication appeared normal
```
