/* CPU test
 *
 * Tests miscellaneous CPU functions like changing the CPU frequency, checking the CPU frequency,
 * reading the CPU temperature, etc.
 */

#include <Arduino.h>
#include <unity.h>

/* Utility global variables */

/* Test functions */

#if SOC_TEMP_SENSOR_SUPPORTED
void get_cpu_temperature() {
  float temp = temperatureRead();
  log_d("CPU temperature: %f", temp);
  TEST_ASSERT_FLOAT_IS_NOT_NAN(temp);
  TEST_ASSERT_FLOAT_WITHIN(40.0, 30.0, temp);
}
#endif

/* Main functions */

void setup() {
  UNITY_BEGIN();

  #if SOC_TEMP_SENSOR_SUPPORTED
  RUN_TEST(get_cpu_temperature);
  #endif

  UNITY_END();
}

void loop() {}
