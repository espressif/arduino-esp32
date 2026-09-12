// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// CHIPoBLE commissioning, then automatic BLE RAM release after a fabric exists.
// onBLEMemoryReleased() runs when that RAM is back on the heap — allocate large
// buffers from loop(), not from the callback (it runs on the CHIP task).
// Do not use the Arduino BLE library (BLE.h / BLEDevice) in this sketch.
// Fallback when CHIPoBLE is not in the build: WiFi.begin(ssid, password); BLE RAM release does not apply.

#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
#include <WiFi.h>
const char *ssid = "your-ssid";
const char *password = "your-password";
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

static uint32_t sHeapBeforeBegin = 0;
static uint32_t sHeapAfterBegin = 0;

// Demo "bigger chunk" after BLE reclaim. Real apps pick a size that fits their peak heap.
static const size_t kPostBleBufferSize = 16 * 1024;
static volatile bool sBleMemoryReleased = false;
static uint8_t *sPostBleBuffer = nullptr;

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

void onMatterEvent(matterEvent_t eventType, const chip::DeviceLayer::ChipDeviceEvent *) {
  // event pointer is only valid during this callback; do not store it.
  if (eventType == MATTER_COMMISSIONING_COMPLETE) {
    printHeap("Commissioning complete");
  }
}

// CHIP task: do not malloc a large block here.
void onBleMemoryReleased() {
  sBleMemoryReleased = true;
}

void tryAllocAfterBleRelease() {
  if (!sBleMemoryReleased || sPostBleBuffer != nullptr) {
    return;
  }
  sBleMemoryReleased = false;
  printHeap("BLE memory released (CHIP kBLEDeinitialized)");
  Serial.printf("Heap vs after begin: %+d bytes\r\n", (int)ESP.getFreeHeap() - (int)sHeapAfterBegin);
  Serial.printf("Heap vs before begin: %+d bytes\r\n", (int)ESP.getFreeHeap() - (int)sHeapBeforeBegin);

  sPostBleBuffer = (uint8_t *)malloc(kPostBleBufferSize);
  if (sPostBleBuffer != nullptr) {
    Serial.printf("Allocated %u bytes after BLE RAM reclaim.\r\n", (unsigned)kPostBleBufferSize);
    printHeap("After post-BLE malloc");
  } else {
    Serial.printf("malloc(%u) failed after BLE reclaim.\r\n", (unsigned)kPostBleBufferSize);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

#if !CONFIG_ENABLE_CHIPOBLE
  Serial.println("CHIPoBLE is not compiled in this build. BLE RAM release does not apply.");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();
#endif

  OnOffLight.begin();
  OnOffLight.onChange(onOffLightCallback);
  Matter.onEvent(onMatterEvent);
  Matter.onBLEMemoryReleased(onBleMemoryReleased);

  // Default is true. Set false to keep NimBLE after CHIPoBLE commissioning.
  Matter.setBLEMemoryReleaseEnabled(true);

  sHeapBeforeBegin = ESP.getFreeHeap();
  printHeap("Before Matter.begin()");
  Matter.begin();
  sHeapAfterBegin = ESP.getFreeHeap();
  printHeap("After Matter.begin()");
  Serial.printf("Heap delta after begin: %+d bytes\r\n", (int)sHeapAfterBegin - (int)sHeapBeforeBegin);
  Serial.printf("BLE commissioning enabled: %s\r\n", Matter.isBLECommissioningEnabled() ? "YES" : "NO");
  Serial.printf("BLE memory release after commissioning: %s\r\n", Matter.isBLEMemoryReleaseEnabled() ? "YES" : "NO");

  if (Matter.isDeviceCommissioned()) {
    if (Matter.isBLEMemoryReleaseEnabled()) {
      Serial.println("Already commissioned. Wait for onBLEMemoryReleased() / MATTER_BLE_DEINITIALIZED.");
    } else {
      Serial.println("Already commissioned.");
    }
  } else {
    Serial.println("Matter Node is not commissioned yet.");
    if (Matter.isBLECommissioningEnabled()) {
      Serial.println("Commission it with CHIPoBLE using the pairing code or QR code.");
    } else {
      Serial.println("Commission it using the pairing code or QR code.");
    }
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
}

void loop() {
  tryAllocAfterBleRelease();

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
