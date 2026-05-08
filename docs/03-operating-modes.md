# Operating Modes

## Normal mode

Use this when reading SPD, dumping registers, comparing state, and doing non-invasive diagnostics.

Sequence:

1. Set HSA to released/input/high.
2. Turn DIMM power off with GPIO32.
3. Delay long enough for cold reset.
4. Turn DIMM power on with GPIO32.
5. Enable PWR_EN.
6. Wait for PWR_GOOD.
7. Scan/read/dump.

## Write / offline tester mode

Use only when deliberately entering programming/recovery workflows.

Sequence:

1. Set HSA forced low with GPIO27.
2. Turn DIMM power off with GPIO32.
3. Delay long enough for cold reset.
4. Turn DIMM power on with GPIO32.
5. Enable PWR_EN.
6. Wait for PWR_GOOD.
7. Run readback first.
8. Only then perform unlock/write/protection changes.

## Why PWR_EN alone is not enough

PWR_EN does not guarantee the hub re-samples HSA. The current harness therefore treats GPIO32-controlled VIN_BULK removal as the authoritative cold-power operation.
