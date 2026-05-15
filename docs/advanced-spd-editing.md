# Advanced SPD Editing

[Back to README](../README.md) | [Safety](safety.md) | [Reference vs checkpoint](reference-vs-checkpoint.md) | [Command reference](spd-tool/command-reference.md)

Advanced SPD editing is experimental. Normal users should stay with read-only diagnostics unless they have a sacrificial module and understand the recovery path.

![Advanced SPD editing warning](images/ui/esp32-spd-tool-advanced-spd-editing-warning-dark.png)

## Current Evidence Boundary

The DDR5-5600 EXPO/XMP edit path has proven preview/write/readback/CRC behavior only. It has not proven BIOS/POST/memory stability.

Voltage editing uses verified encodings only. Timing/profile edits must be previewed, written, read back, and compared, but that still does not prove the module will train.

For the exact byte-level management-plane edit evidence, see [XMP/EXPO DDR5-5200 to DDR5-5600 edit](examples/comparisons/xmp-expo-ddr5-5200-to-5600-edit.md).

## tCK Math

For DDR data rate in MT/s:

```text
tCK_ps = 2000000 / DDR data rate MT/s
```

Integer picosecond storage can make DDR5-5600 display as derived around DDR5-5602. That display artifact does not by itself prove a bad edit.

## Before Any Write

- Save a diagnostic SPD reference.
- Save a tweak checkpoint.
- Confirm the current dump is from the intended module/address.
- Confirm CRC/checksum preview behavior.
- Assume the module may become unbootable.

![EXPO editor expanded](images/ui/esp32-spd-tool-expo-editor-expanded-dark.png)

## What Success Means

Preview success means the firmware can calculate proposed bytes. Write/readback success means the management plane returned those bytes. CRC/checksum success means the payload math checks out.

None of those prove DRAM-side health, BIOS acceptance, POST, or memory-test stability.
