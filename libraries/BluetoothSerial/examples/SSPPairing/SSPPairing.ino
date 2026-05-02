/*
 * BluetoothSerial SSP Pairing
 *
 * Demonstrates Secure Simple Pairing (SSP) with numeric comparison.
 * When a phone pairs, the passkey is printed on Serial. Type 'y' to accept.
 *
 * For legacy PIN pairing instead of SSP, replace enableSSP() with:
 *   SerialBT.disableSSP();
 *   SerialBT.setPin("1234");
 */

#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

bool confirmRequestCallback(uint32_t passkey) {
  Serial.printf("Confirm passkey: %06lu (y/n)? ", passkey);
  while (Serial.available() == 0) {
    delay(100);
  }
  char c = Serial.read();
  if (c == 'y' || c == 'Y') {
    Serial.println("Accepted.");
    return true;
  } else {
    Serial.println("Rejected.");
    return false;
  }
}

void setup() {
  Serial.begin(115200);

  SerialBT.enableSSP(false, true);
  SerialBT.onConfirmRequest(confirmRequestCallback);
  SerialBT.onAuthComplete([](bool success) {
    if (success) {
      Serial.println("Pairing successful!");
    } else {
      Serial.println("Pairing failed.");
    }
  });

  BTStatus status = SerialBT.begin("ESP32-SSP");
  if (!status) {
    Serial.println("Bluetooth init failed!");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Bluetooth started with SSP. Pair with \"ESP32-SSP\"");
}

void loop() {
  while (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }

  while (Serial.available()) {
    SerialBT.write(Serial.read());
  }

  delay(1);
}
