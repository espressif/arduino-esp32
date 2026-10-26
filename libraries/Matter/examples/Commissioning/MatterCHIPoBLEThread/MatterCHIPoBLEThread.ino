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

// CHIPoBLE Thread commissioning. The commissioner delivers the Thread dataset
// over BLE. Do not commit a sketch dataset — that fights the hub.
// For on-network Thread (BLE off + network key in the sketch) see MatterOnNetworkThread.
// Do not start Arduino ESPmDNS. Do not use BLE.h / BLEDevice.
//
// Supported SoCs: C5 (Tools → Matter Network → Thread), C6, and H2.
// ESP32 / S2 / S3 / C3: no Matter-over-Thread in the prebuild — this sketch will halt.
// C6: one dual-stack prebuild. No Matter Network menu. selectNetwork(MATTER_NETWORK_THREAD) uses
// Thread (root Network Commissioning).

#include <Arduino.h>
#include <Matter.h>

MatterOnOffLight OnOffLight;

#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;
#endif

const uint8_t buttonPin = BOOT_PIN;
MatterButton button;

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
  button.begin(buttonPin);
  pinMode(ledPin, OUTPUT);

  if (!Matter.isNetworkSupported(MATTER_NETWORK_THREAD)) {
    halt("Matter-over-Thread is not enabled in this build (C6/H2 prebuild only).");
  }
  if (!Matter.isBLECommissioningEnabled()) {
    halt("CHIPoBLE is not compiled in. Use MatterOnNetworkThread and set the border router network key.");
  }

  // Before any accessory begin(). One-arg form leaves CHIPoBLE on.
  // Do not call setBLECommissioningEnabled(true) — it cannot enable BLE if it is not compiled in.
  if (!Matter.selectNetwork(MATTER_NETWORK_THREAD)) {
    halt("selectNetwork(Thread) failed.");
  }

  OnOffLight.begin();
  OnOffLight.onChange(onOffLightCallback);
  Matter.begin();
  Serial.printf("BLE commissioning enabled: %s\r\n", Matter.isBLECommissioningEnabled() ? "YES" : "NO");
  Serial.printf("Thread Network Commissioning is on endpoint %u.\r\n", Matter.getNetworkEndPointId(MATTER_NETWORK_THREAD));
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }
  delay(500);
}
