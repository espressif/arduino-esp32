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
// ESP32-C3/H2/C5: if the node never commissions (usual low-heap symptom), uncomment this.
// The default 8 KB loop stack is reserved from the heap before setup();
// 4 KB is enough for this sketch and frees 4 KB for CHIPoBLE + Wi-Fi.
// SET_LOOP_TASK_STACK_SIZE(4 * 1024);

// List of Matter Endpoints for this Node
// Three light endpoints: On/Off, Dimmable, and Color.
MatterOnOffLight Light1;
MatterDimmableLight Light2;
MatterColorLight Light3;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// set your board USER BUTTON pin here -  USED to decommission the Matter Node
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Matter Protocol Endpoint Callback for each Light Accessory
bool setLightOnOff1(bool state) {
  Serial.printf("Light1 changed state to: %s\r\n", state ? "ON" : "OFF");
  return true;
}

bool setLightOnOff2(bool state) {
  Serial.printf("Light2 changed state to: %s\r\n", state ? "ON" : "OFF");
  return true;
}

bool setLightOnOff3(bool state) {
  Serial.printf("Light3 changed state to: %s\r\n", state ? "ON" : "OFF");
  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  button.begin(buttonPin);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize all 3 Matter EndPoints
  Light1.begin();
  Light2.begin();
  Light3.begin();
  Light1.onChangeOnOff(setLightOnOff1);
  Light2.onChangeOnOff(setLightOnOff2);
  Light3.onChangeOnOff(setLightOnOff3);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

  static uint32_t timeCounter = 0;

  //displays the Light state every 5 seconds
  if (!(timeCounter++ % 10)) {  // delaying for 500ms x 10 = 5s
    Serial.println("======================");
    Serial.printf("Matter Light #1 is %s\r\n", Light1.getOnOff() ? "ON" : "OFF");
    Serial.printf("Matter Light #2 is %s\r\n", Light2.getOnOff() ? "ON" : "OFF");
    Serial.printf("Matter Light #3 is %s\r\n", Light3.getOnOff() ? "ON" : "OFF");
  }

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Composed Light Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  delay(500);
}
