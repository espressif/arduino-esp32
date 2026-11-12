/*
  Store and read a Preferences string as std::string.

  Pass the std::string you already have as the getString default.
  The same NVS entry is used by the Arduino String and C-string APIs.

  created for arduino-esp32
*/

#include <Arduino.h>
#include <Preferences.h>
#include <string>

Preferences preferences;

void setup() {
  Serial.begin(115200);
  Serial.println();

  std::string name = "arduino-esp32";

  preferences.begin("std-string", false);
  size_t stored = preferences.putString("name", name);
  Serial.printf("Stored %u bytes\n", static_cast<unsigned>(stored));

  name = preferences.getString("name", name);
  Serial.printf("std::string: %s\n", name.c_str());

  preferences.end();
}

void loop() {}
