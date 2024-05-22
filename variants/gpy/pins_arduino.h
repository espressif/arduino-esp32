#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// Sequans Monarch LTE Cat M1/NB1 modem
// NOTE: The Pycom pinout as well as spec sheet block diagram / pin details
// incorrectly list the LTE pins. The correct pins are defined in the source and CSV
// at https://github.com/pycom/pycom-micropython-sigfox/tree/master/esp32/boards/GPY.
#define LTE_CTS  18  // GPIO18 - Sequans modem CTS
#define LTE_RTS  19  // GPIO19 - Sequans modem RTS (pull low to communicate)
#define LTE_RX   23  // GPIO23 - Sequans modem RX
#define LTE_TX   5   // GPIO5  - Sequans modem TX
#define LTE_WAKE 27  // GPIO27 - Sequans modem wake-up interrupt
#define LTE_BAUD 921600

// Neopixel
#define PIN_NEOPIXEL 0  // ->2812 RGB !!!
static const uint8_t LED_BUILTIN = PIN_NEOPIXEL + SOC_GPIO_PIN_COUNT;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

#define ANT_SELECT 21  // GPIO21 - WiFi external / internal antenna switch

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 12;
static const uint8_t SCL = 13;

static const uint8_t SS = 17;
static const uint8_t MOSI = 22;
static const uint8_t MISO = 37;
static const uint8_t SCK = 13;

static const uint8_t A0 = 36;
static const uint8_t A1 = 37;
static const uint8_t A2 = 38;
static const uint8_t A3 = 39;
static const uint8_t A4 = 32;
static const uint8_t A5 = 33;
static const uint8_t A6 = 34;
static const uint8_t A7 = 35;
static const uint8_t A10 = 4;
static const uint8_t A11 = 0;
static const uint8_t A12 = 2;
static const uint8_t A13 = 15;
static const uint8_t A14 = 13;
static const uint8_t A15 = 12;
static const uint8_t A16 = 14;
static const uint8_t A17 = 27;
static const uint8_t A18 = 25;
static const uint8_t A19 = 26;

static const uint8_t T0 = 4;
static const uint8_t T1 = 0;
static const uint8_t T2 = 2;
static const uint8_t T3 = 15;
static const uint8_t T4 = 13;
static const uint8_t T5 = 12;
static const uint8_t T6 = 14;
static const uint8_t T7 = 27;
static const uint8_t T8 = 33;
static const uint8_t T9 = 32;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
