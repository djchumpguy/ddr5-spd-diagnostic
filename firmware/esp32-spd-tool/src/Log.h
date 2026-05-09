#pragma once
#include <Arduino.h>

void outWriteRaw(const char* s);
void outPrintf(const char* fmt, ...);
void outPrint(const char* s);
void outPrintln(const char* s = "");
String getLogString();
void clearLog();
void ansiClear();