#include <Arduino.h>
#include <WiFi.h>
#include "driver/gpio.h"
#include "sniffer_config.h"
#if SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE
#include "BluetoothSerial.h"
#endif
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// ============================================================
// Passive DDR5 boot I2C / I2C-compatible sideband sniffer
//
// INPUT ONLY
// no Wire.begin()
// no internal pullups
// WiFi OFF
//
// Strategy:
//   - optional Bluetooth OFF during capture, depending on profile
//   - auto-arm on boot
//   - wait for bus activity
//   - capture/decode compact events into RAM
//   - force-stop on deterministic limits
//   - THEN start Bluetooth
//
// Commands available after BT starts (and always on USB serial):
//   status
//   summary
//   dump
//   dump <start> <end>
//   dumpcsv
//   stop
//   clear
//   arm
//   help
//
// Notes:
//   - This firmware targets I2C / I2C-compatible boot sideband traffic.
//   - It may observe some I3C-compatible framing during early boot, but it
//     should not be treated as a full I3C analyzer.
//   - True I3C push-pull/high-speed phases may exceed ESP32 GPIO sampling
//     reliability.
//   - "arm"/"clear" after BT is already on are useful for bench tests,
//     but a true cold-boot sniff is still best done from fresh power-up.
// ============================================================

#if SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled for this build target.
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Bluetooth SPP is not enabled / not available for this ESP32 target.
#endif
#endif

// -----------------------------
// Sniffer pins
// -----------------------------
static const gpio_num_t PIN_SCL = GPIO_NUM_34;   // yellow / HSCL tap
static const gpio_num_t PIN_SDA = GPIO_NUM_35;   // green  / HSDA tap
#if ENABLE_EXTERNAL_TRIGGER
static const gpio_num_t PIN_TRIGGER = (gpio_num_t)TRIGGER_PIN;
#endif

#define STR_HELPER(s) #s
#define STR(s) STR_HELPER(s)
#if ENABLE_EXTERNAL_TRIGGER
static const char* TRIGGER_PIN_LABEL = STR(TRIGGER_PIN);
#else
static const char* TRIGGER_PIN_LABEL = "disabled";
#endif

// -----------------------------
// Serial / Bluetooth
// -----------------------------
static const uint32_t SERIAL_BAUD = 115200;
static const char* BT_NAME = "DDR5_I2C_SNIFFER";

#if SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE
BluetoothSerial SerialBT;
#endif
static bool btOnline = false;
static bool btStartAttempted = false;

// -----------------------------
// Capture limits (recommended low-RAM / boot-friendly)
// -----------------------------
static const uint32_t BOOT_FAILSAFE_MS   = 5000;  // always stop capture by this time after boot
static const uint32_t CAPTURE_WINDOW_MS  = SNIFFER_CAPTURE_WINDOW_MS;  // stop this long after first valid bus activity
static const uint32_t BUS_IDLE_DONE_MS   = 20;    // stop early if bus goes quiet
static const uint16_t MAX_EVENTS         = SNIFFER_MAX_EVENTS;  // compact decoded records
static const uint16_t MAX_DECODED_BYTES  = SNIFFER_MAX_DECODED_BYTES;  // addr+data bytes recorded

// 0.5 us start/stop glitch reject, converted to integer cycles at runtime
static uint32_t gCpuMHz = 240;
static uint32_t gCyclesPerMs = 240000;
static uint32_t gGlitchCycles = 120;

// ============================================================
// Compact decoded event log
//
// type:
//   0 = START
//   1 = ADDR
//   2 = DATA
//   3 = STOP
//   4 = MARKER
//   5 = OVERFLOW
//   6 = TRIGGER
//   7 = TIMEOUT
//   8 = IDLE_STOP
//
// flags:
//   bit0 = R/W for ADDR (1=read)
//   bit1 = ACK state (0=ACK, 1=NACK) for ADDR/DATA
// ============================================================
enum : uint8_t {
  EVT_START = 0,
  EVT_ADDR  = 1,
  EVT_DATA  = 2,
  EVT_STOP  = 3,
  EVT_MARKER = 4,
  EVT_OVERFLOW = 5,
  EVT_TRIGGER = 6,
  EVT_TIMEOUT = 7,
  EVT_IDLE_STOP = 8
};

struct __attribute__((packed)) DecodedEvent {
  uint16_t dtUs;   // delta from previous logged event, clamped to 65535 us
  uint8_t type;
  uint8_t value;
  uint8_t flags;
};

static DecodedEvent evBuf[MAX_EVENTS];

// ============================================================
// Capture state
// ============================================================
static volatile uint16_t evCount = 0;
static volatile uint16_t overflowCount = 0;
static volatile uint16_t totalDecodedBytes = 0;

