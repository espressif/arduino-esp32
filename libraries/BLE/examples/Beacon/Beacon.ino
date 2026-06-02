/*
 * BLE iBeacon Example -- New API
 *
 * Broadcasts an Apple iBeacon advertisement. iBeacons are used for
 * indoor positioning and proximity detection. Any BLE scanner can
 * detect the beacon and estimate distance from the calibrated TX power.
 *
 * Use the Beacon_Scanner example on another device to receive the data.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

// Proximity UUID: unique identifier for your beacon deployment (generate your own)
static const BLEUUID PROXIMITY_UUID("8ec76ea3-6668-48da-9f9a-e2c2b3a46cae");

void setup() {
  Serial.begin(115200);
  Serial.println("BLE iBeacon Example");

  BTStatus status = BLE.begin("ESP32-iBeacon");
  if (!status) {
    Serial.printf("BLE init failed! (%s)\n", status.toString());
    return;
  }

  BLEBeacon beacon;
  beacon.setManufacturerId(0x004C);  // Apple's company ID (required for iBeacon format)
  beacon.setProximityUUID(PROXIMITY_UUID);
  beacon.setMajor(1);          // Group identifier (e.g., building or floor)
  beacon.setMinor(1);          // Individual beacon within the group
  beacon.setSignalPower(-59);  // Calibrated TX power at 1 meter (dBm), used for distance estimation

  BLEAdvertising adv = BLE.getAdvertising();
  BLEAdvertisementData advData = beacon.getAdvertisementData();
  // GeneralDisc: device is always discoverable; BrEdrNotSupported: BLE-only, no Classic Bluetooth
  advData.setFlags(BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported);
  adv.setAdvertisementData(advData);
  adv.setType(BLEAdvType::NonConnectable);
  adv.start();

  Serial.println("iBeacon advertising started");
  Serial.printf("UUID: %s\n", beacon.getProximityUUID().toString().c_str());
  Serial.printf("Major: %u, Minor: %u\n", beacon.getMajor(), beacon.getMinor());
}

void loop() {
  delay(1000);
}
