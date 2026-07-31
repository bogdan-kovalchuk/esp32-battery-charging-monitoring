#include "backend_client.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "app_config.h"
#include "tls_config.h"

// Points at the caller's long-lived DeviceConfig so edits made through the web
// UI take effect without re-initialising. Const: this module never writes it.
static const DeviceConfig *pCfg = nullptr;

static const int MAX_RETRIES = 3;
static const uint16_t HTTP_TIMEOUT_MS = 5000;
static const unsigned long RETRY_DELAY_MS = 1000;
static bool timeSyncStarted = false;

void httpInit(const DeviceConfig &cfg) {
  pCfg = &cfg;
}

static bool ensureTlsClock() {
  // X.509 validity checks require wall-clock time. 2024-01-01 is only a
  // plausibility threshold; the certificate library performs the real check.
  if (time(nullptr) >= 1704067200) return true;
  if (!timeSyncStarted) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    timeSyncStarted = true;
  }
  unsigned long startedAt = millis();
  while (time(nullptr) < 1704067200 &&
         millis() - startedAt < TLS_TIME_SYNC_TIMEOUT_MS) {
    delay(100);
  }
  return time(nullptr) >= 1704067200;
}

bool httpSend(const char *eventId, const char *msgType, float voltage) {
  if (!pCfg) return false;

  if (pCfg->serverTls && !ensureTlsClock()) {
    Serial.println("TLS aborted: could not synchronize wall clock");
    return false;
  }

  String url = String(pCfg->serverTls ? "https://" : "http://") +
               pCfg->serverHost + ":" + String(pCfg->serverPort) + "/send";

  JsonDocument doc;
  doc["event_id"] = eventId;
  doc["device_id"] = pCfg->deviceId;
  doc["msg_type"] = msgType;
  doc["voltage"] = roundf(voltage * 10) / 10.0;
  if (strcmp(msgType, "ALERT") == 0) {
    doc["critical_voltage"] = pCfg->critVoltage;
  }

  String body;
  serializeJson(doc, body);

  for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
    HTTPClient http;
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    bool begun = false;
    if (pCfg->serverTls) {
      secureClient.setCACert(SERVER_ROOT_CA);
      begun = http.begin(secureClient, url);
    } else {
      begun = http.begin(plainClient, url);
    }
    if (!begun) {
      Serial.printf("HTTP begin failed for %s\n", url.c_str());
      return false;
    }
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + pCfg->apiToken);

    int code = http.POST(body);
    http.end();

    // 202 is the durable-outbox contract. Accept 200 during a rolling backend
    // upgrade so a device does not need to be reflashed atomically with it.
    if (code == 200 || code == 202) return true;

    // 401/403 mean the token is wrong; retrying cannot fix that.
    if (code == 401 || code == 403) {
      Serial.printf("HTTP POST rejected (%d) — check API token\n", code);
      return false;
    }

    Serial.printf("HTTP POST failed (%d), attempt %d/%d\n", code, attempt + 1,
                  MAX_RETRIES);
    if (attempt + 1 < MAX_RETRIES) delay(RETRY_DELAY_MS);
  }
  return false;
}
