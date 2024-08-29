#include <Arduino.h>

void setup() {
  Serial.begin(); // USB CDC doens't need a baud rate
  while(!Serial) delay(100); // wait for the Serial Monitor to be open
  Serial.println("\r\nStarting...\r\n");
}

void loop() {
  Serial.println("Hello world!");
  delay(1000);
}
