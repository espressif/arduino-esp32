#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x303A
#define USB_PID          0x817F
#define USB_MANUFACTURER "Unexpected Maker"
#define USB_PRODUCT      "BLING!"
#define USB_SERIAL       ""

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS = 21;
static const uint8_t MOSI = 35;
static const uint8_t MISO = 37;
static const uint8_t SDO = 35;
static const uint8_t SDI = 37;
static const uint8_t SCK = 36;

static const uint8_t SD_CS = 21;
static const uint8_t SD_DETECT = 38;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 8;
static const uint8_t A8 = 9;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;

static const uint8_t BUTTON_A = 11;
static const uint8_t BUTTON_B = 10;
static const uint8_t BUTTON_C = 33;
static const uint8_t BUTTON_D = 34;

static const uint8_t VBAT_SENSE = 17;
static const uint8_t VBUS_SENSE = 16;

static const uint8_t I2S_MIC_SEL = 39;
static const uint8_t I2S_MIC_WS = 40;
static const uint8_t I2S_MIC_DATA = 41;
static const uint8_t I2S_MIC_BCLK = 42;

static const uint8_t I2S_AMP_SD = 4;
static const uint8_t I2S_AMP_DATA = 3;
static const uint8_t I2S_AMP_BCLK = 2;
static const uint8_t I2S_AMP_WS = 1;

static const uint8_t RTC_INT = 7;

static const uint8_t RGB_DATA = 18;
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    (RGB_DATA + SOC_GPIO_PIN_COUNT)
#define RGB_BRIGHTNESS 64
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = RGB_BUILTIN;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t RGB_PWR = 6;

#endif /* Pins_Arduino_h */
