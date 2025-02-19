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

/*
   This example is an example code that will create a Matter Device which can be
   commissioned and controlled from a Matter Environment APP.
   Additionally the ESP32 will send debug messages indicating the Matter activity.
   Turning DEBUG Level ON may be useful to following Matter Accessory and Controller messages.
*/

// Matter Manager
#include <Matter.h>
#include <WiFi.h>

// List of Matter Endpoints for this Node
// Matter Thermostat Endpoint
MatterThermostat SimulatedThermostat;

// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control - decommision the Matter Node
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Simulate a system that will activate heating/cooling in addition to a temperature sensor - add your preferred code here
float getSimulatedTemperature(bool isHeating, bool isCooling) {
  // read sensor temperature and apply heating/cooling
  float simulatedTempHWSensor = SimulatedThermostat.getLocalTemperature();

  if (isHeating) {
    // it will increase to simulate a heating system
    simulatedTempHWSensor = simulatedTempHWSensor + 0.5;
  }
  if (isCooling) {
    // it will decrease to simulate a colling system
    simulatedTempHWSensor = simulatedTempHWSensor - 0.5;
  }
  // otherwise, it will keep the temperature stable
  return simulatedTempHWSensor;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(115200);

  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  // Simulated Thermostat in COOLING and HEATING mode with Auto Mode to keep the temperature between setpoints
  // Auto Mode can only be used when the control sequence of operation is Cooling & Heating
  SimulatedThermostat.begin(MatterThermostat::THERMOSTAT_SEQ_OP_COOLING_HEATING, MatterThermostat::THERMOSTAT_AUTO_MODE_ENABLED);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();

  // Check Matter Accessory Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Thermostat Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");

    // after commissioning, set initial thermostat parameters
    // start the thermostat in AUTO mode
    SimulatedThermostat.setMode(MatterThermostat::THERMOSTAT_MODE_AUTO);
    // cooling setpoint must be lower than heating setpoint by at least 2.5C (deadband), in auto mode
    SimulatedThermostat.setCoolingHeatingSetpoints(20.0, 23.00);  // the target cooler and heating setpoint
    // set the local temperature sensor in Celsius
    SimulatedThermostat.setLocalTemperature(12.50);

    Serial.println();
    Serial.printf(
      "Initial Setpoints are %.01fC to %.01fC with a minimum 2.5C difference\r\n", SimulatedThermostat.getHeatingSetpoint(),
      SimulatedThermostat.getCoolingSetpoint()
    );
    Serial.printf("Auto mode is ON. Initial Temperature of %.01fC \r\n", SimulatedThermostat.getLocalTemperature());
    Serial.println("Local Temperature Sensor will be simulated every 10 seconds and changed by a simulated heater and cooler to move in between setpoints.");
  }
}

// This will simulate the thermostat control system (heating and cooling)
// User can set a local temperature using the Serial input (type a number and press Enter)
// New temperature can be an positive or negative temperature in Celsius, between -50C and 50C
// Initial local temperature is 10C as defined in getSimulatedTemperature() function
void readSerialForNewTemperature() {
  static String newTemperatureStr;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (newTemperatureStr.length() > 0) {
        // convert the string to a float value
        float newTemperature = newTemperatureStr.toFloat();
        // check if the new temperature is valid
        if (newTemperature >= -50.0 && newTemperature <= 50.0) {
          // set the new temperature
          SimulatedThermostat.setLocalTemperature(newTemperature);
          Serial.printf("New Temperature is %.01fC\r\n", newTemperature);
        } else {
          Serial.println("Invalid Temperature value. Please type a number between -50 and 50");
        }
        newTemperatureStr = "";
      }
    } else {
      if (c == '+' || c == '-' || (c >= '0' && c <= '9') || c == '.') {
        newTemperatureStr += c;
      } else {
        Serial.println("Invalid character. Please type a number between -50 and 50");
        newTemperatureStr = "";
      }
    }
  }
}

// loop will simulate the thermostat control system
// User can set a local temperature using the Serial input (type a number and press Enter)
// User can change the thermostat mode using the Matter APP (smartphone)
// The loop will simulate a heating and cooling system and the associated local temperature change
void loop() {
  static uint32_t timeCounter = 0;

  // Simulate the heating and cooling systems
  static bool isHeating = false;
  static bool isCooling = false;

  // check if a new temperature is typed in the Serial Monitor
  readSerialForNewTemperature();

  // simulate thermostat with heating/cooling system and the associated local temperature change, every 10s
  if (!(timeCounter++ % 20)) {  // delaying for 500ms x 20 = 10s
    float localTemperature = getSimulatedTemperature(isHeating, isCooling);
    // Print the current thermostat local temperature value
    Serial.printf("Current Local Temperature is %.01fC\r\n", localTemperature);
    SimulatedThermostat.setLocalTemperature(localTemperature);  // publish the new temperature value

    // Simulate the thermostat control system - User has 4 modes: OFF, HEAT, COOL, AUTO
    switch (SimulatedThermostat.getMode()) {
      case MatterThermostat::THERMOSTAT_MODE_OFF:
        // turn off the heating and cooling systems
        isHeating = false;
        isCooling = false;
        break;
      case MatterThermostat::THERMOSTAT_MODE_AUTO:
        // User APP has set the thermostat to AUTO mode -- keeping the tempeature between both setpoints
        // check if the heating system should be turned on or off
        if (localTemperature < SimulatedThermostat.getHeatingSetpoint() + SimulatedThermostat.getDeadBand()) {
          // turn on the heating system and turn off the cooling system
          isHeating = true;
          isCooling = false;
        }
        if (localTemperature > SimulatedThermostat.getCoolingSetpoint() - SimulatedThermostat.getDeadBand()) {
          // turn off the heating system and turn on the cooling system
          isHeating = false;
          isCooling = true;
        }
        break;
      case MatterThermostat::THERMOSTAT_MODE_HEAT:
        // Simulate the heating system - User has turned the heating system ON
        isHeating = true;
        isCooling = false;  // keep the cooling system off as it is in heating mode
        // when the heating system is in HEATING mode, it will be turned off as soon as the local temperature is above the setpoint
        if (localTemperature > SimulatedThermostat.getHeatingSetpoint()) {
          // turn off the heating system
          isHeating = false;
        }
        break;
      case MatterThermostat::THERMOSTAT_MODE_COOL:
        // Simulate the cooling system - User has turned the cooling system ON
        if (SimulatedThermostat.getMode() == MatterThermostat::THERMOSTAT_MODE_COOL) {
          isCooling = true;
          isHeating = false;  // keep the heating system off as it is in cooling mode
          // when the cooling system is in COOLING mode, it will be turned off as soon as the local temperature is bellow the setpoint
          if (localTemperature < SimulatedThermostat.getCoolingSetpoint()) {
            // turn off the cooling system
            isCooling = false;
          }
        }
        break;
      default: log_e("Invalid Thermostat Mode %d", SimulatedThermostat.getMode());
    }
    // Reporting Heating and Cooling status
    Serial.printf(
      "\tThermostat Mode: %s >>> Heater is %s -- Cooler is %s\r\n", MatterThermostat::getThermostatModeString(SimulatedThermostat.getMode()),
      isHeating ? "ON" : "OFF", isCooling ? "ON" : "OFF"
    );
  }
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  if (digitalRead(buttonPin) == HIGH && button_state) {
    button_state = false;  // released
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning Thermostat Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissining again, reboot takes a second or so
  }

  delay(500);
}
