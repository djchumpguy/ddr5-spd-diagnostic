#pragma once

// Board/profile selection.
//
// Define exactly one of these in platformio.ini build_flags to select a board:
//   -DSNIFFER_PROFILE_WROOM_LOW_RAM
//   -DSNIFFER_PROFILE_PSRAM_BUFFERED
//   -DSNIFFER_PROFILE_SD_LOGGER
//   -DSNIFFER_PROFILE_USB_SERIAL_FAST
//
// The default preserves the original low-RAM WROOM behavior.
#if !defined(SNIFFER_PROFILE_WROOM_LOW_RAM) && \
    !defined(SNIFFER_PROFILE_PSRAM_BUFFERED) && \
    !defined(SNIFFER_PROFILE_SD_LOGGER) && \
    !defined(SNIFFER_PROFILE_USB_SERIAL_FAST)
#define SNIFFER_PROFILE_WROOM_LOW_RAM 1
#endif

#if (defined(SNIFFER_PROFILE_WROOM_LOW_RAM) + \
     defined(SNIFFER_PROFILE_PSRAM_BUFFERED) + \
     defined(SNIFFER_PROFILE_SD_LOGGER) + \
     defined(SNIFFER_PROFILE_USB_SERIAL_FAST)) != 1
#error Define exactly one SNIFFER_PROFILE_* board profile.
#endif

// Per-board identity. Override NODE_NAME in platformio.ini for each sniffer.
#ifndef NODE_NAME
#define NODE_NAME "sniffer-a"
#endif

#ifndef BUS_LABEL
#define BUS_LABEL "ddr5-sideband"
#endif

#ifndef FW_VERSION
#define FW_VERSION "sniffer-v0.2"
#endif

// Optional trigger input. Disabled by default; normal auto-arm behavior remains.
#ifndef ENABLE_EXTERNAL_TRIGGER
#define ENABLE_EXTERNAL_TRIGGER 0
#endif

#ifndef TRIGGER_PIN
#define TRIGGER_PIN 27
#endif

// This firmware targets I2C / I2C-compatible boot sideband traffic. It may
// observe some I3C-compatible framing during early boot, but it should not be
// treated as a full I3C analyzer. True I3C push-pull/high-speed phases may
// exceed ESP32 GPIO sampling reliability.

#if defined(SNIFFER_PROFILE_WROOM_LOW_RAM)
#define SNIFFER_PROFILE_NAME "WROOM_LOW_RAM"
#define SNIFFER_STORAGE_CAPABILITY "small fixed RAM decoded-event buffer"
#define SNIFFER_MAX_EVENTS 1024
#define SNIFFER_MAX_DECODED_BYTES 768
#define SNIFFER_CAPTURE_WINDOW_MS 1200
#define SNIFFER_USE_PSRAM 0
#define SNIFFER_ENABLE_SD_EXPORT 0
#define SNIFFER_RAW_EDGE_BUFFER_MODE 0
#define SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE 1
#define SNIFFER_PREFER_USB_SERIAL_DUMP 0
#elif defined(SNIFFER_PROFILE_PSRAM_BUFFERED)
#define SNIFFER_PROFILE_NAME "PSRAM_BUFFERED"
#define SNIFFER_STORAGE_CAPABILITY "larger RAM/PSRAM decoded-event buffer"
#define SNIFFER_MAX_EVENTS 4096
#define SNIFFER_MAX_DECODED_BYTES 3072
#define SNIFFER_CAPTURE_WINDOW_MS 2500
#define SNIFFER_USE_PSRAM 1
#define SNIFFER_ENABLE_SD_EXPORT 0
#define SNIFFER_RAW_EDGE_BUFFER_MODE 1
#define SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE 1
#define SNIFFER_PREFER_USB_SERIAL_DUMP 0
#elif defined(SNIFFER_PROFILE_SD_LOGGER)
#define SNIFFER_PROFILE_NAME "SD_LOGGER"
#define SNIFFER_STORAGE_CAPABILITY "RAM capture with post-capture SD export placeholder"
#define SNIFFER_MAX_EVENTS 4096
#define SNIFFER_MAX_DECODED_BYTES 3072
#define SNIFFER_CAPTURE_WINDOW_MS 2500
#define SNIFFER_USE_PSRAM 0
#define SNIFFER_ENABLE_SD_EXPORT 1
#define SNIFFER_RAW_EDGE_BUFFER_MODE 0
#define SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE 1
#define SNIFFER_PREFER_USB_SERIAL_DUMP 0
#elif defined(SNIFFER_PROFILE_USB_SERIAL_FAST)
#define SNIFFER_PROFILE_NAME "USB_SERIAL_FAST"
#define SNIFFER_STORAGE_CAPABILITY "RAM capture with USB serial dump preferred"
#define SNIFFER_MAX_EVENTS 2048
#define SNIFFER_MAX_DECODED_BYTES 1536
#define SNIFFER_CAPTURE_WINDOW_MS 2000
#define SNIFFER_USE_PSRAM 0
#define SNIFFER_ENABLE_SD_EXPORT 0
#define SNIFFER_RAW_EDGE_BUFFER_MODE 0
#define SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE 0
#define SNIFFER_PREFER_USB_SERIAL_DUMP 1
#endif
