/*
 * BluetoothSerial Device Discovery
 *
 * Performs a BT Classic device inquiry and prints discovered devices
 * with name, address, CoD, and RSSI.
 */

#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);

  BTStatus status = SerialBT.begin("ESP32-Discovery", true);
  if (!status) {
    Serial.println("Bluetooth init failed!");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Starting discovery (10 seconds)...");
  auto results = SerialBT.discover(10000);

  Serial.printf("Found %d devices:\n", results.size());
  for (const auto &dev : results) {
    Serial.printf("  %s  %-20s  CoD: 0x%06lX  RSSI: %d\n", dev.address.toString().c_str(), dev.name.c_str(), dev.cod & 0xFFFFFF, dev.rssi);
  }

  Serial.println("\nAsync discovery (10 seconds)...");
  SerialBT.discoverAsync(
    [](const BluetoothSerial::DiscoveryResult &dev) {
      Serial.printf("  Found: %s %s RSSI:%d\n", dev.address.toString().c_str(), dev.name.c_str(), dev.rssi);
    },
    10000
  );

  delay(12000);
  SerialBT.discoverStop();
  Serial.println("Discovery complete.");
}

void loop() {
  delay(1000);
}
