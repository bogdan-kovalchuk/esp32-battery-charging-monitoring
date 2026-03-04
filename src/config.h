#pragma once

#include <Arduino.h>

struct DeviceConfig {
  String ssid;
  String password;
  String serverIP;
  String deviceId;
  String apiToken;
  String webPassword;
  float critVoltage;
  unsigned long infoInterval;
  unsigned long critInterval;
};

void configInit();
void configLoad(DeviceConfig &cfg);
void configSave(const DeviceConfig &cfg);
void configReset(DeviceConfig &cfg);
