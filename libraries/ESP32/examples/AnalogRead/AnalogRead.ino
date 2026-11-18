#include <Arduino.h>

void setup() {
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  // Set the resolution to 12 bits (0-4095). Not used on ESP32-S31.
#ifndef CONFIG_IDF_TARGET_ESP32S31
  analogReadResolution(12);
#endif
}

void loop() {
  // A0 GPIO: ESP32 36, S2/S3/C2/C5/H2 1, C3/C6/C61 0, S31 42, P4 16.
  const uint8_t adc_pin = A0;
#ifdef CONFIG_IDF_TARGET_ESP32S31
  uint32_t analogValue = analogReadComparator(adc_pin);
#else
  uint32_t analogValue = analogRead(adc_pin);
#endif
  int32_t analogVolts = analogReadMilliVolts(adc_pin);

  // print out the values you read:
  Serial.printf("ADC analog value = %" PRIu32 "\n", analogValue);
  Serial.printf("ADC millivolts value = %" PRId32 "\n", analogVolts);

  delay(100);  // delay in between reads for clear read from serial
}
