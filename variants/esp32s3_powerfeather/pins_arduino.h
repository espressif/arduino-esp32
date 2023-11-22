#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID            0x303A
#define USB_PID            0x81BB
#define USB_MANUFACTURER   "PowerFeather"
#define USB_PRODUCT        "ESP32-S3 PowerFeather"
#define USB_SERIAL         ""

static const uint8_t ALARM = 7;
static const uint8_t INT =   5;

static const uint8_t LED =   46;
static const uint8_t BTN =   0;
static const uint8_t EN =    13;

static const uint8_t TX =    44;
static const uint8_t RX =    42;
static const uint8_t TX0 =   43;

static const uint8_t SS =    -1;
static const uint8_t MISO =  41;
static const uint8_t MOSI =  40;
static const uint8_t SCK =   39;

static const uint8_t SCL =   36;
static const uint8_t SDA =   35;

static const uint8_t A0 =    10;
static const uint8_t A1 =    9;
static const uint8_t A2 =    8;
static const uint8_t A3 =    3;
static const uint8_t A4 =    2;
static const uint8_t A5 =    1;

static const uint8_t D5 =    15;
static const uint8_t D6 =    16;
static const uint8_t D9 =    17;
static const uint8_t D10 =   18;
static const uint8_t D11 =   45;
static const uint8_t D12 =   12;
static const uint8_t D13 =   11;

#define LED_BUILTIN 46
#define BUILTIN_LED  LED_BUILTIN // backward compatibility

#endif /* Pins_Arduino_h */
