/*
 ESP32 start counter example with Non-volatile storage

 A simple example which use Non-volatile storage on ESP32 to store how many 
 times ESP32 module was started.

 created for arduino-esp32 09 Feb 2017
 by Martin Sloup (Arcao)
*/

#include <ESP32NVS.h>

NVSClass keyStorage;

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Open NVS key storage with my-app namespace. Each application module (library, etc.)
  // have to use namespace name to prevent key name colision. We will open storage in 
  // RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars
  keyStorage.begin("my-app", false);

  // Erase my-app namespace key storage
  //keyStorage.erase();

  // Or erase counter key in my-app namespace key storage
  //keyStorage.erase("counter");

  // Get a counter value, if key is not exist return default value 0
  // Note: Key name is limited to 15 chars
  unsigned int counter = keyStorage.readUInt("counter", 0);

  // Increase counter
  counter++;

  // Print counter to Serial
  Serial.printf("Current counter value: %u\n", counter);

  // Store counter to key storage
  keyStorage.writeUInt("counter", counter);

  // Close key storage
  keyStorage.end();

  Serial.println("Restarting in 10 seconds...");
  
  // Wait 10 sec
  delay(10000);

  // Restart ESP
  ESP.restart();
}

void loop() {}
