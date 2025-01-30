#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "ZigbeeCore.h"
#include "ep/ZigbeeWindowCovering.h"


#define ZIGBEE_COVERING_ENDPOINT 10
#define BUTTON_PIN                9  // ESP32-C6/H2 Boot button

#define MAX_LIFT 200 // centimeters from open position
#define MIN_LIFT   0

uint8_t currentLift = MAX_LIFT;
uint8_t reportedLift = MIN_LIFT;

ZigbeeWindowCovering zbCovering = ZigbeeWindowCovering(ZIGBEE_COVERING_ENDPOINT);

void setup() {
  Serial.begin(115200); 

  Serial.println("ESP32S3 initialization completed!");

  // Init button for factory reset
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  //Optional: set Zigbee device name and model
  zbCovering.setManufacturerAndModel("Espriff", "Arduino");
  zbCovering.setCoveringType(ROLLERSHADE);

  // operational, online, not commands_reversed, closed_loop, encoder_controlled
  zbCovering.setConfigStatus(true, true, false, true, true, true, true);

  // not motor_reversed, calibration_mode, not maintenance_mode, not leds_on
  zbCovering.setMode(false, true, false, false);

  zbCovering.setLimits(MIN_LIFT, MAX_LIFT, 0, 0);

  // Set callback function for light change
  zbCovering.onGoToLiftPercentage(goToLiftPercentage);
  zbCovering.onStop(stopMotor);

  //Add endpoint to Zigbee Core
  Serial.println("Adding ZigbeeWindowCovering endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbCovering);

  // When all EPs are registered, start Zigbee. By default acts as ZIGBEE_END_DEVICE
  Serial.println("Calling Zigbee.begin()");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  reportState();
  Serial.println(); 
}

void loop() {
  // Checking button for factory reset
  if (digitalRead(BUTTON_PIN) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.printf("Resetting Zigbee to factory settings, reboot.\n");
        Zigbee.factoryReset();
        delay(30000);
      }
    }
  } 

  reportState();

  delay(500);
}

void goToLiftPercentage(uint8_t liftPercentage) {
  /* This is where you would trigger your motor to go towards liftPercentage, currentLift should
     be updated until liftPercentage has been reached in order to provide feedback to controller */

  // Our cover updates instantly!
  currentLift = (liftPercentage * MAX_LIFT) / 100;

  Serial.printf("New requsted lift from Zigbee: %d (%d)\n", currentLift, liftPercentage);
}

void reportState() {
  if (reportedLift == currentLift) {
    return;
  }

  Serial.printf("Reporting new state: %d (%d)\n", currentLift, reportedLift);

  reportedLift = currentLift;

  // This is where current state is reported to controller, to provide feedback on where the shade is at currently
  zbCovering.setLiftPosition((uint16_t) reportedLift);
}


void stopMotor() {
  // Motor can be stopped while moving cover toward current target
  Serial.println("Stopping motor"); 
}