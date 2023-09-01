#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define Wireless_Stick_Lite_V3 true
#define DISPLAY_HEIGHT 0
#define DISPLAY_WIDTH  0

#define NUM_DIGITAL_PINS        SOC_GPIO_PIN_COUNT    // GPIO 0..48
#define NUM_ANALOG_INPUTS       20                    // GPIO 1..20
#define EXTERNAL_NUM_INTERRUPTS NUM_DIGITAL_PINS      // All GPIOs

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):NOT_AN_INTERRUPT)
#define digitalPinHasPWM(p)         (p < NUM_DIGITAL_PINS)

static const uint8_t LED_BUILTIN = 35;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 2;
static const uint8_t SCL = 3;

static const uint8_t SS    = 34;
static const uint8_t MOSI  = 35;
static const uint8_t SCK   = 36;
static const uint8_t MISO  = 37;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 12;
static const uint8_t A8 = 14;
static const uint8_t A9 = 15;
static const uint8_t A10 = 16;
static const uint8_t A11 = 17;
static const uint8_t A12 = 18;
static const uint8_t A13 = 19;
static const uint8_t A14 = 20;

static const uint8_t T0 = 1;
static const uint8_t T1 = 2;
static const uint8_t T2 = 3;
static const uint8_t T3 = 4;
static const uint8_t T4 = 5;
static const uint8_t T5 = 6;
static const uint8_t T6 = 7;

static const uint8_t Vext = 36;
static const uint8_t LED  = 35;

#endif /* Pins_Arduino_h */
