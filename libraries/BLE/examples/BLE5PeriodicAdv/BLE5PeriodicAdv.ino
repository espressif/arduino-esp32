/*
 * BLE5 Periodic Advertising Example -- New API (Advertiser Side)
 *
 * Demonstrates periodic advertising, a BLE 5.0 feature that broadcasts
 * data at fixed intervals without requiring connections. Periodic
 * advertising builds on top of extended advertising.
 *
 * This example:
 * 1. Configures extended advertising (non-connectable) on instance 0
 * 2. Configures periodic advertising on the same instance
 * 3. Updates the periodic payload every second with a counter
 *
 * Requires an ESP32 chip with BLE 5.0 support (ESP32-C3, ESP32-S3,
 * ESP32-C6, ESP32-H2, etc.) -- not available on original ESP32.
 *
 * Use BLE5Scan example on another device to receive the periodic data.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

void setup() {
  Serial.begin(115200);
  if (!BLE.begin("Periodic-ADV")) {
    Serial.println("BLE init failed!");
    return;
  }

  BLEAdvertising adv = BLE.getAdvertising();

  BLEAdvertising::ExtAdvConfig extConfig;
  extConfig.instance = 0;
  extConfig.type = BLEAdvType::NonConnectable;
  extConfig.primaryPhy = BLEPhy::PHY_1M;
  extConfig.secondaryPhy = BLEPhy::PHY_1M;
  extConfig.sid = 1;
  adv.configureExtended(extConfig);

  BLEAdvertisementData extData;
  extData.setName("ESP32-Periodic");
  adv.setExtAdvertisementData(0, extData);

  BLEAdvertising::PeriodicAdvConfig periodicConfig;
  periodicConfig.instance = 0;
  periodicConfig.intervalMin = 24;  // 30ms (24 * 1.25ms)
  periodicConfig.intervalMax = 24;
  adv.configurePeriodicAdv(periodicConfig);

  adv.startExtended(0);
  adv.startPeriodicAdv(0);
  Serial.println("Periodic advertising started");
}

uint32_t counter = 0;
void loop() {
  BLEAdvertisementData data;
  String payload = "Count:" + String(counter++);
  data.addRaw((const uint8_t *)payload.c_str(), payload.length());
  BLE.getAdvertising().setPeriodicAdvData(0, data);
  delay(1000);
}
