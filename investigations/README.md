# Investigations

This folder contains the good-vs-bad DDR5 module investigation notes.

## Current conclusion

The strongest current conclusion is a **likely DRAM-side / training-path
failure**, inferred from motherboard boot sniffer data.

The key evidence is that the bad-stick boot capture reaches SPD/HUB traffic at
`0x53` and PMIC traffic at `0x4B`, then diverges/stops much earlier than the
good-stick boot capture.

This is an inference from comparative boot behavior, not direct proof of the exact failed DRAM IC, bank, lane, or training step.

## Read these in order

1. [`final-diagnosis-dram-failure.md`](final-diagnosis-dram-failure.md)  
   Current top-level diagnosis and evidence framing.

2. [`good-vs-bad-boot-sniffer-divergence.md`](good-vs-bad-boot-sniffer-divergence.md)  
   Current good-vs-bad boot sniffer comparison.

3. [`good-vs-bad-stick.md`](good-vs-bad-stick.md)  
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
SPD/HUB and PMIC sideband communication appear functional enough to participate in bring-up.
Good-vs-bad boot sniffing points to likely DRAM-side / training-path failure after early management communication.
```
