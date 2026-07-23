// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Two Matter smart buttons — On and Off — sharing the same Generic Switch device type.
// The Descriptor cluster TagList attribute is used to tag each button (On/Off),
// so a Matter controller can tell them apart without relying on endpoint order alone.
// Mirrors the tag usage from the esp-matter generic_switch example:
// https://github.com/espressif/esp-matter/tree/main/examples/generic_switch
// For gesture support (long-press, multi-press) see MatterEnhancedSmartButton.

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
#endif

// List of Matter Endpoints for this Node
// Two Generic Switch Endpoints - work as independent On/Off smart buttons
MatterGenericSwitch ButtonOn;
MatterGenericSwitch ButtonOff;

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif

// set your board pins here
const uint8_t buttonOnPin = 4;                   // On button GPIO — change to match your wiring
const uint8_t buttonOffPin = 5;                  // Off button GPIO — change to match your wiring
const uint8_t decommissionButtonPin = BOOT_PIN;  // hold this button for 5s to decommission

// Semantic tags for the Descriptor cluster TagList attribute.
// Please refer to https://github.com/CHIP-Specifications/connectedhomeip-spec/blob/master/src/namespaces
constexpr const uint8_t kNamespaceSwitches = 0x43;
constexpr const uint8_t kTagSwitchOn = 0;   // Switches Namespace: 0x43, tag 0 (On)
constexpr const uint8_t kTagSwitchOff = 1;  // Switches Namespace: 0x43, tag 1 (Off)

// Tag each button with what it does: ButtonOn = On, ButtonOff = Off
MatterTag onTagList[] = {
  {kNamespaceSwitches, kTagSwitchOn},
};
MatterTag offTagList[] = {
  {kNamespaceSwitches, kTagSwitchOff},
};

// Per-button debouncing state
struct ButtonState {
  uint32_t timeStamp = 0;  // debouncing control
  bool pressed = false;    // false = released | true = pressed
};
ButtonState onButtonState;
ButtonState offButtonState;

const uint32_t debounceTime = 250;             // button debouncing time (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the decommission button pressed for 5s, or longer
uint32_t decommissionStart = 0;                // 0 = decommission button not currently held

static void handleButton(uint8_t pin, ButtonState &state, MatterGenericSwitch &sw, const char *name) {
  // deals with button debouncing
  if (digitalRead(pin) == LOW && !state.pressed) {
    state.timeStamp = millis();  // record the time while the button is pressed.
    state.pressed = true;        // pressed.
    Serial.printf("%s button pressed. Sending InitialPress to the Matter Controller!\r\n", name);
    // Matter Controller will receive an InitialPress event and, if programmed, it will trigger an action
    sw.press();
  }

  uint32_t timeDiff = millis() - state.timeStamp;
  if (state.pressed && timeDiff > debounceTime && digitalRead(pin) == HIGH) {
    state.pressed = false;  // released
    // button is released - send a ShortRelease event to the Matter Controller
    Serial.printf("%s button released. Sending ShortRelease to the Matter Controller!\r\n", name);
    // Matter Controller will receive an event and, if programmed, it will trigger an action
    sw.release();
  }
}

void setup() {
  // Initialize the On/Off buttons and the dedicated decommissioning button
  pinMode(buttonOnPin, INPUT_PULLUP);
  pinMode(buttonOffPin, INPUT_PULLUP);
  pinMode(decommissionButtonPin, INPUT_PULLUP);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\r\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
#endif

  // Initialize the Matter EndPoints
  // FEATURE_SIMPLE (default): momentary switch + short release for a single click
  ButtonOn.begin();
  ButtonOff.begin();

  // Tag each button so Matter controllers can tell them apart, since both share the same
  // Generic Switch device type. Must be called after begin().
  ButtonOn.setTagList(onTagList, 1);
  ButtonOff.setTagList(offTagList, 1);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }
}

void loop() {
  // Check Matter Accessory Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Generic Switch Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }

  // Two independent buttons are used to trigger events to the Matter Controller
  handleButton(buttonOnPin, onButtonState, ButtonOn, "On");
  handleButton(buttonOffPin, offButtonState, ButtonOff, "Off");

  // A dedicated button is kept pressed for longer than 5 seconds in order to decommission matter node
  if (digitalRead(decommissionButtonPin) == LOW) {
    if (decommissionStart == 0) {
      decommissionStart = millis();
    } else if (millis() - decommissionStart > decommissioningTimeout) {
      Serial.println("Decommissioning the Generic Switch Matter Accessories. They shall be commissioned again.");
      Matter.decommission();
      decommissionStart = millis();  // avoid running decommissioning again, reboot takes a second or so
    }
  } else {
    decommissionStart = 0;  // button released - reset the hold timer
  }
}
