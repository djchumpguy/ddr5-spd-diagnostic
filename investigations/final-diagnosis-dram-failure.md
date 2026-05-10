# Current Diagnosis - Likely DRAM-Side Failure from Boot Sniffer Data

## Status

This is the current high-level conclusion for the DDR5 diagnostic project.

The strongest current finding is that the suspect module likely has a
DRAM-side / training-path failure, inferred from sniffer data captured during
motherboard boot initialization.

This replaces earlier working hypotheses that focused on SPD hub state, PMIC state, or SPD write-protection register mismatches.

## Conclusion

The suspect stick appears to fail during motherboard initialization because the
DRAM-side bring-up/training process does not complete correctly.

The SPD hub, SPD payload, PMIC communication, and basic sideband bus behavior
were investigated and did not remain the active root cause. The bad-stick boot
capture reaches SPD/HUB sideband traffic at `0x53` and PMIC sideband traffic at
`0x4B` before diverging/stopping much earlier than the known-good baseline.

## Evidence path

The investigation moved through several layers:

| Layer | Result / interpretation |
|---|---|
| ESP32 sideband communication | Functional enough for repeat reads and diagnostics |
| SPD payload | Recovered / matched against known-good reference during testing |
| SPD hub register access | Readable and stable enough for comparison |
| PMIC communication | Readable; no persistent PMIC-only root cause identified |
| MR11 | Matched; not active root cause |
| MR12/MR13 | Historical mismatch only; later resolved / no longer active |
| Motherboard boot sniffer data | Shows meaningful divergence supporting likely DRAM-side / training-path failure |

- Post-repair direct/offline management-plane evidence:
  [`bad-post-repair-0x50-management-plane-clean.txt`](../logs/examples/spd-tool/bad-post-repair-0x50-management-plane-clean.txt)
  records PMIC compare MATCH, SPD compare MATCH, and MR12/MR13 corrected to
  FF/3C. The remaining failure is after management-plane repair.

## Why this matters

Earlier stages of the project made the bad stick look like it might be failing because of corrupted SPD contents, SPD hub mode state, write-protection/finalization state, or PMIC setup.

The boot sniffer data moved the diagnosis past that.

The important distinction is:

```text
SPD/PMIC sideband access can look healthy
while the DRAM array / training / initialization path still fails.
```

That means a stick can have:

- readable SPD
- matching recovered SPD payload
- stable hub reads
- PMIC visibility
- corrected/historical MR12/MR13 state

…and still fail as a RAM module because the actual DRAM-side initialization path fails.

## Current interpretation

Treat the suspect stick as likely failing in the DRAM-side / training /
initialization path, not merely as a corrupted-SPD module.

The ESP32 harness and sniffer work were still valuable because they ruled out easier causes and showed where the failure moved during real motherboard boot behavior.

## Relationship to MR12/MR13 investigation

The older MR12/MR13 mismatch was a useful intermediate clue, but it is not the final diagnosis.

Current handling:

- Keep MR12/MR13 notes as historical investigation context.
- Do not present MR12/MR13 as the active cause.
- Do not restart the diagnosis from SPD protection state unless new captures show a fresh mismatch.

## Relationship to HSA/addressing notes

HSA strap state and hub address behavior remain important for reproducing ESP32 reads and avoiding false conclusions.

However, the HSA/addressing behavior explains sideband access differences, not
the inferred DRAM-side / training-path boot failure.

## What to preserve from sniffer captures

When adding or summarizing boot sniffer logs, preserve:

- Good-stick boot sequence summary
- Bad-stick boot sequence summary
- First meaningful divergence point
- PMIC enable / VR enable sequence observations
- SPD hub address used during motherboard access
- Any repeated NACK/ACK pattern differences
- Timing/order differences around DRAM initialization
- Whether the bad stick reaches VR enable before divergence
- Whether failure happens after sideband setup appears successful

## Recommended repo language

Use language like:

```text
Current conclusion: likely DRAM-side / training-path failure inferred from
good-vs-bad boot sniffer divergence after SPD/HUB and PMIC sideband
communication appeared functional.
```

Avoid saying:

```text
The stick failed because of MR12/MR13.
```

or:

```text
The stick is only SPD-corrupted.
```

Those were earlier hypotheses, not the final state.

## Short version

The bad stick does not appear to be just an SPD/PMIC sideband access problem.

The sideband path was debugged deeply enough to support a DRAM-side
bring-up/training-path inference from real motherboard boot sniffing.
