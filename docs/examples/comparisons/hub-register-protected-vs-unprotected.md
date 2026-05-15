# Hub Register Protected Vs Unprotected

[Comparisons](README.md) | [Hub register files](../hub-registers/README.md) | [Back to README](../../../README.md)

These snapshots preserve historical SPD hub register context from the investigation.

## Source Files

- [Protected/factory-like snapshot](../hub-registers/oloy-d5u0852382b-k69-hub-regs-protected-mr12-ff-mr13-3c.txt)
- [Unprotected/offline-altered snapshot](../hub-registers/oloy-d5u0852382b-k69-hub-regs-unprotected-mr12-00-mr13-00.txt)

## Compact Comparison

| Register area | Protected/factory-like snapshot | Unprotected/offline-altered snapshot | Why it mattered |
| --- | --- | --- | --- |
| MR12 | `0xFF` in the protected snapshot line | `0x00` in the unprotected snapshot | Suggested a protected/unprotected mode difference during the historical investigation. |
| MR13 | `0x3C` in the protected snapshot line | `0x00` in the unprotected snapshot | Helped explain why hub state seemed suspicious early on. |
| Other bytes | Mostly similar zero-heavy context with some differences | Mostly similar zero-heavy context with some differences | Useful context, but not enough by itself for root cause. |

## Why This Was Useful

The protected/unprotected comparison helped separate SPD payload corruption from SPD hub mode/register state. It also showed why HSA/address mode and access path must be recorded when interpreting hub registers.

## Why It Is Not The Final Diagnosis

These register differences are historical diagnostic context only. Later management-plane evidence showed restored SPD payload, hub access, PMIC access, and repeat-read stability could pass while the DIMM still failed. Current best conclusion points toward DRAM-side/training-path failure, not an active MR12/MR13 mismatch.

## HSA / Address Interpretation

HSA and address behavior are harness-dependent. Direct-ground/offline-style access around `0x50`, normal/resistor-strapped behavior around `0x53`, and floating/high behavior around `0x57` were observations from this project. Record the physical HSA strap, declared hardware config, observed address, and whether VIN_BULK was cold-cycled before comparing hub registers.
