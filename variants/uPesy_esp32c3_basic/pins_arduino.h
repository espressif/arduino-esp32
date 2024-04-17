#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303A
#define USB_PID 0x8195
#define USB_MANUFACTURER "uPesy Electronics"
#define USB_PRODUCT "uPesy ESP32C3 Basic"
#define USB_SERIAL ""

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 3;
static const uint8_t SCL = 10;

static const uint8_t SS = 7;
static const uint8_t MOSI = 6;
static const uint8_t MISO = 5;
static const uint8_t SCK = 4;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;

static const uint8_t VBAT_SENSE = 0;

#endif /* Pins_Arduino_h */
