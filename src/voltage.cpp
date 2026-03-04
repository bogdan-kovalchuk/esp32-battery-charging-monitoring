#include "voltage.h"
#include "app_config.h"
#include <Arduino.h>

void voltageInit() {
  pinMode(PIN_VOLTMETER, OUTPUT);
  digitalWrite(PIN_VOLTMETER, LOW);
}

float voltageConvert(float rawADC) {
  float voltageOut = (rawADC / ADC_MAX) * V_REF;
  float batteryVoltage = voltageOut * (1 + RESISTOR_R1 / RESISTOR_R2);
  return CORR_FACTOR * roundf(batteryVoltage * 10) / 10.0;
}

float voltageRead() {
  digitalWrite(PIN_VOLTMETER, HIGH);
  delay(100);

  long sumADC = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sumADC += analogRead(PIN_VOLTAGE);
    delay(2);
  }

  digitalWrite(PIN_VOLTMETER, LOW);
  return voltageConvert(sumADC / (float)NUM_SAMPLES);
}
