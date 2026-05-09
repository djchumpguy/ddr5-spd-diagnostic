# Investigations

This folder contains the good-vs-bad DDR5 module investigation notes.

## Current conclusion

The strongest current conclusion is a **likely DRAM-side failure**, inferred from motherboard boot sniffer data.

The key evidence is that the bad-stick boot capture diverged from the good-stick boot capture after early SPD/PMIC sideband communication appeared normal and the system would have been moving toward DRAM initialization/training.

This is an inference from comparative boot behavior, not direct proof of the exact failed DRAM IC, bank, lane, or training step.

## Read these in order

1. [`final-diagnosis-dram-failure.md`](final-diagnosis-dram-failure.md)  
   Current top-level diagnosis and evidence framing.

2. [`good-vs-bad-stick.md`](good-vs-bad-stick.md)  
   Historical good-vs-bad investigation record, including the earlier MR12/MR13 hypothesis.

## Historical notes

The earlier MR12/MR13 write-protection mismatch was useful during the investigation, but it is no longer the active explanation.

Current handling:

- Keep MR12/MR13 notes as historical context.
- Do not present MR12/MR13 as the current root cause.
- Do not restart the diagnosis from SPD protection state unless new captures show a fresh mismatch.

## Good investigation hygiene

When adding future captures or notes, record:

- Stick identity
- HSA strap condition
- Whether HSA was manually strapped or GPIO-controlled
- Whether VIN_BULK was fully cold-cycled
- Observed hub address
- PMIC/local-device address
- Whether SPD communication looked normal
- Whether PMIC communication / VR enable looked normal
- First meaningful divergence point between good and bad captures
- What was directly observed versus what was inferred

## Short version

The current repo story is:

```text
SPD/PMIC/hub investigation ruled out the easier sideband-only explanations.
Good-vs-bad boot sniffing points to likely DRAM-side failure after early management communication.
```
