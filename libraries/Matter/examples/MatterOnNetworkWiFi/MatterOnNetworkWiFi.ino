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

// On-network commissioning when CHIPoBLE is off (Arduino IDE ESP32/S2, or setBLECommissioningEnabled(false)).
// Do not use the Arduino BLE library (BLE.h / BLEDevice) in this sketch.

#include <Arduino.h>
#include <Matter.h>
#include <WiFi.h>

MatterOnOffLight OnOffLight;

const char *ssid = "your-ssid";
const char *password = "your-password";

#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;
#endif

const uint8_t buttonPin = BOOT_PIN;
uint32_t button_time_stamp = 0;
bool button_state = false;
const uint32_t decommissioningTimeout = 5000;

static uint32_t sHeapBeforeBegin = 0;

static void printHeap(const char *when) {
  Serial.printf(
    "%s  free=%lu  min=%lu  maxAlloc=%lu\r\n", when, (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
    (unsigned long)ESP.getMaxAllocHeap()
  );
}

bool onOffLightCallback(bool state) {
  digitalWrite(ledPin, state ? HIGH : LOW);
  return true;
}

void onBleMemoryReleased() {
  // CHIP task: print only. Allocate large buffers from loop() after this fires.
  printHeap("After BLE memory released");
  Serial.printf("Heap delta vs before begin: %d bytes\r\n", (int)ESP.getFreeHeap() - (int)sHeapBeforeBegin);
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  // Call before Matter.begin(). Releases BLE RAM after start on CHIPoBLE SoCs.
  Matter.setBLECommissioningEnabled(false);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();
  Serial.print("Wi-Fi connected, IP ");
  Serial.println(WiFi.localIP());

  OnOffLight.begin();
  OnOffLight.onChange(onOffLightCallback);
  Matter.onBLEMemoryReleased(onBleMemoryReleased);

  sHeapBeforeBegin = ESP.getFreeHeap();
  printHeap("Before Matter.begin()");
  Matter.begin();
  printHeap("After Matter.begin()");
  Serial.printf("Heap delta after begin: %d bytes\r\n", (int)ESP.getFreeHeap() - (int)sHeapBeforeBegin);
  Serial.printf("BLE commissioning enabled: %s\r\n", Matter.isBLECommissioningEnabled() ? "YES" : "NO");

  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Commission it on the Wi-Fi network with the pairing code or QR code.");
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
