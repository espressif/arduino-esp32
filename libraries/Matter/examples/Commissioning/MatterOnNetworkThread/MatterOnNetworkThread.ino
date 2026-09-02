// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// On-network commissioning over Thread. Side-by-side with MatterCHIPoBLEThread:
// same On/Off Light; the delta is selectNetwork(THREAD, true), the border router network key, then attach.
// Do not start Arduino ESPmDNS — CHIP owns the mDNS responder.
// Do not use the Arduino BLE library (BLE.h / BLEDevice) in this sketch.
// Do not call OThread.begin() before Matter.begin() — that would start a second stack.
// Do not call OThreadDNSSD.begin() — CHIP owns Thread SRP (_matterc._udp).

#include <Arduino.h>
#include <Matter.h>
#if CONFIG_ENABLE_MATTER_OVER_THREAD
#include <OThread.h>
#endif

MatterOnOffLight OnOffLight;

#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;
#endif

const uint8_t buttonPin = BOOT_PIN;
uint32_t button_time_stamp = 0;
bool button_state = false;
const uint32_t decommissioningTimeout = 5000;

#if CONFIG_ENABLE_MATTER_OVER_THREAD
// Replace with the Thread network this node should join (same values as the border router).
const uint8_t threadNetworkKey[OT_NETWORK_KEY_SIZE] = { 0x00, 0x00, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77 };

static bool provisionThreadNetwork() {
  OThread.begin(false);
  if (!OThread.isAttachedToExternalStack()) {
    Serial.println("OThread.begin() did not attach to Matter's Thread stack.");
    return false;
  }

  // set the Border Router NetworkKey to this node in order to reach the Matter Controller
  DataSet dataset;
  dataset.clear();
  dataset.setNetworkKey(threadNetworkKey);
  OThread.commitDataSet(dataset);
  Serial.println("Committed sketch Thread dataset with the Thread Border Router network key.");

  OThread.networkInterfaceUp();
  OThread.start();

  Serial.println("Waiting for Thread attach...");
  if (!OThread.waitForAttach(60000)) {
    Serial.println("Thread did not attach within 60 s. Check dataset vs the border router.");
    return false;
  }
  if (!Matter.waitForNetwork(15000)) {
    Serial.println("Thread attached but no IPv6 on OT_DEF yet.");
    return false;
  }
  Serial.printf("Thread ready, role=%s endpoint=%u\r\n", OThread.otGetStringDeviceRole(), Matter.getNetworkEndPointId(MATTER_NETWORK_THREAD));
  Serial.println("CHIP will advertise _matterc._udp via Thread SRP once the border router SRP server answers.");
  Serial.println("Alexa typically finds Matter devices over BLE. Use MatterCHIPoBLEThread for that.");
  return true;
}
#endif

bool onOffLightCallback(bool state) {
  digitalWrite(ledPin, state ? HIGH : LOW);
  return true;
}

static void halt(const char *reason) {
  Serial.println(reason);
  Serial.println("Halting. loop() will not run.");
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  if (!Matter.isNetworkSupported(MATTER_NETWORK_THREAD)) {
    halt("Matter-over-Thread is not enabled in this build. On ESP32-C5 set Tools → Matter Network → Thread.");
  }

  // Before any accessory begin(). true disables CHIPoBLE (on-network).
  // Do not also call setBLECommissioningEnabled(false) — this already does that.
  if (!Matter.selectNetwork(MATTER_NETWORK_THREAD, true)) {
    halt("selectNetwork(Thread) failed.");
  }

  OnOffLight.begin();
  OnOffLight.onChange(onOffLightCallback);
  Matter.begin();
  Serial.printf("BLE commissioning enabled: %s\r\n", Matter.isBLECommissioningEnabled() ? "YES" : "NO");

#if CONFIG_ENABLE_MATTER_OVER_THREAD
  if (!provisionThreadNetwork()) {
    halt("Failed to provision or attach to the Thread network. See the last error above.");
  }
#endif

  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Commission it on the Thread network with the pairing code or QR code.");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
}

void loop() {
  if (digitalRead(buttonPin) == LOW && !button_state) {
    button_time_stamp = millis();
    button_state = true;
  }
  if (digitalRead(buttonPin) == HIGH && button_state) {
    button_state = false;
  }
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
    button_time_stamp = millis();
  }
  delay(500);
}
