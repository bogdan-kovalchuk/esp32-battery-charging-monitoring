// Host-side unit tests. These include only voltage_math.h, which is free of
// <Arduino.h>, so they build with a plain host compiler:
//
//     pio test -d firmware -e native
//
// Requires gcc/clang/MSVC on PATH.

#include <unity.h>

#include "../../include/app_config.h"
#include "../../src/voltage_math.h"
#include "../../src/report_scheduler.h"

// Reproduces the divider maths independently of the implementation, so a typo
// in voltage_math.h cannot be mirrored by a typo in the expected value.
static float expectedVoltage(float raw) {
  float out = (raw / 4095.0f) * 3.3f;
  return 1.0468f * roundf(out * (1.0f + 30000.0f / 7500.0f) * 10.0f) / 10.0f;
}

void test_voltage_convert_zero(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, voltageConvert(0.0f));
}

void test_voltage_convert_midscale(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, expectedVoltage(2048.0f), voltageConvert(2048.0f));
}

void test_voltage_convert_full_scale(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, expectedVoltage(4095.0f), voltageConvert(4095.0f));
}

// A nominal 12 V battery must land near 12 V, or the divider constants are
// wrong. 12.0 V across the 30k/7.5k divider gives 2.4 V, i.e. raw 2979.
void test_voltage_convert_nominal_twelve_volts(void) {
  float raw = (12.0f / (1.0f + 30000.0f / 7500.0f)) / 3.3f * 4095.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.3f, 12.0f * 1.0468f, voltageConvert(raw));
}

// The conversion must be monotonic: a higher ADC reading can never report a
// lower battery voltage, or the alert threshold could be crossed backwards.
void test_voltage_convert_is_monotonic(void) {
  float previous = voltageConvert(0.0f);
  for (float raw = 50.0f; raw <= 4095.0f; raw += 50.0f) {
    float current = voltageConvert(raw);
    TEST_ASSERT_TRUE(current >= previous);
    previous = current;
  }
}

void test_minutes_to_millis(void) {
  TEST_ASSERT_EQUAL_UINT32(0UL, minutesToMillis(0));
  TEST_ASSERT_EQUAL_UINT32(60000UL, minutesToMillis(1));
  TEST_ASSERT_EQUAL_UINT32(300000UL, minutesToMillis(5));
  TEST_ASSERT_EQUAL_UINT32(18000000UL, minutesToMillis(300));
}

// CFG_INTERVAL_MAX is one week. The product must not overflow 32 bits, which
// is what millis() returns and what the loop compares against.
void test_minutes_to_millis_at_maximum_interval(void) {
  unsigned long maxMinutes = 10080;
  TEST_ASSERT_EQUAL_UINT32(604800000UL, minutesToMillis(maxMinutes));
  TEST_ASSERT_TRUE(minutesToMillis(maxMinutes) < 0xFFFFFFFFUL);
}

void test_scheduler_reports_initial_state_once(void) {
  ReportScheduler scheduler(60000);
  TEST_ASSERT_TRUE(scheduler.sampleDue(0));
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::INFO,
                        (int)scheduler.recordSample(0, false));
  scheduler.recordSendResult(0, true, 300000, 60000);
  TEST_ASSERT_FALSE(scheduler.sampleDue(59999));
  TEST_ASSERT_TRUE(scheduler.sampleDue(60000));
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::NONE,
                        (int)scheduler.recordSample(60000, false));
}

void test_scheduler_reports_threshold_transitions_immediately(void) {
  ReportScheduler scheduler(60000);
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::INFO,
                        (int)scheduler.recordSample(0, false));
  scheduler.recordSendResult(0, true, 604800000, 60000);
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::ALERT,
                        (int)scheduler.recordSample(60000, true));
  scheduler.recordSendResult(60000, true, 604800000, 60000);
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::INFO,
                        (int)scheduler.recordSample(120000, false));
  scheduler.recordSendResult(120000, true, 604800000, 60000);
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::ALERT,
                        (int)scheduler.recordSample(180000, true));
}

void test_scheduler_retries_failed_send(void) {
  ReportScheduler scheduler(1000);
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::ALERT,
                        (int)scheduler.recordSample(0, true));
  scheduler.recordSendResult(0, false, 10000, 2000);
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::NONE,
                        (int)scheduler.recordSample(1000, true));
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::ALERT,
                        (int)scheduler.recordSample(2000, true));
}

void test_scheduler_invalid_sample_is_bounded(void) {
  ReportScheduler scheduler(60000);
  scheduler.recordInvalidSample(0, 5000);
  TEST_ASSERT_FALSE(scheduler.sampleDue(4999));
  TEST_ASSERT_TRUE(scheduler.sampleDue(5000));
}

void test_scheduler_deadlines_survive_millis_rollover(void) {
  ReportScheduler scheduler(32);
  uint32_t nearWrap = 0xFFFFFFF0u;
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::INFO,
                        (int)scheduler.recordSample(nearWrap, false));
  scheduler.recordSendResult(nearWrap, true, 64, 16);
  TEST_ASSERT_FALSE(scheduler.sampleDue(0x0000000Fu));
  TEST_ASSERT_TRUE(scheduler.sampleDue(0x00000010u));
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::NONE,
                        (int)scheduler.recordSample(0x00000010u, false));
  TEST_ASSERT_EQUAL_INT((int)ReportDecision::INFO,
                        (int)scheduler.recordSample(0x00000030u, false));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_voltage_convert_zero);
  RUN_TEST(test_voltage_convert_midscale);
  RUN_TEST(test_voltage_convert_full_scale);
  RUN_TEST(test_voltage_convert_nominal_twelve_volts);
  RUN_TEST(test_voltage_convert_is_monotonic);
  RUN_TEST(test_minutes_to_millis);
  RUN_TEST(test_minutes_to_millis_at_maximum_interval);
  RUN_TEST(test_scheduler_reports_initial_state_once);
  RUN_TEST(test_scheduler_reports_threshold_transitions_immediately);
  RUN_TEST(test_scheduler_retries_failed_send);
  RUN_TEST(test_scheduler_invalid_sample_is_bounded);
  RUN_TEST(test_scheduler_deadlines_survive_millis_rollover);
  return UNITY_END();
}
