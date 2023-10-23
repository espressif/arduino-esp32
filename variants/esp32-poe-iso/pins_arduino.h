#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_PHY_POWER 12

static const uint8_t KEY_BUILTIN = 34;

static const uint8_t TX = 1;
static const uint8_t RX = 3;

#define TX1 4
#define RX1 36

#define TX2 33	// ext2 pin 5
#define RX2 35	// ext2 pin 3

static const uint8_t SDA = 13;
static const uint8_t SCL = 16;

static const uint8_t SS    = 5;
static const uint8_t MOSI  = 2;
static const uint8_t MISO  = 15;
static const uint8_t SCK   = 14;

#define BOARD_HAS_1BIT_SDMMC

#endif /* Pins_Arduino_h */
