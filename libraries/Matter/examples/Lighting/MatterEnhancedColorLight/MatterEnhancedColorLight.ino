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

// ESP32-C3/H2/C5: if the node never commissions (usual low-heap symptom), uncomment this.
// The default 8 KB loop stack is reserved from the heap before setup();
// 4 KB is enough for this sketch and frees 4 KB for CHIPoBLE + Wi-Fi.
// SET_LOOP_TASK_STACK_SIZE(4 * 1024);

// List of Matter Endpoints for this Node
// Enhanced Color Light Endpoint
MatterEnhancedColorLight EnhancedColorLight;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"


// It will use HSV color to control all Matter Attribute Changes
HsvColor_t currentHSVColor = {0, 0, 0};

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

// Set the RGB LED Light based on the current state of the Enhanced Color Light
bool setLightState(bool state, espHsvColor_t colorHSV, uint8_t brighteness, uint16_t temperature_Mireds) {

  if (state) {
#ifdef RGB_BUILTIN
    // currentHSVColor keeps final color result
    espRgbColor_t rgbColor = espHsvColorToRgbColor(currentHSVColor);
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
  matterPref.putUInt(hsvColorPrefKey, currentHSVColor.h << 16 | currentHSVColor.s << 8 | currentHSVColor.v);
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
  // default HSV color is (21, 216, 25) - Warm White Color at 10% intensity
  uint32_t prefHsvColor = matterPref.getUInt(hsvColorPrefKey, 21 << 16 | 216 << 8 | 25);
  currentHSVColor = {uint8_t(prefHsvColor >> 16), uint8_t(prefHsvColor >> 8), uint8_t(prefHsvColor)};
  EnhancedColorLight.begin(lastOnOffState, currentHSVColor);
  // set the callback function to handle the Light state change
  EnhancedColorLight.onChange(setLightState);

  // lambda functions are used to set the attribute change callbacks
  EnhancedColorLight.onChangeOnOff([](bool state) {
    Serial.printf("Light OnOff changed to %s\r\n", state ? "ON" : "OFF");
    return true;
  });
  EnhancedColorLight.onChangeColorTemperature([](uint16_t colorTemperature) {
    Serial.printf("Light Color Temperature changed to %u\r\n", colorTemperature);
    // get correspondent Hue and Saturation of the color temperature
    HsvColor_t hsvTemperature = espRgbColorToHsvColor(espCTToRgbColor(colorTemperature));
    // keep previous the brightness and just change the Hue and Saturation
    currentHSVColor.h = hsvTemperature.h;
    currentHSVColor.s = hsvTemperature.s;
    return true;
  });
  EnhancedColorLight.onChangeBrightness([](uint8_t brightness) {
    Serial.printf("Light brightness changed to %u\r\n", brightness);
    // change current brightness (HSV value)
    currentHSVColor.v = brightness;
    return true;
  });
  EnhancedColorLight.onChangeColorHSV([](HsvColor_t hsvColor) {
    Serial.printf("Light HSV Color changed to (%u,%u,%u)\r\n", hsvColor.h, hsvColor.s, hsvColor.v);
    // keep the current brightness and just change Hue and Saturation
    currentHSVColor.h = hsvColor.h;
    currentHSVColor.s = hsvColor.s;
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
  Serial.printf(
    "Initial state: %s | RGB Color: (%u,%u,%u) \r\n", EnhancedColorLight ? "ON" : "OFF", EnhancedColorLight.getColorRGB().r,
    EnhancedColorLight.getColorRGB().g, EnhancedColorLight.getColorRGB().b
  );
  EnhancedColorLight.updateAccessory();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_CLICK) {
      Serial.println("User button released. Toggling Light!");
      EnhancedColorLight.toggle();
    } else if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      EnhancedColorLight = false;
      Matter.decommission();
    }
  }
}