static volatile bool captureEnabled = true;
static volatile bool captureStarted = false;
static volatile bool captureDone = false;

static volatile bool sawAnyActivity = false;

static volatile uint32_t bootCycle = 0;
static volatile uint32_t firstActivityCycle = 0;
static volatile uint32_t lastActivityCycle = 0;

static volatile uint32_t captureStartCycle = 0;
static volatile uint32_t captureStopCycle = 0;

static volatile uint32_t lastSdaEdgeCycle = 0;
static volatile uint32_t lastLoggedCycle = 0;

static volatile bool waitingForExternalTrigger = false;
static volatile bool externalTriggerSeen = false;

// decoder state
static volatile bool inTransaction = false;
static volatile bool afterStartAwaitingAddress = false;
static volatile bool byteReadyForAck = false;
static volatile uint8_t bitCount = 0;
static volatile uint8_t currentByte = 0;

// terminal input
static char cmdBuf[64];
static uint8_t cmdLen = 0;

// interrupt attach state
static bool snifferInterruptsAttached = false;

// ============================================================
// Forward declarations
// ============================================================
void IRAM_ATTR isrSCL();
void IRAM_ATTR isrSDA();
void IRAM_ATTR stopCaptureISR();
void IRAM_ATTR noteAnyActivity(uint32_t nowCycles);
void IRAM_ATTR onStartCondition(uint32_t nowCycles);
void IRAM_ATTR onStopCondition(uint32_t nowCycles);
void IRAM_ATTR onSclRiseSample(uint32_t nowCycles);
#if ENABLE_EXTERNAL_TRIGGER
void IRAM_ATTR isrTrigger();
#endif

// ============================================================
// Helpers
// ============================================================
static inline uint32_t fastCycles() {
  return ESP.getCycleCount();
}

static inline uint32_t cyclesToUsInt(uint32_t cycles) {
  return (gCpuMHz == 0) ? 0 : (cycles / gCpuMHz);
}

static inline uint32_t cyclesToMsInt(uint32_t cycles) {
  return (gCyclesPerMs == 0) ? 0 : (cycles / gCyclesPerMs);
}

void outPrint(const char* s) {
  Serial.print(s);
#if SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE
  if (btOnline && SerialBT.hasClient()) SerialBT.print(s);
#endif
}

void outPrintln(const char* s) {
  Serial.println(s);
#if SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE
  if (btOnline && SerialBT.hasClient()) SerialBT.println(s);
#endif
}

void outPrintlnf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  outPrintln(buf);
}

const char* ackName(uint8_t nackBit) {
  return nackBit ? "NACK" : "ACK";
}

const char* eventTypeName(uint8_t type) {
  switch (type) {
    case EVT_START: return "START";
    case EVT_ADDR: return "ADDR";
    case EVT_DATA: return "DATA";
    case EVT_STOP: return "STOP";
    case EVT_MARKER: return "MARKER";
    case EVT_OVERFLOW: return "OVERFLOW";
    case EVT_TRIGGER: return "TRIGGER";
    case EVT_TIMEOUT: return "TIMEOUT";
    case EVT_IDLE_STOP: return "IDLE_STOP";
    default: return "UNKNOWN";
  }
}

static inline void resetDecoderStateISR() {
  inTransaction = false;
  afterStartAwaitingAddress = false;
  byteReadyForAck = false;
  bitCount = 0;
  currentByte = 0;
}

void attachSnifferInterrupts() {
  if (snifferInterruptsAttached) return;
  attachInterrupt(digitalPinToInterrupt((int)PIN_SCL), isrSCL, RISING);
  attachInterrupt(digitalPinToInterrupt((int)PIN_SDA), isrSDA, CHANGE);
  snifferInterruptsAttached = true;
}

void detachSnifferInterrupts() {
  if (!snifferInterruptsAttached) return;
  detachInterrupt(digitalPinToInterrupt((int)PIN_SCL));
  detachInterrupt(digitalPinToInterrupt((int)PIN_SDA));
  snifferInterruptsAttached = false;
}

void armCapture() {
  detachSnifferInterrupts();

  memset((void*)evBuf, 0, sizeof(evBuf));

  noInterrupts();
  evCount = 0;
  overflowCount = 0;
  totalDecodedBytes = 0;

  captureEnabled = true;
  captureStarted = false;
  captureDone = false;
  waitingForExternalTrigger = false;
  externalTriggerSeen = false;

  sawAnyActivity = false;

  bootCycle = fastCycles();
  firstActivityCycle = 0;
  lastActivityCycle = 0;

  captureStartCycle = 0;
  captureStopCycle = 0;

  lastSdaEdgeCycle = fastCycles();
  lastLoggedCycle = 0;

  resetDecoderStateISR();
  interrupts();

  // BT state intentionally not changed here.
  // btStartAttempted remains whatever it already was for this boot.
#if ENABLE_EXTERNAL_TRIGGER
  noInterrupts();
  captureEnabled = false;
  waitingForExternalTrigger = true;
  interrupts();
#endif
  attachSnifferInterrupts();
}

