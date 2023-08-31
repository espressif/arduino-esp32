#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x239A
#define USB_PID 0x80B4
#define USB_MANUFACTURER "Unexpected Maker"
#define USB_PRODUCT "FeatherS2 Neo"
#define USB_SERIAL ""

#define NUM_DIGITAL_PINS        SOC_GPIO_PIN_COUNT    // GPIO 0..46
#define NUM_ANALOG_INPUTS       20                    // GPIO 1..20
#define EXTERNAL_NUM_INTERRUPTS NUM_DIGITAL_PINS      // All GPIOs

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):NOT_AN_INTERRUPT)
#define digitalPinHasPWM(p)         (p < NUM_DIGITAL_PINS)

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS    = 34;
static const uint8_t MOSI  = 35;
static const uint8_t MISO  = 37;
static const uint8_t SDO  = 35;
static const uint8_t SDI  = 37;
static const uint8_t SCK   = 36;

static const uint8_t A0 = 17;
static const uint8_t A1 = 18;
static const uint8_t A2 = 14;
static const uint8_t A3 = 12;
static const uint8_t A4 = 6;
static const uint8_t A5 = 5;
static const uint8_t A6 = 1;
static const uint8_t A7 = 3;
static const uint8_t A8 = 7;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;


static const uint8_t T1 = 1;
static const uint8_t T3 = 3;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T14 = 14;

static const uint8_t DAC1 = 17;
static const uint8_t DAC2 = 18;

static const uint8_t NEOPIXEL_MATRIX_DATA = 21;
static const uint8_t NEOPIXEL_MATRIX_PWR = 4;

static const uint8_t NEOPIXEL_DATA = 40;
static const uint8_t NEOPIXEL_PWR = 39;
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite()
#define RGB_BUILTIN (NEOPIXEL_DATA + SOC_GPIO_PIN_COUNT)  
#define RGB_BRIGHTNESS 64

static const uint8_t VBAT_SENSE = 2;
static const uint8_t VBUS_SENSE = 34;

#endif /* Pins_Arduino_h */
