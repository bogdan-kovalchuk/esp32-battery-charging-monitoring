#include "http.h"
#include "secrets.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

static DeviceConfig *pCfg;
static const int MAX_RETRIES = 3;

void httpInit(const DeviceConfig &cfg) {
  pCfg = &cfg;
}

bool httpSend(const char *msgType, float voltage) {
  if (!pCfg) return false;

  for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
    HTTPClient http;
    String url = "http://" + pCfg->serverIP + ":" + String(SERVER_PORT) + "/send";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + pCfg->apiToken);

    JsonDocument doc;
    doc["device_id"] = pCfg->deviceId;
    doc["msg_type"] = msgType;
    doc["voltage"] = roundf(voltage * 10) / 10.0;
    if (strcmp(msgType, "ALERT") == 0) {
      doc["critical_voltage"] = pCfg->critVoltage;
    }

    String body;
    serializeJson(doc, body);
    int code = http.POST(body);
    http.end();

    if (code == 200) return true;
    Serial.printf("HTTP POST failed (%d), retry %d/%d\n", code, attempt + 1, MAX_RETRIES);
    delay(1000);
  }
  return false;
}
