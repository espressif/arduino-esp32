// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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

// Matter Manager
#include <Matter.h>
#include <WiFi.h>

// List of Matter Endpoints for this Node
// Fan Endpoint - On/Off control + Speed Percent Control + Fan Modes
MatterFan Fan;

// set your board USER BUTTON pin here - used for toggling on/off
const uint8_t buttonPin = 0;  // Set your pin here. Using BOOT Button. C6/C3 use GPIO9.

// set your board Analog Pin here - used for changing the Fan speed
const uint8_t analogPin = A0; // Analog Pin depends on each board

// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will toggle the Fan (on/off)
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the Analog Pin A0 used to read input voltage and to set the Fan speed accordingly
  pinMode(analogPin, INPUT);
  analogReadResolution(10); // 10 bits resolution reading 0..1023
  
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  // enable IPv6
  WiFi.enableIPv6(true);
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

  // On Boot or Reset, Fan is set at 0% speed, OFF, changing between OFF, ON, SMART and HIGH
  Fan.begin(0, MatterFan::FAN_MODE_OFF, MatterFan::FAN_MODE_SEQ_OFF_HIGH);

  // callback functions would control Fan motor
  // the Matter Controller will send new data whenver the User APP or Automation request 
  
  // single feature callbacks take place before the generic (all features) callback
  // This callback will be executed whenever the speed percent matter attribute is updated
  Fan.onChangeSpeedPercent([](uint8_t speedPercent) {
    // setting speed to Zero, while the Fan is ON, shall turn the Fan off
    if (speedPercent == 0 && Fan.getMode() != MatterFan::FAN_MODE_OFF) {
      // ATTR_SET do not update the attribute, just set it to avoid inifinite loop
      return Fan.setOnOff(false, Fan.ATTR_SET);
    }
    // changing the speed to higher than Zero, while the Fan is OFF, shall turn the Fan on
    if (speedPercent > 0 && Fan.getMode() == MatterFan::FAN_MODE_OFF) {
      // ATTR_SET do not update the attribute, just set it to avoid inifinite loop
      return Fan.setOnOff(true, Fan.ATTR_SET);
    }
    // for other case, just return true 
    return true;
  });
  
  // This callback will be executed whenever the fan mode matter attribute is updated
  // This will take action when user APP starts the Fan by changing the mode
  Fan.onChangeMode([](MatterFan::FanMode_t fanMode) {
    // when the Fan is turned on using Mode Selection, while it is OFF, shall start it by setting the speed to 50%
    if (Fan.getSpeedPercent() == 0 && fanMode != MatterFan::FAN_MODE_OFF) {
      Serial.println("Starting Smart Mode -- set speed to 50%");
      // ATTR_SET do not update the attribute, just set it to avoid inifinite loop
      return Fan.setSpeedPercent(50, Fan.ATTR_SET);
    }
    return true;
  });
  
  // Generic callback will be executed as soon as a single feature callback is done
  // In this example, it will just print status messages
  Fan.onChange([](MatterFan::FanMode_t fanMode, uint8_t speedPercent) {
    // just report state
    Serial.printf("Fan State: Mode %s | %d%% speed.\r\n", Fan.getFanModeString(fanMode), speedPercent);
    // returns success
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
  }
}

// Builtin Button control
uint32_t button_time_stamp = 0;                 // debouncing control
bool button_state = false;                      // false = released | true = pressed
const uint32_t debouceTime = 250;               // button debouncing time (ms)
const uint32_t decommissioningTimeout = 10000;  // keep the button pressed for 10s to decommission the Matter Fabric

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
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
  }

  // A builtin button is used to trigger and send a command to the Matter Controller
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  // Onboard User Button is used as a smart button or to decommission it
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > debouceTime && digitalRead(buttonPin) == HIGH) {
    button_state = false;  // released
    // builtin button is released - send a click event to the Matter Controller
    Fan.toggle();
    Serial.printf("User button released. Setting the Fan %s.\r\n", Fan > 0 ? "ON" : "OFF");

    // Factory reset is triggered if the button is pressed longer than 10 seconds
    if (time_diff > decommissioningTimeout) {
      Serial.println("Decommissioning the Generic Switch Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  // checks Analog pin and adjust the speed only if it has changed
  static int lastRead = 0;
  // analog values (0..1023) / 110 => mapped into 10 steps (0..9)
  int anaVal = analogRead(analogPin) / 110;
  if (lastRead != anaVal) { 
    // speed percent moves in steps of 10. Range is 10..100
    if (Fan.setSpeedPercent((anaVal + 1) * 10)) {
      lastRead = anaVal;
    }
  }
}
