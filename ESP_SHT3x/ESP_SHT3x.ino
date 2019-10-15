#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
 
Adafruit_SHT31 sht31 = Adafruit_SHT31();
 
 
void setup() 
{
  Serial.begin(74880);
  if (! sht31.begin(0x44)) 
  {
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }else {
    Serial.println("SHT31 Found");
  }
}
 
 
void loop() 
{
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();
 
  if (! isnan(t)) 
  {
    Serial.print("Temp *C = "); Serial.println(t);
  } 
  else 
  { 
    Serial.println("Failed to read temperature");
  }
 
  if (! isnan(h)) 
  {
    Serial.print("Hum. % = "); Serial.println(h);
  } 
  else 
  { 
    Serial.println("Failed to read humidity");
  }
  if(h > 60) {
    Serial.println("H high");
  }
  Serial.println();
  delay(1000);
}
