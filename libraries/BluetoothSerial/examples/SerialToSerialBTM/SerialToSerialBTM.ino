// This example code is in the Public Domain (or CC0 licensed, at your option.)
// By Victor Tchistiak - 2019
//
// This example demonstrates master mode Bluetooth connection to a slave BT device
// defined either by String "slaveName" by default "ESP32-BT-Slave" or by MAC address
//
// This example creates a bridge between Serial and Classical Bluetooth (SPP)
// This is an extension of the SerialToSerialBT example by Evandro Copercini - 2018
//
// DO NOT try to connect to phone or laptop - they are master
// devices, same as the ESP using this code - you will be able
// to pair, but the serial communication will NOT work!
//
// You can try to flash a second ESP32 with the example SerialToSerialBT - it should
// automatically pair with ESP32 running this code
// Note: Pairing is authenticated automatically by this device

#include "BluetoothSerial.h"

#define USE_NAME // Comment this to use MAC address instead of a slaveName

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
  #error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
  #error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif
BluetoothSerial SerialBT;

#ifdef USE_NAME
  String slaveName = "ESP32-BT-Slave"; // Change this to reflect the real name of your slave BT device
#else
  String MACadd = "AA:BB:CC:11:22:33"; // This only for printing
  uint8_t address[6]  = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33}; // Change this to reflect real MAC address of your slave BT device
#endif

String myName = "ESP32-BT-Master";

void setup() {
  bool connected;
  Serial.begin(115200);

  SerialBT.begin(myName, true);
  //SerialBT.deleteAllBondedDevices(); // Uncomment this to delete paired devices; Must be called after begin
  Serial.printf("The device \"%s\" started in master mode, make sure slave BT device is on!\n", myName.c_str());

  #ifndef USE_NAME
    SerialBT.setPin(pin);
    Serial.println("Using PIN");
  #endif

  // connect(address) is fast (up to 10 secs max), connect(slaveName) is slow (up to 30 secs max) as it needs
  // to resolve slaveName to address first, but it allows to connect to different devices with the same name.
  // Set CoreDebugLevel to Info to view devices Bluetooth address and device names
  #ifdef USE_NAME
    connected = SerialBT.connect(slaveName);
    Serial.printf("Connecting to slave BT device named \"%s\"\n", slaveName.c_str());
  #else
    connected = SerialBT.connect(address);
    Serial.print("Connecting to slave BT device with MAC "); Serial.println(MACadd);
  #endif

  if(connected) {
    Serial.println("Connected Successfully!");
  } else {
    while(!SerialBT.connected(10000)) {
      Serial.println("Failed to connect. Make sure remote device is available and in range, then restart app.");
    }
  }
  // Disconnect() may take up to 10 secs max
  if (SerialBT.disconnect()) {
    Serial.println("Disconnected Successfully!");
  }
  // This would reconnect to the slaveName(will use address, if resolved) or address used with connect(slaveName/address).
  SerialBT.connect();
  if(connected) {
    Serial.println("Reconnected Successfully!");
  } else {
    while(!SerialBT.connected(10000)) {
      Serial.println("Failed to reconnect. Make sure remote device is available and in range, then restart app.");
    }
  }
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
