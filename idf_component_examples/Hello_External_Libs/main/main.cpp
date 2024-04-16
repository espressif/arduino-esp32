#include "Arduino.h"
#include "CircularBuffer.h"

void setup() {
  Serial.begin(115200);

  uint8_t data[10];
  CircularBuffer buffer(data, sizeof(data));
  buffer.put(1);
}

void loop() {
  Serial.println("Hello world!");
  delay(1000);
}
