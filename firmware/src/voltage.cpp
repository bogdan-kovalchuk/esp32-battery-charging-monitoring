#include "voltage.h"
#include "app_config.h"
#include <Arduino.h>

// millis() deadline at which the manual display window closes. 0 means no
// window is open.
static unsigned long displayUntil = 0;
static bool powered = false;

// All deadline comparisons are written as signed differences so they stay
// correct across the ~49-day millis() rollover.
static bool deadlinePassed(unsigned long deadline) {
  return (long)(millis() - deadline) >= 0;
}

static void powerOn() {
  digitalWrite(PIN_VOLTMETER, HIGH);
  powered = true;
}

static void powerOff() {
  digitalWrite(PIN_VOLTMETER, LOW);
  powered = false;
}

void voltageInit() {
  pinMode(PIN_VOLTMETER, OUTPUT);
  displayUntil = 0;
  powerOff();
}

bool voltageDisplayActive() {
  return displayUntil != 0 && !deadlinePassed(displayUntil);
}

void voltageDisplayFor(unsigned long ms) {
  displayUntil = millis() + ms;
  if (displayUntil == 0) displayUntil = 1;  // 0 is the "no window" sentinel
  powerOn();
}

void voltageTick() {
  if (displayUntil != 0 && deadlinePassed(displayUntil)) {
    displayUntil = 0;
    powerOff();
  }
}

float voltageRead() {
  bool wasPowered = powered;
  if (!wasPowered) {
    powerOn();
    delay(VOLTMETER_SETTLE_MS);
  }

  long sumADC = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sumADC += analogRead(PIN_VOLTAGE);
    delay(2);
  }

  // Leave the gauge on if someone pressed the button to look at it.
  if (!voltageDisplayActive()) powerOff();

  return voltageConvert(sumADC / (float)NUM_SAMPLES);
}
