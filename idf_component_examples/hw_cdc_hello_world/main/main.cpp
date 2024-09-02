#include <Arduino.h>

void setup() {
  // USB CDC doesn't need a baud rate
  Serial.begin();

  // wait for the Serial Monitor to be open
  while (!Serial) {
    delay(100);
  }

  Serial.println("\r\nStarting...\r\n");
}

void loop() {
  Serial.println("Hello world!");
  delay(1000);
}
