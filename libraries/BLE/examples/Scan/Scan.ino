/*
 * BLE Scan Example -- New API
 *
 * Performs a blocking BLE scan and prints all discovered devices
 * with their name, address, RSSI, and advertised service UUIDs.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BLE Scan Example ===");

  Serial.print("Initializing BLE... ");
  if (!BLE.begin("Scanner")) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");

  BLEScan scan = BLE.getScan();
  if (!scan) {
    Serial.println("Failed to get scan object!");
    return;
  }

  scan.setActiveScan(true);
  scan.setInterval(100);
  scan.setWindow(99);
  scan.setFilterDuplicates(true);

  Serial.println("Starting scan for 5 seconds...");
  Serial.println();
  BLEScanResults results = scan.startBlocking(5000);

  Serial.printf("Scan complete! Found %d device(s):\n", results.getCount());
  Serial.println("---------------------------------------------");
  for (const BLEAdvertisedDevice &dev : results) {
    Serial.printf("  Address: %s", dev.getAddress().toString().c_str());
    if (dev.haveName()) {
      Serial.printf("  Name: \"%s\"", dev.getName().c_str());
    }
    Serial.printf("  RSSI: %d dBm", dev.getRSSI());
    if (dev.haveServiceUUID()) {
      Serial.printf("  Service: %s", dev.getServiceUUID().toString().c_str());
    }
    if (dev.haveManufacturerData()) {
      Serial.printf("  Mfg: 0x%04X", dev.getManufacturerCompanyId());
    }
    Serial.println();
  }
  Serial.println("---------------------------------------------");
  Serial.println("Scan finished. Will scan again in 10 seconds.");
}

void loop() {
  delay(10000);
  Serial.println();
  Serial.println("Rescanning...");

  BLEScan scan = BLE.getScan();
  BLEScanResults results = scan.startBlocking(5000);

  Serial.printf("Found %d device(s):\n", results.getCount());
  for (const BLEAdvertisedDevice &dev : results) {
    Serial.printf("  %s", dev.getAddress().toString().c_str());
    if (dev.haveName()) {
      Serial.printf(" \"%s\"", dev.getName().c_str());
    }
    Serial.printf(" RSSI:%d", dev.getRSSI());
    Serial.println();
  }
}
