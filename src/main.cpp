#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "app_config.h"
#include "secrets.h"
#include "config.h"
#include "wifi.h"
#include "voltage.h"
#include "webserver.h"
#include "http.h"

static DeviceConfig cfg;
static unsigned long lastInfoTime = 0;
static unsigned long lastCriticalTime = 0;
static unsigned long buttonPressStart = 0;
static bool buttonPressed = false;
static bool buttonHeld = false;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT);

  esp_task_wdt_init(60, true);
  esp_task_wdt_add(NULL);

  configInit();
  configLoad(cfg);
  voltageInit();
  wifiInit(cfg);
  httpInit(cfg);
  webserverInit(cfg);

  float v = voltageRead();
  if (v > 0) {
    if (v <= cfg.critVoltage) {
      httpSend("ALERT", v);
      lastCriticalTime = millis();
    } else {
      httpSend("INFO", v);
      lastInfoTime = millis();
    }
  }
}

void loop() {
  esp_task_wdt_reset();

  unsigned long infoMs = cfg.infoInterval * 60000UL;
  unsigned long critMs = cfg.critInterval * 60000UL;

  if (wifiConnected()) {
    float v = voltageRead();
    if (v > 0) {
      if (v > cfg.critVoltage && millis() - lastInfoTime >= infoMs) {
        if (httpSend("INFO", v)) lastInfoTime = millis();
      }
      if (v <= cfg.critVoltage && millis() - lastCriticalTime >= critMs) {
        if (httpSend("ALERT", v)) lastCriticalTime = millis();
      }
    }
  } else if (WiFi.getMode() == WIFI_AP) {
    webserverHandle();
  } else {
    wifiReconnect(cfg);
  }

  webserverHandle();

  bool btn = digitalRead(PIN_BUTTON);
  if (btn == HIGH && !buttonPressed) {
    buttonPressed = true;
    buttonPressStart = millis();
  }
  if (btn == LOW && buttonPressed) {
    buttonPressed = false;
    buttonHeld = false;
  }
  if (buttonPressed && !buttonHeld && millis() - buttonPressStart >= BUTTON_RESET_MS) {
    buttonHeld = true;
    Serial.println("Reset config");
    configReset(cfg);
    configSave(cfg);
    delay(1000);
    ESP.restart();
  }

  delay(LOOP_DELAY_MS);
}
