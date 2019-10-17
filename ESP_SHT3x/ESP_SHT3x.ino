#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
 
Adafruit_SHT31 sht31 = Adafruit_SHT31();

 // The pins which will be connected to the 2 relay
 int humidPin = 2;
 int tempPin = 14;

 // the temperature and humidity threshholds to trigger relay
 float tempLimit = 22.78;
 float humidLimit = 97.0;
 
void setup() {
  Serial.begin(115200);

  // sets the digital pins as output
  pinMode(humidPin, OUTPUT);
  pinMode(tempPin, OUTPUT);
}

// initializes sht31
void shtInit() {
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

// Sets relay pins to high or low depending on measurements
void relayPower(int pin,float current, float limit) {
  if(current < limit) {
    digitalWrite(pin, LOW);
  } else {
    digitalWrite(pin, HIGH);
  }
}
 
void loop() {
  shtInit();
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  printShtMeasurments(t, h);

  relayPower(humidPin, h, humidLimit);
  relayPower(tempPin, t, tempLimit);
  
  Serial.println();
  
  delay(1000);
}
