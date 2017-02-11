/*
 ESP32 start counter example with Preferences library

 This simple example demonstrate using Preferences library to store how many times
 was ESP32 module started. Preferences library is wrapper around Non-volatile
 storage on ESP32 processor.

 created for arduino-esp32 09 Feb 2017
 by Martin Sloup (Arcao)
*/

#include <Preferences.h>

Preferences preferences;

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Open Preferences with my-app namespace. Each application module, library, etc.
  // has to use namespace name to prevent key name collisions. We will open storage in
  // RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars
  preferences.begin("my-app", false);

  // Remove all preferences under opened namespace
  //preferences.clear();

  // Or remove the counter key only
  //preferences.remove("counter");

  // Get a counter value, if key is not exist return default value 0
  // Note: Key name is limited to 15 chars too
  unsigned int counter = preferences.getUInt("counter", 0);

  // Increase counter
  counter++;

  // Print counter to a Serial
  Serial.printf("Current counter value: %u\n", counter);

  // Store counter to the Preferences
  preferences.putUInt("counter", counter);

  // Close the Preferences
  preferences.end();

  // Wait 10 seconds
  Serial.println("Restarting in 10 seconds...");
  delay(10000);

  // Restart ESP
  ESP.restart();
}

void loop() {}