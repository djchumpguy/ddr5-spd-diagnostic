# TODO

## Codex can finish now

- [ ] Verify `.gitignore` blocks raw logs, private PDFs, binary dumps, bulky media.
- [ ] Search and fix stale wording: `hub enable`, `hub_enable`.
- [ ] Confirm PWR_EN wording is optional PMIC VR / DRAM rail enable, not SPD hub enable.
- [ ] Confirm PWR_GOOD LOW is described as wiring/readiness first, not immediate silicon failure.
- [ ] Confirm HSA changes require true VIN_BULK cold cycle.
- [ ] Confirm GPIO32 VIN_BULK switching is convenient, not mandatory.
- [ ] Confirm manual stable 5 V VIN_BULK is acceptable.
- [ ] Confirm manual HSA strapping is preferred for bench testing.
- [ ] Confirm GPIO27 HSA control is optional/experimental.
- [ ] Confirm PCA9306 remains the safer reference even though direct ESP32 3.3 V open-drain I2C worked in this lab.
- [ ] Add safe read-only quickstart for active SPD/PMIC tool.
- [ ] Add command reference generated from current firmware help/parser.
- [ ] Add glossary.
- [ ] Add "future me / new user start here" checklist.
- [ ] Add clean Mermaid diagrams for active SPD tool wiring options.

## Needs user-provided data

- [ ] Cleaned real command outputs for scan/mapall/SPD dump/pmicdump/timespd/timereg/powerdiag.
- [ ] Actual firmware help output if not easily extractable from source.
- [ ] Sanitized serial/Web logs.
- [ ] Any verified examples of PMIC edits.
- [ ] Any verified examples of SPD speed/profile table edits.

## Lab-only / future experiments

- [ ] Repeat good/bad captures under same setup.
- [ ] Larger-buffer / PSRAM / SD-card sniffer capture.
- [ ] Sniffer parser/compare script validation against more captures.
- [ ] Additional bad/good runs for repeatability.

## Done / integrated

- [x] Active ESP32 SPD/PMIC tool firmware added.
- [x] Passive ESP32 boot sniffer firmware added.
- [x] Known-good boot sniffer baseline added.
- [x] Bad-stick boot sniffer divergence capture added.
- [x] Good-vs-bad boot sniffer divergence analysis added.
- [x] MR12/MR13 marked historical/secondary, not current diagnosis.
- [x] Final/current diagnosis documented as likely DRAM-side / training-path failure.
- [x] Sniffer wiring split into hardware/sniffer.
- [x] Sniffer capture workflow documented.
- [x] Sniffer pin-needle prototype photos added.
- [x] Docs reorganized by universal / SPD tool / sniffer.
- [x] Hardware docs reorganized by active SPD tool / passive sniffer.
- [x] Advanced SPD tool workflows documented.
- [x] DIY/experimental safety disclaimer added.

## Intentionally not doing

- Do not add photos of the active SPD/PMIC tool rat's-nest prototype harness.
  Reason: the prototype was overcomplicated compared with the cleaned wiring
  options and may confuse readers.
- Do not publish raw/private logs or unrelated system paths.
- Do not commit copyrighted datasheet PDFs unless redistribution is clearly allowed.
- Do not claim PMIC edits or SPD timing/profile edits are validated unless tested.
