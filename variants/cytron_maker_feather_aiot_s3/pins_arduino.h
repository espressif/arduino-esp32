#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID            0x303A
#define USB_PID            0x80F8
#define USB_MANUFACTURER   "Cytron"
#define USB_PRODUCT        "Maker Feather AIoT S3"
#define USB_SERIAL         ""

static const uint8_t LED_BUILTIN = 2;                           // Status LED.
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
static const uint8_t RGB_BUILTIN = SOC_GPIO_PIN_COUNT + 46;     // RGB LED.
#define RGB_BUILTIN RGB_BUILTIN  // necessary to make digitalWrite/digitalMode find it
#define RGB_BRIGHTNESS 64

#define LED             LED_BUILTIN
#define RGB             RGB_BUILTIN
#define NEOPIXEL        RGB_BUILTIN
#define RGB_BRIGHTNESS  65



#define VP_EN               11      // V Peripheral Enable.
#define BUZZER              12      // Piezo Buzzer.
#define BOOT                0       // Boot Button.
#define BUTTON              3       // User Button.

#define VIN                 13      // Vin Sense.
#define VBATT               13
#define VOLTAGE_MONITOR     13


static const uint8_t TX = 15;
static const uint8_t RX = 16;

static const uint8_t SDA = 42;
static const uint8_t SCL = 41;

static const uint8_t SS    = 7;
static const uint8_t MOSI  = 8;
static const uint8_t SCK   = 17;
static const uint8_t MISO  = 18;

static const uint8_t A0 = 10;
static const uint8_t A1 = 9;
static const uint8_t A2 = 6;
static const uint8_t A3 = 5;
static const uint8_t A4 = 4;
static const uint8_t A5 = 7;

static const uint8_t A6 = 17;
static const uint8_t A7 = 8;
static const uint8_t A8 = 18;
static const uint8_t A9 = 16;
static const uint8_t A10 = 15;
static const uint8_t A11 = 14;
static const uint8_t A12 = 13;

static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T14 = 14;

#endif /* Pins_Arduino_h */
