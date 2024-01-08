#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303A
#define USB_PID 0x810D
#define USB_MANUFACTURER "Smart Bee Designs"
#define USB_PRODUCT "Bee Motion S3"
#define USB_SERIAL ""

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 36;
static const uint8_t SCL = 37;

static const uint8_t SS    = 5;
static const uint8_t MOSI  = 16;
static const uint8_t MISO  = 38;
static const uint8_t SDO  = 35;
static const uint8_t SDI  = 37;
static const uint8_t SCK   = 15;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 8;
static const uint8_t A8 = 9;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;

static const uint8_t T1 = 1;
static const uint8_t T3 = 3;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T14 = 14;

static const uint8_t BOOT_BTN = 0;
static const uint8_t PIR = 5;



#endif /* Pins_Arduino_h */
