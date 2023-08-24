// This example code is in the Public Domain (or CC0 licensed, at your option.)
// Originally by Victor Tchistiak - 2019
// Rewritten with new API by Tomas Pilny - 2023
//
// This example demonstrates reading and removing paired devices stored on the ESP32 flash memory
// Sometimes you may find your ESP32 device could not connect to the remote device despite
// many successful connections earlier. This is most likely a result of client replacing your paired
// device info with new one from other device. The BT clients store connection info for paired devices,
// but it is limited to a few devices only. When new device pairs and number of stored devices is exceeded,
// one of the previously paired devices would be replaced with new one.
// The only remedy is to delete this saved bound device from your device flash memory
// and pair with the other device again.

#include "BluetoothSerial.h"
//#include "esp_bt_device.h"

#if !defined(CONFIG_BT_SPP_ENABLED)
  #error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

#define REMOVE_BONDED_DEVICES true  // <- Set to `false` to view all bonded devices addresses, set to `true` to remove
#define PAIR_MAX_DEVICES 20
BluetoothSerial SerialBT;

char *bda2str(const uint8_t* bda, char *str, size_t size){
  if (bda == NULL || str == NULL || size < 18){
    return NULL;
  }
  sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
          bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
  return str;
}

void setup(){
  char bda_str[18];
  uint8_t pairedDeviceBtAddr[PAIR_MAX_DEVICES][6];
  Serial.begin(115200);

  SerialBT.begin();
  Serial.printf("ESP32 bluetooth address: %s\n", SerialBT.getBtAddressString().c_str());
  // SerialBT.deleteAllBondedDevices(); // If you want just delete all, this is the way
  // Get the numbers of bonded/paired devices in the BT module
  int count = SerialBT.getNumberOfBondedDevices();
  if(!count){
    Serial.println("No bonded devices found.");
  } else {
    Serial.printf("Bonded device count: %d\n", count);
    if(PAIR_MAX_DEVICES < count){
      count = PAIR_MAX_DEVICES;
      Serial.printf("Reset %d bonded devices\n", count);
    }
    count = SerialBT.getBondedDevices(count, pairedDeviceBtAddr);
    char rmt_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    if(count > 0){
      for(int i = 0; i < count; i++){
        SerialBT.requestRemoteName(pairedDeviceBtAddr[i]);
        while(!SerialBT.readRemoteName(rmt_name)){
          delay(1); // Wait for response with the device name
        }
        Serial.printf("Found bonded device #%d BDA:%s; Name:\"%s\"\n", i, bda2str(pairedDeviceBtAddr[i], bda_str, 18), rmt_name);
        SerialBT.invalidateRemoteName(); // Allows waiting for next reading
        if(REMOVE_BONDED_DEVICES){
          if(SerialBT.deleteBondedDevice(pairedDeviceBtAddr[i])){
            Serial.printf("Removed bonded device # %d\n", i);
          } else {
            Serial.printf("Failed to remove bonded device # %d", i);
          } // if(ESP_OK == tError)
        } // if(REMOVE_BONDED_DEVICES)
      } // for(int i = 0; i < count; i++)
    } // if(ESP_OK == tError)
  } // if(!count)
}

void loop() {}
