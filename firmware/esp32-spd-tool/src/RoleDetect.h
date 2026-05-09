#pragma once
#include <Arduino.h>
#include "AppState.h"

const char* roleName(DeviceRole role);
const char* roleUiName(DeviceRole role);
const char* confidenceName(uint8_t confidence);
DeviceRoleRecord* findRoleRecord(uint8_t addr);

bool cmdAutoDetectRoles();
