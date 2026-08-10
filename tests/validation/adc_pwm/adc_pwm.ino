/*
 * ADC / PWM Validation Test
 *
 * Single-device Unity tests for ADC, LEDC (PWM), and SigmaDelta.
 */

#include <Arduino.h>
#include <unity.h>
#include "pins_config.h"

#if SOC_ADC_SUPPORTED
#include "esp_adc/adc_continuous.h"
#endif

#define LEDC_FREQ 5000
#define LEDC_RES  8

void setUp(void) {}

void tearDown(void) {
#if SOC_LEDC_SUPPORTED
  ledcDetach(LEDC_PIN);
#endif
#if SOC_ADC_SUPPORTED
  analogContinuousStop();
  analogContinuousDeinit();
#endif
}

// ==================== ADC Tests ====================
#if SOC_ADC_SUPPORTED

void test_adc_read_basic(void) {
  uint16_t val = analogRead(ADC_PIN);
  TEST_ASSERT_LESS_OR_EQUAL(4095, val);
}

void test_adc_resolution(void) {
  analogReadResolution(10);
  uint16_t val10 = analogRead(ADC_PIN);
  TEST_ASSERT_LESS_OR_EQUAL(1023, val10);

  analogReadResolution(12);
  uint16_t val12 = analogRead(ADC_PIN);
  TEST_ASSERT_LESS_OR_EQUAL(4095, val12);
}

void test_adc_millivolts(void) {
  analogReadResolution(12);
  uint32_t mv = analogReadMilliVolts(ADC_PIN);
  TEST_ASSERT_LESS_OR_EQUAL(3300, mv);
}

void test_adc_attenuation(void) {
  analogReadResolution(12);

  analogSetAttenuation(ADC_0db);
  uint16_t v0 = analogRead(ADC_PIN);
  TEST_ASSERT_LESS_OR_EQUAL(4095, v0);

  analogSetAttenuation(ADC_11db);
  uint16_t v11 = analogRead(ADC_PIN);
  TEST_ASSERT_LESS_OR_EQUAL(4095, v11);
}

static volatile bool adc_continuous_done = false;

static void IRAM_ATTR adcContinuousISR(void) {
  adc_continuous_done = true;
}

void test_adc_continuous(void) {
  adc_continuous_done = false;
  const uint8_t pins[] = {(uint8_t)ADC_PIN};

  analogContinuousSetWidth(12);
  analogContinuousSetAtten(ADC_11db);

  TEST_ASSERT_TRUE(analogContinuous(pins, 1, 6, 20000, adcContinuousISR));
  TEST_ASSERT_TRUE(analogContinuousStart());

  unsigned long start = millis();
  while (!adc_continuous_done && millis() - start < 3000) {
    delay(10);
  }
  TEST_ASSERT_TRUE(adc_continuous_done);

  adc_continuous_result_t *results = NULL;
  TEST_ASSERT_TRUE(analogContinuousRead(&results, 1000));
  TEST_ASSERT_NOT_NULL(results);

  TEST_ASSERT_TRUE(analogContinuousStop());
  TEST_ASSERT_TRUE(analogContinuousDeinit());
}

#endif  // SOC_ADC_SUPPORTED

// ==================== LEDC Tests ====================
#if SOC_LEDC_SUPPORTED

void test_ledc_attach_detach(void) {
  TEST_ASSERT_TRUE(ledcAttach(LEDC_PIN, LEDC_FREQ, LEDC_RES));
  TEST_ASSERT_TRUE(ledcDetach(LEDC_PIN));
}