void stopCaptureInternal() {
  noInterrupts();
  captureEnabled = false;
  if (!captureDone) {
    captureDone = true;
    captureStopCycle = fastCycles();
  }
  interrupts();

  detachSnifferInterrupts();
}

void IRAM_ATTR stopCaptureISR() {
  captureEnabled = false;
  if (!captureDone) {
    captureDone = true;
    captureStopCycle = fastCycles();
  }
}

// ============================================================
// Compact logging
// ============================================================
bool IRAM_ATTR logDecodedEvent(uint8_t type, uint8_t value, uint8_t flags, uint32_t nowCycles) {
  uint16_t idx = evCount;
  if (idx >= MAX_EVENTS) {
    overflowCount++;
    return false;
  }

  uint32_t ref = (lastLoggedCycle == 0) ? nowCycles : lastLoggedCycle;
  uint32_t dtCycles = nowCycles - ref;
  uint32_t dtUs32 = cyclesToUsInt(dtCycles);
  uint16_t dtUs = (dtUs32 > 65535u) ? 65535u : (uint16_t)dtUs32;

  evBuf[idx].dtUs  = dtUs;
  evBuf[idx].type  = type;
  evBuf[idx].value = value;
  evBuf[idx].flags = flags;

  evCount = idx + 1;
  lastLoggedCycle = nowCycles;
  return true;
}

#if ENABLE_EXTERNAL_TRIGGER
void IRAM_ATTR isrTrigger() {
  if (!waitingForExternalTrigger || captureDone) return;

  uint32_t nowCycles = fastCycles();
  waitingForExternalTrigger = false;
  externalTriggerSeen = true;
  captureEnabled = true;
  bootCycle = nowCycles;
  logDecodedEvent(EVT_TRIGGER, (uint8_t)TRIGGER_PIN, 0, nowCycles);
}
#endif

// ============================================================
// Decoder ISR logic
// ============================================================
void IRAM_ATTR noteAnyActivity(uint32_t nowCycles) {
  if (!captureEnabled || captureDone) return;

  lastActivityCycle = nowCycles;
  if (!sawAnyActivity) {
    sawAnyActivity = true;
    firstActivityCycle = nowCycles;
  }
}

void IRAM_ATTR onStartCondition(uint32_t nowCycles) {
  if (!captureEnabled || captureDone) return;

  if (!captureStarted) {
    captureStarted = true;
    captureStartCycle = nowCycles;
    lastLoggedCycle = nowCycles;
  }

  resetDecoderStateISR();

  if (!logDecodedEvent(EVT_START, 0, 0, nowCycles)) {
    stopCaptureISR();
    return;
  }

  inTransaction = true;
  afterStartAwaitingAddress = true;
  byteReadyForAck = false;
  bitCount = 0;
  currentByte = 0;
}

void IRAM_ATTR onStopCondition(uint32_t nowCycles) {
  if (!captureEnabled || captureDone) return;
  if (!inTransaction) return;

  if (!logDecodedEvent(EVT_STOP, 0, 0, nowCycles)) {
    stopCaptureISR();
    return;
  }

  resetDecoderStateISR();
}

void IRAM_ATTR onSclRiseSample(uint32_t nowCycles) {
  if (!captureEnabled || captureDone) return;
  if (!inTransaction) return;

  uint8_t sda = gpio_get_level(PIN_SDA);

  if (!byteReadyForAck) {
    currentByte = (uint8_t)((currentByte << 1) | (sda & 0x1));
    bitCount++;

    if (bitCount >= 8) {
      bitCount = 0;
      byteReadyForAck = true;
    }
    return;
  }

  uint8_t nackBit = sda & 0x1;
  uint8_t flags = (nackBit ? 0x02 : 0x00);

  if (afterStartAwaitingAddress) {
    uint8_t addr7 = (currentByte >> 1) & 0x7F;
    uint8_t rw = currentByte & 0x1;
    if (rw) flags |= 0x01;

    if (!logDecodedEvent(EVT_ADDR, addr7, flags, nowCycles)) {
      stopCaptureISR();
      return;
    }

    afterStartAwaitingAddress = false;
    totalDecodedBytes++;
  } else {
    if (!logDecodedEvent(EVT_DATA, currentByte, flags, nowCycles)) {
      stopCaptureISR();
      return;
    }

    totalDecodedBytes++;
  }

  currentByte = 0;
  byteReadyForAck = false;
}

