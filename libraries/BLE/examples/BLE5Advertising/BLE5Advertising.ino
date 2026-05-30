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

// BLE 5 supports multiple simultaneous advertising sets. Each set is
// identified by an instance index (0, 1, 2, ...). This example uses
// a single set; change the value to run several sets in parallel.
static const uint8_t ADV_INSTANCE = 0;

void onAdvComplete(uint8_t instance) {
  Serial.printf("Extended advertising instance %d completed\n", instance);
}

void setup() {
  Serial.begin(115200);
  BTStatus status = BLE.begin("BLE5-Server");
  if (!status) {
    Serial.printf("BLE init failed! (%s)\n", status.toString());
    return;
  }

  BLEServer server = BLE.createServer();
  // Short 16-bit UUIDs (custom, for demo purposes only -- use 128-bit UUIDs in production)
  BLEService svc = server.createService("ABCD");
  svc.createCharacteristic("1234", BLEProperty::Read, BLEPermissions::OpenRead).setValue("Hello BLE5");

  server.onConnect([](BLEServer s, const BLEConnInfo &conn) {
    Serial.printf("BLE5 client connected: %s\n", conn.getAddress().toString().c_str());
  });

  server.start();

  BLEAdvertising adv = BLE.getAdvertising();

  // Extended advertising configuration
  adv.setExtType(ADV_INSTANCE, BLEAdvType::ConnectableScannable);  // Allow connections and scan requests
  adv.setExtPhy(ADV_INSTANCE, BLEPhy::PHY_1M, BLEPhy::PHY_2M);    // 1M primary, 2M secondary (faster)
  adv.setExtSID(ADV_INSTANCE, 1);                                  // Advertising Set Identifier

  BLEAdvertisementData data;
  data.setName("ESP32-BLE5-Extended");
  data.addServiceUUID(BLEUUID("ABCD"));
  adv.setExtAdvertisementData(ADV_INSTANCE, data);

  adv.onComplete(onAdvComplete);

  adv.startExtended(ADV_INSTANCE);
  Serial.println("Extended advertising started on 2M PHY");
}

void loop() {
  delay(1000);
}
