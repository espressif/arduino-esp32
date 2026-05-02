/*
 * BluetoothSerial Serial Bridge
 *
 * Bridges Serial (USB) and Bluetooth SPP, forwarding data bidirectionally.
 * Pair from a phone/PC Bluetooth terminal app and type to see data echoed.
 */

#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);

  BTStatus status = SerialBT.begin("ESP32-BT-Bridge");
  if (!status) {
    Serial.println("Bluetooth init failed!");
    while (true) {
      delay(1000);
    }
  }

  Serial.printf("Bluetooth started as \"%s\"\n", "ESP32-BT-Bridge");
  Serial.printf("Local address: %s\n", SerialBT.getAddress().toString().c_str());
  Serial.println("You can now pair from a phone/PC Bluetooth terminal.");
}

void loop() {
  // Forward USB Serial -> BT
  while (Serial.available()) {
    SerialBT.write(Serial.read());
  }

  // Forward BT -> USB Serial
  while (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }

  delay(1);
}
