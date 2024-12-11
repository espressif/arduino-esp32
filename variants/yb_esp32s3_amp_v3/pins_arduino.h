#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303A
#define USB_PID 0x1001

static const uint8_t LED_BUILTIN = 47;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

//I2S for onboard MAX98357A only
static const uint8_t I2S_BCLK = 5;
static const uint8_t I2S_LRCLK = 6;
static const uint8_t I2S_DOUT = 7;

// SPI for onboard microSD only
static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

// SPI2 for public usage
static const uint8_t SS2 = 38;
static const uint8_t MOSI2 = 39;
static const uint8_t MISO2 = 41;
static const uint8_t SCK2 = 40;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 8;
static const uint8_t A5 = 9;
static const uint8_t A6 = 10;
static const uint8_t A7 = 14;
static const uint8_t A8 = 15;
static const uint8_t A9 = 16;
static const uint8_t A10 = 17;
static const uint8_t A11 = 18;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T14 = 14;

#define PIN_DAC_MUTE 47  // only if solder bridge "DAC_MUTE" is closed

#endif /* Pins_Arduino_h */
