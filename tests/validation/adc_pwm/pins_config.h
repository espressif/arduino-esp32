/*
 * Per-target GPIO selection for adc_pwm.
 *
 * Pins must be valid for this sketch per .github/CI_README.md (DevKit GPIO
 * Reference): no strapping / flash / USB-JTAG / input-only pins, and no
 * conflict with peripherals used here (ADC, LEDC, SigmaDelta only —
 * Wire, SPI, and Touch are not initialized).
 */

#ifndef ADC_PWM_PINS_CONFIG_H
#define ADC_PWM_PINS_CONFIG_H

#include <Arduino.h>

#if CONFIG_IDF_TARGET_ESP32
#define ADC_PIN        A4 /* GPIO32, ADC1 — see CI_README Peripheral API constraints */
#define LEDC_PIN       16 /* not T0 (GPIO4); see CI_README LEDC + Touch note */
#define SIGMADELTA_PIN 13
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_PIN        A0
#define LEDC_PIN       4
#define SIGMADELTA_PIN 5
#elif CONFIG_IDF_TARGET_ESP32S3
#define ADC_PIN        A0
#define LEDC_PIN       4
#define SIGMADELTA_PIN 5
#elif CONFIG_IDF_TARGET_ESP32C3
#define ADC_PIN        A0
#define LEDC_PIN       3
#define SIGMADELTA_PIN 10
#elif CONFIG_IDF_TARGET_ESP32C5
#define ADC_PIN        A0
#define LEDC_PIN       4
#define SIGMADELTA_PIN 9
#elif CONFIG_IDF_TARGET_ESP32C6
#define ADC_PIN        A0
#define LEDC_PIN       2
#define SIGMADELTA_PIN 3
#elif CONFIG_IDF_TARGET_ESP32H2
#define ADC_PIN        A0
#define LEDC_PIN       4
#define SIGMADELTA_PIN 5
#elif CONFIG_IDF_TARGET_ESP32P4
#define ADC_PIN        A4 /* GPIO20; A0→16 is SDIO */
#define LEDC_PIN       6
#define SIGMADELTA_PIN 7
#else
#error "adc_pwm: add pins for this target in pins_config.h"
#endif

#endif /* ADC_PWM_PINS_CONFIG_H */
