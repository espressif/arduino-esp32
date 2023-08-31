#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define NUM_DIGITAL_PINS        SOC_GPIO_PIN_COUNT    // GPIO 0..39
#define NUM_ANALOG_INPUTS       16                    // ESP32 has 16 ADC pins
#define EXTERNAL_NUM_INTERRUPTS NUM_DIGITAL_PINS      // All GPIOs

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):NOT_AN_INTERRUPT)
#define digitalPinHasPWM(p)         (p < 34)  // PWM only for GPIO0..33 - NOT GPIO 34,35,36 and 39


static const uint8_t KEY_BUILTIN = 34;

static const uint8_t TX = 1;
static const uint8_t RX = 3;

#define TX1 4
#define RX1 36

static const uint8_t SDA = 13;
static const uint8_t SCL = 16;

static const uint8_t SS    = 17;
static const uint8_t MOSI  = 2;
static const uint8_t MISO  = 15;
static const uint8_t SCK   = 14;

#define BOARD_HAS_1BIT_SDMMC
#define BOARD_MAX_SDMMC_FREQ SDMMC_FREQ_DEFAULT

#endif /* Pins_Arduino_h */
