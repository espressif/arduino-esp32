#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t LED_BUILTIN = ;//<----------------TODO: find it
#define BUILTIN_LED  LED_BUILTIN // backward compatibility


static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

//SPI
static const uint8_t IO5    = 5;
static const uint8_t IO23  = 23;
static const uint8_t IO19  = 19;
static const uint8_t IO18   = 18;

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
//DIGITAL+
static const uint8_t SD2 = ;
static const uint8_t SD3 = ;
static const uint8_t CMD = ;
static const uint8_t CLK = ;
static const uint8_t SD0 = ;
static const uint8_t SD1 = ;

#endif /* Pins_Arduino_h */
