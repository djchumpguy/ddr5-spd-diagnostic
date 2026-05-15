# XMP/EXPO DDR5-5200 To DDR5-5600 Edit

[Comparisons](README.md) | [Advanced SPD editing](../../advanced-spd-editing.md) | [Back to README](../../../README.md)

This page documents one successful **management-plane** edit test. It does not prove BIOS/POST success, memory-controller training, or DRAM stability.

## Summary

| Item | Original profile | Edited profile |
| --- | --- | --- |
| Profile speed | DDR5-5200-ish | DDR5-5600-ish |
| tCK | 384 ps | staged 357 ps, derived about DDR5-5602 |
| VDD / VDDQ | 1.25 V / 1.25 V | 1.35 V / 1.35 V |
| Primary timings | about 38-40-40-76 | 40-42-42-80 |
| tRC | 116 | 122 |
| tWR | 78 | 82 |
| tRFC1 | 767/768 | 821 |
| tRFC2 | 416/417 | 448 |
| tRFCsb | 338/339 | 364 |
| Integrity | original CRC/checksum | CRC/checksum repaired |
| Verification | original dump | write/readback/dump matched expected bytes |

## EXPO Byte Changes

| Offset | Field | Original | Edited | Note |
| --- | --- | --- | --- | --- |
| `0x34A` | VDD | `25` | `35` | 1.25 V to 1.35 V encoding. |
| `0x34B` | VDDQ | `25` | `35` | 1.25 V to 1.35 V encoding. |
| `0x34E-0x34F` | tCK | `80 01` | `65 01` | 384 ps to 357 ps staged tCK. |
| `0x351` | tAA | `39` | `38` | Decodes under new tCK. |
| `0x353` | tRCD | `3C` | `3B` | Decodes under new tCK. |
| `0x355` | tRP | `3C` | `3B` | Decodes under new tCK. |
| `0x357` | tRAS | `72` | `70` | Decodes under new tCK. |
| `0x359` | tRC | `AE` | `AA` | Decodes under new tCK. |
| `0x35A-0x35B` | tWR | `30 75` | `5A 72` | Decodes under new tCK. |
| `0x35C-0x35D` | tRFC1 | `27 01` | `25 01` | Decodes under new tCK. |
| `0x35E-0x35F` | tRFC2 | `A0 00` | `A0 00` | Raw no-op; decodes differently under new tCK. |
| `0x360` | tRFCsb | `82` | `82` | Raw no-op; decodes differently under new tCK. |
| `0x3BE-0x3BF` | EXPO CRC | `AB D0` | `44 C6` | CRC repaired. |

## XMP Byte Changes

| Offset | Field | Original | Edited | Note |
| --- | --- | --- | --- | --- |
| `0x2C1` | VDD | `25` | `35` | 1.25 V to 1.35 V encoding. |
| `0x2C2` | VDDQ | `25` | `35` | 1.25 V to 1.35 V encoding. |
| `0x2C5-0x2C6` | tCK | `80 01` | `65 01` | 384 ps to 357 ps staged tCK. |
| `0x2CE` | tAA | `39` | `38` | Decodes under new tCK. |
| `0x2D0` | tRCD | `3C` | `3B` | Decodes under new tCK. |
| `0x2D2` | tRP | `3C` | `3B` | Decodes under new tCK. |
| `0x2D4` | tRAS | `72` | `70` | Decodes under new tCK. |
| `0x2D6` | tRC | `AE` | `AA` | Decodes under new tCK. |
| `0x2D7-0x2D8` | tWR | `30 75` | `5A 72` | Decodes under new tCK. |
| `0x2D9-0x2DA` | tRFC1 | `27 01` | `25 01` | Decodes under new tCK. |
| `0x2DB-0x2DC` | tRFC2 | `A0 00` | `A0 00` | Raw no-op; decodes differently under new tCK. |
| `0x2DD` | tRFCsb | `82` | `82` | Raw no-op; decodes differently under new tCK. |
| `0x2FE-0x2FF` | XMP CRC/checksum | `BD C9` | `EC 0E` | Checksum/CRC repaired. |

## Why Some Raw Timing Bytes Decrease

Some timing bytes decrease even though the cycle counts increase. The profile fields are encoded as absolute-ish time values, while the displayed cycle count depends on tCK. Raising the data rate lowers tCK, so the same or lower raw absolute time can decode as more cycles under the faster tCK.

The firmware uses:

```text
tCK_ps = 2000000 / DDR data rate MT/s
```

Integer picosecond storage can make DDR5-5600 display as a derived rate around DDR5-5602.

## What This Proved

- Preview math used staged tCK correctly.
- Expected bytes were written and read back.
- EXPO/XMP CRC/checksum repair produced accepted payload math.
- The management-plane bus remained stable enough for dump/readback verification.

## What This Did Not Prove

- BIOS profile acceptance.
- POST success.
- DRAM stability.
- Memory-controller training success.
- Safe overclock settings.
- That SPD editing is generally safe or stable.
