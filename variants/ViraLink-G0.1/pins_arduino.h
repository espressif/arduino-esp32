#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = 5;
#define BUILTIN_LED LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN

static const uint8_t RESET_KEY = 0;

static const uint8_t RS485_TX = 32;
static const uint8_t RS485_RX = 35;

static const uint8_t SDA = 4;
static const uint8_t SCL = 16;

static const uint8_t BUZZER = 12;
static const uint8_t RELAY1_PIN = 2;
static const uint8_t RELAY2_PIN = 13;
static const uint8_t RELAY3_PIN = 14;
static const uint8_t RELAY4_PIN = 33;

static const uint8_t Wiegand1_D0 = 15;
static const uint8_t Wiegand1_D1 = 34;

static const uint8_t Wiegand2_D0 = 39;
static const uint8_t Wiegand2_D1 = 36;

static const uint8_t ETH_CLK_OUT = 17;

static const uint8_t EMAC_MDIO = 18;
static const uint8_t EMAC_TXD0 = 19;
static const uint8_t EMAC_TX_EN = 21;
static const uint8_t EMAC_TXD1 = 22;
static const uint8_t EMAC_MDC = 23;
static const uint8_t EMAC_RXD0 = 25;
static const uint8_t EMAC_RXD1 = 26;
static const uint8_t EMAC_RXD_DV = 27;

static const uint8_t SS = -1;
static const uint8_t MOSI = -1;
static const uint8_t SCK = -1;
static const uint8_t MISO = -1;

#endif /* Pins_Arduino_h */
