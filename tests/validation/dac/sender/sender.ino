/*
 * DAC loopback validation -- Sender (multi-DUT, generic_multi_device)
 *
 * Writes DAC values on DAC1; the reader verifies the output via ADC.
 */

#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("[SENDER] Ready");
  Serial.println("[SENDER] DAC_READY");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("DAC_WRITE ")) {
      int value = cmd.substring(10).toInt();
      dacWrite(DAC1, value);
      delay(50);
      Serial.printf("[SENDER] DAC_WRITTEN %d\n", value);
    } else if (cmd == "DAC_DISABLE") {
      dacDisable(DAC1);
      Serial.println("[SENDER] DAC_DISABLED");
    } else if (cmd == "DONE") {
      Serial.println("[SENDER] DONE");
      while (true) {
        delay(1000);
      }
    }
  }
  delay(10);
}
