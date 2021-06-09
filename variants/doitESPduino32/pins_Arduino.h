#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t LED_BUILTIN = 2;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility


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

// ESP-WROOM-32 does not have GPIO 14, 20(NC), 24, 28, 29, 30, 31, 36, 37, 38, 40+
// All pins should be PWM capable. The board is a clone of WeMos D1 R32.

#endif /* Pins_Arduino_h */
