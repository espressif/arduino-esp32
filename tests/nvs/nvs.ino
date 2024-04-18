#include <Preferences.h>

Preferences preferences;

void setup() {
  Serial.begin(115200);

  while (!Serial) {
    ;
  }

  preferences.begin("my-app", false);

  // Get the counter value, if the key does not exist, return a default value of 0
  unsigned int counter = preferences.getUInt("counter", 0);

  // Print the counter to Serial Monitor
  Serial.printf("Current counter value: %u\n", counter);

  // Increase counter by 1
  counter++;

  // Store the counter to the Preferences
  preferences.putUInt("counter", counter);

  // Close the Preferences
  preferences.end();

  // Wait 1 second
  delay(1000);

  // Restart ESP
  ESP.restart();
}

void loop() {}
