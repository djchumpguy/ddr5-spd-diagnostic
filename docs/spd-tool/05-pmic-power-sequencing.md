# PMIC Power Sequencing Notes

The currently referenced PMIC behavior is Richtek RTQ5119A-style DDR5 VR-on-DIMM PMIC behavior.

This page is about the diagnostic-tool power model, not a complete PMIC design guide.

## Core idea

For this ESP32 diagnostic setup, separate these ideas:

| Thing | Meaning in this project |
|---|---|
| VIN_BULK | 5 V module input rail / main PMIC input path |
| VIN_MGMT | Management supply domain used for PMIC register/NVM access and LDO support |
| Sideband access | SPD hub / PMIC communication path over HSDA/HSCL |
| PWR_EN | PMIC VR enable / DRAM rail enable; pull-up required in the documented basic harness, GPIO33 control optional |
| PWR_GOOD | Pull-up required/recommended; GPIO34-monitored readiness/wiring indicator before trusting bus results |
| HSA | Strap sampled by the SPD hub at power-up |

The diagnostic tool can read SPD/PMIC sideband state without treating PWR_EN as “hub enable.”

PWR_EN itself must be pulled up in the documented basic harness. GPIO33 control of PWR_EN should be reserved for intentional PMIC regulator / DRAM rail experiments.

## Rails and management power

Key PMIC concepts:

| Concept | Meaning |
|---|---|
| VIN_BULK | Main input supply for PMIC output regulators |
| VIN_MGMT | Management supply used for PMIC NVM/register access and LDO support |
| VLDO_1.8V | 1.8 V support rail used by management-side devices such as SPD/TS-related logic |
| VLDO_1.0V | 1.0 V support rail for I3C push-pull/internal use |
| CAMP | Combined control/monitor/status/write-protect related open-drain line |
| GSI_n | General status interrupt output |
| PWR_EN | PMIC VR enable / DRAM rail enable |
| PWR_GOOD | Readiness/status indicator used by the harness as a wiring/readiness gate |

## VIN_BULK power options

The lab harness used GPIO32 to control a MOSFET-switched VIN_BULK rail, but that is a convenience feature, not a hard requirement.

Valid VIN_BULK methods:

| Method | Notes |
|---|---|
| GPIO32-controlled MOSFET switch | Best for automated cold cycles and repeatable HSA testing |
| Manual inline switch | Simple and valid for bench work |
| Bench supply output toggle | Valid if it fully removes/restores VIN_BULK |
| Direct stable 5 V feed | Valid for basic sideband testing, but HSA changes require manually removing/restoring power |

The important rule is:

```text
HSA changes require a real VIN_BULK cold power cycle.
```

That cold cycle can be automated with GPIO32 or done manually.

## PWR_EN / VR enable rule

PWR_EN should not be described as SPD hub enable.

For this project:

```text
PWR_EN = PMIC VR enable / DRAM rail enable
```

not:

```text
PWR_EN = SPD hub enable
```

PWR_EN is useful when intentionally observing or testing PMIC output regulator / DRAM rail behavior.

PWR_EN pull-up is required for the documented basic harness. The optional part is GPIO33 control; basic SPD hub / PMIC sideband reads do not require toggling PWR_EN from firmware.

## VR enable timing caution

Do not issue PMIC VR Enable immediately after applying power.

The PMIC documentation calls out minimum stable times for management and bulk supplies before VR enable. The safe project-level rule is:

```text
Apply VIN_BULK
→ wait/check PWR_GOOD/readiness
→ verify sideband access
→ read PMIC status
→ only then consider GPIO33-controlled VR enable experiments
```

For the referenced RTQ5119A-style behavior:

- VIN_MGMT must be valid before reliable management access.
- The management interface becomes available only after the management-ready delay.
- VR Enable is done by setting the PMIC VR-enable control, such as PMIC register `0x32[7] = 1`, or equivalent command.
- Status registers should be queried before enabling VR.
- VR enable is an experiment/control action, not part of normal SPD dump/read workflow.

