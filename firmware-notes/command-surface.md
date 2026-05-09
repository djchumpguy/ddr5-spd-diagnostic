# ESP32 Firmware Command Surface Notes

This repo does not yet include the firmware source, but the docs should preserve the command intent, safety model, and naming conventions.

The diagnostic tool should default to:

```text
observe → dump → compare → repeat-test → only then consider writes
```

Writes should be treated as a separate danger zone, not as normal workflow.

## Current hardware assumptions

| Area | Current project truth |
|---|---|
| Full DIMM power | GPIO32 controls the MOSFET-switched VIN_BULK rail. HIGH = DIMM off, LOW = DIMM on. |
| HSA mode | Manual HSA strapping became the preferred bench-test method. GPIO27 HSA control was tested but is optional/experimental. |
| HSA reset requirement | HSA is sampled at power-up. Changing HSA requires a true VIN_BULK cold power cycle. |
| PWR_EN / VR enable | GPIO33 was wired/tested, but it should not be treated as hub enable. It controls PMIC VR enable / DRAM rail bring-up behavior and is not required for basic SPD/PMIC sideband communication. |
| PWR_GOOD | GPIO34 can read PMIC power-good/status behavior. Useful for VR/DRAM-rail experiments, but not required for simple SPD/PMIC sideband reads. |
| I2C wiring | PCA9306 level shifting remains the safer reference design, but direct ESP32 3.3 V open-drain I2C to DIMM HSDA/HSCL worked in the lab setup. |
| LEDs | LEDs were optional debug indicators during bring-up and software testing, not required hardware. |

## Important PWR_EN correction

Earlier notes called GPIO33 / PWR_EN “hub enable.” That wording is misleading.

For this diagnostic tool, PWR_EN should be treated as:

```text
PMIC VR enable / DRAM rail enable
```

not:

```text
SPD hub enable
```

For the ESP32 diagnostic workflow, the tool talks to the SPD hub and PMIC over the sideband bus. Those management-side paths can be available before enabling the DRAM switching rails. PWR_EN is only useful when intentionally experimenting with PMIC output regulators / DRAM rail bring-up behavior.

So for basic SPD/PMIC diagnostics:

```text
Required:
- VIN_BULK / management power present
- HSA strap set before power-up
- Sideband I2C working

Not required:
- PWR_EN toggling
- DRAM rail enable
- LED panel
```

## Firmware naming rule

Avoid ambiguous labels like `power` when there are multiple power-ish controls.

Prefer explicit names:

| Name | Meaning |
|---|---|
| `dimm_power` | GPIO32 / switched VIN_BULK full DIMM power |
| `vr_enable` | GPIO33 / PWR_EN / PMIC regulator enable for DRAM rails |
| `pwr_good` | GPIO34 / PWR_GOOD status |
| `hsa_state` | Recorded/manual HSA strap condition |
| `hsa_gpio` | Optional GPIO27 HSA experiment, if firmware supports it |
| `bus_scan` | Current active sideband scan |
| `spd_dump` | SPD EEPROM dump |
| `pmic_dump` | PMIC register dump |

Avoid naming GPIO33 as `hub_enable`. It is not needed for the sideband-only SPD/PMIC read workflow and does not replace a full VIN_BULK reset.

## Safe / read-only discovery

These commands should be safe by default and should not alter SPD, PMIC configuration, protection state, or DRAM rail enable state.

| Command idea | Purpose |
|---|---|
| `scan` | Scan active bus state. |
| `mapall` | Current-mode read-only address mapping. Should not change HSA or power state. |
| `reg read <addr> <reg>` | Generic register read. |
| `biosread <offset>` | BIOS-style one-byte legacy SPD read. |
| `biosdump` | BIOS-style SPD dump path. |
| `spd read <offset>` | Read SPD byte/range. |
| `spd dump` | Dump SPD EEPROM contents. |
| `pmicid` | PMIC identification/read sanity check. |
| `pmicdump` | PMIC register dump. |
| `timespd` | SPD timing/stability test. |
| `timereg` | Register timing/stability test. |
| `powerdiag` | Report GPIO32 power state, optional GPIO33 VR enable state, optional GPIO34 PWR_GOOD state, and recorded HSA note. |
| `compare` | Compare current stick against stored known-good SPD. Read-only against the module. |
| `verifygood` | Verify target matches stored good SPD. Read-only against the module. |

## Power and mode control

Power/mode commands are not necessarily destructive, but they change experiment state and must be logged.

| Command idea | Purpose | Safety notes |
|---|---|---|
| `dimm_power on` | Turn on GPIO32-switched VIN_BULK. | GPIO32 LOW = DIMM on in the current harness. |
| `dimm_power off` | Turn off GPIO32-switched VIN_BULK. | GPIO32 HIGH = DIMM off in the current harness. |
| `dimm_power cycle` | Cold-cycle VIN_BULK. | Required after changing HSA strap state. |
| `vr_enable on` | Release/enable PWR_EN through pull-up. | Optional. Enables PMIC regulator / DRAM rail path; not required for basic SPD/PMIC reads. |
| `vr_enable off` | Pull PWR_EN low. | Optional. Disables PMIC switching regulators / DRAM rails. |
| `powerdiag` | Print power/control/status state. | Should show GPIO32, optional GPIO33, optional GPIO34, and recorded HSA note. |

## HSA handling

Manual HSA strapping is the preferred bench-test workflow.

