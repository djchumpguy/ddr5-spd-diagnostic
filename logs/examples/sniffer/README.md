# Sniffer Example Captures

This folder contains sanitized example sniffer captures.

These are passive boot I2C / I2C-compatible sideband captures. They are not full
I3C analyzer captures. Direct event-by-event interpretation can include decoder
artifacts around repeated-start or special-address behavior, so comparison work
should focus on the larger sequence and where a suspect module diverges from a
known-good baseline.

## Captures

- `good-stick-boot-0x53-baseline.txt`
  - Known-good module boot baseline.
  - Captured with the `WROOM_LOW_RAM` profile.
  - `overflow=yes` means the 1024-event retained buffer filled; this does not
    indicate failure.
  - Active SPD/HUB traffic appears at `0x53`.

- `bad-stick-boot-divergence.txt`
  - Suspect module boot capture.
  - Reaches SPD/HUB traffic at `0x53` and PMIC traffic at `0x4B`.
  - Stops/settles much earlier than the known-good baseline.
  - Supports a DRAM-side / training-path failure inference after SPD/HUB and
    PMIC sideband communication appear functional.
