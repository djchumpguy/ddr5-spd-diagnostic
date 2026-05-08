# Safety Boundaries

This project intentionally pokes at a live DDR5 module's management electronics. That is useful. It is also a fine way to turn hardware into modern art if treated casually.

## Electrical safety rules

- Use a current-limited bench/breadboard supply where possible.
- Keep ESP32 power separate from switched DIMM VIN_BULK.
- Tie grounds together: DIMM, PSU, ESP32, PCA9306, MOSFET control circuit, and LEDs.
- Do not feed ESP32 power through the DIMM power switch.
- Pull PWR_EN and PWR_GOOD to 3.3 V, not 5 V.
- Level shift the sideband bus; do not directly connect ESP32 3.3 V I2C to the DIMM HSCL/HSDA lines.
- Confirm PCA9306 orientation before applying power.

## Write safety rules

- Read and archive the full SPD before attempting any write.
- Capture register state before and after changes.
- Treat write/offline mode as a separate state requiring a deliberate cold power cycle.
- Keep a hardware way to remove DIMM power immediately.
- Never mix “probe what happens” and “write” in the same command path.
- Latched blue/danger LED should only mean an actually dangerous write-state, not merely “armed maybe probably.”

## Repository safety rule

Do not commit raw private serial logs that include unrelated system information. Keep raw captures in `logs/raw/` locally; `.gitignore` keeps common raw/binary log files out unless deliberately added.
