/*
   EddystoneTLM beacon by BeeGee based on https://github.com/pcbreflux/espressif/blob/master/esp32/arduino/sketchbook/ESP32_Eddystone_TLM_deepsleep/ESP32_Eddystone_TLM_deepsleep.ino
   EddystoneTLM frame specification https://github.com/google/eddystone/blob/master/eddystone-tlm/tlm-plain.md
*/

/*
   Create a BLE server that will send periodic Eddystone URL frames.
   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create advertising data
   3. Start advertising.
   4. wait
   5. Stop advertising.
   6. deep sleep
   
   To read data advertised by this beacon use second ESP with example sketch BLE_Beacon_Scanner
*/
#include "sys/time.h"

#include <Arduino.h>

#include "BLEDevice.h"
#include "BLEUtils.h"
#include "BLEBeacon.h"
#include "BLEAdvertising.h"
#include "BLEEddystoneTLM.h"

#include "esp_sleep.h"

#define GPIO_DEEP_SLEEP_DURATION 10     // sleep x seconds and then wake up
#define BEACON_POWER ESP_PWR_LVL_N12
RTC_DATA_ATTR static time_t last;    // remember last boot in RTC Memory
RTC_DATA_ATTR static uint32_t bootcount; // remember number of boots in RTC Memory

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
BLEAdvertising *pAdvertising;
struct timeval nowTimeStruct;

time_t lastTenth;

#define BEACON_UUID "8ec76ea3-6668-48da-9866-75be8bc86f4d" // UUID 1 128-Bit (may use linux tool uuidgen or random numbers via https://www.uuidgenerator.net/)

// Check
// https://github.com/google/eddystone/blob/master/eddystone-tlm/tlm-plain.md
// and http://www.hugi.scene.org/online/coding/hugi%2015%20-%20cmtadfix.htm
// for the temperature value. It is a 8.8 fixed-point notation
void setBeacon()
{
  BLEEddystoneTLM EddystoneTLM;
  EddystoneTLM.setVolt((uint16_t)random(2800, 3700)); // 3300mV = 3.3V
  EddystoneTLM.setTemp(random(-3000, 3000) / 100.0f); // 3000 = 30.00 ˚C
  Serial.printf("Random Battery voltage is %d mV = 0x%04X\n", EddystoneTLM.getVolt(), EddystoneTLM.getVolt());
  Serial.printf("Random temperature is %.2f°C\n", EddystoneTLM.getTemp());
  Serial.printf("Converted to 8.8 format: 0x%04X\n", EddystoneTLM.getRawTemp());

  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  BLEAdvertisementData oScanResponseData = BLEAdvertisementData();
  oScanResponseData.setServiceData(BLEUUID((uint16_t)0xFEAA), String(EddystoneTLM.getData().c_str(), EddystoneTLM.getData().length()));

  oAdvertisementData.setName("ESP32 TLM Beacon");
  pAdvertising->setAdvertisementData(oAdvertisementData);
  pAdvertising->setScanResponseData(oScanResponseData);
}

void setup()
{
  Serial.begin(115200);
  gettimeofday(&nowTimeStruct, NULL);

  Serial.printf("Starting ESP32. Bootcount = %lu\n", bootcount++);
  Serial.printf("Deep sleep (%llds since last reset, %llds since last boot)\n", nowTimeStruct.tv_sec, nowTimeStruct.tv_sec - last);

  last = nowTimeStruct.tv_sec;
  lastTenth = nowTimeStruct.tv_sec * 10; // Time since last reset as 0.1 second resolution counter

  // Create the BLE Device
  BLEDevice::init("TLMBeacon");

  BLEDevice::setPower(BEACON_POWER);

  pAdvertising = BLEDevice::getAdvertising();

  setBeacon();
  // Start advertising
  pAdvertising->start();
  Serial.println("Advertising started for 10s ...");
  delay(10000);
  pAdvertising->stop();
  Serial.printf("Enter deep sleep for 10s\n");
  esp_deep_sleep(1000000LL * GPIO_DEEP_SLEEP_DURATION);
}

void loop()
{
}
