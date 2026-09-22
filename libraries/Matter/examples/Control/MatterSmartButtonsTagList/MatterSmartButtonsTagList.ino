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

// Three Matter smart buttons — On, Off, and a custom-labeled Scene — sharing the same
// Generic Switch device type. The Descriptor cluster TagList attribute is used to tag
// each button so a Matter controller can tell them apart without relying on endpoint order.
// On/Off use standard Switches tags; Scene uses Switches::createCustomTag() with a label.
// Mirrors the tag usage from the esp-matter generic_switch example:
// https://github.com/espressif/esp-matter/tree/main/examples/generic_switch
// For gesture support (long-press, multi-press) see MatterEnhancedSmartButton.

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
// List of Matter Endpoints for this Node
// Three Generic Switch Endpoints - independent On, Off, and custom-labeled Scene buttons
MatterGenericSwitch ButtonOn;
MatterGenericSwitch ButtonOff;
MatterGenericSwitch ButtonScene;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// set your board pins here
const uint8_t buttonOnPin = 4;                   // On button GPIO — change to match your wiring
const uint8_t buttonOffPin = 5;                  // Off button GPIO — change to match your wiring
const uint8_t buttonScenePin = 2;                // Scene button GPIO — LED/strapping on some ESP32 boards; change to match your wiring
const uint8_t decommissionButtonPin = BOOT_PIN;  // hold this button for 5s to decommission

MatterButton buttonOn;
MatterButton buttonOff;
MatterButton buttonScene;
MatterButton decommissionButton;

static void handleButton(MatterButton &btn, MatterGenericSwitch &sw, const char *name) {
  matterButtonEvent_t ev;
  while ((ev = btn.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_PRESS) {
      Serial.printf("%s button pressed. Sending InitialPress to the Matter Controller!\r\n", name);
      sw.press();
    } else if (ev == MATTER_BUTTON_CLICK) {
      Serial.printf("%s button released. Sending ShortRelease to the Matter Controller!\r\n", name);
      sw.release();
    }
  }
}

void setup() {
  // Initialize the On/Off/Scene buttons and the dedicated decommissioning button
  buttonOn.begin(buttonOnPin);
  buttonOff.begin(buttonOffPin);
  buttonScene.begin(buttonScenePin);
  decommissionButton.begin(decommissionButtonPin);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize the Matter EndPoints
  // FEATURE_SIMPLE (default): momentary switch + short release for a single click
  ButtonOn.begin();
  ButtonOff.begin();
  ButtonScene.begin();

  // Tag each button so Matter controllers can tell them apart, since they share the same
  // Generic Switch device type. Must be called after begin() and before Matter.begin().
  // MatterTags (see MatterTags.h) provides named constants for the common Matter semantic
  // tag namespaces, so sketches don't need to hardcode namespace/tag numbers.
  ButtonOn.setTagList({MatterTags::Switches::On});
  ButtonOff.setTagList({MatterTags::Switches::Off});
  // Switches Custom tag with a user-visible label (string literal must outlive the endpoint)
  ButtonScene.setTagList({MatterTags::Switches::createCustomTag("Scene 1")});

  // Matter beginning - Last step, after all EndPoints are initialized
  matterSetExampleIdentity("Smart Buttons");
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

  // Three independent buttons are used to trigger events to the Matter Controller
  handleButton(buttonOn, ButtonOn, "On");
  handleButton(buttonOff, ButtonOff, "Off");
  handleButton(buttonScene, ButtonScene, "Scene 1");

  matterButtonEvent_t ev;
  while ((ev = decommissionButton.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Generic Switch Matter Accessories. They shall be commissioned again.");
      Matter.decommission();
    }
  }
}
