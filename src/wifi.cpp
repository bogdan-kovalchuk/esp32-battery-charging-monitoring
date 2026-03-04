#include "wifi.h"
#include "app_config.h"
#include "secrets.h"
#include <WiFi.h>

static bool apMode = false;

void wifiInit(const DeviceConfig &cfg) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.println(WiFi.localIP());
    apMode = false;
  } else {
    Serial.println("\nWiFi failed, starting AP mode");
    apMode = wifiAPMode();
  }
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void wifiReconnect(const DeviceConfig &cfg) {
  if (wifiConnected()) {
    apMode = false;
    return;
  }
  if (apMode) return;
  
  Serial.println("Reconnecting WiFi...");
  WiFi.disconnect();
  WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(100);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi reconnected");
    Serial.println(WiFi.localIP());
    apMode = false;
  }
}

bool wifiAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  return true;
}
