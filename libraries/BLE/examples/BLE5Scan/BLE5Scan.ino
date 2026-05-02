/*
 * BLE5 Extended Scanning + Periodic Sync Example -- New API
 *
 * Demonstrates BLE 5.0 extended scanning and periodic advertising
 * synchronization (scanner/observer side).
 *
 * 1. Scans for a periodic advertiser named "ESP32-Periodic"
 * 2. Once found, creates a periodic sync to receive data
 * 3. Prints received periodic advertising payloads
 *
 * Pair with the BLE5PeriodicAdv example on another ESP32.
 * Requires BLE 5.0 hardware (not original ESP32).
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

void onSyncEstablished(uint16_t syncHandle, uint8_t sid, const BTAddress &addr, BLEPhy phy, uint16_t interval) {
  Serial.printf("Periodic sync established! handle=%d, SID=%d, interval=%d\n", syncHandle, sid, interval);
}

void onSyncReport(uint16_t syncHandle, int8_t rssi, int8_t txPower, const uint8_t *data, size_t len) {
  Serial.printf("Periodic data (RSSI %d): %.*s\n", rssi, (int)len, data);
}

void onSyncLost(uint16_t syncHandle) {
  Serial.printf("Periodic sync lost! handle=%d\n", syncHandle);
}

void setup() {
  Serial.begin(115200);
  if (!BLE.begin("Periodic-Sync")) {
    Serial.println("BLE init failed!");
    return;
  }

  BLEScan scan = BLE.getScan();
  BTAddress targetAddr;
  uint8_t targetSID = 0;

  scan.onPeriodicSync(onSyncEstablished);
  scan.onPeriodicReport(onSyncReport);
  scan.onPeriodicLost(onSyncLost);
  scan.setActiveScan(true);

  scan.onResult([&](BLEAdvertisedDevice dev) {
    if (dev.getName() == "ESP32-Periodic" && dev.getPeriodicInterval() > 0) {
      targetAddr = dev.getAddress();
      targetSID = dev.getAdvSID();
      scan.stop();
    }
  });

  scan.startBlocking(5000);

  if (!targetAddr.isZero()) {
    scan.createPeriodicSync(targetAddr, targetSID, 0, 10000);
    Serial.println("Waiting for periodic sync...");
  } else {
    Serial.println("Periodic advertiser not found");
  }
}

void loop() {
  delay(100);
}
