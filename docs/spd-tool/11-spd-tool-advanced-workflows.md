# SPD Tool Advanced Workflows

This page documents advanced experimental workflows that can be attempted with
the current ESP32 DDR5 SPD/PMIC diagnostic tool command surface. Some workflows
are already proven in limited bench testing, such as writing a known-good SPD
payload to a corrupted SPD and verifying the result. Other workflows are
possible through existing register/SPD/PMIC commands but remain unvalidated or
high risk.

> [!WARNING]
> These workflows can modify SPD contents, SPD hub registers, write-protection state, PMIC registers, power state, or timing/profile data. A bad write can permanently confuse a DIMM, prevent POST, corrupt SPD contents, or damage hardware if PMIC/power behavior is changed incorrectly. Treat anything beyond read/compare/dump as experimental bench work.

## Workflow confidence labels

| Label | Meaning |
|---|---|
| Proven on bench | Already performed successfully in this project at least once |
| Current-command capable | Uses existing command surface, but not fully tested for this exact workflow |
| Read-only | Should not modify SPD, hub, PMIC, or power state |
| Dangerous write | Can modify persistent SPD/hub/PMIC state or affect module power behavior |
| Recovery-oriented | Intended to restore, compare, or validate a suspect/corrupt module |
| Exploratory | Useful for investigation, but not yet tied to a validated repair outcome |

## Current command surface

The command names below are based on the checked-in firmware command parser/help
text. Exact syntax should still be verified against the firmware help output for
the checked-in firmware revision using `help`, `advanced`, or `danger`.

| Family | Known/example commands | Use |
|---|---|---|
| Power and strap control | `status`, `en on`, `en off`, `dimmpwr`, `dimmpwr on`, `dimmpwr off`, `dimmpwr cycle`, `hsa`, `hsa release`, `hsa ground`, `hsa normal`, `hsa write`, `powerdiag` / `pdiag` | Inspect/control test mode, normal mode, and cold-cycle behavior |
| Bus mapping | `scan`, `autodetect` / `detect` / `roles`, `mapall` / `map` | See what responds in the current mode without assuming address state |
| SPD reads/dumps | `read`, `dump`, `biosmr11`, `biosread` / `bread`, `biosdump` / `bdump`, `biosinteresting` / `bint` | Read SPD contents and BIOS-style legacy access behavior |
| SPD compare/recovery | `capturegood` / `cg`, `compare` / `cmp`, `verifygood` / `vg`, `writegood` / `wg yes`, `autofix` / `af` | Compare or restore SPD payloads where supported; `autofix` is disabled in this build |
| Hub register access | `reg`, `editreg yes`, `timereg` / `treg` | Read or write SPD hub registers and test stability |
| PMIC access | `pmicid`, `pmicdump`, `pmicdumpat`, `capturepmic` / `cpmic`, `comparepmic` / `cmpmic`, `pmicref`, `clearpmic` / `clpmic`, `reg`, `editreg yes` if targeting a PMIC address | Inspect PMIC identity/status/config state |
| Timing diagnostics | `timespd` / `tspd`, `timereg` / `treg` | Repeat reads to check bus/register stability |
| Logging/UI | Web log, serial terminal, `clearlog`, `flash` | Capture output and command history |

## Proven workflow: known-good SPD restore

Labels: Proven on bench, Dangerous write, Recovery-oriented.

Conceptual flow:

```text
capturegood
compare
writegood yes
verifygood
compare
```

A known-good SPD image can be captured and stored by the tool. A
suspect/corrupt SPD can be compared against that known-good image. The tool has
been used in this project to write the good SPD payload to the corrupt module.

Verification should include a byte-for-byte compare and repeat reads/timing
tests such as `verifygood`, `compare`, and `timespd`. This proves the tool can
perform practical SPD recovery operations, but does not prove every failed module
is SPD-fixable.

Cautions:

- Only write an SPD image from an actually matching module/configuration.
- Do not write random SPD data.
- Do not assume a successful SPD write fixes DRAM-side failure.
- After restore, compare boot-sniffer behavior if the module still fails
  training.

## Advanced workflow: hub register comparison/editing

Labels: Current-command capable, Exploratory, Dangerous write when using
`editreg yes`.

The SPD hub exposes volatile/configuration registers that can be read and, in
some modes, written. The current tool's generic register access can be used to
compare good vs suspect module register state.

Read-only flow:

```text
hsa normal
dimmpwr cycle
mapall
reg <hub-address> <register>
timereg <hub-address> <register>
```

Experimental write flow:

```text
hsa write
dimmpwr cycle
reg <hub-address> <register>
editreg yes <hub-address> <register> <value>
reg <hub-address> <register>
timereg <hub-address> <register>
```

Do not present this as universally safe. Hub register writes may depend on
HSA/offline mode, power-cycle timing, and write-protection behavior.

MR12/MR13/protection-map edits belong here as historical/secondary
investigation context only. They are useful to document as register-edit
capability, but they are not the current active failure conclusion.

## Post-repair suspect-module evidence

