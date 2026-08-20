#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc.h"

// === Edge Dalton (ESP32-C3) Custom Pinout ===
static const uint8_t LED_BUILTIN = 10;
#define BUILTIN_LED  LED_BUILTIN 

// Default I2C Pins 
static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

// Default SPI Pins
static const uint8_t SS    = 7;
static const uint8_t MOSI  = 6;
static const uint8_t MISO  = 5;
static const uint8_t SCK   = 4;

// Hardware Serial UART0 
static const uint8_t TX = 21;
static const uint8_t RX = 20;

// Analog Pins (ADC1)
static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;

// Analog Pins (ADC2 - Cannot be used when Wi-Fi is active)
static const uint8_t A5 = 5;

#endif /* Pins_Arduino_h */