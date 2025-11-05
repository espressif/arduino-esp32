#define TwoWire TwoWireInternal
#define Wire    WireInternal
#define Wire1   WireInternal1

#include "pins_arduino.h"
#include "../../libraries/Wire/src/Wire.h"
#include "../../libraries/Wire/src/Wire.cpp"

static bool wireInitialized = false;

// From https://www.diodes.com/datasheet/download/PI4IOE5V6408.pdf
static void writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  WireInternal.beginTransmission(address);
  WireInternal.write(reg);
  WireInternal.write(value);
  WireInternal.endTransmission();
}

static uint8_t readRegister(uint8_t address, uint8_t reg) {
  WireInternal.beginTransmission(address);
  WireInternal.write(reg);
  WireInternal.endTransmission(false);
  WireInternal.requestFrom(address, 1);
  return WireInternal.read();
}

static void writeBitRegister(uint8_t address, uint8_t reg, uint8_t bit, uint8_t value) {
  uint8_t val = readRegister(address, reg);
  if (value) {
    writeRegister(address, reg, val | (1 << bit));
  } else {
    writeRegister(address, reg, val & ~(1 << bit));
  }
}

static bool readBitRegister(uint8_t address, uint8_t reg, uint8_t bit) {
  uint8_t val = readRegister(address, reg);
  return ((val & (1 << bit)) > 0);
}

void pinMode(ExpanderPin pin, uint8_t mode) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
    // reset all registers to default state
    writeRegister(pin.address, 0x1, 0x1);
    // set all pins as high as default state
    writeRegister(pin.address, 0x9, 0xFF);
    // interrupt mask to all pins
    writeRegister(pin.address, 0x11, 0xFF);
    // all input
    writeRegister(pin.address, 0x3, 0);
  }
  writeBitRegister(pin.address, 0x3, pin.pin, mode == OUTPUT);
  if (mode == OUTPUT) {
    // remove high impedance
    writeBitRegister(pin.address, 0x7, pin.pin, false);
  } else if (mode == INPUT_PULLUP) {
    // set pull-up resistor
    writeBitRegister(pin.address, 0xB, pin.pin, true);
    writeBitRegister(pin.address, 0xD, pin.pin, true);
  } else if (mode == INPUT_PULLDOWN) {
    // disable pull-up resistor
    writeBitRegister(pin.address, 0xB, pin.pin, true);
    writeBitRegister(pin.address, 0xD, pin.pin, false);
  } else if (mode == INPUT) {
    // disable pull selector resistor
    writeBitRegister(pin.address, 0xB, pin.pin, false);
  }
}

void digitalWrite(ExpanderPin pin, uint8_t val) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  writeBitRegister(pin.address, 0x5, pin.pin, val == HIGH);
}

int digitalRead(ExpanderPin pin) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  return readBitRegister(pin.address, 0xF, pin.pin);
}

void NessoBattery::enableCharge() {
  // AW32001E - address 0x49
  // set CEB bit low (charge enable)
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  writeBitRegister(0x49, 0x1, 3, false);
}

float NessoBattery::getVoltage() {
  // BQ27220 - address 0x55
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  uint16_t voltage = (readRegister(0x55, 0x9) << 8) | readRegister(0x55, 0x8);
  return (float)voltage / 1000.0f;
}

uint16_t NessoBattery::getChargeLevel() {
  // BQ27220 - address 0x55
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  uint16_t current_capacity = readRegister(0x55, 0x11) << 8 | readRegister(0x55, 0x10);
  uint16_t total_capacity = readRegister(0x55, 0x13) << 8 | readRegister(0x55, 0x12);
  return (current_capacity * 100) / total_capacity;
}

ExpanderPin LORA_LNA_ENABLE(5);
ExpanderPin LORA_ANTENNA_SWITCH(6);
ExpanderPin LORA_ENABLE(7);
ExpanderPin KEY1(0);
ExpanderPin KEY2(1);
ExpanderPin POWEROFF((1 << 8) | 0);
ExpanderPin LCD_RESET((1 << 8) | 1);
ExpanderPin GROVE_POWER_EN((1 << 8) | 2);
ExpanderPin VIN_DETECT((1 << 8) | 5);
ExpanderPin LCD_BACKLIGHT((1 << 8) | 6);
ExpanderPin LED_BUILTIN((1 << 8) | 7);
