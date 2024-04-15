#include "Arduino.h"

float getBatteryVoltage() {
  int analogVolt = analogReadMilliVolts(1);
  float voltage = analogVolt / 1000.0;
  voltage = voltage * (100.0 + 200.0) / 200.0;
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

void (*__onChargeStart__)();
void (*__onChargeEnd__)();
void setOnChargeStart(void (*func)()) {
  __onChargeStart__ = func;
}
void setOnChargeEnd(void (*func)()) {
  __onChargeEnd__ = func;
}

void ARDUINO_ISR_ATTR chargeIsr() {
  if (getChargingState()) {
    __onChargeStart__();
  } else {
    __onChargeEnd__();
  }
}

extern "C" void initVariant(void) {
  pinMode(CHG, INPUT_PULLUP);
  attachInterrupt(CHG, chargeIsr, CHANGE);
  analogReadResolution(12);
}
