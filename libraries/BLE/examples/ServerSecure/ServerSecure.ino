/*
 * Secure BLE Server Example -- New API
 *
 * Creates a GATT server with two characteristics:
 * - A secure characteristic requiring authentication (passkey) for read/write
 * - An open characteristic accessible without pairing
 *
 * Demonstrates BLESecurity configuration: static passkey, IO capability,
 * authentication mode, and the onAuthenticationComplete callback.
 *
 * Test with nRF Connect:
 * 1. Scan for "Secure Server"
 * 2. Connect -- the secure characteristic triggers pairing
 * 3. Enter passkey 123456 when prompted
 * 4. After auth, read the secure characteristic
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

void onAuthDone(const BLEConnInfo &conn, bool success) {
  Serial.printf("Auth %s with %s\n", success ? "OK" : "FAILED", conn.getAddress().toString().c_str());
  if (success) {
    Serial.printf("Local IRK (Base64): %s\n", BLE.getLocalIRKBase64().c_str());
    uint8_t irk[16];
    if (BLE.getPeerIRK(conn.getAddress(), irk)) {
      Serial.printf("Peer IRK: %s\n", BLE.getPeerIRKString(conn.getAddress()).c_str());
    }
  }
}

void setup() {
  Serial.begin(115200);
  if (!BLE.begin("Secure Server")) {
    Serial.println("BLE init failed!");
    return;
  }

  BLESecurity sec = BLE.getSecurity();
  sec.setStaticPassKey(123456);
  sec.setIOCapability(BLESecurity::DisplayOnly);
  sec.setAuthenticationMode(true, true, true);
  sec.onAuthenticationComplete(onAuthDone);
  sec.onPassKeyDisplay([](const BLEConnInfo &conn, uint32_t passKey) {
    Serial.printf("Passkey for %s: %06lu\n", conn.getAddress().toString().c_str(), passKey);
  });

  BLEServer server = BLE.createServer();
  BLEService svc = server.createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");

  BLECharacteristic secureChar =
    svc.createCharacteristic("ff1d2614-e2d6-4c87-9154-6625d39ca7f8", BLEProperty::Read | BLEProperty::Write, BLEPermissions::AuthenticatedReadWrite);
  secureChar.setValue("Secret Data");

  BLECharacteristic openChar =
    svc.createCharacteristic("beb5483e-36e1-4688-b7f5-ea07361b26a8", BLEProperty::Read | BLEProperty::Write, BLEPermissions::OpenReadWrite);
  openChar.setValue("Public Data");

  server.start();
  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
  adv.start();
}

void loop() {
  delay(1000);
}
