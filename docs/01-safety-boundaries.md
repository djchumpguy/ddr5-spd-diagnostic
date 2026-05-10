# Safety Boundaries

This project intentionally pokes at a live DDR5 module's management electronics. That is useful. It is also a fine way to turn hardware into modern art if treated casually.

## Scope

These notes are for an ESP32-assisted DDR5 UDIMM diagnostic harness focused on:

- SPD hub access
- SPD EEPROM reads/dumps/comparison
- PMIC visibility and status reads
- HSA strap/address experiments
- Carefully gated recovery/write workflows

This is not a consumer repair guide, and it is not a general-purpose DDR5 programming recipe.

## Electrical safety rules

1. Use a current-limited bench/breadboard supply where possible.
2. Keep ESP32 power separate from DIMM VIN_BULK power.
3. Tie grounds together:
   - DIMM GND
   - 5 V supply / bench PSU GND
   - ESP32 GND
   - PCA9306 GND, if used
   - MOSFET control circuit GND, if used
4. Do not feed ESP32 power through the DIMM VIN_BULK switch.
5. Confirm VIN_BULK polarity before applying power.
6. Confirm edge connector pin orientation before applying power.
7. Confirm pull-ups before applying power.
8. Keep a hardware way to remove DIMM VIN_BULK immediately.

## VIN_BULK power rules

DDR5 UDIMM VIN_BULK pins used by this harness:

| DIMM pin | Function |
|---:|---|
| 1 | VIN_BULK |
| 145 | VIN_BULK |
| 146 | VIN_BULK |

The lab harness used an ESP32-controlled MOSFET switch on VIN_BULK, but that is a convenience feature, not a requirement.

Valid VIN_BULK methods:

| Method | Notes |
|---|---|
| GPIO32-controlled MOSFET switch | Best for automated cold cycles and repeatable testing. |
| Manual inline switch | Simple and valid for bench work. |
| Bench supply output toggle | Valid if it fully removes/restores VIN_BULK. |
| Direct stable 5 V feed | Valid for basic sideband testing, but HSA changes require manually removing/restoring power. |

The important rule is:

```text
HSA changes require a real VIN_BULK cold power cycle.
```

That can be done by GPIO32, a manual switch, the bench supply, or physically removing/restoring the 5 V feed.

## HSA safety rules

HSA is sampled by the SPD hub at power-up.

Do not assume changing HSA while the DIMM is already powered will change the active hub mode or address.

Safe HSA workflow:

```text
Remove VIN_BULK
Set HSA strap
Restore VIN_BULK
Wait/check PWR_GOOD
Scan/read
Record observed address
```

Record the HSA condition for every test:

- Direct hard-low / tied to GND
- Resistor-selected low strap / slot-ID style strap
- Pull-up / high
- Floating / high-ish
- Optional GPIO27 experiment, if used

PWR_EN alone is not enough to force a new HSA sample.

## PWR_EN safety rules

Do not label PWR_EN as SPD hub enable.

For this project, PWR_EN should be treated as:

```text
PMIC VR enable / DRAM rail enable
```

PWR_EN is optional for basic SPD/PMIC sideband access.

Use PWR_EN only when intentionally experimenting with PMIC regulator / DRAM rail behavior.

| PWR_EN state | Meaning |
|---|---|
| LOW / pulled low | PMIC switching regulators / DRAM rails disabled |
| HIGH / released through pull-up | PMIC VR enable allowed |

PWR_EN is not a substitute for full VIN_BULK removal.

## PWR_GOOD safety rules

PWR_GOOD is a useful readiness/wiring indicator.

Project observation:

```text
Every time PWR_GOOD was pulled LOW in this lab setup, the cause was a wiring/readiness issue.
```

So for this harness:

| PWR_GOOD state | Interpretation |
|---|---|
| HIGH | Harness/DIMM management state appears ready enough to attempt SPD/PMIC access. |
| LOW | Stop and check wiring/readiness before trusting bus results. |

Do not treat PWR_GOOD LOW as immediate proof of a bad SPD hub, bad PMIC, or bad DRAM.

If PWR_GOOD is LOW, check:

