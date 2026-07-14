/*
 * BLE Client Example -- New API
 *
 * Scans for a BLE server advertising a specific service UUID,
 * connects, discovers services, subscribes to notifications,
 * and prints received data.
 *
 * Pair with the Server example on another ESP32.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

// Custom UUIDs -- must match the Server example
static const BLEUUID SVC_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static const BLEUUID CHR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

BLEClient client;
bool connected = false;
volatile bool doConnect = false;
BTAddress serverAddress;

void onNotification(BLERemoteCharacteristic chr, const uint8_t *data, size_t len, bool isNotify) {
  Serial.printf("[%s] data(%d bytes): %.*s\n", isNotify ? "NOTIFY" : "INDICATE", (int)len, (int)len, data);
}

// NOTE: Keep this callback lightweight. Blocking BLE operations (createClient,
// connect, etc.) must NOT be called here — they would deadlock the BLE stack
// task that delivers this callback. Save the address and set a flag; do the
// actual connection from loop().
void onDeviceFound(BLEAdvertisedDevice device) {
  if (!device.isAdvertisingService(SVC_UUID)) {
    return;
  }

  Serial.printf("Found target server: \"%s\" at %s (RSSI %d)\n", device.getName().c_str(), device.getAddress().toString().c_str(), device.getRSSI());
  serverAddress = device.getAddress();
  doConnect = true;
  BLE.getScan().stop();
}

bool connectToServer() {
  Serial.print("Creating client... ");
  client = BLE.createClient();
  if (!client) {
    Serial.println("FAILED!");
    return false;
  }
  Serial.println("OK");

  Serial.printf("Connecting to %s... ", serverAddress.toString().c_str());
  BTStatus status = client.connect(serverAddress);
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    return false;
  }
  Serial.println("OK");

  Serial.print("Discovering services... ");
  if (!client.discoverServices()) {
    Serial.println("FAILED!");
    return false;
  }
  Serial.println("OK");

  Serial.print("Looking for service... ");
  BLERemoteService svc = client.getService(SVC_UUID);
  if (!svc) {
    Serial.println("NOT FOUND!");
    return false;
  }
  Serial.printf("OK (handle 0x%04X)\n", svc.getHandle());

  Serial.print("Looking for characteristic... ");
  BLERemoteCharacteristic chr = svc.getCharacteristic(CHR_UUID);
  if (!chr) {
    Serial.println("NOT FOUND!");
    return false;
  }
  Serial.printf("OK (handle 0x%04X)\n", chr.getHandle());

  if (chr.canNotify()) {
    Serial.print("Subscribing to notifications... ");
    status = chr.subscribe(true, onNotification);
    if (status) {
      Serial.println("OK");
    } else {
      Serial.printf("FAILED! (%s)\n", status.toString());
    }
  }

  String val = chr.readValue();
  Serial.printf("Current value: \"%s\"\n", val.c_str());

  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BLE Client Example ===");

  Serial.print("Initializing BLE... ");
  BTStatus initStatus = BLE.begin("MyClient");
  if (!initStatus) {
    Serial.printf("FAILED! (%s)\n", initStatus.toString());
    while (true) {
      delay(1000);
    }
  }
  Serial.println("OK");

  Serial.println("Starting scan...");
  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);
  scan.setInterval(100);  // Scan interval in 0.625 ms units (100 = 62.5 ms)
  scan.setWindow(99);     // Scan window in 0.625 ms units (nearly continuous scanning)
  scan.onResult(onDeviceFound);
  scan.start(0);
  Serial.println("Scanning for target service UUID...");
}

void loop() {
  if (doConnect) {
    doConnect = false;
    if (connectToServer()) {
      connected = true;
      Serial.println();
      Serial.println("Connected and subscribed! Waiting for notifications...");
      Serial.println();
    } else {
      Serial.println("Connection failed! Restarting scan...");
      BLE.getScan().start(0);
    }
  }

  if (!client || !client.isConnected()) {
    if (connected) {
      Serial.println("Connection lost! Restarting scan...");
      connected = false;
      doConnect = false;
      BLE.getScan().start(0);
    }
  }
  delay(500);
}
