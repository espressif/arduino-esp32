/*
 * BLE Beacon Scanner Example -- New API
 *
 * Scans for BLE beacons and parses three frame types:
 *   - Apple iBeacon  (manufacturer-specific data, company 0x004C)
 *   - Eddystone-URL  (service data, UUID 0xFEAA, frame type 0x10)
 *   - Eddystone-TLM  (service data, UUID 0xFEAA, frame type 0x20)
 *
 * Run alongside the Beacon or Eddystone example on a second board.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

static constexpr uint32_t SCAN_DURATION_MS = 5000;

static void parseIBeacon(const BLEAdvertisedDevice &dev) {
  size_t mfgLen = 0;
  const uint8_t *mfgData = dev.getManufacturerData(&mfgLen);
  if (!mfgData || mfgLen < 25) {
    return;
  }

  uint16_t companyId = mfgData[0] | (mfgData[1] << 8);
  if (companyId != 0x004C) {
    return;
  }

  BLEBeacon beacon;
  beacon.setFromPayload(mfgData, mfgLen);

  Serial.println("  >> iBeacon");
  Serial.printf("     UUID : %s\n", beacon.getProximityUUID().toString().c_str());
  Serial.printf("     Major: %u  Minor: %u\n", beacon.getMajor(), beacon.getMinor());
  Serial.printf("     TX Power: %d dBm\n", beacon.getSignalPower());
}

static void parseEddystoneURL(const BLEAdvertisedDevice &dev) {
  for (size_t i = 0; i < dev.getServiceDataCount(); i++) {
    BLEUUID svcUuid = dev.getServiceDataUUID(i);
    if (svcUuid != BLEEddystoneURL::serviceUUID()) {
      continue;
    }

    size_t len = 0;
    const uint8_t *svcData = dev.getServiceData(i, &len);
    if (!svcData || len < 2 || svcData[0] != 0x10) {
      continue;
    }

    BLEEddystoneURL edUrl;
    edUrl.setFromServiceData(svcData, len);

    Serial.println("  >> Eddystone-URL");
    Serial.printf("     URL: %s\n", edUrl.getURL().c_str());
    Serial.printf("     TX Power: %d dBm\n", edUrl.getTxPower());
  }
}

static void parseEddystoneTLM(const BLEAdvertisedDevice &dev) {
  for (size_t i = 0; i < dev.getServiceDataCount(); i++) {
    BLEUUID svcUuid = dev.getServiceDataUUID(i);
    if (svcUuid != BLEEddystoneTLM::serviceUUID()) {
      continue;
    }

    size_t len = 0;
    const uint8_t *svcData = dev.getServiceData(i, &len);
    if (!svcData || len < 2 || svcData[0] != 0x20) {
      continue;
    }

    BLEEddystoneTLM edTlm;
    edTlm.setFromServiceData(svcData, len);

    Serial.println("  >> Eddystone-TLM");
    Serial.printf("     Battery : %u mV\n", edTlm.getBatteryVoltage());
    Serial.printf("     Temp    : %.2f C\n", edTlm.getTemperature());
    Serial.printf("     Adv Cnt : %lu\n", (unsigned long)edTlm.getAdvertisingCount());
    Serial.printf("     Uptime  : %lu ds\n", (unsigned long)edTlm.getUptime());
  }
}

static void onDeviceFound(BLEAdvertisedDevice dev) {
  Serial.printf("[%s]", dev.getAddress().toString().c_str());
  if (dev.haveName()) {
    Serial.printf(" \"%s\"", dev.getName().c_str());
  }
  Serial.printf(" RSSI:%d\n", dev.getRSSI());

  if (dev.haveManufacturerData()) {
    parseIBeacon(dev);
  }

  BLEAdvertisedDevice::FrameType frame = dev.getFrameType();
  if (frame == BLEAdvertisedDevice::EddystoneURL) {
    parseEddystoneURL(dev);
  } else if (frame == BLEAdvertisedDevice::EddystoneTLM) {
    parseEddystoneTLM(dev);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Beacon Scanner");

  if (!BLE.begin("BeaconScanner")) {
    Serial.println("BLE init failed!");
    return;
  }

  BLEScan scan = BLE.getScan();
  scan.setActiveScan(true);
  scan.setInterval(100);
  scan.setWindow(99);
  scan.setFilterDuplicates(false);
  scan.onResult(onDeviceFound);
}

void loop() {
  Serial.println("--- Scanning ---");
  BLEScanResults results = BLE.getScan().startBlocking(SCAN_DURATION_MS);
  Serial.printf("Scan complete: %d device(s) found\n\n", results.getCount());
  BLE.getScan().clearResults();
  delay(2000);
}
