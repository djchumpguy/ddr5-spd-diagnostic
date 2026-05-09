#pragma once
#include <Arduino.h>

void printHelp();
void execCommandLine(const String& lineIn);
void cliPollSerial();