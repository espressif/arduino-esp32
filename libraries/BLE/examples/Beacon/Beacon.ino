#include <Arduino.h>
#include <BLE.h>

void setup() {
  Serial.begin(115200);
  Serial.println("BLE iBeacon Example");

  BLE.begin("ESP32-iBeacon");

  BLEBeacon beacon;
  beacon.setManufacturerId(0x004C);
  beacon.setProximityUUID(BLEUUID("8ec76ea3-6668-48da-9f9a-e2c2b3a46cae"));
  beacon.setMajor(1);
  beacon.setMinor(1);
  beacon.setSignalPower(-59);

  BLEAdvertising adv = BLE.getAdvertising();
  BLEAdvertisementData advData = beacon.getAdvertisementData();
  advData.setFlags(0x06);
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
