# Test Workflows

This page describes practical diagnostic-tool workflows for the ESP32-assisted DDR5 UDIMM harness.

The default workflow is:

```text
power safely
check readiness
read first
dump first
compare first
repeat-test first
write only deliberately
```

## General logging rule

For every workflow, record:

```text
Date/time:
Stick ID:
Tool / firmware version:
VIN_BULK method:
HSA strap condition:
VIN_BULK cold cycle performed:
PWR_GOOD state:
Observed hub address:
Observed PMIC/local-device address:
Commands run:
Direct observations:
Inferences:
Notes:
```

Do not compare captures unless the HSA state and VIN_BULK cycle method are known.

## Workflow 1 — Read-only baseline check

Use this for normal diagnostics.

Goal:

- Confirm the harness is working.
- Read SPD/PMIC state without changing module configuration.
- Establish repeatable baseline behavior.

### GPIO32-switched VIN_BULK version

```text
Confirm wiring against hardware/harness-wiring.md
dimm_power off
Set HSA strap manually
dimm_power cycle
Wait/check PWR_GOOD
powerdiag
scan
mapall
spd dump
pmicid
pmicdump
timespd
timereg
compare
```

### Direct/manual 5 V VIN_BULK version

```text
Confirm wiring against hardware/harness-wiring.md
Turn off/remove 5 V VIN_BULK manually
Set HSA strap manually
Restore 5 V VIN_BULK manually
Wait/check PWR_GOOD
powerdiag
scan
mapall
spd dump
pmicid
pmicdump
timespd
timereg
compare
```

### Notes

- GPIO32 switching is convenient, not mandatory.
- Direct stable 5 V or a manual switch can power VIN_BULK.
- PWR_EN is not required for basic SPD/PMIC sideband reads.
- If PWR_GOOD is LOW, stop and check wiring/readiness before trusting scan/read results.
- Save logs under `logs/YYYY-MM-DD-stick-id/` locally.
- Commit only sanitized summaries unless the raw logs are intentionally cleaned.

## Workflow 2 — Good-stick capture

Use this to create a known-good reference.

Goal:

- Capture known-good SPD/PMIC/hub state.
- Preserve known-good data without writing to the good stick.

### Sequence

```text
Label stick physically
Confirm wiring
Set intended HSA strap
Cold-cycle VIN_BULK
Wait/check PWR_GOOD
Run read-only baseline check
Dump SPD EEPROM
Dump SPD hub watchlist registers
Dump PMIC registers
Run timing/stability tests
Hash the dumps
Store known-good SPD reference only after confirming stick identity
```

### Required rule

```text
Do not write to the good stick.
```

### Suggested artifacts

Save locally:

```text
logs/YYYY-MM-DD-good-stick/
  spd-dump.bin
  spd-dump.txt
  spd-hub-registers.txt
  pmic-dump.txt
  timing-tests.txt
  hashes.txt
  notes.md
```

## Workflow 3 — Bad-stick comparison

Use this to compare a suspect module against the known-good reference.

Goal:

- Determine what differs persistently.
- Avoid chasing differences caused by HSA strap state, power-cycle method, or wiring state.

### Sequence

```text
Label suspect stick physically
Confirm wiring
Set same intended HSA strap used for comparable good-stick capture
Cold-cycle VIN_BULK
Wait/check PWR_GOOD
Run read-only baseline check
Dump SPD EEPROM
Dump SPD hub watchlist registers
Dump PMIC registers
Run timing/stability tests
Compare SPD EEPROM payload to known-good
Compare SPD hub register watchlist
Compare PMIC status/config dumps
Note only persistent differences
```

### Notes

- If PWR_GOOD is LOW, stop and troubleshoot wiring/readiness first.
- Do not treat one failed read as evidence of a dead SPD hub or PMIC.
- Repeat timing tests before trusting an intermittent difference.
- Do not compare normal-mode data against direct-GND/offline tester data without recording HSA state.

## Workflow 4 — HSA address experiment

Use this when intentionally checking address behavior from HSA strap state.

Goal:

- Confirm which hub address appears for a given HSA state sampled at power-up.

### Sequence

```text
Remove VIN_BULK
Set HSA strap manually
Record intended HSA condition
Restore VIN_BULK
Wait/check PWR_GOOD
powerdiag
scan
Record observed hub address
```

### Observed project address behavior

| HSA condition at power-up | Observed hub address / behavior | Notes |
|---|---|---|
| Direct hard-low / tied to GND | `0x50` | Direct-GND / hard-low / offline tester behavior |
| Resistor-selected low strap / slot-ID style strap | `0x53` | HSA resistor strap / HID-selected observed harness state |
| Floating or high-ish HSA | `0x57` | HSA floating/high-ish observed behavior |

### Notes

- HSA is sampled at power-up.
- PWR_EN alone is not enough to force a new HSA sample.
- A real VIN_BULK cold cycle is required after changing HSA.
- Address behavior is experiment state, not just a fixed property of the stick.

## Workflow 5 — PWR_GOOD troubleshooting

Use this when PWR_GOOD is LOW.