The suspect module originally had corrupted SPD contents. A known-good SPD
payload was written and verified, and the MR12/MR13 protection-map mismatch was
corrected. Post-repair logs show stable SPD/HUB/PMIC access and MATCH results,
but the module still hangs during motherboard boot/training. That supports the
current conclusion of likely DRAM-side / training-path failure after
SPD/HUB/PMIC management-plane repair and validation.

- [`bad-post-repair-stability-suite.txt`](../../logs/examples/spd-tool/bad-post-repair-stability-suite.txt)
- [`bad-post-repair-mapall-normal.txt`](../../logs/examples/spd-tool/bad-post-repair-mapall-normal.txt)
- [`bad-post-repair-spd-verify-known-good.txt`](../../logs/examples/spd-tool/bad-post-repair-spd-verify-known-good.txt)
- [`bad-post-repair-bios-style-reads.txt`](../../logs/examples/spd-tool/bad-post-repair-bios-style-reads.txt)
- [`bad-post-repair-hub-register-snapshot.txt`](../../logs/examples/spd-tool/bad-post-repair-hub-register-snapshot.txt)
- [`bad-post-repair-pmic-snapshot.txt`](../../logs/examples/spd-tool/bad-post-repair-pmic-snapshot.txt)
- [`bad-post-repair-0x50-management-plane-clean.txt`](../../logs/examples/spd-tool/bad-post-repair-0x50-management-plane-clean.txt)

## Advanced workflow: PMIC behavior exploration

Labels: Read-only for dumps/compares, Current-command capable, Exploratory,
Dangerous write if using `editreg yes` against a PMIC address.

The PMIC can often be investigated more effectively through before/after
register state than through a single dump.

### PMIC identity and baseline dump

```text
pmicid
pmicdump
```

Purpose:

- Identify responding PMIC address/state.
- Save a baseline before any edits.
- Compare good vs suspect behavior.

### PMIC before/after event comparison

Conceptual flow:

```text
pmicdump > before
power / mode / boot event
pmicdump > after
compare before after
```

Shell redirection is not firmware syntax. Capture the output from the Web log or
serial log if the terminal is not running on a shell.

Register areas to compare conceptually:

- PMIC status/fault registers
- VR enable/control behavior
- Configuration/protection state
- Power-good/fault latch behavior

Do not label the PMIC failed from isolated NACKs if later transactions ACK
again. Prefer sequence-level interpretation.

### PMIC experimental edits

If the current generic register command can write PMIC registers at the target
address, then PMIC edits are possible but high risk.

Potential PMIC edit classes:

- VR enable/control register behavior
- Fault/status clear bits
- Write-protection/configuration bits
- Power sequencing configuration
- Warning/fault masks

PMIC writes can affect voltage regulator behavior. Do not attempt PMIC writes
without a known-good dump, current limiting where possible, and a clear recovery
path. Never blindly copy PMIC config between non-identical modules.

## Advanced workflow: SPD timing/profile table editing

Labels: Current-command capable, Dangerous write, Exploratory.

Because the tool can write SPD data, it can theoretically modify SPD
timing/profile fields such as speed tables, JEDEC timing data, or vendor profile
areas if the user intentionally edits the SPD payload before writing.

This is current-command capable because the tool can write SPD payloads, but the
actual table editing happens outside the tool unless the firmware/UI later adds
dedicated field editors.

Possible workflow:

```text
dump suspect SPD
save original backup
edit SPD payload externally
recalculate/update required checksum/CRC fields if applicable
write modified SPD
verifygood or compare against modified target
boot test
```

Cautions:

- SPD timing/profile edits can easily produce a module that reads correctly but
  will not train.
- Motherboards may reject invalid timing/profile combinations.
- Do not test aggressive edits on hardware you care about.
- Always keep a byte-for-byte backup of the original SPD.
- Prefer small, reversible changes.

Examples of possible SPD table experiments:

- Changing advertised speed bin
- Editing JEDEC timing fields
- Disabling or altering optional profile blocks
- Matching a suspect module's SPD to a known-good identical module
- Creating a conservative fallback SPD profile for testing

Exact byte offsets are intentionally not listed here unless they are already
documented elsewhere in the repo.

## Suggested advanced test report template

| Area | Command/output | Good module | Suspect module | Notes |
|---|---|---|---|---|
| Current mode/HSA | `powerdiag` / `mapall` | | | |
| SPD payload compare | `compare` / `verifygood` | | | |
| Hub MR11/MR12/MR13 | `reg` / `timereg` | | | Historical/secondary context |
| PMIC identity | `pmicid` | | | |
| PMIC dump | `pmicdump` | | | |
| Timing stability | `timespd` / `timereg` | | | |
| Boot sniffer result | external sniffer dump | | | Good-vs-bad divergence |

## Guardrails for dangerous commands

- Always dump before writing.
- Always compare after writing.
- Always record HSA mode, observed address, and power state.
- Use read-only flows first.
- Never assume `0x50` / `0x53` / `0x57` means the same thing unless HSA state
  and wiring are known.
- Keep active SPD tool wiring separate from passive sniffer wiring.
- GPIO34 is PWR_GOOD in the active SPD tool harness.
- GPIO34 is HSCL sniff in the passive sniffer harness.
- Do not mix these two harness assumptions.
