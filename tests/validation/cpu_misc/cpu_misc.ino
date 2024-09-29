/* CPU test
 *
 * Tests miscellaneous CPU functions like changing the CPU frequency, checking the CPU frequency,
 * reading the CPU temperature, etc.
 */

#include <Arduino.h>
#include <unity.h>

/* Utility global variables */

/* Test functions */

void get_cpu_temperature() {
#if SOC_TEMP_SENSOR_SUPPORTED
  float temp = temperatureRead();
  log_d("CPU temperature: %f", temp);
  TEST_ASSERT_FLOAT_IS_NOT_NAN(temp);
  TEST_ASSERT_FLOAT_WITHIN(40.0, 30.0, temp);
#else
  log_d("CPU temperature not supported");
#endif
}

/* Main functions */

void setup() {
  UNITY_BEGIN();

  RUN_TEST(get_cpu_temperature);

  UNITY_END();
}

void loop() {}
