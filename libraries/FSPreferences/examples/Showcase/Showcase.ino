/*
 ESP32 FSPreferences showcase

 This is a simple demonstration of different features of the FSPreferences library

 created for arduino-esp32
 by Frederik Merz (Curclamas) <Frederik.merz@novalight.de>
*/

#include <SD.h>
#include <SD_MMC.h>
#include <SPIFFS.h>

#include <FSPreferences.h>

/* Select your FS */
// #define MY_FS SD
#define MY_FS SD_MMC
// #define MY_FS SPIFFS

FSPreferences preferences(MY_FS);

void setup() {
  Serial.begin(115200);
  Serial.println();

  MY_FS.begin();

  preferences.begin("my-app1", false);
  preferences.putInt("myInt", 1337);
  preferences.putBool("myBool", true);
  preferences.putBool("myBool2", true);
  preferences.remove("myBool2");
  preferences.putString("myString", "Hello World!");
  preferences.end();

  preferences.begin("my-app2", false);
  preferences.putInt("myInt", 1337);
  preferences.putBool("myBool", true);
  preferences.putString("myString", "Hello World!");
  preferences.end();

  preferences.begin("my-app1", true);
  Serial.println(preferences.getInt("myInt", -1));
  Serial.println(preferences.getBool("myBool", false));
  Serial.println(preferences.getString("myString", "Error!"));  
  preferences.end();

  preferences.begin("my-app2", false);
  preferences.clear();
  preferences.end();

  // Close the FS  
  MY_FS.end();

  // Wait 10 seconds
  Serial.println("Restarting in 10 seconds...");
  delay(10000);

  // Restart ESP
  ESP.restart();
}

void loop() {}