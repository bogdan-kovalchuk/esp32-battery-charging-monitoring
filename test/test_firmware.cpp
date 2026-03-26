#include <unity.h>
#include "../src/voltage.h"
#include "../include/app_config.h"

unsigned long minutesToMillis(unsigned long minutes) {
  return minutes * 60000UL;
}

void test_voltage_convert_normal(void) {
  float result = voltageConvert(2048.0);
  TEST_ASSERT_FLOAT_WITHIN(0.2, 8.2, result);
}

void test_voltage_convert_zero(void) {
  float result = voltageConvert(0.0);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 0.0, result);
}

void test_voltage_convert_max(void) {
  float result = voltageConvert(4095.0);
  TEST_ASSERT_FLOAT_WITHIN(0.2, 16.5, result);
}

void test_minutes_to_millis(void) {
  TEST_ASSERT_EQUAL(60000, minutesToMillis(1));
  TEST_ASSERT_EQUAL(300000, minutesToMillis(5));
  TEST_ASSERT_EQUAL(18000000, minutesToMillis(300));
}

void test_minutes_to_millis_zero(void) {
  TEST_ASSERT_EQUAL(0, minutesToMillis(0));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_voltage_convert_normal);
  RUN_TEST(test_voltage_convert_zero);
  RUN_TEST(test_voltage_convert_max);
  RUN_TEST(test_minutes_to_millis);
  RUN_TEST(test_minutes_to_millis_zero);
  UNITY_END();
  return 0;
}
