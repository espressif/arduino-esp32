// This example demonstrates usage of BluetoothSerial method to retrieve MAC address of local BT device in various formats.
// By Tomas Pilny - 2023

#include "BluetoothSerial.h"

String device_name = "ESP32-example";

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
  #error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
  #error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  SerialBT.begin(device_name); //Bluetooth device name

  uint8_t mac_arr[6]; // Byte array to hold the MAC address from getBtAddress()
  BTAddress mac_obj; // Object holding instance of BTAddress with the MAC (for more details see libraries/BluetoothSerial/src/BTAddress.h)
  String mac_str; // String holding the text version of MAC in format AA:BB:CC:DD:EE:FF

  SerialBT.getBtAddress(mac_arr); // Fill in the array
  mac_obj = SerialBT.getBtAddressObject(); // Instantiate the object
  mac_str = SerialBT.getBtAddressString(); // Copy the string

  Serial.print("This device is instantiated with name "); Serial.println(device_name);

  Serial.print("The mac address using byte array: ");
  for(int i = 0; i < ESP_BD_ADDR_LEN-1; i++){
    Serial.print(mac_arr[i], HEX); Serial.print(":");
  }
  Serial.println(mac_arr[ESP_BD_ADDR_LEN-1], HEX);

  Serial.print("The mac address using BTAddress object using default method `toString()`: "); Serial.println(mac_obj.toString().c_str());
  Serial.print("The mac address using BTAddress object using method `toString(true)`\n\twhich prints the MAC with capital letters: "); Serial.println(mac_obj.toString(true).c_str()); // This actually what is used inside the getBtAddressString()

  Serial.print("The mac address using string: "); Serial.println(mac_str.c_str());
}

void loop(){

}
