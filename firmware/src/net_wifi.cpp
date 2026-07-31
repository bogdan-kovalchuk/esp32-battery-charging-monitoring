#include "net_wifi.h"
#include "app_config.h"
#include <WiFi.h>

static bool apMode = false;
static bool staAttemptActive = false;
static bool staWasConnected = false;
static unsigned long staAttemptStartedAt = 0;
static unsigned long nextStaAttemptAt = 0;
static unsigned long staConnectedAt = 0;

static bool due(unsigned long now, unsigned long deadline) {
  return (long)(now - deadline) >= 0;
}

static void startStaAttempt(const DeviceConfig &cfg, unsigned long now) {
  WiFi.mode(apMode ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
  staAttemptActive = true;
  staAttemptStartedAt = now;
  nextStaAttemptAt = now + WIFI_RECONNECT_INTERVAL_MS;
  Serial.println("Starting WiFi STA association...");
}

void wifiInit(const DeviceConfig &cfg) {
  // Runtime credentials already live in our own NVS record; do not make the
  // WiFi stack persist a second copy on every reconnect.
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  startStaAttempt(cfg, millis());
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool wifiAPActive() {
  return apMode;
}

void wifiTick(const DeviceConfig &cfg) {
  unsigned long now = millis();

  if (wifiConnected()) {
    staAttemptActive = false;
    if (!staWasConnected) {
      staWasConnected = true;
      staConnectedAt = now;
      Serial.println("WiFi connected");
      Serial.println(WiFi.localIP());
    }

    // AP+STA permits recovery without physical access. Retire the fallback AP
    // only after STA has stayed up long enough not to be a transient flap.
    if (apMode && now - staConnectedAt >= WIFI_STABLE_BEFORE_AP_OFF_MS) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      apMode = false;
      Serial.println("WiFi stable; fallback AP stopped");
    }
    return;
  }
  staWasConnected = false;

  if (staAttemptActive && now - staAttemptStartedAt >= WIFI_TIMEOUT_MS) {
    staAttemptActive = false;
    Serial.println("WiFi STA association timed out");
    if (!apMode) {
      Serial.println("Starting fallback AP while STA retries continue");
      wifiAPMode(cfg);
    }
  }

  if (!staAttemptActive && due(now, nextStaAttemptAt)) {
    startStaAttempt(cfg, now);
  }
}

bool wifiAPMode(const DeviceConfig &cfg) {
  // AP+STA keeps the configuration portal reachable while background STA
  // attempts continue. Pure AP mode would make recovery require a reboot.
  WiFi.mode(WIFI_AP_STA);
  // configClamp() guarantees apPassword is at least 8 characters, so softAP()
  // will not silently fall back to an open network.
  if (!WiFi.softAP(cfg.apSsid.c_str(), cfg.apPassword.c_str())) {
    Serial.println("softAP failed");
    apMode = false;
    return false;
  }
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  apMode = true;
  return true;
}
