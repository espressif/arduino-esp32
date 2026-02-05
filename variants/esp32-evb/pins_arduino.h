#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t KEY_BUILTIN = 34;

static const uint8_t TX = 1;
static const uint8_t RX = 3;

#define TX1 4
#define RX1 36

#define TX2 17
#define RX2 16

#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  0
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_POWER -1	                // no GPIO used to toggle power supply, always on
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN	  // clock supplied by CR1

static const uint8_t SDA = 13;
static const uint8_t SCL = 16;

static const uint8_t SS = 17;
static const uint8_t MOSI = 2;
static const uint8_t MISO = 15;
static const uint8_t SCK = 14;

#define BOARD_HAS_1BIT_SDMMC
#define BOARD_MAX_SDMMC_FREQ SDMMC_FREQ_DEFAULT

#endif /* Pins_Arduino_h */
