/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
   Changed to a beacon scanner to report iBeacon, EddystoneURL and EddystoneTLM beacons by beegee-tokyo
*/

#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEEddystoneURL.h>
#include <BLEEddystoneTLM.h>
#include <BLEBeacon.h>

int scanTime = 5; //In seconds
BLEScan *pBLEScan;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
      if (advertisedDevice.haveName())
      {
        Serial.print("Device name: ");
        Serial.println(advertisedDevice.getName().c_str());
        Serial.println("");
      }

      if (advertisedDevice.haveServiceUUID())
      {
        BLEUUID devUUID = advertisedDevice.getServiceUUID();
        Serial.print("Found ServiceUUID: ");
        Serial.println(devUUID.toString().c_str());
        Serial.println("");
      }
      
      if (advertisedDevice.haveManufacturerData() == true)
      {
        std::string strManufacturerData = advertisedDevice.getManufacturerData();

        uint8_t cManufacturerData[100];
        strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);

        if (strManufacturerData.length() == 25 && cManufacturerData[0] == 0x4C && cManufacturerData[1] == 0x00)
        {
          Serial.println("Found an iBeacon!");
          BLEBeacon oBeacon = BLEBeacon();
          oBeacon.setData(strManufacturerData);
          Serial.printf("iBeacon Frame\n");
          Serial.printf("ID: %04X Major: %d Minor: %d UUID: %s Power: %d\n", oBeacon.getManufacturerId(), ENDIAN_CHANGE_U16(oBeacon.getMajor()), ENDIAN_CHANGE_U16(oBeacon.getMinor()), oBeacon.getProximityUUID().toString().c_str(), oBeacon.getSignalPower());
        }
        else
        {
          Serial.println("Found another manufacturers beacon!");
          Serial.printf("strManufacturerData: %d ", strManufacturerData.length());
          for (int i = 0; i < strManufacturerData.length(); i++)
          {
            Serial.printf("[%X]", cManufacturerData[i]);
          }
          Serial.printf("\n");
        }
      }

      uint8_t *payLoad = advertisedDevice.getPayload();
      // search for Eddystone Service Data in the advertising payload
      // *payload shall point to eddystone data or to its end when not found
      const uint8_t serviceDataEddystone[3] = {0x16, 0xAA, 0xFE}; // it has Eddystone BLE UUID
      const size_t payLoadLen = advertisedDevice.getPayloadLength();
      uint8_t *payLoadEnd = payLoad + payLoadLen - 1; // address of the end of payLoad space
      while (payLoad < payLoadEnd) {
        if (payLoad[1] == serviceDataEddystone[0] && payLoad[2] == serviceDataEddystone[1] && payLoad[3] == serviceDataEddystone[2]) {
          // found!
          payLoad += 4;
          break;
        }
        payLoad += *payLoad + 1;  // payLoad[0] has the field Length
      }

      if (payLoad < payLoadEnd) // Eddystone Service Data and respective BLE UUID were found
      {
        if (*payLoad == 0x10)
        {
          Serial.println("Found an EddystoneURL beacon!");
          BLEEddystoneURL foundEddyURL = BLEEddystoneURL();
          uint8_t URLLen = *(payLoad - 4) - 3;  // Get Field Length less 3 bytes (type and UUID) 
          foundEddyURL.setData(std::string((char*)payLoad, URLLen));
          std::string bareURL = foundEddyURL.getURL();
          if (bareURL[0] == 0x00)
          {
            // dumps all bytes in advertising payload
            Serial.println("DATA-->");
            uint8_t *payLoad = advertisedDevice.getPayload();
            for (int idx = 0; idx < payLoadLen; idx++)
            {
              Serial.printf("0x%02X ", payLoad[idx]);
            }
            Serial.println("\nInvalid Data");
            return;
          }

          Serial.printf("Found URL: %s\n", foundEddyURL.getURL().c_str());
          Serial.printf("Decoded URL: %s\n", foundEddyURL.getDecodedURL().c_str());
          Serial.printf("TX power %d\n", foundEddyURL.getPower());
          Serial.println("\n");
        } 
        else if (*payLoad == 0x20)
        {
          Serial.println("Found an EddystoneTLM beacon!");
 
          BLEEddystoneTLM eddystoneTLM;
          eddystoneTLM.setData(std::string((char*)payLoad, 14));
          Serial.printf("Reported battery voltage: %dmV\n", eddystoneTLM.getVolt());
          Serial.printf("Reported temperature: %.2f°C (raw data=0x%04X)\n", eddystoneTLM.getTemp(), eddystoneTLM.getRawTemp());
          Serial.printf("Reported advertise count: %d\n", eddystoneTLM.getCount());
          Serial.printf("Reported time since last reboot: %ds\n", eddystoneTLM.getTime());
          Serial.println("\n");
          Serial.print(eddystoneTLM.toString().c_str());
          Serial.println("\n");
        }
      }
    }
};

void setup()
{
  Serial.begin(115200);
  Serial.println("Scanning...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99); // less or equal setInterval value
}

void loop()
{
  // put your main code here, to run repeatedly:
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  Serial.print("Devices found: ");
  Serial.println(foundDevices.getCount());
  Serial.println("Scan done!");
  pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
  delay(2000);
}
