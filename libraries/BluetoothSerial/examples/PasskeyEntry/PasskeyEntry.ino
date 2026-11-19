/*
 * BluetoothSerial SSP Passkey Entry
 *
 * Demonstrates both sides of Secure Simple Pairing (SSP) Passkey Entry.
 * enableSSP() selects the IO capability, which decides the side:
 *
 *   PASSKEY_DISPLAY_SIDE 0 (default) - KeyboardOnly: the peer displays a
 *     six-digit passkey; onPassKeyRequest() asks for it on Serial.
 *   PASSKEY_DISPLAY_SIDE 1 - DisplayOnly: the stack generates the passkey;
 *     onPassKeyDisplay() prints it for the user to type on the peer.
 *
 * Run the default sketch on two boards to pair them: both are KeyboardOnly,
 * so both consoles prompt; type the same six digits on both. That is the SSP
 * equivalent of a fixed PIN. Press 'c' on one board to connect to the other.
 * Set the switch to 1 on one board to exercise the display side instead.
 *
 * Phones and PCs are DisplayYesNo, which selects Numeric Comparison rather
 * than Passkey Entry; use the SSPPairing example for those.
 *
 * For legacy PIN pairing instead of SSP, replace enableSSP() with:
 *   SerialBT.disableSSP();
 *   SerialBT.setPin("1234");
 */

#include <Arduino.h>
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it.
#endif

// 0 = keyboard side (KeyboardOnly), 1 = display side (DisplayOnly).
#define PASSKEY_DISPLAY_SIDE 0

static const char *PEER_NAME = "ESP32-Passkey";
// The authentication procedure gives up after about 30 s; reply sooner so the
// peer gets an explicit rejection instead of a timeout.
static constexpr uint32_t AUTH_INPUT_TIMEOUT_MS = 20000;

BluetoothSerial SerialBT;

// loop() reads Serial for the 'c' connect command. The passkey callback
// also reads Serial, but from the Bluetooth host task, so the Arduino loop
// must not consume digits while that prompt is active (the acceptor keeps
// running loop() during an incoming pair).
static volatile bool collectingPasskey = false;

#if !PASSKEY_DISPLAY_SIDE
// Keyboard side: the peer shows the code, we return it.
// Runs on the Bluetooth host task -- read with a timeout.
uint32_t passkeyRequestCallback(const BTAddress &peer) {
  collectingPasskey = true;

  // Drop anything typed before the prompt so leftover keystrokes (including
  // a connect 'c' that loop() has not consumed yet) are not taken as digits.
  while (Serial.available()) {
    Serial.read();
  }

  Serial.printf("Peer %s is displaying a passkey.\n", peer.toString().c_str());
  Serial.print("Enter the 6-digit passkey: ");

  uint32_t passkey = 0;
  uint8_t digits = 0;
  uint32_t start = millis();

  while (digits < 6) {
    if (millis() - start > AUTH_INPUT_TIMEOUT_MS) {
      Serial.println("\nTimeout -- rejecting pairing.");
      collectingPasskey = false;
      return 0xFFFFFFFFu;  // > 999999: reject
    }
    if (Serial.available()) {
      char c = Serial.read();
      if (c >= '0' && c <= '9') {
        passkey = passkey * 10 + (c - '0');
        digits++;
        Serial.print(c);
      } else if (c == '\r' || c == '\n') {
        break;
      }
    }
    delay(10);
  }

  Serial.println();
  collectingPasskey = false;
  return passkey;
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("BluetoothSerial Passkey Entry example");

#if PASSKEY_DISPLAY_SIDE
  SerialBT.enableSSP(false, true);  // output + no input -> DisplayOnly
  SerialBT.onPassKeyDisplay([](const BTAddress &peer, uint32_t passkey) {
    Serial.printf("Passkey for %s: %06lu (type it on the peer)\n", peer.toString().c_str(), (unsigned long)passkey);
  });
#else
  SerialBT.enableSSP(true, false);  // input + no output -> KeyboardOnly
  SerialBT.onPassKeyRequest(passkeyRequestCallback);
#endif

  SerialBT.onAuthComplete([](bool success) {
    Serial.printf("Pairing %s\n", success ? "succeeded" : "failed");
  });

  BTStatus status = SerialBT.begin(PEER_NAME);
  if (!status) {
    Serial.printf("Bluetooth init failed! (%s)\n", status.toString());
    while (true) {
      delay(1000);
    }
  }

  Serial.printf("Started as \"%s\". Press 'c' to connect to the other board.\n", PEER_NAME);
}

void loop() {
  if (!collectingPasskey && Serial.available()) {
    char c = Serial.read();
    if ((c == 'c' || c == 'C') && !SerialBT.connected()) {
      Serial.printf("Connecting to \"%s\"...\n", PEER_NAME);
      BTStatus status = SerialBT.connect(String(PEER_NAME), 30000);
      Serial.printf("Connect: %s\n", status ? "OK" : status.toString());
    }
  }

  while (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }
  delay(10);
}
