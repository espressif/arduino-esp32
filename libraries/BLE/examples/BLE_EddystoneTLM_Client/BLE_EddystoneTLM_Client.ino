// Use this with example BLE_EddystoneTLM_Beacon (flash on second ESP)
// Eddystone TLM Beacon client never actually connects - it only listens to the advertisement

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>

BLEUUID SERVICE_UUID("FEAA");

BLEScan* pBLEScan;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.println();
        Serial.print("Advertised Device: ");
        Serial.println(advertisedDevice.toString().c_str());

        Serial.printf("advertisedDevice.haveServiceUUID() = %d; advertisedDevice.getServiceUUID()=%s, SERVICE_UUID=%s)\n", advertisedDevice.haveServiceUUID(), advertisedDevice.getServiceUUID().toString().c_str(), SERVICE_UUID.toString().c_str());
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(SERVICE_UUID)) {
            uint8_t *payload = advertisedDevice.getPayload();

            size_t len =advertisedDevice.getPayloadLength();
            for(int i =0; i < len; ++i){
                Serial.printf("payload[%d]=%x%s\n", i, payload[i], i==26 || i==27 ? " < temp" : (i==22 ? " < expecting 0x20 for TLM" : ""));
            }
            Serial.printf("Temp: payload[26]=%x; payload[27]=%x\n", payload[26], payload[27]);
            float temp = payload[26]+payload[27]/259.0;
            Serial.printf("Float Temp: %f\n", temp);
        }else{
          //Serial.printf("advertised Device does not have UUID, or it is not equal to expected 0xFEAA\n");
        }
        //BLEDevice::getScan()->stop();
        //Serial.printf("Scanning stopped\n");
    }
};

void setup() {
    Serial.begin(115200);
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
}

void loop() {
    pBLEScan->start(-1);
    BLEDevice::getScan()->start(0);
}
