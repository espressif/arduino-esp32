#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = 2;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN


static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

//SPI
static const uint8_t IO5  = 5;  //SS
static const uint8_t IO23 = 23; //MOSI
static const uint8_t IO19 = 19; //MISO
static const uint8_t IO18 = 18; //SCK

static const uint8_t SS   = IO5;
static const uint8_t MOSI = IO23;
static const uint8_t MISO = IO19;
static const uint8_t SCK  = IO18;

//ANALOG
static const uint8_t IO36 = 36;
static const uint8_t IO39 = 39;
static const uint8_t IO4 = 4;
static const uint8_t IO2 = 2;
static const uint8_t IO35 = 35;
//ANALOG+
static const uint8_t IO15 = 15;
static const uint8_t IO33 = 33;
static const uint8_t IO32 = 32;
static const uint8_t IO0 = 0;

//DIGITAL
static const uint8_t IO13 = 13;
static const uint8_t IO12 = 12;
static const uint8_t IO14 = 14;
static const uint8_t IO27 = 27;
static const uint8_t IO16 = 16;
static const uint8_t IO17 = 17;
static const uint8_t IO25 = 25;
static const uint8_t IO26 = 26;
static const uint8_t TX0 = 1;
static const uint8_t RX0 = 3;
//TFlash(uSD)
static const uint8_t SD2 = 9;
static const uint8_t SD3 = 10;
static const uint8_t CMD = 11;
static const uint8_t CLK = 6;
static const uint8_t SD0 = 7;
static const uint8_t SD1 = 8;

//Arduino Uno backward compatibility
static const uint8_t A0 = 2;
static const uint8_t A1 = 4;
static const uint8_t A2 = 35;
static const uint8_t A3 = 34;
static const uint8_t A4 = 36;
static const uint8_t A5 = 39;

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

static const uint8_t TX = 1;
static const uint8_t RX = 3;

#define PIN_WIRE_SDA SDA // backward compatibility
#define PIN_WIRE_SCL SCL // backward compatibility

#define PIN_SPI_SS   SS   // backward compatibility
#define PIN_SPI_MOSI MOSI // backward compatibility
#define PIN_SPI_MISO MISO // backward compatibility
#define PIN_SPI_SCK  SCK  // backward compatibility

#define PIN_A0 A0 // backward compatibility

// ESP-WROOM-32 does not have GPIO 14, 20(NC), 24, 28, 29, 30, 31, 36, 37, 38, 40+
// All pins should be PWM capable. The board is a clone of WeMos D1 R32.

#endif /* Pins_Arduino_h */
