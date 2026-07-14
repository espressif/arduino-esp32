/*
 * BLE Client + Server Coexistence Example -- New API
 *
 * Runs a GATT server and client simultaneously on the same device
 * (dual-role). The server advertises a service while the client scans
 * for a peer running the same example, connects, and reads its value.
 *
 * Run this sketch on two ESP32 boards to see them discover and
 * exchange data with each other.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

// Custom UUIDs generated for this example (use https://www.uuidgenerator.net/ to create your own)
static const BLEUUID SVC_UUID("91bad492-b950-4226-aa2b-4ede9fa42f59");
static const BLEUUID CHR_UUID("0d563a58-196a-48ce-ace2-dfec78acc814");

BLECharacteristic localChr;
BTAddress peerAddr;
bool peerFound = false;

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Client + Server Coexistence Example");

  BTStatus status = BLE.begin("ESP32-DualRole");
  if (!status) {
    Serial.printf("BLE init failed! (%s)\n", status.toString());
    while (true) {
      delay(1000);
    }
  }

  // --- Server side ---
  BLEServer server = BLE.createServer();
  server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    Serial.println("Incoming client connected");
  });
  server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
    Serial.println("Incoming client disconnected");
    BLE.startAdvertising();
  });

  BLEService svc = server.createService(SVC_UUID);
  localChr = svc.createCharacteristic(CHR_UUID, BLEProperty::Read | BLEProperty::Write | BLEProperty::Notify, BLEPermissions::OpenReadWrite);
  localChr.setValue("Hello from dual-role device");
  server.start();

  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(SVC_UUID);
  adv.start();
  Serial.println("Server started and advertising");

  // --- Client side ---
  BLEScan scan = BLE.getScan();
  scan.onResult([](const BLEAdvertisedDevice &dev) {
    if (dev.isAdvertisingService(SVC_UUID) && dev.getName() != "ESP32-DualRole") {
      Serial.printf("Found peer: %s\n", dev.getName().c_str());
      peerAddr = dev.getAddress();
      peerFound = true;
      BLE.getScan().stop();
    }
  });

  Serial.println("Scanning for peers...");
  scan.startBlocking(10000);

  if (peerFound) {
    BLEClient client = BLE.createClient();
    BTStatus status = client.connect(peerAddr);
    if (status) {
      Serial.println("Connected to peer as client!");
      BLERemoteService remoteSvc = client.getService(SVC_UUID);
      if (remoteSvc) {
        BLERemoteCharacteristic remoteChr = remoteSvc.getCharacteristic(CHR_UUID);
        if (remoteChr) {
          String val = remoteChr.readValue();
          Serial.printf("Peer says: %s\n", val.c_str());
        }
      }
      delay(2000);
      client.disconnect();
    }
  } else {
    Serial.println("No peer found. Run this example on two devices!");
  }
}

void loop() {
  delay(2000);
}
