#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include <../d32/d32_core.h>

static const uint8_t LED_BUILTIN = 2;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN
static const uint8_t _VBAT = 35; // battery voltage

#define PIN_WIRE_SDA SDA // backward compatibility
#define PIN_WIRE_SCL SCL // backward compatibility

static const uint8_t D0   = 26;
static const uint8_t D1   = 22;
static const uint8_t D2   = 21;
static const uint8_t D3   = 17;
static const uint8_t D4   = 16;
static const uint8_t D5   = 18;
static const uint8_t D6   = 19;
static const uint8_t D7   = 23;
static const uint8_t D8   = 5;
static const uint8_t RXD  = 3;
static const uint8_t TXD  = 1;

#define PIN_SPI_SS   SS   // backward compatibility
#define PIN_SPI_MOSI MOSI // backward compatibility
#define PIN_SPI_MISO MISO // backward compatibility
#define PIN_SPI_SCK  SCK  // backward compatibility

#define PIN_A0 A0 // backward compatibility

#endif /* Pins_Arduino_h */
