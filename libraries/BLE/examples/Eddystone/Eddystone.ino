/*
 * BLE Eddystone Example -- New API
 *
 * Broadcasts an Eddystone-URL beacon and demonstrates Eddystone-TLM
 * (telemetry) data construction. Eddystone is Google's open beacon
 * format -- any BLE scanner can read the URL without an app.
 *
 * Use the Beacon_Scanner example on another device to receive the data.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Eddystone Example");

  BTStatus status = BLE.begin("ESP32-Eddystone");
  if (!status) {
    Serial.printf("BLE init failed! (%s)\n", status.toString());
    return;
  }

  // Eddystone URL
  BLEEddystoneURL eddystoneUrl;
  eddystoneUrl.setURL("https://espressif.com");
  eddystoneUrl.setTxPower(-20);

  BLEAdvertising adv = BLE.getAdvertising();
  BLEAdvertisementData advData = eddystoneUrl.getAdvertisementData();
  // GeneralDisc: device is always discoverable; BrEdrNotSupported: BLE-only, no Classic Bluetooth
  advData.setFlags(BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported);
  advData.setCompleteServices(BLEEddystoneURL::serviceUUID());
  adv.setAdvertisementData(advData);
  adv.setType(BLEAdvType::NonConnectable);
  adv.start();

  Serial.printf("Eddystone URL: %s\n", eddystoneUrl.getURL().c_str());
  Serial.println("Advertising started");

  // Also demonstrate TLM parsing
  BLEEddystoneTLM tlm;
  tlm.setBatteryVoltage(3300);
  tlm.setTemperature(25.5f);
  tlm.setAdvertisingCount(12345);
  tlm.setUptime(100000);
  Serial.println(tlm.toString().c_str());
}

void loop() {
  delay(1000);
}
