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

// Window covering limits
// Lift limits in centimeters (physical position)
const uint16_t MAX_LIFT = 200;  // Maximum lift position (fully open)
const uint16_t MIN_LIFT = 0;    // Minimum lift position (fully closed)

// Tilt limits (absolute values for conversion, not physical units)
// Tilt is a rotation, not a linear measurement
const uint16_t MAX_TILT = 90;  // Maximum tilt absolute value
const uint16_t MIN_TILT = 0;   // Minimum tilt absolute value

// Current window covering state
// These will be initialized in setup() based on installed limits and saved percentages
uint16_t currentLift = 0;  // Lift position in cm
uint8_t currentLiftPercent = 100;
uint8_t currentTiltPercent = 0;  // Tilt rotation percentage (0-100%)

// Visualize window covering position using RGB LED
// Lift percentage controls brightness (0% = off, 100% = full brightness)
#ifdef RGB_BUILTIN
const uint8_t ledPin = RGB_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined RGB_BUILTIN
#warning "Do not forget to set the RGB LED pin"
#endif

void visualizeWindowBlinds(uint8_t liftPercent, uint8_t tiltPercent) {
#ifdef RGB_BUILTIN
  // Use RGB LED to visualize lift position (brightness) and tilt (color shift)
  float brightness = (float)liftPercent / 100.0;  // 0.0 to 1.0
  // Tilt affects color: 0% = red, 100% = blue
  uint8_t red = (uint8_t)(map(tiltPercent, 0, 100, 255, 0) * brightness);
  uint8_t blue = (uint8_t)(map(tiltPercent, 0, 100, 0, 255) * brightness);
  uint8_t green = 0;
  rgbLedWrite(ledPin, red, green, blue);
#else
  // For non-RGB boards, just use brightness
  uint8_t brightnessValue = map(liftPercent, 0, 100, 0, 255);
  analogWrite(ledPin, brightnessValue);
#endif
}

// Window Covering Callbacks
bool fullOpen() {
  // This is where you would trigger your motor to go to full open state
  // For simulation, we update instantly
  uint16_t openLimit = WindowBlinds.getInstalledOpenLimitLift();
  currentLift = openLimit;
  currentLiftPercent = 100;
  Serial.printf("Opening window covering to full open (position: %d cm)\r\n", currentLift);

  // Update CurrentPosition to reflect actual position (setLiftPercentage now only updates CurrentPosition)
  WindowBlinds.setLiftPercentage(currentLiftPercent);

  // Set operational status to STALL when movement is complete
  WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);

  // Store state
  matterPref.putUChar(liftPercentPrefKey, currentLiftPercent);

  return true;
}

bool fullClose() {
  // This is where you would trigger your motor to go to full close state
  // For simulation, we update instantly
  uint16_t closedLimit = WindowBlinds.getInstalledClosedLimitLift();
  currentLift = closedLimit;
  currentLiftPercent = 0;
  Serial.printf("Closing window covering to full close (position: %d cm)\r\n", currentLift);

  // Update CurrentPosition to reflect actual position (setLiftPercentage now only updates CurrentPosition)
  WindowBlinds.setLiftPercentage(currentLiftPercent);

  // Set operational status to STALL when movement is complete
  WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);

  // Store state
  matterPref.putUChar(liftPercentPrefKey, currentLiftPercent);

  return true;
}

bool goToLiftPercentage(uint8_t liftPercent) {
  // update Lift operational state
  if (liftPercent > currentLiftPercent) {
    // Set operational status to OPEN
    WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::MOVING_UP_OR_OPEN);
  }
  if (liftPercent < currentLiftPercent) {
    // Set operational status to CLOSE
    WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::MOVING_DOWN_OR_CLOSE);
  }

  // This is where you would trigger your motor to go towards liftPercent
  // For simulation, we update instantly
  // Calculate absolute position based on installed limits
  uint16_t openLimit = WindowBlinds.getInstalledOpenLimitLift();
  uint16_t closedLimit = WindowBlinds.getInstalledClosedLimitLift();

  // Linear interpolation: 0% = openLimit, 100% = closedLimit
  if (openLimit < closedLimit) {
    currentLift = openLimit + ((closedLimit - openLimit) * liftPercent) / 100;
  } else {
    currentLift = openLimit - ((openLimit - closedLimit) * liftPercent) / 100;
  }
  currentLiftPercent = liftPercent;
  Serial.printf("Moving lift to %d%% (position: %d cm)\r\n", currentLiftPercent, currentLift);

  // Update CurrentPosition to reflect actual position (setLiftPercentage now only updates CurrentPosition)
  WindowBlinds.setLiftPercentage(currentLiftPercent);

  // Set operational status to STALL when movement is complete
  WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);

  // Store state
  matterPref.putUChar(liftPercentPrefKey, currentLiftPercent);

  return true;
}