Recommended firmware behavior:

| Command idea | Purpose |
|---|---|
| `hsa note low` | Record that HSA is manually hard-low / direct-GND for this test. |
| `hsa note resistor` | Record that HSA is manually resistor-strapped / slot-ID style. |
| `hsa note high` | Record that HSA is manually high / pull-up style. |
| `hsa note floating` | Record that HSA is floating / high-ish for this test. |
| `hsa status` | Display the recorded HSA note and remind user whether VIN_BULK has been cold-cycled since the note changed. |

Optional GPIO27 experiment commands, only if the firmware supports them:

| Command idea | Purpose | Notes |
|---|---|---|
| `hsa_gpio low` | Drive GPIO27 low to force HSA low. | Experimental. Requires VIN_BULK cold-cycle afterward. |
| `hsa_gpio release` | Release GPIO27 / input mode. | Experimental. Requires VIN_BULK cold-cycle afterward. |

Avoid presenting GPIO27 as the required/default HSA method. It was useful during testing, but manual strapping plus full VIN_BULK reset was simpler and clearer.

## HSA address expectations

The tool should print these as reminders when scanning or changing HSA notes:

| HSA condition at power-up | Observed hub address / behavior |
|---|---|
| Direct hard-low / tied to GND | `0x50` offline/write-programmer style behavior |
| Resistor-selected low strap / slot-ID style strap | `0x53` later/current normal-runtime observation |
| Floating or high-ish HSA | `0x57` older observation / historical context |

The firmware should not assume the address alone identifies the mode. Address interpretation requires the HSA strap condition and whether a full VIN_BULK cold power cycle occurred.

## Known-good storage and comparison

These commands interact with stored reference data but should avoid writing to the target module unless explicitly requested.

| Command idea | Purpose | Safety notes |
|---|---|---|
| `capturegood` | Store known-good SPD to ESP32 NVS. | Should require confirmation and print source stick reminder. |
| `cleargood` | Clear stored known-good SPD. | Should require confirmation. |
| `compare` | Compare current stick against stored good SPD. | Read-only against target stick. |
| `verifygood` | Verify target matches stored good SPD. | Read-only against target stick. |

## Dangerous / write-capable commands

These must be strongly gated and should never run from casual button clicks.

| Command idea | Purpose | Required gating |
|---|---|---|
| `reg write <addr> <reg> <value>` | Generic register write. | Require explicit write mode and confirmation. |
| `spd write <offset> <value>` | Write SPD byte/range. | Require backup, target confirmation, and write-mode confirmation. |
| `writegood` | Write stored good SPD payload to target stick. | Require known-good exists, target identity confirmation, full backup, and final confirmation. |
| `autofix` | Attempt automated recovery sequence. | Should remain strongly gated; never casual. |
| `protect clone` | Future MR12/MR13 protection map clone command. | Historical/caution only; not part of current final diagnosis path. |
| `pmic write` | Write PMIC register/config. | Require explicit PMIC write unlock / lab mode confirmation. |
| `vr_enable on` | Enable PMIC regulators / DRAM rails. | Not an SPD read requirement; log this separately when testing rail bring-up. |

Suggested dangerous-write confirmation text:

```text
I UNDERSTAND THIS CAN DAMAGE THE MODULE
```

For extra safety, require the user to type the target command twice or type the detected module identifier before writes.

## MR12/MR13 handling

MR12/MR13 protection-register mismatch is historical context, not the current active root-cause path.

Firmware may preserve commands for reading or cloning protection maps, but docs and UI should label them clearly:

```text
Historical / advanced / dangerous
```

Do not present MR12/MR13 cloning as the normal fix path.

## Suggested command flow

### Normal read-only module check

```text
dimm_power off
set HSA strap manually
dimm_power cycle
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

### HSA address experiment

```text
dimm_power off
set HSA strap manually
hsa note low|resistor|high|floating
dimm_power cycle
powerdiag
scan
record observed hub address
```

### Optional DRAM rail / VR enable experiment

```text
dimm_power cycle
scan
pmicid
pmicdump
vr_enable on
powerdiag
pmicdump
vr_enable off
```

This is not required for basic SPD/PMIC sideband communication. Use it only when intentionally observing PMIC regulator / DRAM rail behavior.

### Recovery/write workflow

```text
capturegood        # known-good stick only
dimm_power off
install target stick
set HSA for intended write/offline state
dimm_power cycle
scan
spd dump          # backup target before writes
compare
writegood         # only after confirmations
verifygood
```

## UI behavior notes

For a web UI or serial CLI:

- Show current GPIO32 DIMM power state.
- Show GPIO33 as optional `vr_enable`, not `hub_enable`.
- Show GPIO34 PWR_GOOD state only as optional PMIC/rail status.
- Show recorded HSA note.
- Show whether VIN_BULK has been cold-cycled since HSA note changed.
- Put write commands behind a separate danger section.
- Do not make write buttons visually similar to read/dump buttons.
- Treat LEDs as optional debug outputs, not required system state.

## Short version

The diagnostic firmware should be boring by default:

```text
read first
dump first
compare first
repeat-test first
write only when absolutely intentional
```

PWR_EN is not part of the required SPD/PMIC read path. Treat it as optional VR/DRAM-rail experimentation, not hub enable.

The tool exists to prevent guessing, not to automate expensive mistakes at microcontroller speed.
