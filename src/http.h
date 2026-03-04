#pragma once

#include "config.h"

void httpInit(const DeviceConfig &cfg);
bool httpSend(const char *msgType, float voltage);