bool goToTiltPercentage(uint8_t tiltPercent) {
  // update Tilt operational state
  if (tiltPercent < currentTiltPercent) {
    // Set operational status to OPEN
    WindowBlinds.setOperationalState(MatterWindowCovering::TILT, MatterWindowCovering::MOVING_UP_OR_OPEN);
  }
  if (tiltPercent > currentTiltPercent) {
    // Set operational status to CLOSE
    WindowBlinds.setOperationalState(MatterWindowCovering::TILT, MatterWindowCovering::MOVING_DOWN_OR_CLOSE);
  }

  // This is where you would trigger your motor to rotate the shade to tiltPercent
  // For simulation, we update instantly
  currentTiltPercent = tiltPercent;
  Serial.printf("Rotating tilt to %d%%\r\n", currentTiltPercent);

  // Update CurrentPosition to reflect actual position
  WindowBlinds.setTiltPercentage(currentTiltPercent);

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
  // (setLiftPercentage and setTiltPercentage now only update CurrentPosition)
  WindowBlinds.setLiftPercentage(currentLiftPercent);
  WindowBlinds.setTiltPercentage(currentTiltPercent);

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
  // default lift percentage is 100% (fully open) if not stored before
  uint8_t lastLiftPercent = matterPref.getUChar(liftPercentPrefKey, 100);
  // default tilt percentage is 0% if not stored before
  uint8_t lastTiltPercent = matterPref.getUChar(tiltPercentPrefKey, 0);

  // Initialize window covering with BLIND_LIFT_AND_TILT type
  WindowBlinds.begin(lastLiftPercent, lastTiltPercent, MatterWindowCovering::BLIND_LIFT_AND_TILT);

  // Configure installed limits for lift and tilt
  WindowBlinds.setInstalledOpenLimitLift(MIN_LIFT);
  WindowBlinds.setInstalledClosedLimitLift(MAX_LIFT);
  WindowBlinds.setInstalledOpenLimitTilt(MIN_TILT);
  WindowBlinds.setInstalledClosedLimitTilt(MAX_TILT);

  // Initialize current positions based on percentages and installed limits
  uint16_t openLimitLift = WindowBlinds.getInstalledOpenLimitLift();
  uint16_t closedLimitLift = WindowBlinds.getInstalledClosedLimitLift();
  currentLiftPercent = lastLiftPercent;
  if (openLimitLift < closedLimitLift) {
    currentLift = openLimitLift + ((closedLimitLift - openLimitLift) * lastLiftPercent) / 100;
  } else {
    currentLift = openLimitLift - ((openLimitLift - closedLimitLift) * lastLiftPercent) / 100;
  }

  currentTiltPercent = lastTiltPercent;

  Serial.printf(
    "Window Covering limits configured: Lift [%d-%d cm], Tilt [%d-%d]\r\n", WindowBlinds.getInstalledOpenLimitLift(),
    WindowBlinds.getInstalledClosedLimitLift(), WindowBlinds.getInstalledOpenLimitTilt(), WindowBlinds.getInstalledClosedLimitTilt()
  );
  Serial.printf("Initial positions: Lift=%d cm (%d%%), Tilt=%d%%\r\n", currentLift, currentLiftPercent, currentTiltPercent);

  // Set callback functions
  WindowBlinds.onOpen(fullOpen);
  WindowBlinds.onClose(fullClose);
  WindowBlinds.onGoToLiftPercentage(goToLiftPercentage);
  WindowBlinds.onGoToTiltPercentage(goToTiltPercentage);
  WindowBlinds.onStop(stopMotor);

  // Generic callback for Lift or Tilt change
  WindowBlinds.onChange([](uint8_t liftPercent, uint8_t tiltPercent) {
    Serial.printf("Window Covering changed: Lift=%d%%, Tilt=%d%%\r\n", liftPercent, tiltPercent);
    visualizeWindowBlinds(liftPercent, tiltPercent);
    return true;
  });

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
    Serial.printf("Initial state: Lift=%d%%, Tilt=%d%%\r\n", WindowBlinds.getLiftPercentage(), WindowBlinds.getTiltPercentage());
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
    Serial.printf("Initial state: Lift=%d%%, Tilt=%d%%\r\n", WindowBlinds.getLiftPercentage(), WindowBlinds.getTiltPercentage());
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
    Serial.printf("User button released. Setting lift to %d%%\r\n", targetLiftPercent);
    WindowBlinds.setTargetLiftPercent100ths(targetLiftPercent * 100);
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Window Covering Matter Accessory. It shall be commissioned again.");
    WindowBlinds.setLiftPercentage(0);  // close the covering
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissioning again, reboot takes a second or so
  }
}
