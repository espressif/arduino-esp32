#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SCL = 32;
static const uint8_t SDA = 33;

static const uint8_t SS = 15;
static const uint8_t MOSI = 13;
static const uint8_t MISO = 12;
static const uint8_t SCK = 14;

static const uint8_t A0 = 36;
static const uint8_t A1 = 37;
static const uint8_t A2 = 38;
static const uint8_t A3 = 39;
static const uint8_t A6 = 34;
static const uint8_t A7 = 35;

static const uint8_t T0 = 4;
static const uint8_t T2 = 2;
static const uint8_t T3 = 15;
static const uint8_t T4 = 13;
static const uint8_t T5 = 12;
static const uint8_t T6 = 14;

#define ETH_PHY_ADDR  0
#define ETH_PHY_POWER -1
#define ETH_PHY_MDC   8
#define ETH_PHY_MDIO  7
#define ETH_PHY_TYPE  ETH_PHY_LAN867X
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN

#endif /* Pins_Arduino_h */
