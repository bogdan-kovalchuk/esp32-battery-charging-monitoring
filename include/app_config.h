#pragma once

const int PIN_VOLTAGE = 32;
const int PIN_BUTTON = 34;
const int PIN_VOLTMETER = 4;

const float RESISTOR_R1 = 30000.0f;
const float RESISTOR_R2 = 7500.0f;
const float V_REF = 3.3f;
const float ADC_MAX = 4095.0f;
const int NUM_SAMPLES = 100;
const float CORR_FACTOR = 1.0468f;

const unsigned long VOLT_ON_TIME_MS = 20000;
const unsigned long WIFI_TIMEOUT_MS = 30000;
const unsigned long BUTTON_RESET_MS = 10000;
const unsigned long LOOP_DELAY_MS = 100;
