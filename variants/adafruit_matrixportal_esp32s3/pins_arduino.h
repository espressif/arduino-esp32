#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID            0x239A
#define USB_PID            0x8125
#define USB_MANUFACTURER   "Adafruit"
#define USB_PRODUCT        "MatrixPortal ESP32-S3"
#define USB_SERIAL         "" // Empty string for MAC adddress

#define EXTERNAL_NUM_INTERRUPTS 49
#define NUM_DIGITAL_PINS        49
#define NUM_ANALOG_INPUTS       6

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<EXTERNAL_NUM_INTERRUPTS)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

#define LED_BUILTIN         13

#define PIN_NEOPIXEL        4
#define NEOPIXEL_PIN        4
#define NEOPIXEL_NUM        1
#define PIN_LIGHTSENSOR     A5

#define PIN_BUTTON_UP       6
#define PIN_BUTTON_DOWN     7

static const uint8_t TX = 18;
static const uint8_t RX = 8;
#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 16;
static const uint8_t SCL = 17;

static const uint8_t SS   = -1;
static const uint8_t MOSI = -1;
static const uint8_t SCK  = -1;
static const uint8_t MISO = -1;

static const uint8_t A0 = 12;
static const uint8_t A1 = 3;
static const uint8_t A2 = 9;
static const uint8_t A3 = 10;
static const uint8_t A4 = 11;
static const uint8_t A5 = 5; // Light

static const uint8_t T3  = 3;  // Touch pin IDs map directly
static const uint8_t T8  = 8;  // to underlying GPIO numbers NOT
static const uint8_t T9  = 9;  // the analog numbers on board silk
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;

#endif /* Pins_Arduino_h */
