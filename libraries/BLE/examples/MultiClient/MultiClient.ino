/*
 * BLE Multi-Client Example -- New API
 *
 * Scans for devices advertising the Heart Rate service (0x180D),
 * then connects to up to 3 servers simultaneously. Demonstrates
 * the multi-instance client design.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>
#include <algorithm>

static BLEUUID svcUUID("180D");

void onClientDisconnected(BLEClient client, const BLEConnInfo &conn, uint8_t reason) {
  Serial.printf("Client disconnected from %s (reason 0x%02X)\n", conn.getAddress().toString().c_str(), reason);
}

void setup() {
  Serial.begin(115200);
  if (!BLE.begin("MultiClient")) {
    Serial.println("BLE init failed!");
    return;
  }

  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);

  BLEScanResults results = scan.startBlocking(5000);
  Serial.printf("Found %d devices\n", results.getCount());

  std::vector<BLEClient> clients;

  for (size_t i = 0; i < std::min(results.getCount(), (size_t)3); i++) {
    BLEAdvertisedDevice dev = results.getDevice(i);
    if (!dev.isAdvertisingService(svcUUID)) {
      continue;
    }

    BLEClient client = BLE.createClient();
    client.onDisconnect(onClientDisconnected);
    client.onConnect([i](BLEClient c, const BLEConnInfo &conn) {
      Serial.printf("Client[%d] connected to %s\n", i, conn.getAddress().toString().c_str());
    });

    if (client.connect(dev)) {
      if (client.discoverServices()) {
        BLERemoteService svc = client.getService(svcUUID);
        if (svc) {
          Serial.printf("Client[%d] found Heart Rate service\n", i);
        }
      }
      clients.push_back(client);
    }
  }

  Serial.printf("Connected to %d servers\n", clients.size());
}

void loop() {
  delay(1000);
}
