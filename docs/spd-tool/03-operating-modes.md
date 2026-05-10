# Operating Modes

This page describes practical operating modes for the ESP32-assisted DDR5 diagnostic harness.

The main rule is:

```text
HSA is sampled at power-up.
Changing HSA requires a real VIN_BULK cold power cycle.
```

That cold cycle can be done with:

- ESP32 GPIO32 controlling a MOSFET VIN_BULK switch
- A manual inline 5 V switch
- A bench supply output toggle
- Physically removing/restoring the 5 V VIN_BULK feed

GPIO32 switching is convenient, not mandatory.

PWR_EN is not SPD hub enable. Treat PWR_EN as optional PMIC VR / DRAM rail enable.

## Mode 1 — Normal read-only diagnostic mode

Use this for:

- Reading SPD
- Dumping SPD contents
- Reading SPD hub registers
- Reading PMIC registers
- Comparing against a known-good reference
- Running timing/stability tests
- Mapping current bus addresses

### Goal

Access the SPD/PMIC sideband path without changing module state.

### Preferred sequence

```text
Remove VIN_BULK
Set HSA strap manually for normal/runtime test condition
Restore VIN_BULK
Wait/check PWR_GOOD
Run powerdiag
Scan bus
Read/dump/compare
```

### GPIO32-switched sequence

Use this if the MOSFET VIN_BULK switch is installed and controlled by ESP32 GPIO32.

```text
dimm_power off
set HSA strap manually
dimm_power cycle
wait/check PWR_GOOD
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

### Direct/manual 5 V sequence

Use this if VIN_BULK is powered directly from a stable 5 V source or through a manual switch.

```text
turn off/remove 5 V VIN_BULK manually
set HSA strap manually
restore 5 V VIN_BULK manually
wait/check PWR_GOOD
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

- PWR_EN does not need to be toggled for basic SPD/PMIC sideband reads.
- If PWR_GOOD is LOW, stop and check wiring/readiness before trusting scan/read results.
- Do not treat a failed scan with PWR_GOOD LOW as proof of a dead SPD hub or PMIC.

## Mode 2 — HSA address experiment mode

Use this when intentionally checking how the SPD hub address changes based on HSA strap state.

### Goal

Record address behavior for a specific HSA condition sampled at power-up.

### Sequence

```text
Remove VIN_BULK
Set HSA strap manually
Record intended HSA condition
Restore VIN_BULK
Wait/check PWR_GOOD
Scan bus
Record observed hub address
```

### Observed HSA/address behavior

| HSA mode | Practical wiring | Observed behavior in this project | Use |
|---|---|---|---|
| Direct hard-low / ground | HSA tied directly to ground | SPD/HUB observed at `0x50` | Direct-ground / hard-low / offline-style testing |
| Resistor-selected strap | HSA through the nominal 36.0 kΩ HSA/HID strap; measured ~34.4 kΩ in-circuit on this adapter/harness | SPD/HUB observed around `0x53` | Normal tested harness behavior |
| Floating/high | HSA released/floating/high | SPD/HUB observed around `0x57` | Experimental high/floating behavior |

The 36.0 kΩ value is the nominal/reference HSA/HID strap value from the SPD hub reference material tracked in `sources/source-index.md`. The ~34.4 kΩ value was measured in-circuit on this project's adapter/harness. Verify the actual strap resistance and address behavior on your own setup with `scan`, `autodetect`, and `mapall`.

### Notes

- Address behavior is part of experiment state.
- Do not compare address scans unless HSA strap condition and VIN_BULK cycle method are recorded.
- PWR_EN alone is not enough to force a new HSA sample.

## Mode 3 — Direct-GND / offline tester recovery mode

Use this only when deliberately entering programming/recovery workflows.

This mode is dangerous because it can alter SPD contents, hub state, protection registers, or PMIC state if write commands are used.

### Goal

Enter a known direct-GND/offline tester condition and perform readback before any write.

### Sequence

```text
Remove VIN_BULK
Set HSA hard-low / intended direct-GND/offline strap
Restore VIN_BULK
Wait/check PWR_GOOD
Run powerdiag
Scan bus
Read back SPD/hub/PMIC state first
Back up target SPD
Compare against known-good reference
Only then consider write-capable commands
```

### Write safety gate

Before any write command:

- Confirm the known-good reference exists.
- Dump and archive the target SPD.
- Record HSA strap condition.
- Record observed hub address.
- Confirm PWR_GOOD is HIGH.
- Confirm this is the intended target stick.
- Require explicit dangerous-write confirmation.

Suggested confirmation text:

```text
I UNDERSTAND THIS CAN DAMAGE THE MODULE
```

### Notes

- HSA hard-low behavior was associated with `0x50` direct-GND/offline tester access.
- Do not present MR12/MR13 protection cloning as the normal fix path.
- MR12/MR13 mismatch is historical context, not the current active root cause.

## Mode 4 — Optional PMIC VR / DRAM rail experiment

Use this only when intentionally observing PMIC output regulator / DRAM rail behavior.

PWR_EN belongs here, not in the basic SPD/PMIC read workflow.

### Goal

Observe PMIC/DRAM rail enable behavior after sideband communication is already known-good.

### Sequence

```text
Apply/cycle VIN_BULK
Wait/check PWR_GOOD
Scan bus
pmicid
pmicdump
vr_enable on
powerdiag
pmicdump
vr_enable off
```

### Notes

- PWR_EN / GPIO33 should be treated as optional PMIC VR enable / DRAM rail enable.
- PWR_EN is not SPD hub enable.
- PWR_EN is not required for basic SPD/PMIC sideband access.
- PWR_EN is not a replacement for VIN_BULK cold cycling.

## Why PWR_EN alone is not enough

PWR_EN does not force the SPD hub to re-sample HSA.

HSA is sampled at power-up, so the authoritative reset for HSA experiments is full VIN_BULK removal/restoration.

Correct model:

```text
Change HSA
→ cold-cycle VIN_BULK
→ wait/check PWR_GOOD
→ scan/read
```

Incorrect model:

```text
Change HSA
→ toggle PWR_EN
→ assume address/mode changed
```

That shortcut can make the same stick appear inconsistent when the harness state is actually the thing changing.

## PWR_GOOD interpretation

PWR_GOOD is a readiness/wiring indicator.

Project observation:

```text
Every time PWR_GOOD was pulled LOW in this lab setup, the cause was a wiring/readiness issue.
```

| PWR_GOOD state | Interpretation |
|---|---|
| HIGH | Harness/DIMM management state appears ready enough to attempt SPD/PMIC access |
| LOW | Stop and check wiring/readiness before trusting bus results |

If PWR_GOOD is LOW, check:

- VIN_BULK supply or switch state
- Ground continuity
- PWR_GOOD pull-up
- Sideband pull-ups
- HSA strap state
- DIMM seating / edge connector contact
- MOSFET switch wiring, if used
- Accidental shorts
- Missing common ground

## Short version

```text
Normal diagnostics:
  Apply VIN_BULK
  Wait/check PWR_GOOD
  Scan/read/dump/compare

HSA change:
  Remove VIN_BULK
  Change HSA strap
  Restore VIN_BULK
  Wait/check PWR_GOOD
  Scan/read

Direct-GND/offline tester:
  HSA hard-low before power-up
  Cold-cycle VIN_BULK
  Read back first
  Back up first
  Write only with explicit confirmation

PWR_EN:
  Optional PMIC VR / DRAM rail enable
  Not hub enable
  Not needed for basic SPD/PMIC reads
```
