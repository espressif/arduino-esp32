// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

// Matter Manager
#include <Arduino.h>
#include <Matter.h>

// List of Matter Endpoints for this Node
// Water Valve Endpoint
MatterWaterValve WaterValve;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// set your board LED pin here
#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#warning "Do not forget to set the LED pin"
#endif

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Open the valve for 10 seconds whenever it is commanded open - replace with your own irrigation/valve timing
const uint32_t openDurationSeconds = 10;

// Matter Protocol Endpoint Callbacks - replace with your real valve actuator control (relay, solenoid driver, etc.)
bool onValveOpen() {
  Serial.println("User Callback :: Opening the water valve");
  digitalWrite(ledPin, HIGH);
  return true;
}

void onValveClose() {
  Serial.println("User Callback :: Closing the water valve");
  digitalWrite(ledPin, LOW);
}

void setup() {
  button.begin(buttonPin);
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Starts closed. open() with no argument stays open; this sketch passes a duration on button click.
  WaterValve.begin();
  WaterValve.onOpen(onValveOpen);
  WaterValve.onClose(onValveClose);

  matterSetExampleIdentity("WaterValve");
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_CLICK) {
      if (WaterValve.isOpen()) {
        Serial.println("User button released. Closing the water valve!");
        WaterValve.close();
      } else {
        Serial.println("User button released. Opening the water valve!");
        WaterValve.open(openDurationSeconds);
      }
    } else if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Water Valve Matter Accessory. It shall be commissioned again.");
      WaterValve.close();
      Matter.decommission();
    }
  }

  static uint32_t remainingDurationPrev = 0;
  uint32_t remainingDuration = WaterValve.getRemainingDuration();
  if (remainingDurationPrev != remainingDuration) {
    if (remainingDuration > 0) {
      Serial.printf("Water valve remaining duration: %lu s\r\n", remainingDuration);
    }
    remainingDurationPrev = remainingDuration;
  }
}
