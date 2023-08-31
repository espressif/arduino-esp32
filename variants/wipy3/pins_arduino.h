#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define NUM_DIGITAL_PINS        SOC_GPIO_PIN_COUNT    // GPIO 0..39
#define NUM_ANALOG_INPUTS       16                    // ESP32 has 16 ADC pins
#define EXTERNAL_NUM_INTERRUPTS NUM_DIGITAL_PINS      // All GPIOs

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):NOT_AN_INTERRUPT)
#define digitalPinHasPWM(p)         (p < 34)  // PWM only for GPIO0..33 - NOT GPIO 34,35,36 and 39

// Neopixel
#define PIN_NEOPIXEL 0 // ->2812 RGB !!!
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = (PIN_NEOPIXEL + SOC_GPIO_PIN_COUNT) 
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite()
#define RGB_BUILTIN (PIN_NEOPIXEL + SOC_GPIO_PIN_COUNT)
#define RGB_BRIGHTNESS 64

#define ANT_SELECT 21   // GPIO21 - External Antenna Switch

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 12;
static const uint8_t SCL = 13;

static const uint8_t SS    = 2;
static const uint8_t MOSI  = 22;
static const uint8_t MISO  = 37;
static const uint8_t SCK   = 13;

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