void test_ledc_write_duty(void) {
  TEST_ASSERT_TRUE(ledcAttach(LEDC_PIN, LEDC_FREQ, LEDC_RES));

  TEST_ASSERT_TRUE(ledcWrite(LEDC_PIN, 0));
  delay(5);
  TEST_ASSERT_EQUAL(0, ledcRead(LEDC_PIN));

  TEST_ASSERT_TRUE(ledcWrite(LEDC_PIN, 128));
  delay(5);
  TEST_ASSERT_EQUAL(128, ledcRead(LEDC_PIN));

  TEST_ASSERT_TRUE(ledcWrite(LEDC_PIN, 255));
  delay(5);
  /* 8-bit duty max can read back as 255 or 256 depending on SoC/timer */
  TEST_ASSERT_UINT32_WITHIN(1, 255, ledcRead(LEDC_PIN));

  ledcDetach(LEDC_PIN);
}

void test_ledc_read_freq(void) {
  TEST_ASSERT_TRUE(ledcAttach(LEDC_PIN, LEDC_FREQ, LEDC_RES));
  TEST_ASSERT_TRUE(ledcWrite(LEDC_PIN, 128));

  uint32_t freq = ledcReadFreq(LEDC_PIN);
  TEST_ASSERT_UINT32_WITHIN(500, LEDC_FREQ, freq);

  ledcDetach(LEDC_PIN);
}

void test_ledc_change_frequency(void) {
  TEST_ASSERT_TRUE(ledcAttach(LEDC_PIN, LEDC_FREQ, LEDC_RES));
  TEST_ASSERT_TRUE(ledcWrite(LEDC_PIN, 128));

  uint32_t new_freq = ledcChangeFrequency(LEDC_PIN, 10000, LEDC_RES);
  TEST_ASSERT_UINT32_WITHIN(1000, 10000, new_freq);

  ledcDetach(LEDC_PIN);
}

void test_ledc_fade(void) {
  TEST_ASSERT_TRUE(ledcAttach(LEDC_PIN, LEDC_FREQ, LEDC_RES));
  TEST_ASSERT_TRUE(ledcFade(LEDC_PIN, 0, 255, 500));
  delay(600);
  ledcDetach(LEDC_PIN);
}

void test_ledc_tone(void) {
  TEST_ASSERT_TRUE(ledcAttach(LEDC_PIN, LEDC_FREQ, LEDC_RES));
  uint32_t freq = ledcWriteTone(LEDC_PIN, 440);
  TEST_ASSERT_UINT32_WITHIN(10, 440, freq);
  ledcDetach(LEDC_PIN);
}

#endif  // SOC_LEDC_SUPPORTED

// ==================== SigmaDelta Tests ====================
#if SOC_SDM_SUPPORTED

void test_sigmadelta_attach_detach(void) {
  TEST_ASSERT_TRUE(sigmaDeltaAttach(SIGMADELTA_PIN, 1000000));
  TEST_ASSERT_TRUE(sigmaDeltaWrite(SIGMADELTA_PIN, 0));
  TEST_ASSERT_TRUE(sigmaDeltaWrite(SIGMADELTA_PIN, 127));
  TEST_ASSERT_TRUE(sigmaDeltaWrite(SIGMADELTA_PIN, -128));
  TEST_ASSERT_TRUE(sigmaDeltaDetach(SIGMADELTA_PIN));
}

#endif  // SOC_SDM_SUPPORTED

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();

#if SOC_ADC_SUPPORTED
  RUN_TEST(test_adc_read_basic);
  RUN_TEST(test_adc_resolution);
  RUN_TEST(test_adc_millivolts);
  RUN_TEST(test_adc_attenuation);
  RUN_TEST(test_adc_continuous);
#endif

#if SOC_LEDC_SUPPORTED
  RUN_TEST(test_ledc_attach_detach);
  RUN_TEST(test_ledc_write_duty);
  RUN_TEST(test_ledc_read_freq);
  RUN_TEST(test_ledc_change_frequency);
  RUN_TEST(test_ledc_fade);
  RUN_TEST(test_ledc_tone);
#endif

#if SOC_SDM_SUPPORTED
  RUN_TEST(test_sigmadelta_attach_detach);
#endif

  UNITY_END();
}

void loop() {}
