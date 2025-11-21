#define TwoWire TwoWireInternal
#define Wire    WireInternal
#define Wire1   WireInternal1

#include "pins_arduino.h"
#include "../../libraries/Wire/src/Wire.h"
#include "../../libraries/Wire/src/Wire.cpp"

static bool wireInitialized = false;

// IO expander datasheet from https://www.diodes.com/datasheet/download/PI4IOE5V6408.pdf
// Battery charger datasheet from https://www.awinic.com/en/productDetail/AW32001ACSR
// battery gauge datasheet from https://www.ti.com/product/BQ27220

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

void NessoBattery::begin(uint16_t current, uint16_t voltage, UnderVoltageLockout uvlo, uint8_t timeout) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }

  setChargeCurrent(current);
  setChargeVoltage(voltage);
  setWatchdogTimer(timeout);
  setBatUVLO(uvlo);
  setHiZ(false);
  setChargeEnable(true);
}

void NessoBattery::enableCharge() {
  setChargeEnable(true);
}

void NessoBattery::setChargeEnable(bool enable) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  // bit 3 set charge enable
  writeBitRegister(AW32001_I2C_ADDR, AW3200_POWER_ON_CFG, 3, !enable);
}

void NessoBattery::setBatUVLO(UnderVoltageLockout uvlo) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  uint8_t reg_value = readRegister(AW32001_I2C_ADDR, AW3200_POWER_ON_CFG);
  // bits 2-0 set UVLO
  reg_value &= ~0b00000111;
  reg_value |= (uvlo & 0b00000111);
  writeRegister(AW32001_I2C_ADDR, AW3200_POWER_ON_CFG, reg_value);
}

void NessoBattery::setChargeCurrent(uint16_t current) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  if (current < 8) {
    current = 8;
  }
  if (current > 456) {
    current = 456;
  }
  uint8_t reg_value = readRegister(AW32001_I2C_ADDR, AW3200_CHG_CURRENT);
  // bits 5-0 set charge current
  reg_value &= ~0b00111111;
  reg_value |= ((current - 8) / 8) & 0b00111111;
  writeRegister(AW32001_I2C_ADDR, AW3200_CHG_CURRENT, reg_value);
}

void NessoBattery::setDischargeCurrent(uint16_t current) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  if (current < 200) {
    current = 200;
  }
  if (current > 3200) {
    current = 3200;
  }
  uint8_t reg_value = readRegister(AW32001_I2C_ADDR, AW3200_TERM_CURRENT);
  // bits 7-4 set discharge current
  reg_value &= ~0b11110000;
  reg_value |= (((current - 200) / 200) & 0b00001111) << 4;
  writeRegister(AW32001_I2C_ADDR, AW3200_TERM_CURRENT, reg_value);
}

void NessoBattery::setChargeVoltage(uint16_t voltage) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  if (voltage < 3600) {
    voltage = 3600;
  }
  if (voltage > 4545) {
    voltage = 4545;
  }
  uint8_t reg_value = readRegister(AW32001_I2C_ADDR, AW3200_CHG_VOLTAGE);
  // bits 7-2 set charge voltage
  reg_value &= ~0b11111100;
  reg_value |= ((voltage - 3600) / 15) << 2;
  writeRegister(AW32001_I2C_ADDR, AW3200_CHG_VOLTAGE, reg_value);
}

void NessoBattery::setWatchdogTimer(uint8_t sec) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }

  uint8_t reg_value = readRegister(AW32001_I2C_ADDR, AW3200_TIMER_WD);
  uint8_t bits = 0;
  switch (sec) {
    case 0:
      bits = 0b00;  // disable watchdog
      break;
    case 40:  bits = 0b01; break;
    case 80:  bits = 0b10; break;
    case 160: bits = 0b11; break;
    default:  bits = 0b11; break;
  }
  // bits 6-5 set watchdog timer
  reg_value &= ~(0b11 << 5);
  reg_value |= (bits << 5);
  writeRegister(AW32001_I2C_ADDR, AW3200_TIMER_WD, reg_value);
}

void NessoBattery::feedWatchdog() {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  // bit 6 set feed watchdog
  writeBitRegister(AW32001_I2C_ADDR, AW3200_CHG_CURRENT, 6, true);
}

void NessoBattery::setShipMode(bool en) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  // bit 5 set ship mode
  writeBitRegister(AW32001_I2C_ADDR, AW3200_MAIN_CTRL, 5, en);
}

NessoBattery::ChargeStatus NessoBattery::getChargeStatus() {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  uint8_t status = readRegister(AW32001_I2C_ADDR, AW3200_SYS_STATUS);
  // bits 4-3 set charge status
  uint8_t charge_status = (status >> 3) & 0b11;
  return static_cast<ChargeStatus>(charge_status);
}

void NessoBattery::setHiZ(bool enable) {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  // bit 4 set Hi-Z mode
  writeBitRegister(AW32001_I2C_ADDR, AW3200_POWER_ON_CFG, 4, enable);
}

float NessoBattery::getVoltage() {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  uint16_t voltage = (readRegister(BQ27220_I2C_ADDR, BQ27220_VOLTAGE + 1) << 8) | readRegister(BQ27220_I2C_ADDR, BQ27220_VOLTAGE);
  return (float)voltage / 1000.0f;
}

float NessoBattery::getCurrent() {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  int16_t current = (readRegister(BQ27220_I2C_ADDR, BQ27220_CURRENT + 1) << 8) | readRegister(BQ27220_I2C_ADDR, BQ27220_CURRENT);
  return (float)current / 1000.0f;
}

uint16_t NessoBattery::getChargeLevel() {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  uint16_t current_capacity = readRegister(BQ27220_I2C_ADDR, BQ27220_REMAIN_CAPACITY + 1) << 8 | readRegister(BQ27220_I2C_ADDR, BQ27220_REMAIN_CAPACITY);
  uint16_t total_capacity = readRegister(BQ27220_I2C_ADDR, BQ27220_FULL_CAPACITY + 1) << 8 | readRegister(BQ27220_I2C_ADDR, BQ27220_FULL_CAPACITY);
  return (current_capacity * 100) / total_capacity;
}

uint16_t NessoBattery::getAvgPower() {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  uint16_t avg_power = readRegister(BQ27220_I2C_ADDR, BQ27220_AVG_POWER + 1) << 8 | readRegister(BQ27220_I2C_ADDR, BQ27220_AVG_POWER);
  return avg_power;
}

float NessoBattery::getTemperature() {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  uint16_t temp = readRegister(BQ27220_I2C_ADDR, BQ27220_TEMPERATURE + 1) << 8 | readRegister(BQ27220_I2C_ADDR, BQ27220_TEMPERATURE);
  return ((float)temp / 10.0f) - 273.15f;
}

uint16_t NessoBattery::getCycleCount() {
  if (!wireInitialized) {
    WireInternal.begin(SDA, SCL);
    wireInitialized = true;
  }
  uint16_t cycle_count = readRegister(BQ27220_I2C_ADDR, BQ27220_CYCLE_COUNT + 1) << 8 | readRegister(BQ27220_I2C_ADDR, BQ27220_CYCLE_COUNT);
  return cycle_count;
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
