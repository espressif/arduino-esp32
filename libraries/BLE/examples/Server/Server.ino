/*
 * BLE Server Example -- New API
 *
 * Creates a GATT server with a single service containing a read/write/notify
 * characteristic. Demonstrates connection callbacks, value writes from a
 * client, and periodic notifications.
 *
 * Test with a BLE scanner app (e.g. nRF Connect):
 * 1. Scan for "MyServer"
 * 2. Connect and explore the service
 * 3. Write a value to the characteristic
 * 4. Enable notifications to receive the incrementing counter
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

static const char *SVC_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
static const char *CHR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
BLECharacteristic notifyChr;

void onClientConnect(BLEServer server, const BLEConnInfo &conn) {
  Serial.printf("Client connected: %s (MTU %d)\n", conn.getAddress().toString().c_str(), conn.getMTU());
}

void onClientDisconnect(BLEServer server, const BLEConnInfo &conn, uint8_t reason) {
  Serial.printf("Client disconnected (reason 0x%02X), restarting advertising...\n", reason);
  server.startAdvertising();
  Serial.println("Advertising restarted");
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BLE Server Example ===");

  Serial.print("Initializing BLE... ");
  if (!BLE.begin("MyServer")) {
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

  server.onConnect(onClientConnect);
  server.onDisconnect(onClientDisconnect);

  Serial.print("Creating service (UUID: ");
  Serial.print(SVC_UUID);
  Serial.print(")... ");
  BLEService svc = server.createService(SVC_UUID);
  if (!svc) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");

  Serial.print("Creating characteristic (UUID: ");
  Serial.print(CHR_UUID);
  Serial.print(")... ");
  notifyChr = svc.createCharacteristic(CHR_UUID, BLEProperty::Read | BLEProperty::Write | BLEProperty::Notify, BLEPermissions::OpenReadWrite);
  if (!notifyChr) {
    Serial.println("FAILED!");
    return;
  }
  Serial.println("OK");

  notifyChr.setValue("Hello World");
  notifyChr.setDescription("My Characteristic");
  notifyChr.onWrite([](BLECharacteristic c, const BLEConnInfo &conn) {
    Serial.printf("Written by %s: %s\n", conn.getAddress().toString().c_str(), c.getStringValue().c_str());
  });

  Serial.print("Starting server... ");
  BTStatus status = server.start();
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    return;
  }
  Serial.println("OK");

  Serial.print("Configuring advertising... ");
  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(SVC_UUID);
  Serial.println("OK");

  Serial.print("Starting advertising... ");
  status = adv.start();
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    return;
  }
  Serial.println("OK");

  Serial.println();
  Serial.println("Server is ready! Waiting for connections...");
  Serial.printf("Device name: %s\n", BLE.getDeviceName().c_str());
  Serial.println();
}

void loop() {
  static uint32_t count = 0;
  static bool wasSubscribed = false;

  if (notifyChr && notifyChr.getSubscribedCount() > 0) {
    if (!wasSubscribed) {
      Serial.println("Client subscribed to notifications!");
      Serial.println("Sending notifications every second");
      wasSubscribed = true;
    }
    notifyChr.setValue(count + (uint32_t)'0');
    notifyChr.notify();
    if (count % 10 == 0) {
      Serial.printf("[%lus] Notification #%lu sent\n", millis() / 1000, (unsigned long)count);
    }
    count++;
  } else if (wasSubscribed) {
    Serial.println("No more subscribers.");
    wasSubscribed = false;
  }

  delay(1000);
}
