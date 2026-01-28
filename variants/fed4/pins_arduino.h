#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x303A
#define USB_PID          0x82E5
#define USB_MANUFACTURER "Smart Bee Designs LLC"
#define USB_PRODUCT      "FED4"
#define USB_SERIAL       ""

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;
static const uint8_t SDA2 = 20;
static const uint8_t SCL2 = 19;

static const uint8_t SS = 47;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;
static const uint8_t SDCS = 10;  // sd cs pin
static const uint8_t DSCS = 14;  //display cs pin

static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;
static const uint8_t A6 = 6;

static const uint8_t D1 = 1;
static const uint8_t D2 = 2;
static const uint8_t D3 = 3;
static const uint8_t D4 = 4;
static const uint8_t D5 = 5;
static const uint8_t D6 = 6;
static const uint8_t D8 = 8;
static const uint8_t D13 = 13;
static const uint8_t D9 = 9;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;

static const uint8_t BOOT_BTN = 0;
static const uint8_t VBAT_VOLTAGE = 7;
#define BAT_VOLT_PIN VBAT_VOLTAGE
static const uint8_t LDO2 = 47;
static const uint8_t STATUS_RGB = 35;
static const uint8_t RGB_STRIP = 36;
static const uint8_t INTERRUPT_PIN = 18;
static const uint8_t USER_BTN_1 = 14;
static const uint8_t USER_BTN_2 = 39;
static const uint8_t USER_BTN_3 = 40;
static const uint8_t AMP_DIN = 39;
static const uint8_t AMP_SD = 42;
static const uint8_t AMP_BCLK = 45;
static const uint8_t AMP_LRCLK = 48;
static const uint8_t MSBY = 15;
static const uint8_t TRRS_1 = 4;
static const uint8_t TRRS_2 = 2;
static const uint8_t TRRS_3 = 3;

#define PIN_RGB_LED STATUS_RGB
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

#endif /* Pins_Arduino_h */