- VIN_BULK supply or switch state
- Ground continuity
- PWR_GOOD pull-up
- Sideband pull-ups
- HSA strap state
- DIMM seating / edge connector contact
- MOSFET switch wiring, if used
- Any accidental shorts
- Missing common ground

Do not trust failed SPD/PMIC reads while PWR_GOOD is LOW.

## I2C / sideband bus safety rules

The conservative reference design uses a PCA9306 or equivalent level shifter between the ESP32 3.3 V I2C bus and the DIMM sideband bus.

Conservative design:

| Side | Signals | Pull-up |
|---|---|---|
| ESP32 side | GPIO21 SDA / GPIO22 SCL | 3.3 V |
| DIMM side | HSDA / HSCL | 1.8 V or module-appropriate sideband voltage |

Actual lab observation:

```text
Direct ESP32 3.3 V open-drain I2C to DIMM HSDA/HSCL worked in this harness.
```

Direct wiring that worked:

| ESP32 | DIMM |
|---|---|
| GPIO21 SDA | HSDA / pin 5 |
| GPIO22 SCL | HSCL / pin 4 |

Safety boundary:

- Treat direct 3.3 V wiring as a lab-proven shortcut for this setup.
- Do not treat direct 3.3 V wiring as a universal DDR5 rule.
- SDA/SCL must be open-drain I2C lines.
- Do not actively drive SDA/SCL high as push-pull GPIO.
- If in doubt, use the level shifter.

## Read-first workflow

Default diagnostic flow:

```text
Apply VIN_BULK
Wait/check PWR_GOOD
Scan
Dump
Compare
Repeat-test
Only then consider writes
```

Read-only commands and workflows should not alter:

- SPD EEPROM contents
- SPD hub protection state
- PMIC configuration
- HSA strap state
- VIN_BULK power state
- PWR_EN / VR enable state

## Write safety rules

1. Read and archive the full SPD before attempting any write.
2. Capture relevant register state before and after changes.
3. Treat direct-GND/offline tester mode as a separate state requiring a deliberate VIN_BULK cold power cycle.
4. Keep a hardware way to remove DIMM VIN_BULK immediately.
5. Never mix “probe what happens” and “write” in the same command path.
6. Never make write commands look visually similar to read/dump commands.
7. Require explicit confirmation before any write.
8. Require target identity confirmation before writing a recovered/good payload.
9. Do not write if PWR_GOOD is LOW.
10. Do not present MR12/MR13 protection cloning as the normal fix path.

Suggested dangerous-write confirmation text:

```text
I UNDERSTAND THIS CAN DAMAGE THE MODULE
```

## MR12/MR13 safety rule

MR12/MR13 protection-register mismatch is historical investigation context, not the current active root-cause path.

Do not start from “clone MR12/MR13” as the default repair strategy.

Only revisit MR12/MR13 if new captures show a fresh mismatch, and record:

- Stick identity
- HSA condition
- VIN_BULK cycle method
- Observed hub address
- MR11 value
- MR12 value
- MR13 value
- Whether the result reproduces after a true cold power cycle

## Sniffer diagnosis boundary

The current strongest finding is likely DRAM-side / training-path failure
inferred from good-vs-bad motherboard boot sniffer divergence after SPD/HUB and
PMIC sideband communication appeared functional.

Do not overstate that as direct proof of a specific failed DRAM IC, bank, lane, or training step.

Use language like:

```text
Likely DRAM-side / training-path failure inferred from boot-sniffer divergence.
```

Avoid language like:

```text
The sniffer directly proves the exact DRAM chip is bad.
```

## Repository safety rule

Do not commit raw private serial logs that include unrelated system information.

Keep raw captures locally under ignored paths such as:

```text
logs/raw/
assets/raw/
sources/private/
```

Commit only:

- Sanitized summaries
- Reduced example logs
- Notes that explain what was observed
- Reproducible command sequences
- Source indexes instead of redistributed copyrighted PDFs

## Short version

```text
Power carefully.
Record HSA state.
Cold-cycle VIN_BULK after HSA changes.
Wait/check PWR_GOOD.
Read first.
Dump first.
Compare first.
Write only on purpose.
If PWR_GOOD is LOW, check wiring before blaming silicon.
```
