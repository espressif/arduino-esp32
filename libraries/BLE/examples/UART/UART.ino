/*
 * BLE UART Service Example -- BLEStream API
 *
 * Exposes the Nordic UART Service (NUS) using BLEStream and bridges data
 * between USB Serial and BLE using the Arduino Stream interface.
 *
 * Compatible with nRF Connect, nRF Toolbox UART, and BLE serial terminal apps.
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <BLE.h>

BLEStream bleSerial;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== BLE UART Service Example (BLEStream) ===");

  bleSerial.onConnect([](const BLEConnInfo &connInfo) {
    Serial.println("UART client connected.");
    Serial.println("Type in Serial Monitor to send data over BLE.");
  });

  bleSerial.onDisconnect([](const BLEConnInfo &connInfo, uint8_t reason) {
    Serial.println("UART client disconnected.");
    Serial.println("Waiting for a new connection...");
  });

  Serial.print("Starting BLEStream... ");
  BTStatus status = bleSerial.begin("UART Service");
  if (!status) {
    Serial.printf("FAILED! (%s)\n", status.toString());
    return;
  }
  Serial.println("OK");

  Serial.println();
  Serial.println("UART service ready! Waiting for connections...");
  Serial.printf("Device: %s\n", BLE.getDeviceName().c_str());
  Serial.println("Connect with nRF Connect or nRF Toolbox UART app.");
  Serial.println();
}

void loop() {
  // BLE -> USB Serial
  while (bleSerial.available()) {
    int c = bleSerial.read();
    if (c >= 0) {
      Serial.write(static_cast<uint8_t>(c));
    }
  }

  // USB Serial -> BLE
  if (Serial.available()) {
    uint8_t buffer[128];
    int bytesToRead = Serial.available();
    if (bytesToRead > static_cast<int>(sizeof(buffer))) {
      bytesToRead = sizeof(buffer);
    }

    size_t bytesRead = Serial.readBytes(reinterpret_cast<char *>(buffer), bytesToRead);
    if (bytesRead > 0) {
      bleSerial.write(buffer, bytesRead);
    }
  }

  delay(5);
}
