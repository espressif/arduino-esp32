#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define ETH_PHY_TYPE ETH_PHY_LAN8720
#define ETH_PHY_ADDR 0
#define ETH_PHY_MDC 23
#define ETH_PHY_MDIO 18
#define ETH_PHY_POWER 12
#if defined BOARD_HAS_PSRAM  // when PSRAM is enabled pins 16 and 17 are used for the PSRAM and alternative pins are used for respectively I2C SCL and Ethernet Clock GPIO
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_OUT
#else
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#endif


static const uint8_t KEY_BUILTIN = 34;

static const uint8_t TX = 1;
static const uint8_t RX = 3;

#define TX1 4
#define RX1 36

#define TX2 33  // ext2 pin 5
#define RX2 35  // ext2 pin 3

static const uint8_t SDA = 13;
#if defined BOARD_HAS_PSRAM  // when PSRAM is enabled pins 16 and 17 are used for the PSRAM and alternative pins are used for respectively I2C SCL and Ethernet Clock GPIO
static const uint8_t SCL = 33;
#else
static const uint8_t SCL = 16;
#endif

static const uint8_t SS = 5;
static const uint8_t MOSI = 2;
static const uint8_t MISO = 15;
static const uint8_t SCK = 14;

#define BOARD_HAS_1BIT_SDMMC

#endif /* Pins_Arduino_h */
