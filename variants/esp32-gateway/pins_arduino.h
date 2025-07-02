#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#if defined(ARDUINO_ESP32_GATEWAY_E) || defined(ARDUINO_ESP32_GATEWAY_F)
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  0
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_POWER 5
#define ETH_CLK_MODE  ETH_CLOCK_GPIO17_OUT
#endif

static const uint8_t LED_BUILTIN = 33;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t KEY_BUILTIN = 34;

static const uint8_t SCL = 16;  // This is extension pin 11
static const uint8_t SDA = 32;  // This is extension pin 13

static const uint8_t SS = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 18;

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t A0 = 36;
static const uint8_t A3 = 39;
static const uint8_t A4 = 32;
static const uint8_t A7 = 35;

static const uint8_t T9 = 32;

#if defined(ARDUINO_ESP32_GATEWAY_F)
#define BOARD_HAS_1BIT_SDMMC
#endif

#endif /* Pins_Arduino_h */
