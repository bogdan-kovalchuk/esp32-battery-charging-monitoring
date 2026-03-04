#pragma once

#include "config.h"

void wifiInit(const DeviceConfig &cfg);
bool wifiConnected();
void wifiReconnect(const DeviceConfig &cfg);
bool wifiAPMode();
