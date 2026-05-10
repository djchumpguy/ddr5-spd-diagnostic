# TODO

## Current priority

Keep the public repo clear, accurate, and easy to navigate. Prefer cleaned
diagrams, tables, and workflows over messy prototype photos.

## Done / integrated

- [x] Active ESP32 SPD/PMIC tool firmware added
- [x] Passive ESP32 boot sniffer firmware added
- [x] Known-good boot sniffer baseline added
- [x] Bad-stick boot sniffer divergence capture added
- [x] Good-vs-bad boot sniffer divergence analysis added
- [x] MR12/MR13 marked historical/secondary, not current diagnosis
- [x] Final/current diagnosis documented as likely DRAM-side / training-path failure
- [x] Sniffer wiring split into its own hardware/sniffer doc
- [x] Sniffer capture workflow documented
- [x] Sniffer pin-needle prototype photos added
- [x] Docs reorganized by universal / SPD tool / sniffer
- [x] Hardware docs reorganized by active SPD tool / passive sniffer
- [x] Advanced SPD tool workflows documented
- [x] DIY/experimental safety disclaimer added

## Keep / still useful

- [ ] Verify `.gitignore` blocks raw logs, private PDFs, binary dumps, bulky media
- [ ] Search repo for stale wording:
  - `hub enable`
  - `hub_enable`
- [ ] Confirm docs consistently describe:
  - PWR_EN as optional PMIC VR / DRAM rail enable
  - PWR_GOOD LOW as wiring/readiness first, not instant silicon failure
  - HSA changes require true VIN_BULK cold cycle
  - GPIO32 VIN_BULK switching is convenient, not mandatory
  - manual stable 5 V VIN_BULK is acceptable
  - manual HSA strapping is preferred for bench testing
  - GPIO27 HSA control is optional/experimental
  - PCA9306 remains the safer reference, even though direct ESP32 3.3 V
    open-drain I2C worked in this lab
- [ ] Add command reference generated from current firmware help output
- [ ] Add safe read-only quickstart for the active SPD tool
- [ ] Add cleaned example outputs for:
  - `scan`
  - `mapall`
  - `dump` / SPD dump
  - `pmicdump`
  - `timespd`
  - `timereg`
  - `powerdiag`
- [ ] Add firmware build notes
- [ ] Add sanitized example serial logs only if cleaned
- [ ] Add parser/compare script for sniffer captures
- [ ] Add troubleshooting flowchart
- [ ] Add glossary
- [ ] Add one-page "future me start here" checklist

## Intentionally not doing

- Do not add photos of the active SPD/PMIC tool rat's-nest prototype harness.
  Reason: the bench prototype was overcomplicated compared with the cleaned
  wiring options and may confuse readers. Use cleaned diagrams, pin tables, and
  bring-up checklists instead.
- Do not publish raw/private logs or unrelated system paths.
- Do not commit copyrighted datasheet PDFs unless redistribution is clearly
  allowed.

## Optional future hardware docs

- [ ] Clean diagram: minimal direct/manual 5 V setup
- [ ] Clean diagram: optional GPIO32-switched VIN_BULK setup
- [ ] Clean diagram: conservative PCA9306 level-shifted I2C setup
- [ ] Clean diagram: direct 3.3 V open-drain I2C lab setup
- [ ] Connector orientation / pin-1 sanity check
- [ ] Short harness bring-up checklist

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
  likely DRAM-side / training-path failure inferred from good-vs-bad motherboard boot sniffer divergence after SPD/HUB and PMIC sideband communication appeared functional
```
