# TODO

This list is intentionally limited to hands-on bench work, hardware validation,
captures, measurements, and user-provided data.

Documentation cleanup, Markdown polish, repo navigation, and other
Codex-finishable tasks are not tracked here.

## Physical bench tasks

- [ ] Repeat the known-good boot sniffer capture using the same motherboard, slot, HSA/strap state, and sniffer profile.
- [ ] Repeat the suspect/bad-stick boot sniffer capture under the same conditions as the known-good capture.
- [ ] Capture at least one additional good-vs-bad run pair to confirm the divergence is repeatable.
- [ ] Save new sniffer captures with clear names identifying stick, slot, date, sniffer profile, and whether overflow occurred.

## Active SPD/PMIC tool bench validation

- [ ] Run the active SPD/PMIC tool on a known-good module and save clean outputs for:
  - scan
  - mapall
  - powerdiag
  - SPD dump
  - pmicid
  - pmicdump
  - timespd
  - timereg
- [ ] Run the same active SPD/PMIC tool commands on the suspect module and save matching clean outputs.
- [ ] Confirm the active tool's observed addresses match the documented HSA/power state for the current wiring.
- [ ] Verify any command examples against the actual firmware help output before publishing them as examples.
- [ ] Keep raw noisy terminal logs private until cleaned/sanitized.

## Optional advanced experiments

- [ ] If desired, test controlled PMIC register edits only after saving a known-good PMIC dump and confirming a recovery path.
- [ ] If desired, test SPD timing/profile-table edits only on sacrificial hardware with a verified original SPD backup.
- [ ] If desired, test whether a conservative edited SPD profile can still be read, verified, and attempted in motherboard POST.
- [ ] Document any PMIC or SPD edit as experimental unless it is repeated and verified.
- [ ] Do not claim PMIC edits or SPD timing/profile edits are validated until they have been physically tested.

## Hardware sanity checks

- [ ] Physically verify DDR5 socket/adapter orientation and pin numbering before any new wiring.
- [ ] Verify pin-needle sniffer taps contact only DDR5 pins 4 and 5 and do not deform the socket.
- [ ] Verify USB header ground to ESP32 GND continuity before boot-sniffer capture.
- [ ] Verify active SPD/PMIC tool wiring separately from passive sniffer wiring before powering either setup.
- [ ] Confirm no accidental cross-use of GPIO34 between active harness and passive sniffer harness.

## Intentionally not doing

- [x] Do not add photos of the active SPD/PMIC rat's-nest prototype harness.
  - Reason: the prototype was overcomplicated compared with the cleaned wiring options and may confuse readers.
- [x] Do not publish raw/private logs or unrelated system paths.
- [x] Do not publish datasheet PDFs unless redistribution is clearly allowed.
- [x] Do not present untested advanced PMIC/SPD edits as validated workflows.
- [x] Do not validate additional ESP32 board variants.
  - Reason: the project goal was achieved on an ESP32-WROOM-class board. Larger PSRAM/SD-card boards may improve capture depth or persistence, but they are optional future experiments for others.

## Done / already integrated

- [x] Active ESP32 SPD/PMIC tool firmware added.
- [x] Passive ESP32 boot sniffer firmware added.
- [x] Known-good boot sniffer baseline added.
- [x] Bad-stick boot sniffer divergence capture added.
- [x] Good-vs-bad boot sniffer divergence analysis added.
- [x] Current diagnosis documented as likely DRAM-side / training-path failure.
- [x] MR12/MR13 retained as historical/secondary context only.
- [x] Passive sniffer wiring split into its own hardware/sniffer section.
- [x] Passive sniffer capture workflow documented.
- [x] Sniffer pin-needle prototype photos added.
- [x] Docs reorganized by universal / SPD tool / sniffer.
- [x] Hardware docs reorganized by active SPD tool / passive sniffer.
- [x] Advanced SPD tool workflows documented.
- [x] DIY/experimental safety disclaimer added.
