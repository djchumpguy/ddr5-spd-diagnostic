# Bad-Stick Management-Plane Before/After

[Comparisons](README.md) | [Repair case files](../repair-cases/oloy-d5u0852382b-k69-bad-stick-management-plane-evidence/README.md) | [Back to README](../../../README.md)

This page explains the bad-stick evidence in human terms. It is not a confirmed DIMM
repair story.

## Short Version

The suspect DIMM originally had corrupted/suspect SPD management-plane state. A
known-good/original SPD payload was restored and then verified through management-plane
reads. Afterward, the SPD payload compared cleanly, hub access worked, PMIC access
worked, and repeat-read stability checks passed.

The DIMM still did not boot/work. Current best conclusion remains
DRAM-side/training-path failure, not an active SPD hub MR12/MR13 mismatch.

## Before

The investigation began with a bad stick that did not work normally and had
suspect/corrupt SPD-side state. Historical hub-register captures also showed
protected/unprotected differences, including MR12/MR13 differences, but those are
diagnostic context rather than the final active diagnosis.

## After Management-Plane Restoration

The known-good SPD payload was restored to the suspect DIMM and read back. The available
evidence shows:

- SPD payload compared against the known-good reference with `COMPARE: MATCH`.
- Scan/autodetect found expected management-plane devices.
- SPD/HUB access worked at the observed address for the current harness state.
- PMIC ID and PMIC register snapshots were readable.
- PMIC compare matched the saved PMIC reference in the captured evidence.
- Repeat-read timing/stability checks passed at the management plane.

## Compact Evidence Table

| Check | Evidence file | Result | Interpretation |
| --- | --- | --- | --- |
| SPD payload verify against known-good | [bad-stick-spd-payload-verifies-against-known-good.txt](../repair-cases/oloy-d5u0852382b-k69-bad-stick-management-plane-evidence/bad-stick-spd-payload-verifies-against-known-good.txt) | Pass: `COMPARE: MATCH` | Restored/readback SPD payload matched the diagnostic reference. |
| Scan/autodetect | [bad-post-repair-mapall.txt](../repair-cases/oloy-d5u0852382b-k69-bad-stick-management-plane-evidence/bad-post-repair-mapall.txt) | Pass: PMIC, SPD/HUB, reserved address found | Management-plane devices responded in current mode. |
| Hub access | [bad-stick-hub-register-snapshot.txt](../repair-cases/oloy-d5u0852382b-k69-bad-stick-management-plane-evidence/bad-stick-hub-register-snapshot.txt) | Pass: repeated register reads matched | Hub register access was stable in that capture. |
| PMIC ID/snapshot | [bad-stick-pmic-snapshot.txt](../repair-cases/oloy-d5u0852382b-k69-bad-stick-management-plane-evidence/bad-stick-pmic-snapshot.txt) | Pass: PMIC ID readable and compare MATCH | PMIC management-plane access and reference compare worked. |
| Speed/stability suite | [bad-stick-repair-stability-suite.txt](../repair-cases/oloy-d5u0852382b-k69-bad-stick-management-plane-evidence/bad-stick-repair-stability-suite.txt) | Pass: repeated SPD/register reads matched | I2C/SPD/PMIC bus-read stability evidence only. |
| Boot result | Investigation conclusion | Still failed / no working-DIMM result | Management-plane success did not become a working DIMM. |

## What This Proves

- The known-good SPD payload could be restored and verified at the management plane.
- Hub access and PMIC access could pass after restoration.
- Repeat-read stability can pass even on the bad stick.

## What This Does Not Prove

- That the bad stick was fixed.
- That the DIMM can POST.
- That DRAM cells are healthy.
- That memory-controller training succeeds.
- That MR12/MR13 was the final active cause.

The current best conclusion points toward DRAM-side/training-path failure.
