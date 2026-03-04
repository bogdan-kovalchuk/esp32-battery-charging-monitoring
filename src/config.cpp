#include "config.h"
#include "secrets.h"
#include <Preferences.h>

static Preferences prefs;
static const char *NS = "config";

void configInit() {
  prefs.begin(NS, false);
}

void configLoad(DeviceConfig &cfg) {
  if (!prefs.isKey("ssid")) {
    configReset(cfg);
    configSave(cfg);
    return;
  }
  cfg.ssid = prefs.getString("ssid", WIFI_SSID);
  cfg.password = prefs.getString("password", WIFI_PASSWORD);
  cfg.serverIP = prefs.getString("serverIP", SERVER_IP);
  cfg.deviceId = prefs.getString("deviceId", DEVICE_ID);
  cfg.apiToken = prefs.getString("apiToken", API_TOKEN);
  cfg.webPassword = prefs.getString("webPassword", WEB_PASSWORD);
  cfg.critVoltage = prefs.getFloat("critVoltage", CRIT_VOLTAGE);
  cfg.infoInterval = prefs.getULong("infoInterval", INFO_INTERVAL);
  cfg.critInterval = prefs.getULong("critInterval", CRIT_INTERVAL);
}

void configSave(const DeviceConfig &cfg) {
  prefs.clear();
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("password", cfg.password);
  prefs.putString("serverIP", cfg.serverIP);
  prefs.putString("deviceId", cfg.deviceId);
  prefs.putString("apiToken", cfg.apiToken);
  prefs.putString("webPassword", cfg.webPassword);
  prefs.putFloat("critVoltage", cfg.critVoltage);
  prefs.putULong("infoInterval", cfg.infoInterval);
  prefs.putULong("critInterval", cfg.critInterval);
}

void configReset(DeviceConfig &cfg) {
  cfg.ssid = WIFI_SSID;
  cfg.password = WIFI_PASSWORD;
  cfg.serverIP = SERVER_IP;
  cfg.deviceId = DEVICE_ID;
  cfg.apiToken = API_TOKEN;
  cfg.webPassword = WEB_PASSWORD;
  cfg.critVoltage = CRIT_VOLTAGE;
  cfg.infoInterval = INFO_INTERVAL;
  cfg.critInterval = CRIT_INTERVAL;
}
