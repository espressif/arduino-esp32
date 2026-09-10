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

/*
 * Matter On/Off Light that demonstrates the Identify cluster.
 * onIdentify(bool) still starts/stops feedback. getIdentifyRequest() tells
 * the sketch which command/effect it was so Blink, Breathe, Okay, and
 * IdentifyTime can look different.
 */

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi.h / WiFi.begin() only when this build has no CHIPoBLE (CONFIG_ENABLE_CHIPOBLE=n). Hub-delivered Wi-Fi still uses CHIP's stack.
#include <WiFi.h>
#endif

// List of Matter Endpoints for this Node
// Single On/Off Light Endpoint - at least one per node
MatterOnOffLight OnOffLight;

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
// Wi-Fi is manually set and started
const char *ssid = "your-ssid";          // Change this to your Wi-Fi SSID
const char *password = "your-password";  // Change this to your Wi-Fi password
#endif

// Light GPIO that can be controlled by Matter APP
#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#endif

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control - decommission the Matter Node
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Identify Flag and blink time - Blink the LED
const uint8_t identifyLedPin = ledPin;  // uses the same LED as the Light - change if needed
volatile bool identifyFlag = false;     // Flag to start the Blink when in Identify state
bool identifyBlink = false;             // Blink state when in Identify state
uint32_t identifyPeriodMs = 500;        // on/off half-period
uint32_t identifyUntilMs = 0;           // 0 = run until onIdentify(false); else millis() deadline
uint32_t identifyLastToggleMs = 0;

const char *identifyEffectName(const MatterIdentifyRequest &req) {
  if (!req.valid) {
    return "None";
  }
  // START/STOP pass CHIP's leftover/default effect id (often Blink). Not the session type.
  if (!req.fromTriggerEffect) {
    return "IdentifyTime";
  }
  switch (req.effectId) {
    case MatterIdentifyRequest::BLINK:          return "Blink";
    case MatterIdentifyRequest::BREATHE:        return "Breathe";
    case MatterIdentifyRequest::OKAY:           return "Okay";
    case MatterIdentifyRequest::CHANNEL_CHANGE: return "ChannelChange";
    case MatterIdentifyRequest::FINISH:         return "Finish";
    case MatterIdentifyRequest::STOP:           return "Stop";
    default:                                    return "Unknown";
  }
}

void applyIdentifyLed(bool on) {
#ifdef LED_BUILTIN
  uint8_t brightness = on ? 32 : 0;
  rgbLedWrite(identifyLedPin, brightness, 0, 0);
#else
  digitalWrite(identifyLedPin, on ? HIGH : LOW);
#endif
}

void stopIdentifyFeedback() {
  identifyFlag = false;
  identifyUntilMs = 0;
  // force returning to the original state by toggling the light twice
  OnOffLight.toggle();
  OnOffLight.toggle();
}

// Matter Protocol Endpoint (On/OFF Light) Callback
bool onOffLightCallback(bool state) {
  digitalWrite(ledPin, state ? HIGH : LOW);
  // This callback must return the success state to Matter core
  return true;
}

// Identify: blink the LED GPIO (same pin as the On/Off light).
// Use getIdentifyRequest() for TriggerEffect vs IdentifyTime and the effect id.
bool onIdentifyLightCallback(bool identifyIsActive) {
  const MatterIdentifyRequest req = OnOffLight.getIdentifyRequest();
  Serial.printf(
    "Identify %s (%s effect=%s 0x%02x variant=%u)\r\n", identifyIsActive ? "Active" : "Inactive", req.fromTriggerEffect ? "TriggerEffect" : "IdentifyTime",
    identifyEffectName(req), req.effectId, req.effectVariant
  );

  if (!identifyIsActive) {
    stopIdentifyFeedback();
    return true;
  }

  identifyFlag = true;
  identifyBlink = true;
  identifyLastToggleMs = millis();

  if (!req.fromTriggerEffect) {
    // Identify / IdentifyTime: blink until STOP
    identifyPeriodMs = 500;
    identifyUntilMs = 0;
    return true;
  }

  // TriggerEffect has no later STOP. Time the animation here.
  switch (req.effectId) {
    case MatterIdentifyRequest::BLINK:
      identifyPeriodMs = 200;
      identifyUntilMs = millis() + 2000;
      break;
    case MatterIdentifyRequest::BREATHE:
      identifyPeriodMs = 800;
      identifyUntilMs = millis() + 15000;
      break;
    case MatterIdentifyRequest::OKAY:
      // One short flash
      identifyPeriodMs = 400;
      identifyUntilMs = millis() + 400;
      break;
    case MatterIdentifyRequest::CHANNEL_CHANGE:
      identifyPeriodMs = 150;
      identifyUntilMs = millis() + 8000;
      break;
    default:
      identifyPeriodMs = 500;
      identifyUntilMs = millis() + 2000;
      break;
  }
  applyIdentifyLed(true);
  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the LED GPIO
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  // Manually connect to Wi-Fi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
#endif

  // Initialize at least one Matter EndPoint
  OnOffLight.begin();

  // On Identify Callback - Blink the LED
  OnOffLight.onIdentify(onIdentifyLightCallback);

  // Associate a callback to the Matter Controller
  OnOffLight.onChange(onOffLightCallback);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();

  // Check Matter Accessory Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter On/Off Light commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }
}

void loop() {
  const uint32_t now = millis();

  if (identifyFlag) {
    if (identifyUntilMs != 0 && (int32_t)(now - identifyUntilMs) >= 0) {
      stopIdentifyFeedback();
    } else if ((now - identifyLastToggleMs) >= identifyPeriodMs) {
      identifyLastToggleMs = now;
      identifyBlink = !identifyBlink;
      applyIdentifyLed(identifyBlink);
    }
  }

  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  if (digitalRead(buttonPin) == HIGH && button_state) {
    button_state = false;  // released
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissining again, reboot takes a second or so
  }

  delay(10);
}
