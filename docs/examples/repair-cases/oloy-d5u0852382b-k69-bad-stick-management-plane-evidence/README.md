# OLOy D5U0852382B-K69 Bad-Stick Management-Plane Evidence

[Repair cases](../README.md) | [Examples](../../README.md) | [Back to README](../../../../README.md)

This is not a confirmed full DIMM repair. It is management-plane repair evidence.

The known-good/original SPD payload could be restored and verified at the SPD management-plane level. The evidence includes readback, compare, CRC/checksum behavior, hub access, PMIC access, and repeat-read stability. The bad stick still does not boot/work.

Current best conclusion: boot-time/sniffer behavior points toward DRAM-side failure, not an active SPD hub MR12/MR13 mismatch. The MR12/MR13 protected/unprotected snapshots are historical context.

## Files

- [bad-stick-spd-payload-verifies-against-known-good.txt](bad-stick-spd-payload-verifies-against-known-good.txt)
- [bad-post-repair-0x50-management-plane-clean.txt](bad-post-repair-0x50-management-plane-clean.txt)
- [bad-post-repair-mapall.txt](bad-post-repair-mapall.txt)
- [bad-stick-bios-style-reads.txt](bad-stick-bios-style-reads.txt)
- [bad-stick-hub-register-snapshot.txt](bad-stick-hub-register-snapshot.txt)
- [bad-stick-pmic-snapshot.txt](bad-stick-pmic-snapshot.txt)
- [bad-stick-repair-stability-suite.txt](bad-stick-repair-stability-suite.txt)

Speed/stability evidence here is I2C/SPD/PMIC bus-read stability only. It is not DRAM cell or memory-controller stability.