// ============================================================
// GPIO ISRs
// ============================================================
void IRAM_ATTR isrSCL() {
  if (!captureEnabled || captureDone) return;

  uint32_t now = fastCycles();
  noteAnyActivity(now);
  onSclRiseSample(now);
}

void IRAM_ATTR isrSDA() {
  if (!captureEnabled || captureDone) return;

  uint32_t now = fastCycles();
  uint32_t dtCycles = now - lastSdaEdgeCycle;
  lastSdaEdgeCycle = now;

  if (dtCycles < gGlitchCycles) {
    return;
  }

  uint8_t scl = gpio_get_level(PIN_SCL);
  if (scl != 1) return;

  noteAnyActivity(now);

  uint8_t sda = gpio_get_level(PIN_SDA);

  if (sda == 0) {
    onStartCondition(now);
  } else {
    onStopCondition(now);
  }
}

// ============================================================
// Summary / status / dump
// ============================================================
void printStatus() {
  uint16_t count, ov, totalBytes;
  bool enabled, started, done, anyActivity, waitingTrigger, triggerSeen;
  uint32_t bootC, firstC, lastC, startC, stopC;
  uint8_t scl, sda;

  noInterrupts();
  count = evCount;
  ov = overflowCount;
  totalBytes = totalDecodedBytes;
  enabled = captureEnabled;
  started = captureStarted;
  done = captureDone;
  anyActivity = sawAnyActivity;
  waitingTrigger = waitingForExternalTrigger;
  triggerSeen = externalTriggerSeen;
  bootC = bootCycle;
  firstC = firstActivityCycle;
  lastC = lastActivityCycle;
  startC = captureStartCycle;
  stopC = captureStopCycle;
  interrupts();

  scl = gpio_get_level(PIN_SCL);
  sda = gpio_get_level(PIN_SDA);

  outPrintln("# status");
  outPrintlnf("# node=%s bus=%s fw=%s profile=%s",
              NODE_NAME,
              BUS_LABEL,
              FW_VERSION,
              SNIFFER_PROFILE_NAME);
  outPrintlnf("# enabled=%s started=%s done=%s rawActivity=%s btOnline=%s client=%s",
              enabled ? "yes" : "no",
              started ? "yes" : "no",
              done ? "yes" : "no",
              anyActivity ? "yes" : "no",
              btOnline ? "yes" : "no",
#if SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE
              (btOnline && SerialBT.hasClient()) ? "yes" : "no");
#else
              "no");
#endif
  outPrintlnf("# externalTrigger=%s waiting=%s seen=%s triggerPin=%s",
              ENABLE_EXTERNAL_TRIGGER ? "enabled" : "disabled",
              waitingTrigger ? "yes" : "no",
              triggerSeen ? "yes" : "no",
              TRIGGER_PIN_LABEL);

  outPrintlnf("# events=%u overflow=%u decodedBytes=%u",
              (unsigned)count,
              (unsigned)ov,
              (unsigned)totalBytes);

  outPrintlnf("# liveLines SCL=%u SDA=%u",
              (unsigned)scl,
              (unsigned)sda);

  uint32_t nowC = fastCycles();
  outPrintlnf("# sinceBoot_ms=%lu",
              (unsigned long)cyclesToMsInt(nowC - bootC));

  if (anyActivity && firstC != 0) {
    outPrintlnf("# firstActivity_ms=%lu",
                (unsigned long)cyclesToMsInt(firstC - bootC));
  }

  if (anyActivity && lastC != 0) {
    outPrintlnf("# lastActivity_ms=%lu",
                (unsigned long)cyclesToMsInt(lastC - bootC));
  }

  if (started && startC != 0) {
    uint32_t refEnd = (done && stopC != 0) ? stopC : nowC;
    outPrintlnf("# decodedCapture_ms=%lu",
                (unsigned long)cyclesToMsInt(refEnd - startC));
  }
}

