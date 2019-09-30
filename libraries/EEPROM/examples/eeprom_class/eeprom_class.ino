/*
  ESP32 eeprom_class example with EEPROM library
  This simple example demonstrates using EEPROM library to store different data in
  ESP32 Flash memory in a multiple user-defined EEPROM class objects.
  
  Created for arduino-esp32 on 25 Dec, 2017
  by Elochukwu Ifediora (fedy0)
  converted to nvs by lbernstone - 06/22/2019
*/

#include "EEPROM.h"

// Instantiate eeprom objects with parameter/argument names and sizes
EEPROMClass  NAMES("eeprom0", 0x500);
EEPROMClass  HEIGHT("eeprom1", 0x200);
EEPROMClass  AGE("eeprom2", 0x100);

void setup() {
  Serial.begin(115200);
  Serial.println("Testing EEPROMClass\n");
  if (!NAMES.begin(NAMES.length())) {
    Serial.println("Failed to initialise NAMES");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }
  if (!HEIGHT.begin(HEIGHT.length())) {
    Serial.println("Failed to initialise HEIGHT");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }
  if (!AGE.begin(AGE.length())) {
    Serial.println("Failed to initialise AGE");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }

  const char* name = "Teo Swee Ann";
  char rname[32];
  double height = 5.8;
  uint32_t age = 47;

  // Write: Variables ---> EEPROM stores
  NAMES.put(0, name);
  HEIGHT.put(0, height);
  AGE.put(0, age);
  Serial.print("name: ");   Serial.println(name);
  Serial.print("height: "); Serial.println(height);
  Serial.print("age: ");    Serial.println(age);
  Serial.println("------------------------------------\n");

  // Clear variables
  name = '\0';
  height = 0;
  age = 0;
  Serial.print("name: ");   Serial.println(name);
  Serial.print("height: "); Serial.println(height);
  Serial.print("age: ");    Serial.println(age);
  Serial.println("------------------------------------\n");

  // Read: Variables <--- EEPROM stores
  NAMES.get(0, rname);
  HEIGHT.get(0, height);
  AGE.get(0, age);
  Serial.print("name: ");   Serial.println(rname);
  Serial.print("height: "); Serial.println(height);
  Serial.print("age: ");    Serial.println(age);
  
  Serial.println("Done!");
}

void loop() {
  delay(0xFFFFFFFF);
}
