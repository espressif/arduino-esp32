#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x303A
#define USB_PID          0x1001
#define USB_MANUFACTURER "Arduino"
#define USB_PRODUCT      "Nesso N1"
#define USB_SERIAL       ""

static const uint8_t TX = -1;
static const uint8_t RX = -1;

static const uint8_t SDA = 10;
static const uint8_t SCL = 8;

static const uint8_t MOSI = 21;
static const uint8_t MISO = 22;
static const uint8_t SCK = 20;
static const uint8_t SS = 23;

static const uint8_t D1 = 7;
static const uint8_t D2 = 2;
static const uint8_t D3 = 6;

static const uint8_t IR_TX_PIN = 9;
static const uint8_t BEEP_PIN = 11;

static const uint8_t GROVE_IO_0 = 5;
static const uint8_t GROVE_IO_1 = 4;

static const uint8_t LORA_IRQ = 15;
static const uint8_t LORA_CS = 23;
static const uint8_t LORA_BUSY = 19;

static const uint8_t SYS_IRQ = 3;

static const uint8_t LCD_CS = 17;
static const uint8_t LCD_RS = 16;

#if !defined(MAIN_ESP32_HAL_GPIO_H_) && defined(__cplusplus)
/* address: 0x43/0x44 */
class ExpanderPin {
public:
  ExpanderPin(uint16_t _pin) : pin(_pin & 0xFF), address(_pin & 0x100 ? 0x44 : 0x43){};
  uint8_t pin;
  uint8_t address;
};

class NessoBattery {
public:
  static constexpr uint8_t AW32001_I2C_ADDR = 0x49;
  static constexpr uint8_t BQ27220_I2C_ADDR = 0x55;

  enum AW32001Reg : uint8_t {
    AW3200_INPUT_SRC = 0x00,
    AW3200_POWER_ON_CFG = 0x01,
    AW3200_CHG_CURRENT = 0x02,
    AW3200_TERM_CURRENT = 0x03,
    AW3200_CHG_VOLTAGE = 0x04,
    AW3200_TIMER_WD = 0x05,
    AW3200_MAIN_CTRL = 0x06,
    AW3200_SYS_CTRL = 0x07,
    AW3200_SYS_STATUS = 0x08,
    AW3200_FAULT_STATUS = 0x09,
    AW3200_CHIP_ID = 0x0A,
  };

  enum BQ27220Reg : uint8_t {
    BQ27220_VOLTAGE = 0x08,
    BQ27220_CURRENT = 0x0C,
    BQ27220_REMAIN_CAPACITY = 0x10,
    BQ27220_FULL_CAPACITY = 0x12,
    BQ27220_AVG_POWER = 0x24,
    BQ27220_TEMPERATURE = 0x28,
    BQ27220_CYCLE_COUNT = 0x2A,
  };

  enum ChargeStatus {
    NOT_CHARGING = 0,
    PRE_CHARGE = 1,
    CHARGING = 2,
    FULL_CHARGE = 3,
  };

  enum UnderVoltageLockout {
    UVLO_2430mV = 0,
    UVLO_2490mV = 1,
    UVLO_2580mV = 2,
    UVLO_2670mV = 3,
    UVLO_2760mV = 4,
    UVLO_2850mV = 5,
    UVLO_2940mV = 6,
    UVLO_3030mV = 7,
  };

  NessoBattery(){};
  void begin(
    uint16_t current = 256, uint16_t voltage = 4200, UnderVoltageLockout uvlo = UVLO_2760mV, uint8_t timeout = 0
  );  // default: 256, 4200mV, 2760mV, disable watchdog

  // AW32001 functions
  void enableCharge();                         // enable charging
  void setChargeEnable(bool enable);           // charge control
  void setBatUVLO(UnderVoltageLockout uvlo);   // set battery under voltage lockout(2430mV, 2490mV, 2580mV, 2670mV, 2760mV, 2850mV, 2940mV, 3030mV)
  void setChargeCurrent(uint16_t current);     // set charging current, 8mA ~ 456mA(step 8mA, default 128mA)
  void setDischargeCurrent(uint16_t current);  // set discharging current, 200mA ~ 3200mA(step 200mA, default 2000mA)
  void setChargeVoltage(uint16_t voltage);     // set charging voltage, 3600mV ~ 4545mV(step 15mV, default 4200mV)
  void setWatchdogTimer(uint8_t sec);          // set charge watchdog timeout(0s, 40s, 80s, 160s, default 160s, 0 to disable)
  void feedWatchdog();                         // feed watchdog timer
  void setShipMode(bool en);                   // set ship mode
  ChargeStatus getChargeStatus();              // get charge status
  void setHiZ(bool enable);                    // set Hi-Z mode, true: USB -x-> SYS, false: USB -> SYS

  // BQ27220 functions
  float getVoltage();         // get battery voltage in Volts
  float getCurrent();         // get battery current in Amperes
  uint16_t getChargeLevel();  // get battery charge level in percents
  uint16_t getAvgPower();     // get average power in mWatts
  float getTemperature();     // get battery temperature in Celsius
  uint16_t getCycleCount();   // get battery cycle count
};

extern ExpanderPin LORA_LNA_ENABLE;
extern ExpanderPin LORA_ANTENNA_SWITCH;
extern ExpanderPin LORA_ENABLE;
extern ExpanderPin POWEROFF;
extern ExpanderPin GROVE_POWER_EN;
extern ExpanderPin VIN_DETECT;
extern ExpanderPin LCD_RESET;
extern ExpanderPin LCD_BACKLIGHT;
extern ExpanderPin LED_BUILTIN;
extern ExpanderPin KEY1;
extern ExpanderPin KEY2;

void pinMode(ExpanderPin pin, uint8_t mode);
void digitalWrite(ExpanderPin pin, uint8_t val);
int digitalRead(ExpanderPin pin);
#endif

#endif /* Pins_Arduino_h */
