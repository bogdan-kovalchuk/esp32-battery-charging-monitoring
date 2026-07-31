#pragma once

#include "device_config.h"

void wifiInit(const DeviceConfig &cfg);
bool wifiConnected();
// Advances STA reconnect/AP fallback without blocking the Arduino loop.
void wifiTick(const DeviceConfig &cfg);

// Brings up the fallback access point using cfg.apSsid / cfg.apPassword.
bool wifiAPMode(const DeviceConfig &cfg);

// True while the fallback access point is up.
bool wifiAPActive();
