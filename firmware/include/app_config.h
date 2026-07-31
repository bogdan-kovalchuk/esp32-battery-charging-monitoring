#pragma once

// Compile-time hardware and timing constants. Everything an operator may need
// to change at runtime lives in DeviceConfig (src/device_config.h) instead.

const int PIN_VOLTAGE = 32;
const int PIN_BUTTON = 34;
const int PIN_VOLTMETER = 4;

// Divider: 12V --[R1]--+--[R2]-- GND, tap into GPIO 32.
const float RESISTOR_R1 = 30000.0f;
const float RESISTOR_R2 = 7500.0f;
const float V_REF = 3.3f;
const float ADC_MAX = 4095.0f;
const int NUM_SAMPLES = 100;
// Calibrated against a reference meter; compensates the ESP32 ADC curve.
const float CORR_FACTOR = 1.0468f;

// How long a button press keeps the analog gauge powered so it can be read.
const unsigned long VOLT_ON_TIME_MS = 20000;
// Settling time after powering the divider before the first sample.
const unsigned long VOLTMETER_SETTLE_MS = 100;

const unsigned long WIFI_TIMEOUT_MS = 30000;
// Retry STA association in the background while the fallback AP remains up.
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 30000;
// Keep the AP briefly after reconnect so a flapping STA does not hide the
// configuration portal immediately.
const unsigned long WIFI_STABLE_BEFORE_AP_OFF_MS = 10000;

// Measurement cadence is independent from notification cadence. This bounds
// threshold-crossing detection latency without continuously powering the ADC
// divider while the inactive message deadline is overdue.
const unsigned long VOLTAGE_SAMPLE_INTERVAL_MS = 60000;
const unsigned long INVALID_SAMPLE_RETRY_MS = 5000;

// TLS certificate validation needs a plausible wall clock.
const unsigned long TLS_TIME_SYNC_TIMEOUT_MS = 10000;

// Hold the button this long to restore the compile-time configuration.
const unsigned long BUTTON_RESET_MS = 10000;
// A level must persist this long before it counts as a change.
const unsigned long BUTTON_DEBOUNCE_MS = 50;

// After a failed send, retry this soon instead of waiting out the full
// configured interval.
const unsigned long SEND_RETRY_INTERVAL_MS = 60000;

const unsigned long LOOP_DELAY_MS = 100;
