/*
  FreeBTMemory.ino

  Demonstrates overriding btInUse() to RELEASE BT controller memory when
  Bluetooth is not used by the sketch. Return true only if you need BT/BLE.
*/

#include <Arduino.h>

// Strong override of the weak default in the core.
bool btInUse(void) {
  return false;  // free BTDM memory because this sketch does not use Bluetooth
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("btInUse override active: releasing BT controller memory");
  Serial.println("No Bluetooth activity in this sketch.");
  uint32_t free_memory = ESP.getFreeHeap();
  Serial.printf("Free memory: %lu bytes\n", free_memory);
}

void loop() {
  delay(1000);
}
