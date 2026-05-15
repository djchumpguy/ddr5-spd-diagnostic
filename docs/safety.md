# Safety

[Back to README](../README.md) | [Quick start](quick-start.md) | [Troubleshooting](troubleshooting.md)

The current basic read setup is simpler than earlier versions of this project, but it is still not consumer-safe. You are wiring power and management signals directly to a DDR5 module through a lab adapter.

## Main Risks

- wrong voltage on DDR5 sideband or power pins,
- missing shared ground between ESP32 and DIMM supply,
- shorted or floating adapter wiring,
- incorrect VIN_BULK or PWR_EN handling,
- interpreting PWR_GOOD without understanding the harness,
- changing HSA without a real DIMM power cycle,
- write commands applied to the wrong module or address,
- assuming CRC/readback means the DIMM will boot.

## Safer Defaults

- Start read-only.
- Verify rails before connecting a DIMM.
- Use current-limited supplies where practical.
- Prefer a proper adapter PCB for repeated use.
- Add strain relief to every adapter wire.
- Keep the active SPD tool and passive sniffer wiring separate.
- Do not run write commands unless you can afford to lose the module.

## Minimum Setup Does Not Mean Universal Safety

In this lab setup, ESP32 GPIO21/GPIO22 connected directly to HSDA/HSCL on a DDR5 adapter, using the ESP32 internal SDA/SCL pull-ups. No PCA9306 level shifter or external SDA/SCL pull-ups were required for successful basic reads.

That does not make the setup universal for every DDR5 module, adapter, or bench supply. Other harnesses may need protection, level shifting, or extra pull-ups.

## Write Operations

SPD writes are dangerous. Advanced SPD editing is experimental. The DDR5-5600 EXPO/XMP edit path has proven preview/write/readback/CRC behavior only, not BIOS/POST/memory stability.

CRC/checksum repair confirms bytes/checksum. Readback confirms the bytes were written. Neither confirms bootability.

## Hardware Interpretation

PWR_GOOD must be interpreted based on the configured harness. HSA GPIO state may not be authoritative if HSA is physically strapped. Direct-ground/offline-style `0x50` and floating/high `0x57` behavior are harness-dependent observations from this project.
