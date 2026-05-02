/*
 * BluetoothSerial Master Connect
 *
 * Demonstrates connecting as an SPP master/initiator to a remote device
 * by name or by MAC address. Once connected, data is bridged with USB Serial.
 *
 * To connect by MAC instead of name, uncomment USE_ADDRESS and set the target.
 */

#include <Arduino.h>
#include "BluetoothSerial.h"
#include "BTAddress.h"

BluetoothSerial SerialBT;

// #define USE_ADDRESS

#ifdef USE_ADDRESS
BTAddress targetAddr("AA:BB:CC:11:22:33");
#else
const char *targetName = "ESP32-BT-Bridge";
#endif

void setup() {
  Serial.begin(115200);

  BTStatus status = SerialBT.begin("ESP32-Master", true);
  if (!status) {
    Serial.println("Bluetooth init failed!");
    while (true) {
      delay(1000);
    }
  }

#ifdef USE_ADDRESS
  Serial.printf("Connecting to %s...\n", targetAddr.toString().c_str());
  status = SerialBT.connect(targetAddr);
#else
  Serial.printf("Connecting to \"%s\"...\n", targetName);
  status = SerialBT.connect(String(targetName), 15000);
#endif

  if (status) {
    Serial.println("Connected!");
  } else {
    Serial.println("Connection failed.");
  }
}

void loop() {
  if (SerialBT.connected()) {
    while (Serial.available()) {
      SerialBT.write(Serial.read());
    }
    while (SerialBT.available()) {
      Serial.write(SerialBT.read());
    }
  } else {
    Serial.println("Disconnected. Retrying in 5s...");
    delay(5000);
#ifdef USE_ADDRESS
    SerialBT.connect(targetAddr);
#else
    SerialBT.connect(String(targetName), 15000);
#endif
  }

  delay(1);
}
