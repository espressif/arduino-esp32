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
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi.h / WiFi.begin() only when this build has no CHIPoBLE (CONFIG_ENABLE_CHIPOBLE=n). Hub-delivered Wi-Fi still uses CHIP's stack.
#include <WiFi.h>
#endif
#include <Preferences.h>

// List of Matter Endpoints for this Node
// Color Temperature CW/WW Light Endpoint
MatterColorTemperatureLight CW_WW_Light;

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
// Wi-Fi is manually set and started
const char *ssid = "your-ssid";          // Change this to your Wi-Fi SSID
const char *password = "your-password";  // Change this to your Wi-Fi password
#endif

// it will keep last OnOff & Brightness state stored, using Preferences
Preferences matterPref;
const char *onOffPrefKey = "OnOff";
const char *brightnessPrefKey = "Brightness";
const char *temperaturePrefKey = "Temperature";

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

// Set the RGB LED Light based on the current state of the Color Temperature Light
bool setLightState(bool state, uint8_t brightness, uint16_t temperature_Mireds) {

  if (state) {
#ifdef RGB_BUILTIN
    espRgbColor_t rgb_ct = espCTToRgbColor(temperature_Mireds);
    // simple intensity correction
    float brightnessPercent = (float)brightness / MatterColorTemperatureLight::MAX_BRIGHTNESS;
    rgb_ct.r = brightnessPercent * rgb_ct.r;
    rgb_ct.g = brightnessPercent * rgb_ct.g;
    rgb_ct.b = brightnessPercent * rgb_ct.b;
    // set the RGB LED
    rgbLedWrite(ledPin, rgb_ct.r, rgb_ct.g, rgb_ct.b);
#else
    // No Color RGB LED, just use the brightness to control the LED
    analogWrite(ledPin, brightness);
#endif
  } else {
#ifndef RGB_BUILTIN
    // after analogWrite(), it is necessary to set the GPIO to digital mode first
    pinMode(ledPin, OUTPUT);
#endif
    digitalWrite(ledPin, LOW);
  }
  // store last Brightness and OnOff state for when the Light is restarted / power goes off
  matterPref.putUChar(brightnessPrefKey, brightness);
  matterPref.putBool(onOffPrefKey, state);
  matterPref.putUShort(temperaturePrefKey, temperature_Mireds);
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
  // We start by connecting to a Wi-Fi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  // Manually connect to Wi-Fi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\r\nWi-Fi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
#endif

  // Initialize Matter EndPoint
  matterPref.begin("MatterPrefs", false);
  // default OnOff state is ON if not stored before
  bool lastOnOffState = matterPref.getBool(onOffPrefKey, true);
  // default brightness ~= 6% (15/255)
  uint8_t lastBrightness = matterPref.getUChar(brightnessPrefKey, 15);
  // default temperature ~= 454 Mireds (Warm White)
  uint16_t lastTemperature = matterPref.getUShort(temperaturePrefKey, WARM_WHITE_COLOR_TEMPERATURE.ctMireds);
  CW_WW_Light.begin(lastOnOffState, lastBrightness, lastTemperature);
  // set the callback function to handle the Light state change
  CW_WW_Light.onChange(setLightState);

  // lambda functions are used to set the attribute change callbacks
  CW_WW_Light.onChangeOnOff([](bool state) {
    Serial.printf("Light OnOff changed to %s\r\n", state ? "ON" : "OFF");
    return true;
  });
  CW_WW_Light.onChangeBrightness([](uint8_t level) {
    Serial.printf("Light Brightness changed to %u\r\n", level);
    return true;
  });
  CW_WW_Light.onChangeColorTemperature([](uint16_t temperature) {
    Serial.printf("Light Color Temperature changed to %u\r\n", temperature);
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
  Serial.printf(
    "Initial state: %s | brightness: %u | Color Temperature: %u mireds \r\n", CW_WW_Light ? "ON" : "OFF", CW_WW_Light.getBrightness(),
    CW_WW_Light.getColorTemperature()
  );
  CW_WW_Light.updateAccessory();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_CLICK) {
      Serial.println("User button released. Toggling Light!");
      CW_WW_Light.toggle();
    } else if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      CW_WW_Light = false;
      Matter.decommission();
    }
  }
}
