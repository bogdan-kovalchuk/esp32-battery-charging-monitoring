#pragma once

#include "device_config.h"

void httpInit(const DeviceConfig &cfg);
bool httpSend(const char *eventId, const char *msgType, float voltage);
