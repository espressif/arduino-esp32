//This example code is in the Public Domain (or CC0 licensed, at your option.)
//By Evandro Copercini - 2018
//
//This example creates a bridge between Serial and Classical Bluetooth (SPP)
//and also demonstrate that SerialBT have the same functionalities of a normal Serial

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

String MACadd = "AA:BB:CC:11:22:33";
uint8_t address[6]  = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};
//uint8_t address[6]  = {0x00, 0x1D, 0xA5, 0x02, 0xC3, 0x22};
String name = "OBDII";
char *pin = "1234"; //<- standard pin would be provided by default

void setup() {
  Serial.begin(115200);
  //SerialBT.setPin(pin);
  SerialBT.begin("ESP32test", true); 
  //SerialBT.setPin(pin);
  Serial.println("The device started in master mode, make sure remote BT device is on!");
  
  // connect(name) is slow as it needs to resolve name to address first, 
  // but allows to connect to different devices with the same name
  // Enable CoreDebugLevel to Info to view devices bluetooth address or Verbose to see device names
  //SerialBT.connect(name); 
  SerialBT.connect(address); 
  
  // wait at least 30 secs after connect(name) or 5 secs after connect(address)
  if(SerialBT.connected(30*1000)) { 
    Serial.println("Connected Succesfully!");
  } else {
    while(!SerialBT.connected()) {
      Serial.println("Not connected yet?");
      delay(10000);
    }
  }
  // wait for a few second for disconnect to complete
  if (SerialBT.disconnect(2000)) {
    Serial.println("Disconnected Succesfully!");
  } else {
    Serial.println("Not disconnected yet?");
  }
  // calling another connect(...) while other connect(...) still in progress may cause crash,
  // use connected(timeout) with recommended timeout before issuing another connect(...)
  SerialBT.connect(); 
}

void loop() {
  if (Serial.available()) {
    SerialBT.write(Serial.read());
  }
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }
  delay(20);
}
