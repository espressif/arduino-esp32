#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t TX = 1;
static const uint8_t RX = 3;

// IO
static const uint8_t LED_BUILTIN = 5;
static const uint8_t SW1 = 4;
static const uint8_t SW2 = 18;
static const uint8_t SW3 = 19;
static const uint8_t SW4 = 21;

//I2S DAC
static const uint8_t I2S_MCLK = 2;    // CLOCK must be an integer multiplier of SCLK
static const uint8_t I2S_LRCLK = 25;  // LRCLK
static const uint8_t I2S_SCLK = 26;   // SCLK - Fs (44100 Hz)
static const uint8_t I2S_DOUT = 22;   // DATA

//GPIO
static const uint8_t D0 = 34; // GPI - Input Only
static const uint8_t D1 = 35; // GPI - Input Only
static const uint8_t D2 = 32; // GPO - Output Only
static const uint8_t D3 = 33; // GPO - Output Only
static const uint8_t D4 = 27;
static const uint8_t D5 = 14;
static const uint8_t D6 = 12;
static const uint8_t D7 = 13;
static const uint8_t D8 = 15;
static const uint8_t D9 = 23;
static const uint8_t D10 = 0;

// I2C BUS, 2k2 hardware pull-ups
static const uint8_t SDA = 16;
static const uint8_t SCL = 17;

// SPI - unused but you can create your own definition in your sketch
static const int8_t SCK = -1;
static const int8_t MISO = -1;
static const int8_t MOSI = -1;
static const int8_t SS = -1;

#endif /* Pins_Arduino_h */
