#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303a
#define USB_PID          0x822B
#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Zero"
#define USB_SERIAL       ""  // Empty string for MAC address

// Partial voltage measurement method
#define WS_RGB 21

// Mapping based on the ESP32S3 data sheet - alternate for OUTPUT
static const uint8_t OUTPUT_IO1 = 1;
static const uint8_t OUTPUT_IO2 = 2;
static const uint8_t OUTPUT_IO3 = 3;
static const uint8_t OUTPUT_IO4 = 4;
static const uint8_t OUTPUT_IO5 = 5;
static const uint8_t OUTPUT_IO6 = 6;
static const uint8_t OUTPUT_IO7 = 7;
static const uint8_t OUTPUT_IO8 = 8;
static const uint8_t OUTPUT_IO9 = 9;
static const uint8_t OUTPUT_IO10 = 10;
static const uint8_t OUTPUT_IO11 = 11;
static const uint8_t OUTPUT_IO12 = 12;
static const uint8_t OUTPUT_IO13 = 13;

// Def for I2C that shares the IMU I2C pins
static const uint8_t SDA = -1;
static const uint8_t SCL = -1;

// Mapping based on the ESP32S3 data sheet - alternate for SPI2
static const uint8_t SS = 34;    // FSPICS0
static const uint8_t MOSI = 35;  // FSPID
static const uint8_t MISO = 37;  // FSPIQ
static const uint8_t SCK = 36;   // FSPICLK

// UART0 pins
static const uint8_t TX = 43;
static const uint8_t RX = 44;

// Analog capable pins on the header
static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;

// GPIO capable pins on the header
static const uint8_t D0 = 7;
static const uint8_t D1 = 6;
static const uint8_t D2 = 5;
static const uint8_t D3 = 4;
static const uint8_t D4 = 3;
static const uint8_t D5 = 2;
static const uint8_t D6 = 1;
static const uint8_t D7 = 44;
static const uint8_t D8 = 43;
static const uint8_t D9 = 40;
static const uint8_t D10 = 39;
static const uint8_t D11 = 38;
static const uint8_t D12 = 37;
static const uint8_t D13 = 36;
static const uint8_t D14 = 35;
static const uint8_t D15 = 34;
static const uint8_t D16 = 33;

// Touch input capable pins on the header
static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;

#endif /* Pins_Arduino_h */
