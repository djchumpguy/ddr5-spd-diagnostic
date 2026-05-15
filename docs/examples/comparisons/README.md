# Example Comparisons

[Examples](../README.md) | [Back to README](../../../README.md)

The files under `docs/examples` are curated evidence, not generic DDR5 truth tables. Read them as snapshots from one lab setup, one module family, and one investigation.

## How To Read These Examples

Start with the comparison pages, then open the raw files when you want to verify the original command output.

- [XMP/EXPO DDR5-5200 to DDR5-5600 edit](xmp-expo-ddr5-5200-to-5600-edit.md)
- [Bad-stick management-plane before/after](bad-stick-management-plane-before-after.md)
- [Hub register protected vs unprotected](hub-register-protected-vs-unprotected.md)

## What "Before" And "After" Mean

| Term | Meaning | What it can prove | What it cannot prove |
| --- | --- | --- | --- |
| Original known-good SPD | SPD dump/report captured from a known-good reference module. | Baseline bytes and decoded profile values for comparison. | That another DIMM is healthy. |
| Repaired/restored SPD payload | Known-good/original SPD payload written back to a suspect/corrupt SPD image and read back. | Management-plane write/readback/compare/CRC evidence. | That the DIMM boots or that DRAM is healthy. |
| Tweaked XMP/EXPO profile | Experimental profile byte changes with CRC/checksum repair and readback verification. | Preview math, expected byte writes, CRC/checksum repair, and management-plane readback. | BIOS acceptance, POST, memory training, or DRAM stability. |
| Hub register historical context | Protected/unprotected hub register snapshots from the investigation. | That hub register state differed in historical captures. | That MR12/MR13 was the final active cause. |
| PMIC snapshot | PMIC ID/register dump and optional comparison to a PMIC reference. | PMIC management-plane access and register comparison. | DRAM-side power integrity or memory stability. |

## Evidence Boundary

The examples prove management-plane behavior only: SPD bytes, hub access, PMIC access, CRC/checksum status, compare results, and repeat-read stability. The documented bad stick still did not boot/work. Current best conclusion points toward DRAM-side/training-path failure, not an active MR12/MR13 mismatch.
