/*
  ESP32 eeprom_extra example with EEPROM library

  This simple example demonstrates using other EEPROM library resources

  Created for arduino-esp32 on 25 Dec, 2017
  by Ifediora Elochukwu (fedy0)
*/

#include "EEPROM.h"

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Testing EEPROM Library\n");
  if (!EEPROM.begin(EEPROM.length())) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
