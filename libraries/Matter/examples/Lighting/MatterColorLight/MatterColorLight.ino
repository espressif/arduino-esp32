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
#include <Preferences.h>

// List of Matter Endpoints for this Node
// Color Light Endpoint
MatterColorLight ColorLight;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// it will keep last OnOff & HSV Color state stored, using Preferences
Preferences matterPref;
const char *onOffPrefKey = "OnOff";
const char *hsvColorPrefKey = "HSV";

// set your board RGB LED pin here
#ifdef RGB_BUILTIN
const uint8_t ledPin = RGB_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if the board has no RGB_BUILTIN
#warning "Do not forget to set the RGB LED pin"
#endif

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// Set the RGB LED Light based on the current state of the Color Light
bool setLightState(bool state, espHsvColor_t colorHSV) {

  if (state) {
#ifdef RGB_BUILTIN
    espRgbColor_t rgbColor = espHsvColorToRgbColor(colorHSV);
    // set the RGB LED
    rgbLedWrite(ledPin, rgbColor.r, rgbColor.g, rgbColor.b);
#else
    // No Color RGB LED, just use the HSV value (brightness) to control the LED
    analogWrite(ledPin, colorHSV.v);
#endif
  } else {
#ifndef RGB_BUILTIN
    // after analogWrite(), it is necessary to set the GPIO to digital mode first
    pinMode(ledPin, OUTPUT);
#endif
    digitalWrite(ledPin, LOW);
  }
  // store last HSV Color and OnOff state for when the Light is restarted / power goes off
  matterPref.putBool(onOffPrefKey, state);
  matterPref.putUInt(hsvColorPrefKey, colorHSV.h << 16 | colorHSV.s << 8 | colorHSV.v);
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as a toggle switch
  button.begin(buttonPin);
  // Initialize the LED (light) GPIO and Matter End Point
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Initialize Matter EndPoint
  matterPref.begin("MatterPrefs", false);
  // default OnOff state is ON if not stored before
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, true);
  // default HSV color is blue HSV(169, 254, 254)
  uint32_t prefHsvColor = matterPref.getUInt(hsvColorPrefKey, 169 << 16 | 254 << 8 | 254);
  espHsvColor_t lastHsvColor = {uint8_t(prefHsvColor >> 16), uint8_t(prefHsvColor >> 8), uint8_t(prefHsvColor)};
  ColorLight.begin(lastOnOffState, lastHsvColor);
  // set the callback function to handle the Light state change
  ColorLight.onChange(setLightState);

  // lambda functions are used to set the attribute change callbacks
  ColorLight.onChangeOnOff([](bool state) {
    Serial.printf("Light OnOff changed to %s\r\n", state ? "ON" : "OFF");
    return true;
  });
  ColorLight.onChangeColorHSV([](HsvColor_t hsvColor) {
    Serial.printf("Light HSV Color changed to (%u,%u,%u)\r\n", hsvColor.h, hsvColor.s, hsvColor.v);
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
  Serial.printf(
    "Initial state: %s | RGB Color: (%u,%u,%u) \r\n", ColorLight ? "ON" : "OFF", ColorLight.getColorRGB().r, ColorLight.getColorRGB().g,
    ColorLight.getColorRGB().b
  );
  ColorLight.updateAccessory();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_CLICK) {
      Serial.println("User button released. Toggling Light!");
      ColorLight.toggle();
    } else if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      ColorLight = false;
      Matter.decommission();
    }
  }
}
