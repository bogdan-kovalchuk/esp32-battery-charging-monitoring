#include <Arduino.h>
#include <esp_task_wdt.h>
#include "app_config.h"
#include "device_config.h"
#include "net_wifi.h"
#include "voltage.h"
#include "config_portal.h"
#include "backend_client.h"
#include "report_scheduler.h"
#include "voltage_math.h"
#include <esp_system.h>

static DeviceConfig cfg;
static ReportScheduler reportScheduler(VOLTAGE_SAMPLE_INTERVAL_MS);

// A retry reuses the same event id and payload. If the backend persisted the
// event but the HTTP response was lost, its durable outbox can deduplicate it.
static String pendingEventId;
static BatteryState pendingEventState = BatteryState::UNKNOWN;
static float pendingVoltage = 0.0f;
static uint32_t bootNonceA = 0;
static uint32_t bootNonceB = 0;
static uint32_t eventSequence = 0;

// Debounced button state.
static bool buttonStable = false;
static bool buttonRaw = false;
static unsigned long buttonChangedAt = 0;
static unsigned long buttonPressStart = 0;
static bool factoryResetDone = false;

static String nextEventId() {
  char id[32];
  snprintf(id, sizeof(id), "%08lx%08lx-%08lx", (unsigned long)bootNonceA,
           (unsigned long)bootNonceB, (unsigned long)++eventSequence);
  return String(id);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT);
  bootNonceA = esp_random();
  bootNonceB = esp_random();

  esp_task_wdt_init(60, true);
  esp_task_wdt_add(NULL);

  configInit();
  configLoad(cfg);
  voltageInit();
  wifiInit(cfg);
  httpInit(cfg);
  webserverInit(cfg);

  // Start with a fresh watchdog budget before the first measurement and
  // possible multi-attempt HTTPS request.
  esp_task_wdt_reset();

  // The scheduler's first sample deadline is zero, so loop() measures and
  // reports the initial state immediately after connectivity is available.
}

static void sampleAndReportIfDue() {
  uint32_t now = millis();
  if (!reportScheduler.sampleDue(now)) return;

  float v = voltageRead();
  if (v <= 0) {
    reportScheduler.recordInvalidSample(now, INVALID_SAMPLE_RETRY_MS);
    return;
  }

  ReportDecision decision = reportScheduler.recordSample(now, v <= cfg.critVoltage);
  if (decision == ReportDecision::NONE) return;

  BatteryState state = reportScheduler.state();
  if (pendingEventId.length() == 0 || pendingEventState != state) {
    pendingEventId = nextEventId();
    pendingEventState = state;
    pendingVoltage = v;
  }

  const char *msgType =
      decision == ReportDecision::ALERT ? "ALERT" : "INFO";
  bool accepted = httpSend(pendingEventId.c_str(), msgType, pendingVoltage);
  unsigned long repeat = decision == ReportDecision::ALERT
                             ? minutesToMillis(cfg.critInterval)
                             : minutesToMillis(cfg.infoInterval);
  reportScheduler.recordSendResult(millis(), accepted, repeat,
                                   SEND_RETRY_INTERVAL_MS);
  if (accepted) {
    pendingEventId = "";
    pendingEventState = BatteryState::UNKNOWN;
  }
}

static void handleButton() {
  bool raw = digitalRead(PIN_BUTTON) == HIGH;

  if (raw != buttonRaw) {
    buttonRaw = raw;
    buttonChangedAt = millis();
    return;
  }
  if (raw == buttonStable) return;
  if (millis() - buttonChangedAt < BUTTON_DEBOUNCE_MS) return;

  buttonStable = raw;
  if (buttonStable) {
    buttonPressStart = millis();
    factoryResetDone = false;
    // Power the analog gauge so it can be read by hand.
    voltageDisplayFor(VOLT_ON_TIME_MS);
    Serial.println("Button pressed: voltmeter on");
  }
}

static void handleFactoryReset() {
  if (!buttonStable || factoryResetDone) return;
  if (millis() - buttonPressStart < BUTTON_RESET_MS) return;

  factoryResetDone = true;
  Serial.println("Button held: restoring compile-time configuration");
  DeviceConfig resetCfg;
  configReset(resetCfg);
  configClamp(resetCfg);
  if (!configSave(resetCfg)) {
    Serial.println("Factory reset persistence failed; keeping current config");
    factoryResetDone = false;
    return;
  }
  cfg = resetCfg;
  delay(1000);
  ESP.restart();
}

void loop() {
  esp_task_wdt_reset();

  // Served in both STA and AP mode, so this runs unconditionally rather than
  // once inside the AP branch and again below, as it used to.
  webserverHandle();
  voltageTick();
  handleButton();
  handleFactoryReset();
  wifiTick(cfg);

  if (wifiConnected()) {
    sampleAndReportIfDue();
  }

  delay(LOOP_DELAY_MS);
}
