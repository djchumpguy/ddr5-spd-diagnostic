# SPD Tool Example Logs

These logs document active ESP32 SPD/PMIC tool behavior from the suspect DDR5
module after SPD payload repair and MR12/MR13 correction.

## Post-repair bad-stick evidence

| Log | Purpose |
|---|---|
| `bad-post-repair-stability-suite.txt` | Repeated read-only stability checks showing power/readiness, SPD, hub, and PMIC access stability |
| `bad-post-repair-mapall-normal.txt` | Normal/released-HSA map showing SPD/HUB at 0x53 and PMIC at 0x4B |
| `bad-post-repair-spd-verify-known-good.txt` | Verification that repaired bad-stick SPD matches the known-good reference |
| `bad-post-repair-bios-style-reads.txt` | BIOS-style/legacy SPD read path evidence |
| `bad-post-repair-hub-register-snapshot.txt` | Hub register snapshot after MR12/MR13 correction |
| `bad-post-repair-pmic-snapshot.txt` | PMIC identity/dump/compare evidence |
| `bad-post-repair-0x50-management-plane-clean.txt` | Offline/direct-ground style 0x50/0x48 evidence showing SPD compare MATCH and PMIC compare MATCH |

These logs support the conclusion that the suspect module's SPD/HUB/PMIC
management plane was repaired and stable after intervention, while the module
still failed motherboard boot/training. That remaining failure is therefore
treated as likely DRAM-side / training-path failure, not active SPD/PMIC
corruption.