## PWR_GOOD interpretation

PWR_GOOD is pulled up to 3.3 V through 10 kOhm and monitored by ESP32 GPIO34 in the basic setup. GPIO34 is input-only. PWR_GOOD is a readiness/wiring indicator before trusting SPD/PMIC communication, not an enable control.

Project observation:

```text
Every time PWR_GOOD was pulled LOW in this lab setup, the cause was a wiring/readiness issue.
```

So for this harness:

| PWR_GOOD state | Interpretation |
|---|---|
| HIGH | Harness/DIMM management state appears ready enough to attempt SPD/PMIC access |
| LOW | Stop and check wiring/readiness before trusting bus results |

If PWR_GOOD is LOW, do not treat failed SPD/PMIC reads as proof that the SPD hub or PMIC is dead.

Check:

- VIN_BULK supply or switch state
- Ground continuity
- PWR_GOOD pull-up
- Sideband pull-ups
- HSA strap state
- DIMM seating / edge connector contact
- MOSFET switch wiring, if used
- Accidental shorts
- Missing common ground

## Basic sideband-read sequence

Use this when the goal is SPD/PMIC discovery, not DRAM rail enable testing.

```text
Apply VIN_BULK
Wait/check PWR_GOOD
Scan sideband bus
Read SPD hub state
Read PMIC ID/status/registers
Dump SPD/PMIC state as needed
```

PWR_EN must already be pulled up for the documented harness, but it does not need to be toggled from GPIO33 for this workflow.

## Optional PMIC VR / DRAM rail experiment

Use this only when intentionally observing PMIC regulator / DRAM rail behavior.

```text
Apply/cycle VIN_BULK
Wait/check PWR_GOOD
Scan sideband bus
pmicid
pmicdump
vr_enable on
powerdiag
pmicdump
vr_enable off
```

Notes:

- GPIO33 control is not required for basic SPD/PMIC sideband communication, but the PWR_EN pull-up still is.
- Record this separately from normal read-only diagnostics.
- Do not use VR enable as a casual “maybe it helps” step.
- If PWR_GOOD is LOW, stop and fix wiring/readiness before interpreting PMIC behavior.

## CAMP behavior in plain English

CAMP is not just “power good.”

It has multiple roles:

- Write-protect / configuration state
- Fail/status signaling
- PMIC power-good style output after VR enable

That makes CAMP a dangerous line to oversimplify in firmware names.

Use explicit names where possible:

| Name | Meaning |
|---|---|
| `camp_status` | General CAMP state readback |
| `camp_fault` | CAMP interpreted as fault/status |
| `camp_write_protect_state` | CAMP interpreted in a write-protect/configuration context |
| `vr_enable` | PMIC regulator / DRAM rail enable |
| `pwr_good` | Harness readiness/wiring status signal |

Avoid naming everything “power good” or “enable” without saying which PMIC/harness behavior is meant.

## Relationship to final diagnosis

The final project conclusion is not that PMIC sideband access itself remained the active failure cause.

Current project diagnosis:

```text
Likely DRAM-side / training-path failure inferred from good-vs-bad motherboard
boot sniffer divergence after SPD/HUB and PMIC sideband communication appeared
functional.
```

PMIC notes remain useful because they document how sideband access, VR enable, PWR_GOOD, and rail behavior were separated during diagnosis.

## Short version

```text
VIN_BULK powers the module.
GPIO32 switching is convenient, not mandatory.
PWR_EN pull-up is required; GPIO33 control is optional.
PWR_EN is PMIC VR / DRAM rail enable, not SPD hub enable.
PWR_GOOD LOW means check wiring/readiness first.
SPD/PMIC sideband reads do not require casual GPIO33-controlled DRAM rail enable experiments.
Read PMIC status before VR enable.
VR enable is a deliberate experiment, not a default read workflow step.
```