void printDumpMetadataHeader(uint16_t count, uint16_t ov) {
  outPrintln("# DDR5_BOOT_SNIFFER_DUMP v0.2");
  outPrintlnf("# node=%s", NODE_NAME);
  outPrintlnf("# bus=%s", BUS_LABEL);
  outPrintlnf("# firmware=%s", FW_VERSION);
  outPrintlnf("# profile=%s", SNIFFER_PROFILE_NAME);
  outPrintlnf("# storage=%s", SNIFFER_STORAGE_CAPABILITY);
  outPrintln("# capture_scope=boot_i2c_compatible_sideband");
  outPrintln("# full_i3c_analyzer=false");
  outPrintlnf("# scl_pin=%d", (int)PIN_SCL);
  outPrintlnf("# sda_pin=%d", (int)PIN_SDA);
#if ENABLE_EXTERNAL_TRIGGER
  outPrintlnf("# trigger_pin=%d", TRIGGER_PIN);
#else
  outPrintln("# trigger_pin=disabled");
#endif
  outPrintlnf("# max_events=%u", (unsigned)MAX_EVENTS);
  outPrintlnf("# max_decoded_bytes=%u", (unsigned)MAX_DECODED_BYTES);
  outPrintlnf("# capture_window_ms=%lu", (unsigned long)CAPTURE_WINDOW_MS);
  outPrintlnf("# bus_idle_done_ms=%lu", (unsigned long)BUS_IDLE_DONE_MS);
  outPrintlnf("# psram_used=%s", SNIFFER_USE_PSRAM ? "yes" : "no");
  outPrintlnf("# sd_export_enabled=%s", SNIFFER_ENABLE_SD_EXPORT ? "yes" : "no");
  outPrintlnf("# raw_edge_buffer_mode=%s", SNIFFER_RAW_EDGE_BUFFER_MODE ? "yes" : "no");
  outPrintlnf("# usb_serial_dump_preferred=%s", SNIFFER_PREFER_USB_SERIAL_DUMP ? "yes" : "no");
  outPrintlnf("# overflow=%s", (ov > 0) ? "yes" : "no");
  outPrintlnf("# events=%u", (unsigned)count);
}

void printSummary() {
  uint16_t count, ov, totalBytes;
  bool started, done, anyActivity;
  uint32_t bootC, firstC, startC, stopC;

  noInterrupts();
  count = evCount;
  ov = overflowCount;
  totalBytes = totalDecodedBytes;
  started = captureStarted;
  done = captureDone;
  anyActivity = sawAnyActivity;
  bootC = bootCycle;
  firstC = firstActivityCycle;
  startC = captureStartCycle;
  stopC = captureStopCycle;
  interrupts();

  outPrintln("# summary");
  outPrintlnf("# events=%u overflow=%u decodedBytes=%u started=%s done=%s rawActivity=%s",
              (unsigned)count,
              (unsigned)ov,
              (unsigned)totalBytes,
              started ? "yes" : "no",
              done ? "yes" : "no",
              anyActivity ? "yes" : "no");

  uint8_t scl = gpio_get_level(PIN_SCL);
  uint8_t sda = gpio_get_level(PIN_SDA);
  outPrintlnf("# liveLines SCL=%u SDA=%u", (unsigned)scl, (unsigned)sda);

  if (anyActivity && firstC != 0) {
    outPrintlnf("# firstActivity_ms=%lu",
                (unsigned long)cyclesToMsInt(firstC - bootC));
  }

  if (!started) {
    outPrintln("# no decoded START captured");
    return;
  }

  uint32_t refEnd = (done && stopC != 0) ? stopC : fastCycles();
  uint32_t elapsedMs = (startC != 0) ? cyclesToMsInt(refEnd - startC) : 0;
  outPrintlnf("# decodedCapture_ms=%lu", (unsigned long)elapsedMs);

  struct AddrStat {
    uint16_t seen;
    uint16_t reads;
    uint16_t writes;
    uint16_t acked;
    uint16_t nacked;
  };

  AddrStat stats[128] = {};

  for (uint16_t i = 0; i < count; i++) {
    const DecodedEvent& e = evBuf[i];
    if (e.type == EVT_ADDR) {
      AddrStat& s = stats[e.value & 0x7F];
      s.seen++;
      if (e.flags & 0x01) s.reads++;
      else s.writes++;
      if (e.flags & 0x02) s.nacked++;
      else s.acked++;
    }
  }

  for (uint16_t a = 0; a < 128; a++) {
    if (stats[a].seen == 0) continue;
    outPrintlnf("ADDR 0x%02X: tx=%u W=%u R=%u ACK=%u NACK=%u",
                (unsigned)a,
                (unsigned)stats[a].seen,
                (unsigned)stats[a].writes,
                (unsigned)stats[a].reads,
                (unsigned)stats[a].acked,
                (unsigned)stats[a].nacked);
  }
}

