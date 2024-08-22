#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x239A
#define USB_PID          0x8111
#define USB_MANUFACTURER "Adafruit"
#define USB_PRODUCT      "QT Py ESP32-S2"
#define USB_SERIAL       ""  // Empty string for MAC address

// Neopixel
#define PIN_NEOPIXEL 39
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_NEOPIXEL;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

#define NEOPIXEL_NUM      1     // number of neopixels
#define NEOPIXEL_POWER    38    // power pin
#define NEOPIXEL_POWER_ON HIGH  // power pin state when on

static const uint8_t SDA = 7;
static const uint8_t SCL = 6;

#define WIRE1_PIN_DEFINED 1  // See Wire.cpp at bool TwoWire::initPins(int sdaPin, int sclPin)
static const uint8_t SDA1 = 41;
static const uint8_t SCL1 = 40;

static const uint8_t SS = 42;
static const uint8_t MOSI = 35;
static const uint8_t SCK = 36;
static const uint8_t MISO = 37;

static const uint8_t A0 = 18;
static const uint8_t A1 = 17;
static const uint8_t A2 = 9;
static const uint8_t A3 = 8;
static const uint8_t A4 = 7;
static const uint8_t A5 = 6;
static const uint8_t A6 = 5;
static const uint8_t A7 = 16;

static const uint8_t TX = 5;
static const uint8_t RX = 16;
#define TX1 TX
#define RX1 RX

static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;

static const uint8_t DAC1 = 17;
static const uint8_t DAC2 = 18;

#endif /* Pins_Arduino_h */