Project observation:

```text
Every time PWR_GOOD was pulled LOW in this lab setup, the cause was a wiring/readiness issue.
```

Goal:

- Fix harness/readiness before interpreting bus failures.

### Sequence

```text
Stop read/write testing
Remove VIN_BULK
Check VIN_BULK routing
Check common ground
Check PWR_GOOD pull-up
Check sideband pull-ups
Check HSA strap
Check DIMM seating / edge connector contact
Check MOSFET switch wiring, if used
Check for shorts
Restore VIN_BULK
Wait/check PWR_GOOD
Run powerdiag
Only then scan/read again
```

### Rule

```text
Do not trust failed SPD/PMIC reads while PWR_GOOD is LOW.
```

PWR_GOOD LOW is not immediate proof of bad silicon.

## Workflow 6 — Optional PMIC VR / DRAM rail experiment

Use this only when intentionally observing PMIC output regulator / DRAM rail behavior.

PWR_EN belongs here, not in the basic SPD/PMIC read workflow.

Goal:

- Observe PMIC/DRAM rail enable behavior after sideband communication is already known-good.

### Sequence

```text
Apply/cycle VIN_BULK
Wait/check PWR_GOOD
scan
pmicid
pmicdump
vr_enable on
powerdiag
pmicdump
vr_enable off
```

### Notes

- PWR_EN / GPIO33 is optional PMIC VR enable / DRAM rail enable.
- PWR_EN is not SPD hub enable.
- PWR_EN is not required for basic SPD/PMIC sideband access.
- PWR_EN is not a replacement for VIN_BULK cold cycling.
- Record this separately from read-only baseline diagnostics.

## Workflow 7 — Recovery/write workflow

Use this only when deliberately entering programming/recovery workflows.

Goal:

- Back up first.
- Compare first.
- Confirm target identity.
- Write only deliberately.
- Verify after.

### Sequence

```text
capturegood        # known-good stick only, done earlier
Remove VIN_BULK
Install target stick
Set HSA for intended direct-GND/offline tester state
Restore VIN_BULK
Wait/check PWR_GOOD
powerdiag
scan
spd dump          # backup target before writes
pmicdump
compare
Confirm target identity
Confirm known-good reference
Confirm direct-GND/offline tester intent
writegood         # only after confirmations
verifygood
```

### Required write gates

Before writing:

- Known-good reference must exist.
- Target SPD must be backed up.
- Target stick identity must be confirmed.
- HSA state must be recorded.
- Observed hub address must be recorded.
- PWR_GOOD must be HIGH.
- User must explicitly confirm the dangerous operation.

Suggested confirmation phrase:

```text
I UNDERSTAND THIS CAN DAMAGE THE MODULE
```

## Historical workflow — MR12/MR13 protection clone

This workflow is historical/advanced and should not be treated as the normal fix path.

The earlier MR12/MR13 protection-register mismatch is no longer the active diagnosis.

Current handling:

- Keep this as historical context.
- Do not run blindly.
- Do not present it as the expected repair.
- Only revisit it if new captures show a fresh MR12/MR13 mismatch after a true VIN_BULK cold cycle.

### Historical target map from earlier good-stick notes

```text
MR12 = 0xFF
MR13 = 0x3C
```

### Historical draft sequence

```text
Enter direct-GND/offline tester mode intentionally
Read MR12/MR13 before write
Confirm current SPD payload matches known-good
Write MR12/MR13 to target protection map
Power-cycle VIN_BULK
Read MR12/MR13 in direct-GND/offline tester mode
Power-cycle VIN_BULK into normal read mode
Read MR12/MR13 normal mode
Test motherboard boot
```

### Warning

This changes protection state.

Do not run this unless firmware supports readback verification, safety gating, and the new capture actually justifies it.

## Workflow 8 — Final diagnosis documentation

The current final diagnosis is:

```text
Likely DRAM-side failure inferred from good-vs-bad motherboard boot sniffer divergence after SPD/PMIC communication appeared normal.
```

When documenting that diagnosis, keep the boundary clear:

- Direct observation: good and bad boot sniffs diverged after SPD/PMIC communication appeared normal.
- Inference: likely DRAM-side initialization/training failure.
- Not proven: exact failed DRAM IC, bank, lane, or training sub-step.

Sniffer detail belongs in separate sniffer notes/log summaries, but diagnostic-tool captures should still preserve:

- SPD reads
- PMIC reads
- HSA/address state
- PWR_GOOD state
- Register watchlist
- Timing/stability tests

## Short version

```text
Read-only baseline:
  apply VIN_BULK
  wait/check PWR_GOOD
  scan/dump/compare/repeat-test

HSA address test:
  remove VIN_BULK
  change HSA strap
  restore VIN_BULK
  scan/record address

PWR_GOOD LOW:
  check wiring/readiness first

PMIC VR experiment:
  optional
  PWR_EN is VR/DRAM rail enable, not hub enable

Write/recovery:
  backup first
  compare first
  confirm target
  write only deliberately
  verify after

MR12/MR13:
  historical, not current active diagnosis
```
