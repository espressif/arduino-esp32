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

// The authentication procedure gives up after about 30 s; reply sooner so the
// peer gets an explicit rejection instead of a timeout.
static constexpr uint32_t AUTH_INPUT_TIMEOUT_MS = 20000;

// loop() forwards Serial into SerialBT. The confirm callback also reads Serial,
// but from the Bluetooth host task, so the Arduino loop must not consume y/n
// while that prompt is active.
static volatile bool awaitingConfirm = false;

bool confirmRequestCallback(uint32_t passkey) {
  awaitingConfirm = true;

  // Drop anything typed before the prompt so leftover keystrokes are not taken
  // as the confirmation.
  while (Serial.available()) {
    Serial.read();
  }

  Serial.printf("Confirm passkey: %06lu (y/n)? ", passkey);

  uint32_t start = millis();
  while (!Serial.available()) {
    if (millis() - start > AUTH_INPUT_TIMEOUT_MS) {
      Serial.println("\nTimeout -- rejecting pairing.");
      awaitingConfirm = false;
      return false;
    }
    delay(10);
  }
  char c = Serial.read();
  awaitingConfirm = false;
  if (c == 'y' || c == 'Y') {
    Serial.println("Accepted.");
    return true;
  }
  Serial.println("Rejected.");
  return false;
}

void setup() {
  Serial.begin(115200);

  SerialBT.enableSSP(true, true);  // display + keyboard -> numeric comparison with a phone
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
    Serial.printf("Bluetooth init failed! (%s)\n", status.toString());
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

  if (!awaitingConfirm) {
    while (Serial.available()) {
      SerialBT.write(Serial.read());
    }
  }

  delay(1);
}
