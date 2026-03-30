#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// power button
#define BUTTON 3

static const uint8_t RX = 20;

static const uint8_t SS = 21;
static const uint8_t MOSI = 10;
static const uint8_t MISO = 7;
static const uint8_t SCK = 8;

static const uint8_t EPD_RESET = 5;
static const uint8_t EPD_BUSY = 6;
static const uint8_t EPD_DC = 4;

static const uint8_t SD_CS = 12;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;

// not broken out
static const uint8_t SCL = -1;
static const uint8_t SDA = -1;

#endif /* Pins_Arduino_h */
