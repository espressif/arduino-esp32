#include <Arduino.h>

void setup() {
  Serial.begin(); // USB CDC doens't need a baud rate
}

void loop() {
  Serial.println("Hello world!");
  delay(1000);
}