void dumpEventsRange(uint16_t startIdx, uint16_t endIdx) {
  uint16_t count, ov, totalBytes;

  noInterrupts();
  count = evCount;
  ov = overflowCount;
  totalBytes = totalDecodedBytes;
  interrupts();

  outPrintln("");
  outPrintln("# ===== DDR5 SNIFFER DUMP BEGIN =====");
  printDumpMetadataHeader(count, ov);
  outPrintlnf("# eventCount=%u overflow=%u decodedBytes=%u range=%u-%u",
              (unsigned)count,
              (unsigned)ov,
              (unsigned)totalBytes,
              (unsigned)startIdx,
              (unsigned)endIdx);

  if (count == 0) {
    outPrintln("# no events captured");
    outPrintln("# ===== DDR5 SNIFFER DUMP END =====");
    outPrintln("");
    return;
  }

  if (startIdx >= count) {
    outPrintln("# requested range starts past captured events");
    outPrintln("# ===== DDR5 SNIFFER DUMP END =====");
    outPrintln("");
    return;
  }

  if (endIdx >= count) endIdx = count - 1;
  if (endIdx < startIdx) endIdx = startIdx;

  uint32_t runningUs = 0;
  for (uint16_t i = 0; i <= endIdx; i++) {
    const DecodedEvent& e = evBuf[i];
    runningUs += e.dtUs;

    if (i < startIdx) continue;

    switch (e.type) {
      case EVT_START:
        outPrintlnf("E%04u @%luus START",
                    (unsigned)i, (unsigned long)runningUs);
        break;

      case EVT_ADDR:
        outPrintlnf("E%04u @%luus ADDR 0x%02X %c %s",
                    (unsigned)i,
                    (unsigned long)runningUs,
                    (unsigned)e.value,
                    (e.flags & 0x01) ? 'R' : 'W',
                    ackName((e.flags & 0x02) ? 1 : 0));
        break;

      case EVT_DATA:
        outPrintlnf("E%04u @%luus DATA 0x%02X %s",
                    (unsigned)i,
                    (unsigned long)runningUs,
                    (unsigned)e.value,
                    ackName((e.flags & 0x02) ? 1 : 0));
        break;

      case EVT_STOP:
        outPrintlnf("E%04u @%luus STOP",
                    (unsigned)i, (unsigned long)runningUs);
        break;

      case EVT_MARKER:
      case EVT_OVERFLOW:
      case EVT_TRIGGER:
      case EVT_TIMEOUT:
      case EVT_IDLE_STOP:
        outPrintlnf("E%04u @%luus %s value=0x%02X flags=0x%02X",
                    (unsigned)i,
                    (unsigned long)runningUs,
                    eventTypeName(e.type),
                    (unsigned)e.value,
                    (unsigned)e.flags);
        break;

      default:
        outPrintlnf("E%04u @%luus UNKNOWN type=%u val=0x%02X flags=0x%02X",
                    (unsigned)i,
                    (unsigned long)runningUs,
                    (unsigned)e.type,
                    (unsigned)e.value,
                    (unsigned)e.flags);
        break;
    }
  }

  outPrintln("# ===== DDR5 SNIFFER DUMP END =====");
  outPrintln("");
}

void dumpCsvEvents() {
  uint16_t count, ov;

  noInterrupts();
  count = evCount;
  ov = overflowCount;
  interrupts();

  outPrintln("");
  printDumpMetadataHeader(count, ov);
  outPrintln("idx,time_us,dt_us,type,value,flags,ack");

  uint32_t runningUs = 0;
  for (uint16_t i = 0; i < count; i++) {
    const DecodedEvent& e = evBuf[i];
    runningUs += e.dtUs;
    const char* ack = "";
    if (e.type == EVT_ADDR || e.type == EVT_DATA) {
      ack = ackName((e.flags & 0x02) ? 1 : 0);
    }
    outPrintlnf("%u,%lu,%u,%s,0x%02X,0x%02X,%s",
                (unsigned)i,
                (unsigned long)runningUs,
                (unsigned)e.dtUs,
                eventTypeName(e.type),
                (unsigned)e.value,
                (unsigned)e.flags,
                ack);
  }
  outPrintln("");
}

void exportSdDump() {
#if SNIFFER_ENABLE_SD_EXPORT
  outPrintln("SD export not implemented in this build.");
#else
  outPrintln("# SD export is disabled for this profile.");
#endif
}

void dumpEvents() {
  uint16_t count;
  noInterrupts();
  count = evCount;
  interrupts();

  if (count == 0) {
    dumpEventsRange(0, 0);
    return;
  }

  dumpEventsRange(0, count - 1);
}

