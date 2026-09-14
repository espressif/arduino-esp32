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
// Fan Endpoint - On/Off control + Speed Percent Control + Fan Modes
MatterFan Fan;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"


// set your board USER BUTTON pin here - used for toggling On/Off and decommission the Matter Node
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

// set your board Analog Pin here - used for changing the Fan speed
const uint8_t analogPin = A0;  // Analog Pin depends on each board

// set your board PWM Pin here - used for controlling the Fan speed (DC motor example)
// for this example, it will use the builtin board RGB LED to simulate the Fan DC motor using its brightness
#ifdef RGB_BUILTIN
const uint8_t dcMotorPin = RGB_BUILTIN;
#else
const uint8_t dcMotorPin = 2;  // Set your pin here if the board has no RGB_BUILTIN
#warning "Do not forget to set the RGB LED pin"
#endif

void fanDCMotorDrive(bool fanState, uint8_t speedPercent) {
  // drive the Fan DC motor
  if (fanState == false) {
    // turn off the Fan
#ifndef RGB_BUILTIN
    // after analogWrite(), it is necessary to set the GPIO to digital mode first
    pinMode(dcMotorPin, OUTPUT);
#endif
    digitalWrite(dcMotorPin, LOW);
  } else {
    // set the Fan speed
    uint8_t fanDCMotorPWM = map(speedPercent, 0, 100, 0, 255);
#ifdef RGB_BUILTIN
    rgbLedWrite(dcMotorPin, fanDCMotorPWM, fanDCMotorPWM, fanDCMotorPWM);
#else
    analogWrite(dcMotorPin, fanDCMotorPWM);
#endif
  }
}

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will toggle the Fan (On/Off) and decommission the Matter Node
  button.begin(buttonPin);
  // Initialize the Analog Pin A0 used to read input voltage and to set the Fan speed accordingly
  pinMode(analogPin, INPUT);
  analogReadResolution(10);  // 10 bits resolution reading 0..1023
  // Initialize the PWM output pin for a Fan DC motor
  pinMode(dcMotorPin, OUTPUT);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Boot: 0% speed, Off. Sequence is Off/High, so setOnOff(true) / FAN_MODE_ON store High.
  Fan.begin(0, MatterFan::FAN_MODE_OFF, MatterFan::FAN_MODE_SEQ_OFF_HIGH);

  // callback functions would control Fan motor
  // the Matter Controller will send new data whenever the User APP or Automation request

  // single feature callbacks take place before the generic (all features) callback
  // This callback will be executed whenever the speed percent matter attribute is updated
  Fan.onChangeSpeedPercent([](uint8_t speedPercent) {
    // setting speed to Zero, while the Fan is ON, shall turn the Fan OFF
    if (speedPercent == MatterFan::OFF_SPEED && Fan.getMode() != MatterFan::FAN_MODE_OFF) {
      // ATTR_UPDATE reports FanMode so the APP confirms Off. Cache stops re-entry.
      return Fan.setOnOff(false, Fan.ATTR_UPDATE);
    }
    // changing the speed to higher than Zero, while the Fan is OFF, shall turn the Fan ON
    if (speedPercent > MatterFan::OFF_SPEED && Fan.getMode() == MatterFan::FAN_MODE_OFF) {
      return Fan.setOnOff(true, Fan.ATTR_UPDATE);
    }
    // for other case, just return true
    return true;
  });

  // This callback will be executed whenever the fan mode matter attribute is updated
  // This will take action when user APP starts the Fan by changing the mode
  Fan.onChangeMode([](MatterFan::FanMode_t fanMode) {
    // when the Fan is turned ON using Mode Selection, while it is OFF, shall start it by setting the speed to 50%
    if (Fan.getSpeedPercent() == MatterFan::OFF_SPEED && fanMode != MatterFan::FAN_MODE_OFF) {
      Serial.printf("Fan set to %s mode -- speed percentage will go to 50%%\r\n", Fan.getFanModeString(fanMode));
      return Fan.setSpeedPercent(50, Fan.ATTR_UPDATE);
    }
    return true;
  });

  // Generic callback will be executed as soon as a single feature callback is done
  // In this example, it will just print status messages
  Fan.onChange([](MatterFan::FanMode_t fanMode, uint8_t speedPercent) {
    // just report state
    Serial.printf("Fan State: Mode %s | %u%% speed.\r\n", Fan.getFanModeString(fanMode), speedPercent);
    // drive the Fan DC motor
    fanDCMotorDrive(fanMode != MatterFan::FAN_MODE_OFF, speedPercent);
    // returns success
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_CLICK) {
      Fan.toggle();
      Serial.printf("User button released. Setting the Fan %s.\r\n", Fan > 0 ? "ON" : "OFF");
    } else if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning Fan Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  // checks Analog pin and adjust the speed only if it has changed
  static int lastRead = 0;
  // analog values (0..1023) / 103 => mapped into 10 steps (0..9)
  int anaVal = analogRead(analogPin) / 103;
  if (lastRead != anaVal) {
    // speed percent moves in steps of 10. Range is 10..100
    if (Fan.setSpeedPercent((anaVal + 1) * 10)) {
      lastRead = anaVal;
    }
  }
}
