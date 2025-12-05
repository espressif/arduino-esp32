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

#if !defined(MAIN_ESP32_HAL_GPIO_H_) && defined(__cplusplus) /* && !defined(ARDUINO_CORE_BUILD) */

#define ATTRIBUTE_ERROR __attribute__((error("Please include Arduino_Nesso_N1.h")))

class ExpanderPinError {
public:
  ExpanderPinError(uint16_t p) {};
};

void ATTRIBUTE_ERROR pinMode(ExpanderPinError pin, uint8_t mode);
void ATTRIBUTE_ERROR digitalWrite(ExpanderPinError pin, uint8_t val);
int ATTRIBUTE_ERROR digitalRead(ExpanderPinError pin);

extern ExpanderPinError _LORA_LNA_ENABLE;
extern ExpanderPinError _LORA_ANTENNA_SWITCH;
extern ExpanderPinError _LORA_ENABLE;
extern ExpanderPinError _POWEROFF;
extern ExpanderPinError _GROVE_POWER_EN;
extern ExpanderPinError _VIN_DETECT;
extern ExpanderPinError _LCD_RESET;
extern ExpanderPinError _LCD_BACKLIGHT;
extern ExpanderPinError _LED_BUILTIN;
extern ExpanderPinError _KEY1;
extern ExpanderPinError _KEY2;

#define LORA_LNA_ENABLE _LORA_LNA_ENABLE
#define LORA_ANTENNA_SWITCH _LORA_ANTENNA_SWITCH
#define LORA_ENABLE _LORA_ENABLE
#define POWEROFF _POWEROFF
#define GROVE_POWER_EN _GROVE_POWER_EN
#define VIN_DETECT _VIN_DETECT
#define LCD_RESET _LCD_RESET
#define LCD_BACKLIGHT _LCD_BACKLIGHT
#define LED_BUILTIN _LED_BUILTIN
#define KEY1 _KEY1
#define KEY2 _KEY2

#endif

#endif /* Pins_Arduino_h */