void printHelp() {
  outPrintln("# commands:");
  outPrintln("#   status");
  outPrintln("#   summary");
  outPrintln("#   dump");
  outPrintln("#   dump <start> <end>");
  outPrintln("#   dumpcsv");
  outPrintln("#   exportsd");
  outPrintln("#   stop");
  outPrintln("#   clear");
  outPrintln("#   arm");
  outPrintln("#   help");
  outPrintln("# examples:");
  outPrintln("#   dump 0 99");
  outPrintln("#   dump 100 199");
  outPrintln("#   dump 200 299");
  outPrintln("#   dump 300 399");
}

// ============================================================
// Command handling
// ============================================================
void handleCommand(char* cmd) {
  while (*cmd == ' ' || *cmd == '\t') cmd++;
  if (*cmd == '\0') return;

  if (!strcasecmp(cmd, "status")) {
    printStatus();
    return;
  }

  if (!strcasecmp(cmd, "summary")) {
    printSummary();
    return;
  }

  if (!strcasecmp(cmd, "dump")) {
    dumpEvents();
    return;
  }

  if (!strcasecmp(cmd, "dumpcsv")) {
    dumpCsvEvents();
    return;
  }

  if (!strcasecmp(cmd, "exportsd")) {
    exportSdDump();
    return;
  }

  if (!strncasecmp(cmd, "dump ", 5)) {
    unsigned startIdx = 0;
    unsigned endIdx = 0;
    int parsed = sscanf(cmd + 5, "%u %u", &startIdx, &endIdx);

    if (parsed == 2) {
      if (startIdx >= MAX_EVENTS) startIdx = MAX_EVENTS - 1;
      if (endIdx >= MAX_EVENTS) endIdx = MAX_EVENTS - 1;
      dumpEventsRange((uint16_t)startIdx, (uint16_t)endIdx);
      return;
    }

    outPrintln("# usage: dump <start> <end>");
    outPrintln("# example: dump 0 99");
    return;
  }

  if (!strcasecmp(cmd, "stop")) {
    stopCaptureInternal();
    outPrintln("# capture manually stopped");
    return;
  }

  if (!strcasecmp(cmd, "clear")) {
    armCapture();
#if ENABLE_EXTERNAL_TRIGGER
    outPrintln("# capture buffer cleared and armed, waiting for external trigger");
#else
    outPrintln("# capture buffer cleared and re-armed");
#endif
    return;
  }

  if (!strcasecmp(cmd, "arm")) {
    armCapture();
#if ENABLE_EXTERNAL_TRIGGER
    outPrintln("# armed, waiting for external trigger");
#else
    outPrintln("# armed, waiting for first bus activity");
#endif
    return;
  }

  if (!strcasecmp(cmd, "help")) {
    printHelp();
    return;
  }

  outPrintln("# valid commands: status / summary / dump / dump <start> <end> / dumpcsv / exportsd / stop / clear / arm / help");
}

void pollCommandStream(Stream& s) {
  while (s.available()) {
    char c = (char)s.read();

    if (c == '\r' || c == '\n') {
      if (cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        handleCommand(cmdBuf);
        cmdLen = 0;
      }
    } else if (c == 8 || c == 127) {
      if (cmdLen > 0) cmdLen--;
    } else {
      if (cmdLen < sizeof(cmdBuf) - 1) {
        cmdBuf[cmdLen++] = c;
      }
    }
  }
}

// ============================================================
// Setup / loop
// ============================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);

  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true, true);

  gCpuMHz = (uint32_t)ESP.getCpuFreqMHz();
  if (gCpuMHz == 0) gCpuMHz = 240;
  gCyclesPerMs = gCpuMHz * 1000UL;
  gGlitchCycles = gCpuMHz / 2U;
  if (gGlitchCycles == 0) gGlitchCycles = 1;

  pinMode((int)PIN_SCL, INPUT);
  pinMode((int)PIN_SDA, INPUT);
#if ENABLE_EXTERNAL_TRIGGER
  pinMode((int)PIN_TRIGGER, INPUT);
#endif

  gpio_pullup_dis(PIN_SCL);
  gpio_pulldown_dis(PIN_SCL);
  gpio_pullup_dis(PIN_SDA);
  gpio_pulldown_dis(PIN_SDA);
#if ENABLE_EXTERNAL_TRIGGER
  gpio_pullup_dis(PIN_TRIGGER);
  gpio_pulldown_dis(PIN_TRIGGER);
  attachInterrupt(digitalPinToInterrupt((int)PIN_TRIGGER), isrTrigger, RISING);
