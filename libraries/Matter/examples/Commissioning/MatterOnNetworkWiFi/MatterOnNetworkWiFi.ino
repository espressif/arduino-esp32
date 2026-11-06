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

// On-network commissioning over Wi-Fi. Side-by-side with MatterCHIPoBLEWiFi:
// same On/Off Light; the delta is selectNetwork(MATTER_NETWORK_WIFI, true) then WiFi.begin().
// Do not use the Arduino BLE library (BLE.h / BLEDevice) in this sketch.
//
// Supported SoCs: ESP32, S2, S3, C3, C5, C6. H2: no Wi-Fi — use a Thread example.
// C5: Tools → Matter Network → Wi-Fi (default). C6: one dual-stack prebuild.
// No Matter Network menu. selectNetwork(MATTER_NETWORK_WIFI, true) keeps Wi-Fi and turns CHIPoBLE off.

#include <Arduino.h>
#include <Matter.h>
#include <WiFi.h>

MatterOnOffLight OnOffLight;

// This sketch always joins Wi-Fi itself (on-network commissioning, no BLE).
// Set your AP here.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;
#endif

const uint8_t buttonPin = BOOT_PIN;
MatterButton button;

static uint32_t sHeapBeforeBegin = 0;

static void printHeap(const char *when) {
  Serial.printf(
    "%s  free=%lu  min=%lu  maxAlloc=%lu\r\n", when, (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getMaxAllocHeap()
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

  if (!Matter.isNetworkSupported(MATTER_NETWORK_WIFI)) {
    halt("Wi-Fi station is not enabled in this build.");
  }
  // Before any accessory begin(). true turns CHIPoBLE off (on-network).
  // Do not also call setBLECommissioningEnabled(false) — this already does that.
  if (!Matter.selectNetwork(MATTER_NETWORK_WIFI, true)) {
    halt("selectNetwork(Wi-Fi) failed.");
  }

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
#if CONFIG_LWIP_IPV6
  WiFi.enableIPv6(true);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();
  Serial.println("Wi-Fi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);

  OnOffLight.begin();
  OnOffLight.onChange(onOffLightCallback);
  Matter.onBLEMemoryReleased(onBleMemoryReleased);

  sHeapBeforeBegin = ESP.getFreeHeap();
  printHeap("Before Matter.begin()");
  Matter.begin();
  printHeap("After Matter.begin()");
  Serial.printf("Heap delta after begin: %d bytes\r\n", (int)ESP.getFreeHeap() - (int)sHeapBeforeBegin);
  Serial.printf("BLE commissioning enabled: %s\r\n", Matter.isBLECommissioningEnabled() ? "YES" : "NO");
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
