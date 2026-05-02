/*
 * BLE5 Extended Advertising Example -- New API
 *
 * Creates a GATT server and starts extended advertising on the 2M PHY.
 * Extended advertising supports larger payloads and secondary PHYs
 * introduced in Bluetooth 5.0.
 *
 * Requires an ESP32 chip with BLE 5.0 support (ESP32-C3, ESP32-S3,
 * ESP32-C6, ESP32-H2, etc.) -- not available on original ESP32.
 *
 * Test with nRF Connect (BLE5-capable phone):
 * 1. Enable "Extended" in the scanner settings
 * 2. Look for "ESP32-BLE5-Extended"
 * 3. Connect and explore the service
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

void onAdvComplete(uint8_t instance) {
  Serial.printf("Extended advertising instance %d completed\n", instance);
}

void setup() {
  Serial.begin(115200);
  if (!BLE.begin("BLE5-Server")) {
    Serial.println("BLE init failed!");
    return;
  }

  BLEServer server = BLE.createServer();
  BLEService svc = server.createService("ABCD");
  svc.createCharacteristic("1234", BLEProperty::Read, BLEPermissions::OpenRead).setValue("Hello BLE5");

  server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    Serial.printf("BLE5 client connected: %s\n", conn.getAddress().toString().c_str());
  });

  server.start();

  BLEAdvertising adv = BLE.getAdvertising();

  BLEAdvertising::ExtAdvConfig config;
  config.instance = 0;
  config.type = BLEAdvType::ConnectableScannable;
  config.primaryPhy = BLEPhy::PHY_1M;
  config.secondaryPhy = BLEPhy::PHY_2M;
  config.sid = 1;
  adv.configureExtended(config);

  BLEAdvertisementData data;
  data.setName("ESP32-BLE5-Extended");
  data.addServiceUUID(BLEUUID("ABCD"));
  adv.setExtAdvertisementData(0, data);

  adv.onComplete(onAdvComplete);

  adv.startExtended(0);
  Serial.println("Extended advertising started on 2M PHY");
}

void loop() {
  delay(1000);
}
