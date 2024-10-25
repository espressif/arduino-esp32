#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// BN: ESP32 Family Device
#define USB_VID 0x303a
#define USB_PID 0x8242

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-LCD-1.9"
#define USB_SERIAL       ""


// UART0 pins
static const uint8_t TX = 43;
static const uint8_t RX = 44;

//esp32s3-PSFlash   SPI1/SPI0
static const uint8_t SS = 34;    // FSPICS0
static const uint8_t MOSI = 35;  // FSPID
static const uint8_t MISO = 37;  // FSPIQ
static const uint8_t SCK = 36;   // FSPICLK
#endif                           /* Pins_Arduino_h */