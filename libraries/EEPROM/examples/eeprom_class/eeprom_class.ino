/*
  ESP32 eeprom_class example with EEPROM library

  This simple example demonstrates using EEPROM library to store different data in
  ESP32 Flash memory in a multiple user-defined EEPROM partition (0x1000 or 4KB max size or less).
  
  Install 'ESP32 Partiton Manager' ONCE from https://github.com/francis94c/ESP32Partitions
  And generate different partitions with 'partition_name'
  Usage: EEPROMClass ANY_OBJECT_NAME("partition_name", size);
  
  Generated partition that would work perfectly with this example
  #Name,   Type, SubType, Offset,   Size,    Flags
  nvs,     data, nvs,     0x9000,   0x5000,
  otadata, data, ota,     0xe000,   0x2000,
  app0,    app,  ota_0,   0x10000,  0x140000,
  app1,    app,  ota_1,   0x150000, 0x140000,
  eeprom0, data, 0x99,    0x290000, 0x1000,
  eeprom1, data, 0x9a,    0x291000, 0x500,
  eeprom2, data, 0x9b,    0x292000, 0x100,
  spiffs,  data, spiffs,  0x293000, 0x16d000,

  Created for arduino-esp32 on 25 Dec, 2017
  by Elochukwu Ifediora (fedy0)
*/

#include "EEPROM.h"

// Instantiate eeprom objects with parameter/argument names and size same as in the partition table
EEPROMClass  NAMES("eeprom0", 0x1000);
EEPROMClass  HEIGHT("eeprom1", 0x500);
EEPROMClass  AGE("eeprom2", 0x100);

void setup() {
  // put your setup code here, to run once:
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

  char* name = "Teo Swee Ann";
  double height = 5.8;
  uint32_t age = 47;

  // Write: Variables ---> EEPROM partitions
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

  // Read: Variables <--- EEPROM partitions
  NAMES.get(0, name);
  HEIGHT.get(0, height);
  AGE.get(0, age);
  Serial.print("name: ");   Serial.println(name);
  Serial.print("height: "); Serial.println(height);
  Serial.print("age: ");    Serial.println(age);
  
  Serial.println("Done!");
}

void loop() {
  // put your main code here, to run repeatedly:

}
