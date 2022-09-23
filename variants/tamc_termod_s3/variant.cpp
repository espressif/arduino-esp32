#include "Arduino.h"

float getBatteryVoltage() {
  int analogVolt = analogReadMilliVolts(1); 
  float voltage = analogVolt / 1000.0;
  voltage = voltage * (100.0+200.0) / 200.0;
  return voltage;
}

float getBatteryCapacity() {
  float voltage = getBatteryVoltage();
  float capacity = (voltage - 3.3) / (4.2 - 3.3) * 100.0;
  capacity = constrain(capacity, 0, 100);
  return capacity;
}

bool getChargingState() {
  return !digitalRead(CHG);
}

extern "C" void initVariant(void){
  pinMode(CHG, INPUT_PULLUP);
  analogReadResolution(12);
}
