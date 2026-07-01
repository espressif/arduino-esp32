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
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
#endif
#include <Preferences.h>

// List of Matter Endpoints for this Node
// Window Covering Endpoint
MatterWindowCovering WindowBlinds;

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif

// it will keep last Lift & Tilt state stored, using Preferences
Preferences matterPref;
const char *liftPercentPrefKey = "LiftPercent";
const char *tiltPercentPrefKey = "TiltPercent";

// set your board USER BUTTON pin here
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t debounceTime = 250;             // button debouncing time (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Local motor calibration (not exposed as Matter attributes in ESP-Matter 1.5)
// Lift limits in centimeters (physical position at open/closed ends)
// Matter percent: 0 = open at open limit, 100 = closed at closed limit
const MatterWindowCovering::PositionCalibration LIFT_CALIBRATION = {.open = 0, .closed = 200};

// Tilt limits (absolute values for conversion, not physical units)
// Tilt is a rotation, not a linear measurement
const MatterWindowCovering::PositionCalibration TILT_CALIBRATION = {.open = 0, .closed = 90};

// Current window covering state
// These will be initialized in setup() based on motor calibration and saved percentages
// Matter percent: 0 = fully open, 100 = fully closed
uint16_t currentLift = LIFT_CALIBRATION.open;  // Lift position in cm
uint8_t currentLiftPercent = 0;
uint8_t currentTiltPercent = 0;  // Tilt rotation percentage (0-100%)

// Visualize window covering position using RGB LED
// Brightness follows openness (inverted Matter percent: more open = brighter)
#ifdef RGB_BUILTIN
const uint8_t ledPin = RGB_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined RGB_BUILTIN
#warning "Do not forget to set the RGB LED pin"
#endif

void visualizeWindowBlinds(uint8_t liftPercent, uint8_t tiltPercent) {
#ifdef RGB_BUILTIN
  // Use RGB LED to visualize lift position (brightness) and tilt (color shift)
  // Brighter when more open (lower Matter percent)
  uint8_t openness = 100 - liftPercent;
  float brightness = (float)openness / 100.0;  // 0.0 to 1.0
  // Tilt affects color: 0% = red, 100% = blue
  uint8_t red = (uint8_t)(map(tiltPercent, 0, 100, 255, 0) * brightness);
  uint8_t blue = (uint8_t)(map(tiltPercent, 0, 100, 0, 255) * brightness);
  uint8_t green = 0;
  rgbLedWrite(ledPin, red, green, blue);
#else
  // For non-RGB boards, just use brightness
  uint8_t openness = 100 - liftPercent;
  uint8_t brightnessValue = map(openness, 0, 100, 0, 255);
  analogWrite(ledPin, brightnessValue);
#endif
}

// Convert Matter lift percent (0 = open, 100 = closed) to local motor units (cm)
static uint16_t liftPercentToCm(uint8_t liftPercent) {
  const auto cal = WindowBlinds.getLiftCalibration();
  // Linear interpolation: 0% = open limit, 100% = closed limit
  if (cal.open < cal.closed) {
    return cal.open + ((cal.closed - cal.open) * liftPercent) / 100;
  }
  return cal.open - ((cal.open - cal.closed) * liftPercent) / 100;
}

// Window Covering Callbacks
bool fullOpen() {
  // This is where you would trigger your motor to go to full open state
  // For simulation, we update instantly
  currentLift = WindowBlinds.getLiftCalibration().open;
  currentLiftPercent = 0;
  Serial.printf("Opening window covering to full open (position: %u cm)\r\n", currentLift);

  // Update CurrentPosition to reflect actual position (Percent100ths on the Matter cluster)
  WindowBlinds.setCurrentLiftPercent100ths(0);

  // Set operational status to STALL when movement is complete
  WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);

  // Store state
  matterPref.putUChar(liftPercentPrefKey, currentLiftPercent);

  return true;
}

bool fullClose() {
  // This is where you would trigger your motor to go to full close state
  // For simulation, we update instantly
  currentLift = WindowBlinds.getLiftCalibration().closed;
  currentLiftPercent = 100;
  Serial.printf("Closing window covering to full close (position: %u cm)\r\n", currentLift);

  // Update CurrentPosition to reflect actual position (Percent100ths on the Matter cluster)
  WindowBlinds.setCurrentLiftPercent100ths(10000);

  // Set operational status to STALL when movement is complete
  WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);

  // Store state
  matterPref.putUChar(liftPercentPrefKey, currentLiftPercent);

  return true;
}

bool goToLiftPercentage(uint8_t liftPercent) {
  // update Lift operational state
  if (liftPercent > currentLiftPercent) {
    // Set operational status to CLOSE
    WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::MOVING_DOWN_OR_CLOSE);
  } else if (liftPercent < currentLiftPercent) {
    // Set operational status to OPEN
    WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::MOVING_UP_OR_OPEN);
  }

  // This is where you would trigger your motor to go towards liftPercent
  // For simulation, we update instantly
  currentLift = liftPercentToCm(liftPercent);
  currentLiftPercent = liftPercent;
  Serial.printf("Moving lift to %u%% (position: %u cm)\r\n", currentLiftPercent, currentLift);

  // Update CurrentPosition to reflect actual position (Percent100ths on the Matter cluster)
  WindowBlinds.setCurrentLiftPercent100ths(liftPercent * 100);

  // Set operational status to STALL when movement is complete
  WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);

  // Store state
  matterPref.putUChar(liftPercentPrefKey, currentLiftPercent);

  return true;
}

