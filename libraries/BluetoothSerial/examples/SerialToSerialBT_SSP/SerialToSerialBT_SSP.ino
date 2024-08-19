// This example code is in the Public Domain (or CC0 licensed, at your option.)
// By Richard Li - 2020
//
// This example creates a bridge between Serial and Classical Bluetooth (SPP with authentication)
// and also demonstrate that SerialBT have the same functionalities of a normal Serial
// SSP - Simple Secure Pairing - The device (ESP32) will display random number and the user is responsible of comparing it to the number
// displayed on the other device (for example phone).
// If the numbers match the user authenticates the pairing on both devices - on phone simply press "Pair" and in terminal for the sketch send 'Y' or 'y' to confirm.
// Alternatively uncomment AUTO_PAIR to skip the terminal confirmation.

#include "BluetoothSerial.h"

//#define AUTO_PAIR // Uncomment to automatically authenticate ESP32 side

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

const char *deviceName = "ESP32_SSP_example";

// The following lines defines the method of pairing
// When both Input and Output are false only the other device authenticates pairing without any pin.
// When Output is true and Input is false only the other device authenticates pairing without any pin.
// When both Input and Output are true both devices display randomly generated code and if they match authenticate pairing on both devices
//   - This must be implemented by registering callback via onConfirmRequest() and in this callback request user input and call confirmReply(true); if the authenticated
//      otherwise call `confirmReply(false)` to reject the pairing.
// When Input is true and Output is false User will be required to input the passkey to the ESP32 device to authenticate.
//   - This must be implemented by registering callback via onKeyRequest() and in this callback the entered passkey will be responded via respondPasskey(passkey);
const bool INPUT_CAPABILITY = false;  // Defines if ESP32 device has input method (Serial terminal, keyboard or similar)
const bool OUTPUT_CAPABILITY = true;  // Defines if ESP32 device has output method (Serial terminal, display or similar)

BluetoothSerial SerialBT;
bool confirmRequestDone = false;

void BTConfirmRequestCallback(uint32_t numVal) {
  confirmRequestDone = false;
#ifndef AUTO_PAIR
  Serial.printf(
    "The PIN is: %06lu. If it matches number displayed on the other device write \'Y\' or \'y\':\n", numVal
  );  // Note the formatting "%06lu" - PIN can start with zero(s) which would be ignored with simple "%lu"
  while (!Serial.available()) {
    delay(1);  // Feed the watchdog
    // Wait until data is available on the Serial port.
  }
  Serial.printf("Oh you sent %d Bytes, lets see...", Serial.available());
  int dat = Serial.read();
  if (dat == 'Y' || dat == 'y') {
    SerialBT.confirmReply(true);
  } else {
    SerialBT.confirmReply(false);
  }
#else
  SerialBT.confirmReply(true);
#endif
}

void BTKeyRequestCallback() {
  Serial.println("BTKeyRequestCallback");  // debug
  char buffer[7] = {0};                    // 6 bytes for number, one for termination '0'
  while (1) {
    Serial.print("Enter the passkey displayed on the other device: ");
    while (!Serial.available()) {
      delay(1);  // Feed the watchdog
      // Wait until data is available on the Serial port.
    }
    size_t len = Serial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';  // Null-terminate the string.
    try {
      uint32_t passkey = std::stoi(buffer);
      Serial.printf("Entered PIN: %lu\n", passkey);
      SerialBT.respondPasskey(passkey);
      return;
    } catch (...) {
      Serial.print("Wrong PIN! Try again.");
    }  // try
  }  // while(1)
}

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
  SerialBT.enableSSP(INPUT_CAPABILITY, OUTPUT_CAPABILITY);  // Must be called before begin
  SerialBT.onConfirmRequest(BTConfirmRequestCallback);
  SerialBT.onKeyRequest(BTKeyRequestCallback);
  SerialBT.onAuthComplete(BTAuthCompleteCallback);
  SerialBT.begin(deviceName);  // Initiate Bluetooth device with name in parameter
  //SerialBT.deleteAllBondedDevices(); // Uncomment this to delete paired devices; Must be called after begin
  Serial.printf("The device started with name \"%s\", now you can pair it with Bluetooth!\n", deviceName);
  if (INPUT_CAPABILITY and OUTPUT_CAPABILITY) {
    Serial.println("Both devices will display randomly generated code and if they match authenticate pairing on both devices");
  } else if (not INPUT_CAPABILITY and not OUTPUT_CAPABILITY) {
    Serial.println("Authenticate pairing on the other device. No PIN is used");
  } else if (not INPUT_CAPABILITY and OUTPUT_CAPABILITY) {
    Serial.println("Authenticate pairing on the other device. No PIN is used");
  } else if (INPUT_CAPABILITY and not OUTPUT_CAPABILITY) {
    Serial.println("After pairing is initiated you will be required to enter the passkey to the ESP32 device to authenticate\n > The Passkey will displayed on "
                   "the other device");
  }
}

void loop() {
  if (confirmRequestDone) {
    serial_response();
  } else {
    delay(1);  // Feed the watchdog
  }
}
