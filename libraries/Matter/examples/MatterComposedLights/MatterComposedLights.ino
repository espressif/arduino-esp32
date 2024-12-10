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
// There will be 3 On/Off Light Endpoints in the same Node
MatterOnOffLight Light1;
MatterDimmableLight Light2;
MatterColorLight Light3;

// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

// set your board USER BUTTON pin here -  USED to decommission the Matter Node
#ifdef BOOT_PIN
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
#else
const uint8_t buttonPin = 0;  // Set your button pin here.
#warning "Do not forget to set the USER BUTTON pin"
#endif

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
  pinMode(buttonPin, INPUT_PULLUP);

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
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);

  // Initialize all 3 Matter EndPoints
  Light1.begin();
  Light2.begin();
  Light3.begin();
  Light1.onChangeOnOff(setLightOnOff1);
  Light2.onChangeOnOff(setLightOnOff2);
  Light3.onChangeOnOff(setLightOnOff3);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
}

// Button control
uint32_t button_time_stamp = 0;                 // debouncing control
bool button_state = false;                      // false = released | true = pressed
const uint32_t decommissioningTimeout = 10000;  // keep the button pressed for 10s to decommission the light

void loop() {
  // Check Matter Light Commissioning state
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Light Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
  }

  //displays the Light state every 5 seconds
  Serial.println("======================");
  Serial.printf("Matter Light #1 is %s\r\n", Light1.getOnOff() ? "ON" : "OFF");
  Serial.printf("Matter Light #2 is %s\r\n", Light2.getOnOff() ? "ON" : "OFF");
  Serial.printf("Matter Light #3 is %s\r\n", Light3.getOnOff() ? "ON" : "OFF");

  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  // Onboard User Button is used to decommission matter fabric
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout && digitalRead(buttonPin) == HIGH) {
    button_state = false;  // released
    // Factory reset is triggered if the button is pressed longer than 10 seconds
    if (time_diff > decommissioningTimeout) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  delay(5000);
}
