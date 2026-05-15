# Good vs Bad Boot Sniffer Divergence

## Summary

The known-good and suspect DDR5 modules both show usable SPD/HUB sideband
communication. The suspect module also reaches PMIC sideband traffic. The
suspect capture diverges/stops much earlier than the known-good baseline after
SPD/HUB and PMIC communication appear functional.

Current working conclusion: likely DRAM-side / training-path failure, inferred
from boot-time sniffer divergence, not an active SPD/PMIC sideband access
failure.

Repeatability: the project observed the high-level good-vs-bad divergence in
more than one capture set. The current organized examples focus on the
management-plane repair evidence; this note preserves the sniffer interpretation
without treating the WROOM captures as complete protocol traces.

## Captures compared

| Capture | File | Stick | Events | Overflow | Notes |
|---|---|---|---:|---|---|
| Good baseline | historical boot-sniffer capture | known-good | 1024 | yes | Retained WROOM buffer fills during continued `0x53` activity |
| Bad divergence | historical boot-sniffer capture | suspect | 337 | no | Reaches `0x53` and `0x4B` then stops/settles earlier |

## Good-stick baseline behavior

- Early probes/NACKs are present.
- Host reaches active SPD/HUB traffic at `0x53`.
- The `WROOM_LOW_RAM` retained buffer fills; `overflow=yes` is expected on this
  low-RAM profile and does not indicate failure.
- Continued retained `0x53` activity appears through the end of the buffer.

## Bad-stick behavior

- Early probes/NACKs are present.
- SPD/HUB traffic at `0x53` ACKs.
- PMIC traffic at `0x4B` appears.
- PMIC register `0x32` traffic appears consistent with VR-enable-style
  sequencing.
- The capture stops/settles much earlier than the good baseline.
- The PMIC should not be described as dead or missing based on this capture.

## Interpretation

Sideband SPD/HUB access appears functional enough to enumerate/read. PMIC
sideband access appears functional enough to participate in bring-up. The
suspect sequence diverges after the sideband bring-up phase.

This supports a DRAM-side / training / initialization-path failure inference.

## Historical context

Earlier MR12/MR13 protection-register mismatch remains historical investigation
context. It is not the current active diagnosis unless new data points back to
it. Current strongest evidence is boot sniffer divergence.

## Caveats

- `WROOM_LOW_RAM` captures are retained windows, not complete unlimited boot
  traces.
- The sniffer targets boot I2C / I2C-compatible sideband traffic, not full I3C
  decode.
- Some odd addresses or NACK/data combinations may be decoder artifacts around
  repeated-start/special-address behavior.
- More captures with larger retained buffers or persistent storage could deepen
  analysis, but are optional future extensions rather than prerequisites for the
  current conclusion.
