# ESP32 DDR5 Boot Sideband Sniffer

## Status

Working prototype passive sniffer for early DDR5 boot I2C / I2C-compatible sideband traffic.

## What It Is

This is a passive SCL/SDA sniffer intended for boot-time DDR5 SPD/PMIC sideband captures. It records compact decoded events into RAM during the capture window, then dumps the result after capture over serial or Bluetooth depending on the selected board profile.

It is useful for comparing good-vs-bad boot sequences without making the ESP32 an active participant on the bus.

The firmware target is:

Passive DDR5 boot I2C / I2C-compatible sideband sniffer

## What It Is Not

This is not a full I3C analyzer. It does not drive SCL or SDA, is not an active bus participant, and is not guaranteed for true I3C push-pull high-speed traffic.

DDR5 SPD5 hubs can support I2C and I3C sideband operation. This firmware
targets I2C / I2C-compatible boot sideband traffic. It may observe some
I3C-compatible framing during early boot, but it should not be treated as a full
I3C analyzer. True I3C push-pull/high-speed phases may exceed ESP32 GPIO
sampling reliability.

Bigger storage can extend capture length, but it does not make a normal ESP32 GPIO sniffer a complete I3C protocol analyzer.

## Board Profiles

Profiles are selected at compile time by defining exactly one `SNIFFER_PROFILE_*` macro. If none is defined, the default is `SNIFFER_PROFILE_WROOM_LOW_RAM`.

Available profiles:

- `SNIFFER_PROFILE_WROOM_LOW_RAM`: preserves the original low-RAM behavior with small fixed RAM decoded-event buffers and Bluetooth after capture.
- `SNIFFER_PROFILE_PSRAM_BUFFERED`: uses larger decoded-event limits and marks PSRAM/raw-edge-buffer capability for larger boards. Capture still happens first, then dump after capture.
- `SNIFFER_PROFILE_SD_LOGGER`: uses larger decoded-event limits and enables a post-capture SD export placeholder. It does not stream every edge or event live to SD.
- `SNIFFER_PROFILE_USB_SERIAL_FAST`: keeps the same decode core but prefers USB serial dump and disables Bluetooth after capture.

Example `platformio.ini` override:

```ini
build_flags =
  -DCORE_DEBUG_LEVEL=0
  -DSNIFFER_PROFILE_USB_SERIAL_FAST
  -DNODE_NAME=\"sniffer-b\"
```

Per-board identity defaults:

- `NODE_NAME`: `"sniffer-a"`
- `BUS_LABEL`: `"ddr5-sideband"`
- `FW_VERSION`: `"sniffer-v0.2"`

## Wiring

- SCL input pin: GPIO34 by default.
- SDA input pin: GPIO35 by default.
- Optional trigger input: disabled by default. If enabled, `TRIGGER_PIN` defaults to GPIO27.
- Common ground is required.
- Sniffer pins must be input-only/passive.
- Use level shifting or buffering if tapping true DDR5 low-voltage sideband directly.

## Safety

- The sniffer must not drive DDR5 bus lines.
- Only one active bus controller should exist.
- Do not connect 3.3 V outputs to 1.8 V or 1.0 V sideband lines.
- Direct sniffing may work on the bench, but buffering and level shifting are safer.

## Commands

Commands are available over USB serial. Bluetooth commands are available after capture only for profiles that enable Bluetooth after capture.

```text
status
summary
arm
dump
dump <start> <end>
dumpcsv
exportsd
clear
stop
help
```

`dumpcsv` exports already-decoded events without changing capture behavior:

```text
idx,time_us,dt_us,type,value,flags,ack
```

`exportsd` is currently a guarded placeholder. In `SNIFFER_PROFILE_SD_LOGGER` builds it prints:

```text
SD export not implemented in this build.
```

## Dump Metadata

Every dump begins with metadata like:

```text
# DDR5_BOOT_SNIFFER_DUMP v0.2
# node=sniffer-a
# profile=WROOM_LOW_RAM
# capture_scope=boot_i2c_compatible_sideband
# full_i3c_analyzer=false
# scl_pin=34
# sda_pin=35
# trigger_pin=disabled
# max_events=1024
# capture_window_ms=1200
# bus_idle_done_ms=20
# overflow=no
# events=0
```

## Optional External Trigger

External trigger support is compile-time disabled by default:

```ini
build_flags =
  -DENABLE_EXTERNAL_TRIGGER=1
  -DTRIGGER_PIN=27
```

When enabled, `arm` clears the buffer and waits for a rising edge on `TRIGGER_PIN`. The trigger inserts a `TRIGGER` marker event into the dump and then allows the normal SCL/SDA capture logic to run. A future controller ESP32 can drive this trigger pin, but the boards must share ground.

Normal use does not require an external trigger.

