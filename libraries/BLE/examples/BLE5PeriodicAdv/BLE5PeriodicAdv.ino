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

// BLE 5 supports multiple simultaneous advertising sets. Each set is
// identified by an instance index (0, 1, 2, ...). This example uses
// a single set; change the value to run several sets in parallel.
static const uint8_t ADV_INSTANCE = 0;

void setup() {
  Serial.begin(115200);
  BTStatus status = BLE.begin("Periodic-ADV");
  if (!status) {
    Serial.printf("BLE init failed! (%s)\n", status.toString());
    return;
  }

  BLEAdvertising adv = BLE.getAdvertising();

  // Extended advertising (non-connectable is required for periodic)
  adv.setExtType(ADV_INSTANCE, BLEAdvType::NonConnectable);
  adv.setExtPhy(ADV_INSTANCE, BLEPhy::PHY_1M, BLEPhy::PHY_1M);
  adv.setExtSID(ADV_INSTANCE, 1);  // Scanner uses SID to sync with periodic advertising

  BLEAdvertisementData extData;
  extData.setName("ESP32-Periodic");
  adv.setExtAdvertisementData(ADV_INSTANCE, extData);

  // Periodic advertising: broadcasts data at fixed intervals without connections.
  // Interval is in 1.25 ms units: 24 * 1.25 ms = 30 ms
  adv.setPeriodicAdvInterval(ADV_INSTANCE, 24, 24);

  adv.startExtended(ADV_INSTANCE);
  adv.startPeriodicAdv(ADV_INSTANCE);
  Serial.println("Periodic advertising started");
}

uint32_t counter = 0;
void loop() {
  BLEAdvertisementData data;
  String payload = "Count:" + String(counter++);
  data.addRaw((const uint8_t *)payload.c_str(), payload.length());
  BLE.getAdvertising().setPeriodicAdvData(ADV_INSTANCE, data);
  delay(1000);
}
