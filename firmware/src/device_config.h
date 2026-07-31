#pragma once

#include <Arduino.h>

// Runtime device configuration. Seeded from secrets.h on first boot, then
// persisted in NVS and editable through the web UI. Everything the device
// needs at runtime lives here — nothing outside configReset() reads secrets.h
// directly, so a field that is not in this struct cannot be changed without
// reflashing.
struct DeviceConfig {
  String ssid;
  String password;
  String serverHost;
  uint16_t serverPort;
  bool serverTls;
  String deviceId;
  String apiToken;
  String apSsid;
  String apPassword;
  String webUser;
  String webPassword;
  float critVoltage;
  unsigned long infoInterval;  // minutes
  unsigned long critInterval;  // minutes
};

// Accepted ranges, shared between the web UI and configClamp().
constexpr float CFG_CRIT_VOLTAGE_MIN = 5.0f;
constexpr float CFG_CRIT_VOLTAGE_MAX = 15.0f;
constexpr unsigned long CFG_INTERVAL_MIN = 1;      // minutes
constexpr unsigned long CFG_INTERVAL_MAX = 10080;  // minutes — one week

// Hard limits imposed by the ESP32 WiFi stack. Exceeding them makes softAP()
// either truncate silently or fail outright, so they are enforced rather than
// left to the browser's maxlength attributes.
constexpr unsigned int CFG_SSID_MAX_LEN = 32;
constexpr unsigned int CFG_WIFI_PASSWORD_MAX_LEN = 63;
constexpr unsigned int CFG_AP_PASSWORD_MIN_LEN = 8;  // WPA2 minimum

// Applied to the runtime editors too, so the web UI cannot set a credential
// weaker than the one the build would have refused to compile.
constexpr unsigned int CFG_WEB_PASSWORD_MIN_LEN = 8;
constexpr unsigned int CFG_API_TOKEN_MIN_LEN = 16;
constexpr unsigned int CFG_TEXT_MAX_LEN = 128;

void configInit();
void configLoad(DeviceConfig &cfg);
// Atomically persists a verified config record. False means the previous
// committed slot remains active and the caller must not report success.
bool configSave(const DeviceConfig &cfg);
void configReset(DeviceConfig &cfg);

// Clamps out-of-range fields into the CFG_* bounds above. Returns true if
// every field was already valid, false if anything had to be corrected.
bool configClamp(DeviceConfig &cfg);

// True if the value is one of the weak defaults that were published in this
// repository's git history. Such a value must never be used at runtime, no
// matter how it got into NVS.
bool configIsPublishedDefault(const String &value);
