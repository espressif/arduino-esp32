#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 15;
static const uint8_t SCL = 16;

static const uint8_t SS = 4;
static const uint8_t MISO = 5;
static const uint8_t MOSI = 6;
static const uint8_t SCK = 7;
static const uint8_t SD_SS = 42;

// Analog
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 8;

// Touch
static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;

// Ethernet
#define ETH_PHY_TYPE     ETH_PHY_W5500
#define ETH_PHY_ADDR     1
#define ETH_PHY_CS       9
#define ETH_PHY_IRQ      13
#define ETH_PHY_RST      14
#define ETH_PHY_SPI_HOST SPI2_HOST
#define ETH_PHY_SPI_SCK  10
#define ETH_PHY_SPI_MISO 11
#define ETH_PHY_SPI_MOSI 12

#endif /* Pins_Arduino_h */