bool goToTiltPercentage(uint8_t tiltPercent) {
  // update Tilt operational state
  if (tiltPercent > currentTiltPercent) {
    // Set operational status to CLOSE
    WindowBlinds.setOperationalState(MatterWindowCovering::TILT, MatterWindowCovering::MOVING_DOWN_OR_CLOSE);
  } else if (tiltPercent < currentTiltPercent) {
    // Set operational status to OPEN
    WindowBlinds.setOperationalState(MatterWindowCovering::TILT, MatterWindowCovering::MOVING_UP_OR_OPEN);
  }

  // This is where you would trigger your motor to rotate the shade to tiltPercent
  // For simulation, we update instantly
  currentTiltPercent = tiltPercent;
  Serial.printf("Rotating tilt to %u%%\r\n", currentTiltPercent);

  // Update CurrentPosition to reflect actual position (Percent100ths on the Matter cluster)
  WindowBlinds.setCurrentTiltPercent100ths(tiltPercent * 100);

  // Set operational status to STALL when movement is complete
  WindowBlinds.setOperationalState(MatterWindowCovering::TILT, MatterWindowCovering::STALL);

  // Store state
  matterPref.putUChar(tiltPercentPrefKey, currentTiltPercent);

  return true;
}

bool stopMotor() {
  // Motor can be stopped while moving cover toward current target
  Serial.println("Stopping window covering motor");

  // Update CurrentPosition to reflect actual position when stopped
  WindowBlinds.setCurrentLiftPercent100ths(currentLiftPercent * 100);
  WindowBlinds.setCurrentTiltPercent100ths(currentTiltPercent * 100);

  // Set operational status to STALL for both lift and tilt
  WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);
  WindowBlinds.setOperationalState(MatterWindowCovering::TILT, MatterWindowCovering::STALL);

  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the RGB LED GPIO
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

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

  // Initialize Matter EndPoint
  matterPref.begin("MatterPrefs", false);
  // default lift percentage is 0% (fully open) if not stored before — Matter semantics
  uint8_t lastLiftPercent = matterPref.getUChar(liftPercentPrefKey, 0);
  // default tilt percentage is 0% if not stored before
  uint8_t lastTiltPercent = matterPref.getUChar(tiltPercentPrefKey, 0);

  // Initialize window covering with BLIND_LIFT_AND_TILT type and local motor calibration
  WindowBlinds.begin(lastLiftPercent, lastTiltPercent, MatterWindowCovering::BLIND_LIFT_AND_TILT, &LIFT_CALIBRATION, &TILT_CALIBRATION);

  // Initialize current positions based on percentages and motor calibration
  currentLiftPercent = lastLiftPercent;
  currentLift = liftPercentToCm(lastLiftPercent);
  currentTiltPercent = lastTiltPercent;

  Serial.printf(
    "Motor calibration: Lift [%u-%u cm], Tilt [%u-%u]\r\n", WindowBlinds.getLiftCalibration().open, WindowBlinds.getLiftCalibration().closed,
    WindowBlinds.getTiltCalibration().open, WindowBlinds.getTiltCalibration().closed
  );
  Serial.printf("Initial positions: Lift=%u cm (%u%%), Tilt=%u%%\r\n", currentLift, currentLiftPercent, currentTiltPercent);

  // Set callback functions
  WindowBlinds.onOpen(fullOpen);
  WindowBlinds.onClose(fullClose);
  WindowBlinds.onGoToLiftPercentage(goToLiftPercentage);
  WindowBlinds.onGoToTiltPercentage(goToTiltPercentage);
  WindowBlinds.onStop(stopMotor);

  // Generic callback for Lift or Tilt change
  WindowBlinds.onChange([](uint8_t liftPercent, uint8_t tiltPercent) {
    Serial.printf("Window Covering changed: Lift=%u%%, Tilt=%u%%\r\n", liftPercent, tiltPercent);
    visualizeWindowBlinds(liftPercent, tiltPercent);
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
    Serial.printf("Initial state: Lift=%u%%, Tilt=%u%%\r\n", WindowBlinds.getLiftPercentage(), WindowBlinds.getTiltPercentage());
    // Update visualization based on initial state
    visualizeWindowBlinds(WindowBlinds.getLiftPercentage(), WindowBlinds.getTiltPercentage());
  }
}

void loop() {
  // Check Matter Window Covering Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Window Covering Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.printf("Initial state: Lift=%u%%, Tilt=%u%%\r\n", WindowBlinds.getLiftPercentage(), WindowBlinds.getTiltPercentage());
    // Update visualization based on initial state
    visualizeWindowBlinds(WindowBlinds.getLiftPercentage(), WindowBlinds.getTiltPercentage());
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }

  // A button is also used to control the window covering
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  // Onboard User Button is used to manually change lift percentage or to decommission
  uint32_t time_diff = millis() - button_time_stamp;
  if (digitalRead(buttonPin) == HIGH && button_state && time_diff > debounceTime) {
    // Button is released - cycle lift percentage by 20%
    button_state = false;  // released
    uint8_t targetLiftPercent = currentLiftPercent;
    // go to the closest next 20% or move 20% more
    if ((targetLiftPercent % 20) != 0) {
      targetLiftPercent = ((targetLiftPercent / 20) + 1) * 20;
    } else {
      targetLiftPercent += 20;
    }
    if (targetLiftPercent > 100) {
      targetLiftPercent = 0;
    }
    Serial.printf("User button released. Setting lift to %u%%\r\n", targetLiftPercent);
    WindowBlinds.setTargetLiftPercent100ths(targetLiftPercent * 100);
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Window Covering Matter Accessory. It shall be commissioned again.");
    WindowBlinds.setCurrentLiftPercent100ths(10000);  // fully closed
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissioning again, reboot takes a second or so
  }
}
