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

// Matter Manager
#include <Arduino.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
#endif

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif

// List of Matter Endpoints for this Node
// Water Valve Endpoint
MatterWaterValve WaterValve;

// set your board LED pin here
#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#warning "Do not forget to set the LED pin"
#endif

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t debouceTime = 250;              // button debouncing time (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Open the valve for 10 seconds whenever it is commanded open - replace with your own irrigation/valve timing
const uint32_t openDurationSeconds = 10;

// Matter Protocol Endpoint Callbacks - replace with your real valve actuator control (relay, solenoid driver, etc.)
bool onValveOpen() {
  Serial.println("User Callback :: Opening the water valve");
  digitalWrite(ledPin, HIGH);
  // This callback must return the success state to Matter core
  return true;
}

bool onValveClose() {
  Serial.println("User Callback :: Closing the water valve");
  digitalWrite(ledPin, LOW);
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as an open/close toggle
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the LED GPIO used to represent the valve state
  pinMode(ledPin, OUTPUT);

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

  // Initialize the Matter Water Valve EndPoint - starts closed, no default open duration configured
  WaterValve.begin();
  WaterValve.onOpen(onValveOpen);
  WaterValve.onClose(onValveClose);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
    Serial.printf("Initial state: %s\r\n", WaterValve.isOpen() ? "OPEN" : "CLOSED");
  }
}

void loop() {
  // Check Matter Water Valve Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Water Valve Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }

  // Log the remaining duration of a timed open operation as it counts down - this is driven
  // automatically by the Matter stack, the sketch just reads it back for display purposes.
  static uint32_t remainingDurationPrev = 0;
  uint32_t remainingDuration = WaterValve.getRemainingDuration();
  if (remainingDurationPrev != remainingDuration) {
    if (remainingDuration > 0) {
      Serial.printf("Water valve remaining duration: %lu s\r\n", remainingDuration);
    }
    remainingDurationPrev = remainingDuration;
  }

  // A button is used to manually toggle the valve open/closed (open() uses a fixed duration, for
  // example purposes - a real irrigation valve could instead call open() with no duration and stay
  // open until explicitly closed)
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > debouceTime && digitalRead(buttonPin) == HIGH) {
    button_state = false;  // released
    // Toggle button is released - toggle the valve
    if (WaterValve.isOpen()) {
      Serial.println("User button released. Closing the water valve!");
      WaterValve.close();  // Matter Controller also can see the change
    } else {
      Serial.println("User button released. Opening the water valve!");
      WaterValve.open(openDurationSeconds);  // Matter Controller also can see the change
    }
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Water Valve Matter Accessory. It shall be commissioned again.");
    WaterValve.close();  // close the valve
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissioning again, reboot takes a second or so
  }
}
