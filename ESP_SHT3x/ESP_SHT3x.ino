#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
 
Adafruit_SHT31 sht31 = Adafruit_SHT31();

 
void setup() {
  Serial.begin(74880);
  /*
  // initializes sht31
  if (! sht31.begin(0x44)) {
    Serial.println("Couldn't find SHT31");
  } else {
    Serial.println("SHT31 Found");
  }
  */
}

void shtInit() {
  // initializes sht31
  if (! sht31.begin(0x44)) {
    Serial.println("Couldn't find SHT31");
  } else {
    Serial.println("SHT31 Found");
  }  
}

// Outputs measured temperature and humidity to serial monitor
void printShtMeasurments(float temp, float hum) {
   if (! isnan(temp)) {
    Serial.print("Temp *C = "); Serial.println(temp);
  } else { 
    Serial.println("Failed to read temperature");
  }
 
  if (! isnan(hum)) {
    Serial.print("Hum. % = "); Serial.println(hum);
  } else { 
    Serial.println("Failed to read humidity");
  }
}
 
void loop() {
  shtInit();
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  printShtMeasurments(t, h);
  
  Serial.println();
  
  delay(1000);
}
