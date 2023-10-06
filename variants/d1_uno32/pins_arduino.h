#ifndef Pins_Arduino_h
#define Pins_Arduino_h

// Board Pinmap: https://www.botnroll.com/en/esp/3639-wemos-d1-r32-w-esp32-uno-r3-pinout.html

#include <stdint.h>

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS    = 5;
static const uint8_t MOSI  = 23;
static const uint8_t MISO  = 19;
static const uint8_t SCK   = 18;

static const uint8_t A0 = 2;
static const uint8_t A1 = 4;
static const uint8_t A2 = 35;
static const uint8_t A3 = 34;
static const uint8_t A4 = 36;
static const uint8_t A5 = 39;

static const uint8_t LED_BUILTIN = 2;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

#define PIN_WIRE_SDA SDA // backward compatibility
#define PIN_WIRE_SCL SCL // backward compatibility

static const uint8_t D0   = 3;
static const uint8_t D1   = 1;
static const uint8_t D2   = 26;
static const uint8_t D3   = 25;
static const uint8_t D4   = 17;
static const uint8_t D5   = 16;
static const uint8_t D6   = 27;
static const uint8_t D7   = 14;
static const uint8_t D8   = 12;
static const uint8_t D9   = 13;
static const uint8_t D10  = 5;
static const uint8_t D11  = 23;
static const uint8_t D12  = 19;
static const uint8_t D13  = 18;

#define PIN_SPI_SS   SS   // backward compatibility
#define PIN_SPI_MOSI MOSI // backward compatibility
#define PIN_SPI_MISO MISO // backward compatibility
#define PIN_SPI_SCK  SCK  // backward compatibility

#define PIN_A0 A0 // backward compatibility

#endif /* Pins_Arduino_h */