#endif

  armCapture();

  Serial.println();
  Serial.println("Passive DDR5 boot I2C / I2C-compatible sideband sniffer ready.");
  Serial.println("Not a full I3C analyzer; true I3C push-pull/high-speed phases may exceed ESP32 GPIO sampling reliability.");
  Serial.printf("Node: %s  Bus: %s  FW: %s\n", NODE_NAME, BUS_LABEL, FW_VERSION);
  Serial.printf("Profile: %s (%s)\n", SNIFFER_PROFILE_NAME, SNIFFER_STORAGE_CAPABILITY);
  Serial.printf("SCL=GPIO%d SDA=GPIO%d\n", (int)PIN_SCL, (int)PIN_SDA);
#if ENABLE_EXTERNAL_TRIGGER
  Serial.printf("External trigger: enabled GPIO%d\n", TRIGGER_PIN);
#else
  Serial.println("External trigger: disabled");
#endif
  Serial.printf("CPU MHz: %lu\n", (unsigned long)gCpuMHz);
#if SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE
  Serial.printf("BT will start only AFTER capture completes: %s\n", BT_NAME);
#else
  Serial.println("Bluetooth after capture: disabled by profile");
#endif
  Serial.printf("Max events: %u\n", (unsigned)MAX_EVENTS);
  Serial.printf("Max decoded bytes: %u\n", (unsigned)MAX_DECODED_BYTES);
  Serial.printf("Boot failsafe: %lu ms\n", (unsigned long)BOOT_FAILSAFE_MS);
  Serial.printf("Capture window: %lu ms\n", (unsigned long)CAPTURE_WINDOW_MS);
  Serial.printf("Idle stop: %lu ms\n", (unsigned long)BUS_IDLE_DONE_MS);
  Serial.printf("Start/Stop glitch reject: %lu cycles (~0.5 us)\n", (unsigned long)gGlitchCycles);
#if SNIFFER_USE_PSRAM
  Serial.printf("PSRAM requested by profile: %s\n", psramFound() ? "found" : "not found");
#endif
#if SNIFFER_ENABLE_SD_EXPORT
  Serial.println("SD export profile selected; dump-to-SD is a post-capture placeholder in this build.");
#endif
#if ENABLE_EXTERNAL_TRIGGER
  Serial.println("# armed, waiting for external trigger");
#else
  Serial.println("# armed, waiting for first bus activity");
#endif
  Serial.println("# USB serial commands always available: status / summary / dump / dump <start> <end> / dumpcsv / exportsd / stop / clear / arm / help");
  Serial.println();
}

void loop() {
  bool done, anyActivity, waitingTrigger;
  uint16_t bytes, count;
  uint32_t nowC = fastCycles();
  uint32_t bootC, firstC, lastC;

  noInterrupts();
  done = captureDone;
  anyActivity = sawAnyActivity;
  waitingTrigger = waitingForExternalTrigger;
  bytes = totalDecodedBytes;
  count = evCount;
  bootC = bootCycle;
  firstC = firstActivityCycle;
  lastC = lastActivityCycle;
  interrupts();

  if (!done && !waitingTrigger) {
    bool stopForBootFailsafe = (cyclesToMsInt(nowC - bootC) >= BOOT_FAILSAFE_MS);
    bool stopForWindow = anyActivity && (firstC != 0) &&
                         (cyclesToMsInt(nowC - firstC) >= CAPTURE_WINDOW_MS);
    bool stopForIdle = anyActivity && (lastC != 0) &&
                       (cyclesToMsInt(nowC - lastC) >= BUS_IDLE_DONE_MS);
    bool stopForEvents = (count >= MAX_EVENTS);
    bool stopForData = (bytes >= MAX_DECODED_BYTES);

    if (stopForBootFailsafe || stopForWindow || stopForIdle || stopForEvents || stopForData) {
      stopCaptureInternal();
      done = true;
    }
  }

  if (done && snifferInterruptsAttached) {
    detachSnifferInterrupts();
  }

  if (captureDone && !btOnline && !btStartAttempted) {
    btStartAttempted = true;
    delay(50);  // let ISR activity settle before BT init

#if SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE
    btOnline = SerialBT.begin(BT_NAME);

    Serial.println("# capture complete");
    Serial.printf("# bt begin: %s\n", btOnline ? "OK" : "FAIL");

    if (btOnline) {
      Serial.println("# bluetooth started; connect now and type: status / summary / dump 0 99");
    } else {
      Serial.println("# bluetooth failed to start; use USB serial for status / summary / dump");
    }
#else
    Serial.println("# capture complete");
    Serial.println("# profile prefers USB serial; bluetooth disabled");
#endif
  }

  pollCommandStream(Serial);

#if SNIFFER_ENABLE_BLUETOOTH_AFTER_CAPTURE
  if (btOnline && SerialBT.hasClient()) {
    pollCommandStream(SerialBT);
  }
#endif

  delay(5);
}
