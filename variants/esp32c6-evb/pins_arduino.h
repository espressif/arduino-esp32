#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

static const uint8_t LED_BUILTIN = 8;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN  LED_BUILTIN // allow testing #ifdef LED_BUILTIN

static const uint8_t BUT_BUILTIN = 9;
#define BUILTIN_BUT  BUT_BUILTIN // backward compatibility
#define BUT_BUILTIN  BUT_BUILTIN // allow testing #ifdef BUT_BUILTIN

#define	REL1	10
#define	REL2	11
#define	REL3	22
#define	REL4	23

#define	DIN1	1
#define	DIN2	2
#define	DIN3	3
#define	DIN4	15

// available at UEXT and pUEXT +
static const uint8_t TX1 = 5;
static const uint8_t RX1 = 4;

static const uint8_t SDA = 6;
static const uint8_t SCL = 7;

static const uint8_t SS    = 21;
static const uint8_t MOSI  = 18;
static const uint8_t MISO  = 20;
static const uint8_t SCK   = 19;
// available at UEXT and pUEXT -

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;
static const uint8_t A6 = 6;

#endif /* Pins_Arduino_h */
