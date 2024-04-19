#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x1B4F
#define USB_PID          0x0035
#define USB_MANUFACTURER "SparkFun"
#define USB_PRODUCT      "SparkFun_Pro_Micro-ESP32C3"
#define USB_SERIAL       ""  // Empty string for MAC address

static const uint8_t LED_BUILTIN = 10;

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;

static const uint8_t D0 = 0;
static const uint8_t D1 = 1;
static const uint8_t D2 = 2;
static const uint8_t D3 = 3;
static const uint8_t D4 = 4;
static const uint8_t D5 = 5;
static const uint8_t D6 = 6;
static const uint8_t D7 = 7;
static const uint8_t D8 = 8;
static const uint8_t D9 = 9;
static const uint8_t D10 = 10;

static const uint8_t SDA = 5;
static const uint8_t SCL = 6;

static const uint8_t SS = 10;
static const uint8_t MOSI = 3;
static const uint8_t MISO = 1;
static const uint8_t SCK = 0;

static const uint8_t PIN_I2S_SCK = 6;      // Frame clock, no bit clock
static const uint8_t PIN_I2S_SD_DOUT = 7;  // data out
static const uint8_t PIN_I2S_SD_IN = 5;    // data in
static const uint8_t PIN_I2S_FS = 10;      // frame select

#endif /* Pins_Arduino_h */
