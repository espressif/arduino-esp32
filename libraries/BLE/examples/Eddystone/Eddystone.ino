#include <Arduino.h>
#include <BLE.h>

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Eddystone Example");

  BLE.begin("ESP32-Eddystone");

  // Eddystone URL
  BLEEddystoneURL eddystoneUrl;
  eddystoneUrl.setURL("https://espressif.com");
  eddystoneUrl.setTxPower(-20);

  BLEAdvertising adv = BLE.getAdvertising();
  BLEAdvertisementData advData = eddystoneUrl.getAdvertisementData();
  advData.setFlags(0x06);
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
