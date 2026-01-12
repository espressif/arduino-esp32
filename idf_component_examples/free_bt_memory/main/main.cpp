#include "Arduino.h"

// Strong override of the weak btInUse() in the core.
bool btInUse(void) {
  return false;  // free BTDM memory because this component does not use Bluetooth
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("btInUse override active: releasing BT controller memory");
  Serial.println("No Bluetooth activity in this example.");
}

void loop() {
  delay(1000);
}
