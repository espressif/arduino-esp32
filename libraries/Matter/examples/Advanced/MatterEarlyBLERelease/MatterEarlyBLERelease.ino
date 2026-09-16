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

// One On/Off Light. Shows when BLE RAM is released — not a many-endpoint demo.
//
// MATTER_EARLY_BLE_RELEASE (set just after the includes, or -D...=0 / =1):
//
//   1 (default)  Free BLE at boot, then commission on-network over Wi-Fi.
//                A strong C bleInUse() returns false so initArduino() releases
//                BLE RAM before setup(). selectNetwork(MATTER_NETWORK_WIFI, true) turns CHIPoBLE
//                off; WiFi.begin() joins the AP. Do not also call
//                setBLECommissioningEnabled().
//
//   0            Regular CHIPoBLE. No sketch Wi-Fi. The hub sends credentials
//                over BLE. The library reclaims BLE RAM after Matter.begin()
//                (already commissioned) or after the first commission.
//
// bleInUse() must be extern "C". A C++ bool bleInUse() is mangled: no override,
// no link error, BLE stays reserved.
//
// Do not include BLE.h / BLEDevice. Neither mode hands the radio to Arduino BLE.
//
// ESP32 / ESP32-S2 have no CHIPoBLE in the Arduino prebuild — the sketch uses
// Wi-Fi. ESP32-H2 has no Wi-Fi: use mode 0 (CHIPoBLE / Thread).
// C5: Tools → Matter Network → Wi-Fi. C6: one dual-stack image.
//
// [heap] prints internal RAM and PSRAM. With PSRAM initialized, the data-model
// wrap may put the light in SPIRAM (psram= drops a little). CHIP / NimBLE still
// use internal malloc.

#include <Arduino.h>
#include <Matter.h>

#ifndef MATTER_EARLY_BLE_RELEASE
#define MATTER_EARLY_BLE_RELEASE 1  // 1 = BLE free at boot + Wi-Fi; 0 = CHIPoBLE
#endif

#if MATTER_EARLY_BLE_RELEASE || !CONFIG_ENABLE_CHIPOBLE
#include <WiFi.h>
#endif
// Fill these in for mode 1 (on-network) and for boards with no BLE commissioning
// (ESP32 / ESP32-S2). In mode 0, Matter commissions over BLE (CHIPoBLE) and
// the hub sends the Wi-Fi credentials — leave the placeholders.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

MatterOnOffLight OnOffLight;

#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;
#endif

const uint8_t buttonPin = BOOT_PIN;
MatterButton button;

static volatile bool sBleMemoryReleased = false;

#if MATTER_EARLY_BLE_RELEASE && SOC_BT_SUPPORTED
// Replaces the HAL weak C bleInUse(). Matter sets _bleLibraryInUse so the
// default would return true and keep BLE reserved through initArduino().
extern "C" bool bleInUse(void) {
  return false;
}
#endif

// Tools → PSRAM Enabled defines BOARD_HAS_PSRAM. S3 can still init the chip when
// that menu is Disabled (harvest keeps CONFIG_SPIRAM=y). psramFound() is the
// runtime heap the data-model wrap uses.
static void printPsramStatus() {
#ifdef BOARD_HAS_PSRAM
  const char *menu = "Enabled";
#else
  const char *menu = "Disabled";
#endif
  if (psramFound()) {
    Serial.printf("PSRAM: Tools=%s, initialized, free=%lu bytes\r\n", menu, (unsigned long)ESP.getFreePsram());
  } else {
    Serial.printf("PSRAM: Tools=%s, not initialized\r\n", menu);
  }
}

static void printHeap(const char *when) {
  Serial.printf("[heap] %s  int=%lu  intMax=%lu", when, (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
  if (psramFound()) {
    Serial.printf("  psram=%lu", (unsigned long)ESP.getFreePsram());
  } else {
    Serial.print("  psram=none");
  }
  Serial.println();
}

static void halt(const char *reason) {
  Serial.println(reason);
  Serial.println("Halting.");
  while (true) {
    delay(1000);
  }
}

bool onOffLightCallback(bool state) {
  digitalWrite(ledPin, state ? HIGH : LOW);
  Serial.printf("Light EP%u -> %s\r\n", OnOffLight.getEndPointId(), state ? "ON" : "OFF");
  return true;
}

// Runs on the CHIP task. Only set a flag; print heap from loop().
void onBleMemoryReleased() {
  sBleMemoryReleased = true;
}

void setup() {
  Serial.begin(115200);
  button.begin(buttonPin);
  pinMode(ledPin, OUTPUT);

  Serial.println();
#if MATTER_EARLY_BLE_RELEASE
  Serial.println("MatterEarlyBLERelease: mode 1 — BLE free at boot, on-network Wi-Fi.");
#else
  Serial.println("MatterEarlyBLERelease: mode 0 — CHIPoBLE commissioning.");
#endif
#if !CONFIG_ENABLE_CHIPOBLE
  Serial.println("This prebuild has no CHIPoBLE (ESP32 / ESP32-S2). Using sketch Wi-Fi.");
#endif
  printPsramStatus();
  printHeap("setup() after Serial");

#if MATTER_EARLY_BLE_RELEASE
  if (!Matter.isNetworkSupported(MATTER_NETWORK_WIFI)) {
    halt("Mode 1 needs Wi-Fi. Set MATTER_EARLY_BLE_RELEASE 0 on ESP32-H2.");
  }
  // Before OnOffLight.begin(). true = CHIPoBLE off (on-network QR only).
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
  printHeap("after Wi-Fi connected");
#elif !CONFIG_ENABLE_CHIPOBLE
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
  printHeap("after Wi-Fi connected");
#endif

  if (!OnOffLight.begin(false)) {
    halt("OnOffLight.begin() failed.");
  }
  OnOffLight.onChangeOnOff(onOffLightCallback);
  printHeap("after OnOffLight.begin()");

  Matter.onBLEMemoryReleased(onBleMemoryReleased);

  printHeap("before Matter.begin()");
  Matter.begin();
  printHeap("after Matter.begin()");
  matterWaitUntilReady();
  printHeap("after waitUntilReady()");
}

void loop() {
  matterRestartIfNoFabric();

  if (sBleMemoryReleased) {
    sBleMemoryReleased = false;
    Serial.println("onBLEMemoryReleased(): BLE RAM is back on the heap.");
    printHeap("after onBLEMemoryReleased()");
  }

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Matter Node. It shall be commissioned again.");
      Matter.decommission();
    }
  }
  delay(50);
}
