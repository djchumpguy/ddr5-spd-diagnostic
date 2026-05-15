# Safety

[Back to README](../README.md) | [Quick start](quick-start.md) | [Troubleshooting](troubleshooting.md)

This repository describes experimental DDR5 lab work. Treat every step as capable of damaging hardware.

## Main Risks

- wrong voltage on DDR5 sideband or power pins,
- shorted or floating temporary wiring,
- incorrect VIN_BULK or PWR_EN sequencing,
- interpreting PWR_GOOD without understanding the harness,
- changing HSA without a real DIMM power cycle,
- write commands applied to the wrong module or address,
- assuming CRC/readback means the DIMM will boot.

## Safer Defaults

- Start read-only.
- Verify rails before connecting a DIMM.
- Use current-limited supplies where practical.
- Prefer a proper adapter PCB for repeated use.
- Add strain relief to every temporary wire.
- Keep the active SPD tool and passive sniffer wiring separate.
- Do not run write commands unless you can afford to lose the module.

## Write Operations

SPD writes are dangerous. Advanced SPD editing is experimental. The DDR5-5600 EXPO/XMP edit path has proven preview/write/readback/CRC behavior only, not BIOS/POST/memory stability.

CRC/checksum repair confirms bytes/checksum. Readback confirms the bytes were written. Neither confirms bootability.

## Hardware Interpretation

PWR_GOOD must be interpreted based on the configured harness. HSA GPIO state may not be authoritative if HSA is physically strapped. Direct-ground/offline-style `0x50` and floating/high `0x57` behavior are harness-dependent observations from this project.
