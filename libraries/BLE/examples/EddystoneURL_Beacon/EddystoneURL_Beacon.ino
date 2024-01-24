/*
   EddystoneURL beacon by BeeGee
   EddystoneURL frame specification https://github.com/google/eddystone/blob/master/eddystone-url/README.md

   Upgraded on: Feb 20, 2023
       By: Tomas Pilny
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

*/
#include "sys/time.h"

#include <Arduino.h>

#include "BLEDevice.h"
#include "BLEUtils.h"
#include "BLEBeacon.h"
#include "BLEAdvertising.h"
#include "BLEEddystoneURL.h"
#include "esp_sleep.h"

char unprintable[] = {0x01, 0xFF, 0xDE, 0xAD};
String URL[] = {
  "http://www.espressif.com/", // prefix 0x00, suffix 0x00
  "https://www.texas.gov",     // prefix 0x01, suffix 0x0D
  "http://en.mapy.cz",         // prefix 0x02, no valid suffix
  "https://arduino.cc",        // prefix 0x03, no valid suffix
  "google.com",                // URL without specified prefix - the function will assume default prefix "http://www." = 0x00
  "diginfo.tv",                // URL without specified prefix - the function will assume default prefix "http://www." = 0x00
//  "http://www.URLsAbove17BytesAreNotAllowed.com", // Too long URL - setSmartURL() will return 0 = ERR
//  "",  // Empty string - setSmartURL() will return 0 = ERR
//  String(unprintable), // Unprintable characters / corrupted String - setSmartURL() will return 0 = ERR
};

#define GPIO_DEEP_SLEEP_DURATION 10      // sleep x seconds and then wake up
#define BEACON_POWER ESP_PWR_LVL_N12
RTC_DATA_ATTR static time_t last;        // remember last boot in RTC Memory
RTC_DATA_ATTR static uint32_t bootcount; // remember number of boots in RTC Memory

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
BLEAdvertising *pAdvertising;
struct timeval now;

int setBeacon()
{
  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  BLEAdvertisementData oScanResponseData = BLEAdvertisementData();

  BLEEddystoneURL EddystoneURL;
  EddystoneURL.setPower(BEACON_POWER); // This is only information about the power. The actual power is set by `BLEDevice::setPower(BEACON_POWER)`
  if(EddystoneURL.setSmartURL(URL[bootcount%(sizeof(URL)/sizeof(URL[0]))])){
    String frame = EddystoneURL.getFrame();
    String data(EddystoneURL.getFrame().c_str(), frame.length());
    oAdvertisementData.addData(data);
    oScanResponseData.setName("ESP32 URLBeacon");
    pAdvertising->setAdvertisementData(oAdvertisementData);
    pAdvertising->setScanResponseData(oScanResponseData);
    Serial.printf("Advertise URL \"%s\"\n", URL[bootcount%(sizeof(URL)/sizeof(URL[0]))].c_str());
    return 1; // OK
  }else{
    Serial.println("Smart URL set ERR");
    return 0; // ERR
  }
}

void setup()
{
  Serial.begin(115200);
  gettimeofday(&now, NULL);

  Serial.printf("Start ESP32 %lu\n", bootcount++);
  Serial.printf("Deep sleep (%llds since last reset, %llds since last boot)\n", now.tv_sec, now.tv_sec - last);

  last = now.tv_sec;

  // Create the BLE Device
  BLEDevice::init("URLBeacon");
  BLEDevice::setPower(BEACON_POWER);

  // Create the BLE Server
  // BLEServer *pServer = BLEDevice::createServer(); // <-- no longer required to instantiate BLEServer, less flash and ram usage

  pAdvertising = BLEDevice::getAdvertising();

  if(setBeacon()){
    // Start advertising
    pAdvertising->start();
    Serial.println("Advertising started...");
    delay(10000);
    pAdvertising->stop();
  }
  Serial.println("Enter deep sleep");
  bootcount++;
  esp_deep_sleep(1000000LL * GPIO_DEEP_SLEEP_DURATION);
}

void loop()
{
}
