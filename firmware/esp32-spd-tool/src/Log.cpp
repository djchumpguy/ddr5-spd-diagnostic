#include "Log.h"
#include "AppState.h"
#include "AppConfig.h"
#include <stdarg.h>
#include <string.h>

static inline void logAppendChar(char c) {
  gApp.logBuf[gApp.logHead++] = c;
  if (gApp.logHead >= LOG_CAP) {
    gApp.logHead = 0;
    gApp.logWrapped = true;
  }
}

static inline void outWriteChar(char c) {
  Serial.write((uint8_t)c);
  logAppendChar(c);
}

void outWriteRaw(const char* s) {
  if (!s) return;
  while (*s) outWriteChar(*s++);
}

void outPrintf(const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  outWriteRaw(buf);
}

void outPrint(const char* s) {
  outWriteRaw(s);
}

void outPrintln(const char* s) {
  outWriteRaw(s);
  outWriteRaw("\n");
}

String getLogString() {
  String s;
  s.reserve(LOG_CAP + 64);

  if (!gApp.logWrapped) {
    for (size_t i = 0; i < gApp.logHead; i++) s += gApp.logBuf[i];
    return s;
  }

  for (size_t i = gApp.logHead; i < LOG_CAP; i++) s += gApp.logBuf[i];
  for (size_t i = 0; i < gApp.logHead; i++) s += gApp.logBuf[i];
  return s;
}

void clearLog() {
  memset(gApp.logBuf, 0, sizeof(gApp.logBuf));
  gApp.logHead = 0;
  gApp.logWrapped = false;
}

void ansiClear() {
  outPrint("\x1b[2J\x1b[H");
}