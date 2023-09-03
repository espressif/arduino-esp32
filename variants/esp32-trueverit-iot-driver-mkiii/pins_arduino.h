#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

static const uint8_t LED_BUILTIN = 18;
#define BUILTIN_LED LED_BUILTIN // backward compatibility

#define NUM_DIGITAL_PINS        SOC_GPIO_PIN_COUNT    // GPIO 0..39
#define NUM_ANALOG_INPUTS       16                    // ESP32 has 16 ADC pins
#define EXTERNAL_NUM_INTERRUPTS NUM_DIGITAL_PINS      // All GPIOs

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):NOT_AN_INTERRUPT)
#define digitalPinHasPWM(p)         (p < 34)  // PWM only for GPIO0..33 - NOT GPIO 34,35,36 and 39

#define TX1 12
#define RX1 13
#define TX2 33
#define RX2 39

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SCL = 4;
static const uint8_t SDA = 15;

static const uint8_t SS = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 32;
static const uint8_t SCK = 18;

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

#define ETH_PHY_ADDR 0
#define ETH_PHY_POWER 2
#define ETH_PHY_MDC 16
#define ETH_PHY_MDIO 14
#define ETH_PHY_TYPE ETH_PHY_RTL8201
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT

#endif /* Pins_Arduino_h */
