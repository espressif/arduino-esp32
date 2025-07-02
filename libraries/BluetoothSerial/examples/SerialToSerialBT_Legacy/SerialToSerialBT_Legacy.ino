// This example code is in the Public Domain (or CC0 licensed, at your option.)
//
// This example creates a bridge between Serial and Classical Bluetooth (SPP with authentication)
// and also demonstrate that SerialBT have the same functionalities of a normal Serial
// Legacy pairing TODO
// Must be run as idf component ... todo

#include "BluetoothSerial.h"

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

const char *deviceName = "ESP32_Legacy_example";

BluetoothSerial SerialBT;
bool confirmRequestDone = false;

void BTAuthCompleteCallback(boolean success) {
  if (success) {
    confirmRequestDone = true;
    Serial.println("Pairing success!!");
  } else {
    Serial.println("Pairing failed, rejected by user!!");
  }
}

void serial_response() {
  if (Serial.available()) {
    SerialBT.write(Serial.read());
  }
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }
  delay(20);
}

void setup() {
  Serial.begin(115200);
  SerialBT.onAuthComplete(BTAuthCompleteCallback);
  SerialBT.begin(deviceName);  // Initiate Bluetooth device with name in parameter
  SerialBT.setPin("1234", 4);
  Serial.printf("The device started with name \"%s\", now you can pair it with Bluetooth!\n", deviceName);
}

void loop() {
  if (confirmRequestDone) {
    serial_response();
  } else {
    delay(1);  // Feed the watchdog
  }
}
