# Boot Sniffer

## Purpose

The boot sniffer is a passive ESP32 firmware image for capturing early DDR5 boot
sideband activity. It watches SCL/SDA and records compact decoded events during
the boot window, then dumps the retained capture after the window closes.

Firmware location:

[`firmware/esp32-boot-sniffer/`](../firmware/esp32-boot-sniffer/)

Known-good example baseline:

[`logs/examples/sniffer/good-stick-boot-0x53-baseline.txt`](../logs/examples/sniffer/good-stick-boot-0x53-baseline.txt)

The intended use is to compare known-good and suspect module boot sequences,
especially the point where the suspect module diverges from normal SPD/HUB or
PMIC sideband behavior.

## Electrical Rules

The sniffer must be passive. It must not drive SCL or SDA, and it must not become
an active bus participant.

Common ground is required between the sniffer and the system being observed.

If tapping true DDR5 low-voltage sideband lines directly, level shifting or
buffering is safer than direct ESP32 GPIO attachment.

## Wiring reference

For the physical wiring schematic, see:

[Passive Boot Sniffer Wiring](../hardware/passive-boot-sniffer-wiring.md)

That document covers the corrected passive harness setup: GPIO34/GPIO35 are
read-only sniff inputs, the motherboard remains the bus master, and ESP32 GND is
referenced from a motherboard USB header ground pin rather than a DDR5 socket
ground probe.

## Prototype probe photos

Prototype probe photos are included with the passive wiring schematic:
[Passive Boot Sniffer Wiring](../hardware/passive-boot-sniffer-wiring.md)

## Capture workflow

### RAM-buffer / no-SD-card workflow

This is the workflow used for the current WROOM-style sniffer profile where the
capture is stored in RAM.

1. Power the ESP32 sniffer from a source that will stay on independently of the
   PC being tested.
   - A wall USB charger or battery bank works.
   - Avoid powering it only from the PC being tested if unplugging or shutdown
     would reset the ESP32.
   - If the ESP32 loses power, the RAM capture is erased.

2. Connect to the sniffer before powering on the PC.
   - Use a Bluetooth serial app on a phone, or a PC Bluetooth/serial terminal.
   - Confirm the sniffer responds to:

     ```text
     status
     ```

3. Arm the sniffer before motherboard power-on.
   - Send:

     ```text
     clear
     arm
     status
     ```

   - The sniffer should be armed before the motherboard begins DDR5
     initialization.

4. Power on the PC / motherboard.
   - Let the board boot, fail training, or sit for about 60 seconds.
   - Do not reset or power-cycle the ESP32 during this time.

5. Dump the capture before removing ESP32 power.
   - From phone Bluetooth serial app, enable logging first if possible.
   - From PC terminal, use a logfile if possible.
   - Useful commands:

     ```text
     status
     summary
     dump
     ```

6. If the terminal/app cannot handle the full dump at once, dump in segments.
   - Use the event range shown by `summary`.
   - Examples:

     ```text
     dump 0 199
     dump 200 399
     dump 400 599
     dump 600 799
     dump 800 1023
     ```

   - Adjust the final range based on the event count in `summary`.

7. Optional CSV export:
   - If the serial app or terminal is logging, run:

     ```text
     dumpcsv
     ```

   - `dumpcsv` prints CSV text over serial. It does not magically create a file
     on the phone unless the serial app is logging.

8. Save the dump file.
   - Suggested names:

     ```text
     good-stick-boot-sniffer.txt
     bad-stick-boot-sniffer.txt
     good-stick-boot-dumpcsv.txt
     bad-stick-boot-dumpcsv.txt
     ```

> [!WARNING]
> On RAM-only sniffer builds, the capture is volatile. Powering off, unplugging, resetting, or re-flashing the ESP32 erases the captured boot traffic. Dump and save the data before changing power or reconnecting the board.

### Phone Bluetooth serial dump

- Connect to the sniffer over Bluetooth serial.
- Start logging/recording in the app before running `dump` or `dumpcsv`.
- Run `status`, `summary`, then `dump`.
- Stop logging and export/share the saved text file.
- If logging was not enabled, the dump may only exist in terminal scrollback.

### PC terminal dump

- If the ESP32 can remain powered while connected to the PC, USB serial is
  easiest.
- If the ESP32 is powered separately and Bluetooth is being used, a PC can
  connect through Bluetooth serial/rfcomm/picocom.
- Use terminal logging so the dump is saved directly to a text file.
- Do not unplug the ESP32 from its separate power source just to move it to the
  PC if doing so would erase RAM.

### SD-card / persistent-storage boards

- Boards with SD card, PSRAM plus export support, or persistent storage can use
  larger/different workflows.
- On storage-capable builds, prefer saving/exporting to storage before power
  removal.
- The current documented baseline captures were made with a small fixed RAM
  decoded-event buffer, so the RAM workflow remains the safest default.

## Scope

This is a boot I2C / I2C-compatible sideband sniffer, not a full I3C analyzer.
It may observe useful early boot traffic, but normal ESP32 GPIO sampling should
not be treated as complete coverage for true I3C push-pull or high-speed phases.

Bigger ESP32 boards with PSRAM or SD storage can support longer retained
captures. Storage does not magically make an ESP32 a full I3C analyzer.

## Profiles

The current firmware profile family is:

- `WROOM_LOW_RAM`
- `PSRAM_BUFFERED`
- `SD_LOGGER`
- `USB_SERIAL_FAST`

`WROOM_LOW_RAM` captures a limited retained window. In the included known-good
baseline, `overflow=yes` means the 1024-event buffer filled; it does not mean the
boot failed.

## Current Baseline

`good-stick-boot-0x53-baseline.txt` is a good-stick boot baseline, not bad-stick
failure data. The dump metadata records:

- `profile=WROOM_LOW_RAM`
- `capture_scope=boot_i2c_compatible_sideband`
- `full_i3c_analyzer=false`
- `overflow=yes`
- `events=1024`

Active SPD/HUB traffic appears at `0x53` in the captured HSA/strap state.

## Good-vs-bad comparison

The sniffer is most useful when comparing a known-good baseline against a
suspect module under the same motherboard/slot/HSA/strap conditions.

Compared captures and investigation note:

- [`logs/examples/sniffer/good-stick-boot-0x53-baseline.txt`](../logs/examples/sniffer/good-stick-boot-0x53-baseline.txt)
- [`logs/examples/sniffer/bad-stick-boot-divergence.txt`](../logs/examples/sniffer/bad-stick-boot-divergence.txt)
- [`investigations/good-vs-bad-boot-sniffer-divergence.md`](../investigations/good-vs-bad-boot-sniffer-divergence.md)

The current comparison supports likely DRAM-side / training-path failure because
the suspect module reaches SPD/HUB and PMIC sideband traffic before
diverging/stopping earlier than the known-good baseline.
