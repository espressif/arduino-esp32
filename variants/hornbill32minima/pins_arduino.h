#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t TX = 1;  //taken out on pgm header
static const uint8_t RX = 3;  //taken out on pgm header

static const uint8_t SDA = 21;  //1
static const uint8_t SCL = 22;  //2

static const uint8_t SS    = 2; //3
static const uint8_t MOSI  = 23; //4
static const uint8_t MISO  = 19; //5	
static const uint8_t SCK   = 18; //6



static const uint8_t A6 = 34;  //7
static const uint8_t A7 = 35;  //8 
static const uint8_t A10 = 4;  //9
static const uint8_t A11 = 0;  // taken out on pgm header
static const uint8_t A12 = 2;  // with SPI SS
static const uint8_t A13 = 15; //10
static const uint8_t A14 = 13; //11



static const uint8_t DAC1 = 25;  //12
static const uint8_t DAC2 = 26;  //13


static const uint8_t T0 = 4;  //used
static const uint8_t T1 = 0;  // taken out on pgm header
static const uint8_t T2 = 2;  //used
static const uint8_t T3 = 15; //used




#endif /* Pins_Arduino_h */
