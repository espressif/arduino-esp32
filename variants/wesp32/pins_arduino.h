#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SCL = 4;
static const uint8_t SDA = 2;

static const uint8_t SS    = 5;
static const uint8_t MOSI  = 23;
static const uint8_t MISO  = 32;
static const uint8_t SCK   = 18;

static const uint8_t A0 = 36;
static const uint8_t A3 = 39;
static const uint8_t A4 = 32;
static const uint8_t A5 = 33;
static const uint8_t A6 = 34;
static const uint8_t A7 = 35;

static const uint8_t T0 = 4;
static const uint8_t T2 = 2;
static const uint8_t T8 = 33;
static const uint8_t T9 = 32;

#define ETH_PHY_ADDR  0
#define ETH_PHY_POWER -1
#define ETH_PHY_MDC   16
#define ETH_PHY_MDIO  17
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN

#endif /* Pins_Arduino_h */
