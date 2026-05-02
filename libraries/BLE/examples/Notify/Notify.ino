/*
 * BLE Notify Example -- New API
 *
 * Creates a GATT server with a notify characteristic.
 * Sends an incrementing counter as notifications every 2 seconds
 * to any subscribed client.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic notifyChr;
uint32_t counter = 0;
bool clientConnected = false;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BLE Notify Example ===");

  Serial.print("Initializing BLE... ");
  if (!BLE.begin("ESP32-Notify")) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");

  Serial.print("Creating server... ");
  BLEServer server = BLE.createServer();
  if (!server) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");

  server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    clientConnected = true;
    Serial.printf("Client connected: %s (MTU %d)\n", conn.getAddress().toString().c_str(), conn.getMTU());
  });
  server.onDisconnect([](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
    clientConnected = false;
    Serial.printf("Client disconnected (reason 0x%02X), restarting advertising...\n", reason);
    BLE.startAdvertising();
    Serial.println("Advertising restarted");
  });

  Serial.print("Creating service... ");
  BLEService svc = server.createService(BLEUUID(SERVICE_UUID));
  if (!svc) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");

  Serial.print("Creating notify characteristic... ");
  notifyChr = svc.createCharacteristic(BLEUUID(CHARACTERISTIC_UUID), BLEProperty::Read | BLEProperty::Notify, BLEPermissions::OpenRead);
  if (!notifyChr) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");
  notifyChr.setValue(counter);

  Serial.print("Starting server... ");
  BTStatus status = server.start();
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    return;
  }
  Serial.println("OK");

  Serial.print("Starting advertising... ");
  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(BLEUUID(SERVICE_UUID));
  status = adv.start();
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    return;
  }
  Serial.println("OK");

  Serial.println();
  Serial.println("Server ready! Waiting for connections...");
  Serial.printf("Device: %s\n", BLE.getDeviceName().c_str());
  Serial.println("Notifications will be sent every 2 seconds once subscribed.");
  Serial.println();
}

void loop() {
  if (notifyChr.getSubscribedCount() > 0) {
    counter++;
    notifyChr.setValue(counter);
    notifyChr.notify();
    Serial.printf("[%lu] Notified: %lu (subscribers: %d)\n", millis() / 1000, (unsigned long)counter, notifyChr.getSubscribedCount());
  } else if (clientConnected) {
    static uint32_t lastHint = 0;
    if (millis() - lastHint > 10000) {
      Serial.println("Client connected but not subscribed to notifications yet.");
      lastHint = millis();
    }
  }
  delay(2000);
}
