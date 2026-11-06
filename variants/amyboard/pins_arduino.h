#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0xCAF0
#define USB_PID          0x0001
#define USB_MANUFACTURER "AMYboard"
#define USB_PRODUCT      "AMYboard"
#define USB_SERIAL       ""

// UART (directly exposed on header)
static const uint8_t TX = 43;
static const uint8_t RX = 44;

// I2C master bus (PCM9211, OLED display, ADC, etc.)
static const uint8_t SDA = 17;
static const uint8_t SCL = 18;

// SPI (directly exposed on header for SD card, etc.)
static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

#endif /* Pins_Arduino_h */
