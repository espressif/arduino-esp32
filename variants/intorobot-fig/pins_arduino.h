#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define NUM_DIGITAL_PINS        SOC_GPIO_PIN_COUNT    // GPIO 0..39
#define NUM_ANALOG_INPUTS       16                    // ESP32 has 16 ADC pins
#define EXTERNAL_NUM_INTERRUPTS NUM_DIGITAL_PINS      // All GPIOs

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):NOT_AN_INTERRUPT)
#define digitalPinHasPWM(p)         (p < 34)  // PWM only for GPIO0..33 - NOT GPIO 34,35,36 and 39

static const uint8_t LED_BUILTIN = 4;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility

static const uint8_t RGB_R_BUILTIN = 27;
static const uint8_t RGB_G_BUILTIN = 21;
static const uint8_t RGB_B_BUILTIN = 22;

static const uint8_t KEY_BUILTIN = 0;

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 23;
static const uint8_t SCL = 19;

static const uint8_t SS    = 5;
static const uint8_t MOSI  = 16;
static const uint8_t MISO  = 17;
static const uint8_t SCK   = 18;

static const uint8_t A0 = 36;
static const uint8_t A1 = 39;
static const uint8_t A2 = 35;
static const uint8_t A3 = 25;
static const uint8_t A4 = 26;
static const uint8_t A5 = 14;
static const uint8_t A6 = 12;
static const uint8_t A7 = 15;
static const uint8_t A8 = 13;
static const uint8_t A9 = 2;

static const uint8_t D0 = 19;
static const uint8_t D1 = 23;
static const uint8_t D2 = 18;
static const uint8_t D3 = 17;
static const uint8_t D4 = 16;
static const uint8_t D5 = 5;
static const uint8_t D6 = 4;

static const uint8_t T0 = 19;
static const uint8_t T1 = 23;
static const uint8_t T2 = 18;
static const uint8_t T3 = 17;
static const uint8_t T4 = 16;
static const uint8_t T5 = 5;
static const uint8_t T6 = 4;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
