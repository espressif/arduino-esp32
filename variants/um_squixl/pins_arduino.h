#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x303A
#define USB_PID          0x82DF
#define USB_MANUFACTURER "Unexpected Maker"
#define USB_PRODUCT      "SQUiXL"
#define USB_SERIAL       ""

static const uint8_t SDA = 1;
static const uint8_t SCL = 2;

static const uint8_t SS = 42;
static const uint8_t MOSI = 46;
static const uint8_t MISO = 41;
static const uint8_t SDO = 46;
static const uint8_t SDI = 41;
static const uint8_t SCK = 45;

#endif /* Pins_Arduino_h */
