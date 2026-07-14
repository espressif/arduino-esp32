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

// Short 16-bit UUIDs (custom, for demo purposes only -- use 128-bit UUIDs in production)
static const BLEUUID SVC_UUID("ABCD");
static const BLEUUID CHR_UUID("1234");

void onAdvComplete(uint8_t instance) {
  Serial.printf("Extended advertising instance %d completed\n", instance);
}

void setup() {
  Serial.begin(115200);
  BTStatus status = BLE.begin("BLE5-Server");
  if (!status) {
    Serial.printf("BLE init failed! (%s)\n", status.toString());
    while (true) {
      delay(1000);
    }
  }

  BLEServer server = BLE.createServer();
  BLEService svc = server.createService(SVC_UUID);
  svc.createCharacteristic(CHR_UUID, BLEProperty::Read, BLEPermissions::OpenRead).setValue("Hello BLE5");

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
  data.addServiceUUID(SVC_UUID);
  adv.setExtAdvertisementData(ADV_INSTANCE, data);

  adv.onComplete(onAdvComplete);

  BTStatus advStatus = adv.startExtended(ADV_INSTANCE);
  if (!advStatus) {
    // NotSupported here means this build/SoC lacks BLE 5 extended advertising.
    Serial.printf("Extended advertising did not start (%s)\n", advStatus.toString());
    while (true) {
      delay(1000);
    }
  }
  Serial.println("Extended advertising started on 2M PHY");
}

void loop() {
  delay(1000);
}
