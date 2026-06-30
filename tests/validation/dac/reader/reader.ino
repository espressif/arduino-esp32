/*
 * DAC loopback validation -- Reader (multi-DUT, generic_multi_device)
 *
 * Reads ADC on the pin connected to the sender's DAC output.
 * Pin-to-pin mapping: sender DAC1 <-> reader DAC1
 * (GPIO25 on ESP32, GPIO17 on ESP32-S2).
 */

#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(DAC1, INPUT);

  Serial.println("[READER] Ready");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "ADC_READ") {
      uint32_t mv = analogReadMilliVolts(DAC1);
      Serial.printf("[READER] ADC_MV=%lu\n", (unsigned long)mv);
    } else if (cmd == "DONE") {
      Serial.println("[READER] DONE");
      while (true) {
        delay(1000);
      }
    }
  }
  delay(10);
}
