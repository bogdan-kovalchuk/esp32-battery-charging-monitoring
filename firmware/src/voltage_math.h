#pragma once

// Pure conversion maths, deliberately free of <Arduino.h> so the host-side
// unit tests can compile and run it without an ESP32 toolchain. Anything that
// touches a pin belongs in voltage.h/voltage.cpp instead.

#include <cmath>

#include "app_config.h"

// Converts an averaged raw ADC reading into a battery voltage.
//
//   12V BAT --[R1]--+--[R2]-- GND
//                   |
//                GPIO 32
//
// The divider scales the battery voltage into the ADC's 0..V_REF range;
// CORR_FACTOR compensates for the ESP32 ADC's non-ideal transfer curve and is
// calibrated against a reference meter.
inline float voltageConvert(float rawADC) {
  float dividerOutput = (rawADC / ADC_MAX) * V_REF;
  float batteryVoltage = dividerOutput * (1.0f + RESISTOR_R1 / RESISTOR_R2);
  return CORR_FACTOR * roundf(batteryVoltage * 10.0f) / 10.0f;
}

// Message intervals are configured in minutes but compared against millis().
inline unsigned long minutesToMillis(unsigned long minutes) {
  return minutes * 60000UL;
}
