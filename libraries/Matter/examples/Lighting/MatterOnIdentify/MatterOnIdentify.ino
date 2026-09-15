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

/*
 * Matter On/Off Light that demonstrates the Identify cluster.
 * onIdentify(bool) still starts/stops feedback. getIdentifyRequest() tells
 * the sketch which command/effect it was so Blink, Breathe, Okay, and
 * IdentifyTime can look different.
 */

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
// List of Matter Endpoints for this Node
// Single On/Off Light Endpoint - at least one per node
MatterOnOffLight OnOffLight;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"


// Light GPIO that can be controlled by Matter APP
#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#endif

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

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
  button.begin(buttonPin);
  // Initialize the LED GPIO
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize at least one Matter EndPoint
  OnOffLight.begin();

  // On Identify Callback - Blink the LED
  OnOffLight.onIdentify(onIdentifyLightCallback);

  // Associate a callback to the Matter Controller
  OnOffLight.onChange(onOffLightCallback);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

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

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  delay(10);
}
