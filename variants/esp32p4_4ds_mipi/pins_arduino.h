#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// Use default UART0 pins
static const uint8_t TX = 37;
static const uint8_t RX = 38;

// Default pins (7 and 8) are used by on-board components already,
// for libraries, this can be set manually
// so let's keep the default for the user
static const uint8_t SDA = 2;  // careful, also used as T0 pin
static const uint8_t SCL = 3;  // careful, also used as T1 pin

static const uint8_t SCK = 6;    // careful, also used as T2 pin
static const uint8_t MOSI = 14;  // careful, also used as T1 pin
static const uint8_t MISO = 15;  // careful, also used as T0 pin
static const uint8_t SS = 16;    // careful, also used as A9 pin

static const uint8_t A0 = 21;
static const uint8_t A1 = 20;
static const uint8_t A2 = 19;
static const uint8_t A3 = 18;
static const uint8_t A4 = 17;
static const uint8_t A5 = 52;
static const uint8_t A6 = 51;
static const uint8_t A7 = 50;
static const uint8_t A8 = 49;
static const uint8_t A9 = 16;  // careful, also used as SPI SS pin

static const uint8_t T0 = 15;  // careful, also used as SPI MISO pin
static const uint8_t T1 = 14;  // careful, also used as SPI MOSI pin
static const uint8_t T2 = 6;   // careful, also used as SPI SCK pin
static const uint8_t T3 = 3;   // careful, also used as I2C SCL pin
static const uint8_t T4 = 2;   // careful, also used as I2C SDA pin

/* 4D Systems ESP32-P4 board specific definitions */
// LCD
#define LCD_INTERFACE_MIPI

#define LCD_BL_IO        22
#define LCD_BL_ON_LEVEL  1
#define LCD_BL_OFF_LEVEL !LCD_BL_ON_LEVEL

#define LCD_RST_IO          23
#define LCD_RST_ACTIVE_HIGH true

// I2C for on-board components
#define I2C_SDA 7
#define I2C_SCL 8

// Touch
#define CTP_RST 4
#define CTP_INT 5

// Audio
#define AMP_CTRL   53
#define I2S_DSDIN  9
#define I2S_LRCK   10
#define I2S_ASDOUT 11
#define I2S_SCLK   12
#define I2S_MCLK   13

// SDMMC
#define BOARD_HAS_SDMMC
#define BOARD_SDMMC_SLOT           0
#define BOARD_SDMMC_POWER_CHANNEL  4
#define BOARD_SDMMC_POWER_PIN      45
#define BOARD_SDMMC_POWER_ON_LEVEL LOW

#endif /* Pins_Arduino_h */
