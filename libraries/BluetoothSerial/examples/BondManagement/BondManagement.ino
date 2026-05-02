/*
 * BluetoothSerial Bond Management
 *
 * Lists bonded devices, and demonstrates how to delete individual bonds
 * or all bonds. Useful for troubleshooting pairing issues.
 */

#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

void printBondedDevices() {
  auto bonded = SerialBT.getBondedDevices();
  Serial.printf("Bonded devices: %d\n", bonded.size());
  for (size_t i = 0; i < bonded.size(); i++) {
    Serial.printf("  [%d] %s\n", i, bonded[i].toString().c_str());
  }
}

void setup() {
  Serial.begin(115200);

  BTStatus status = SerialBT.begin("ESP32-Bonds");
  if (!status) {
    Serial.println("Bluetooth init failed!");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Bluetooth started. Commands:");
  Serial.println("  'l' - List bonded devices");
  Serial.println("  'd' - Delete all bonds");
  Serial.println("  'q' - Quit (end Bluetooth)");

  printBondedDevices();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'l':
      case 'L': printBondedDevices(); break;
      case 'd':
      case 'D':
      {
        BTStatus s = SerialBT.deleteAllBonds();
        Serial.printf("Delete all bonds: %s\n", s ? "OK" : "Failed");
        printBondedDevices();
        break;
      }
      case 'q':
      case 'Q':
        Serial.println("Ending Bluetooth...");
        SerialBT.end(false);
        Serial.println("Done.");
        break;
    }
  }

  while (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }

  delay(10);
}
