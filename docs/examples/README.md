# Examples And Evidence

[Back to README](../../README.md) | [Quick start](../quick-start.md) | [Troubleshooting](../troubleshooting.md)

This folder contains curated example captures and diagnostic evidence.

## Folders

- [comparisons](comparisons/README.md): human-readable before/after and side-by-side evidence guides.
- [dumps](dumps/README.md): original known-good OLOy SPD dump/report examples.
- [hub-registers](hub-registers/README.md): historical protected/unprotected hub register comparison.
- [repair-cases](repair-cases/README.md): bad-stick management-plane repair evidence.

## Comparison Guides

- [How to read the examples](comparisons/README.md)
- [XMP/EXPO DDR5-5200 to DDR5-5600 edit](comparisons/xmp-expo-ddr5-5200-to-5600-edit.md)
- [Bad-stick management-plane before/after](comparisons/bad-stick-management-plane-before-after.md)
- [Hub register protected vs unprotected](comparisons/hub-register-protected-vs-unprotected.md)

## Evidence Boundary

The examples document management-plane behavior: SPD payload bytes, CRC/checksum status, hub access, PMIC access, and repeat-read stability. They do not prove DRAM cell health or memory-controller training stability.
