/*
   BLE5 extended scan example for esp32 C3 and S3
   with this code it is simple to scan legacy (BLE4) compatible advertising,
   and BLE5 extended advertising. New coded added in BLEScan is not changing old behavior,
   which can be used with old esp32, but is adding functionality to use on C3/S3.
   With this new API advertised device wont be stored in API, it is now user responsibility

   author: chegewara
*/
#ifndef CONFIG_BT_BLE_50_FEATURES_SUPPORTED
#warning "Not compatible hardware"
#else
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

uint32_t scanTime = 100; //In 10ms (1000ms)
BLEScan* pBLEScan;

class MyBLEExtAdvertisingCallbacks: public BLEExtAdvertisingCallbacks {
    void onResult(esp_ble_gap_ext_adv_reprot_t report) {
      if(report.event_type & ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY){
        // here we can receive regular advertising data from BLE4.x devices
        Serial.println("BLE4.2");
      } else {
        // here we will get extended advertising data that are advertised over data channel by BLE5 divices
        Serial.printf("Ext advertise: data_le: %d, data_status: %d \n", report.adv_data_len, report.data_status);
      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Scanning...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setExtendedScanCallback(new MyBLEExtAdvertisingCallbacks());
  pBLEScan->setExtScanParams(); // use with pre-defined/default values, overloaded function allows to pass parameters
  delay(1000); // it is just for simplicity this example, to let ble stack to set extended scan params
  pBLEScan->startExtScan(scanTime, 3); // scan duration in n * 10ms, period - repeat after n seconds (period >= duration)
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000);
}
#endif // CONFIG_BT_BLE_50_FEATURES_SUPPORTED
