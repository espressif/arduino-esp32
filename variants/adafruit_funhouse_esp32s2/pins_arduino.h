#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>


#define USB_VID            0x239A
#define USB_PID            0x80F9
#define USB_MANUFACTURER   "Adafruit"
#define USB_PRODUCT        "Funhouse ESP32-S2"
#define USB_SERIAL         "" // Empty string for MAC adddress

#define LED_BUILTIN   37
#define BUILTIN_LED  LED_BUILTIN // backward compatibility

#define PIN_BUTTON1   3
#define PIN_BUTTON2   4
#define PIN_BUTTON3   5
#define PIN_BUTTON4   0  // BOOT0 switch

static const uint8_t PIN_DOTSTAR_DATA = 14;
static const uint8_t PIN_DOTSTAR_CLOCK = 15;

static const uint8_t TFT_BACKLIGHT = 21;
static const uint8_t TFT_DC        = 39;
static const uint8_t TFT_CS        = 40;
static const uint8_t TFT_RESET     = 41;

static const uint8_t SPEAKER       = 42;
static const uint8_t BUTTON_DOWN   = PIN_BUTTON1;
static const uint8_t BUTTON_SELECT = PIN_BUTTON2;
static const uint8_t BUTTON_UP     = PIN_BUTTON3;
static const uint8_t SENSOR_PIR    = 16;
static const uint8_t SENSOR_LIGHT  = 18;

static const uint8_t SDA = 34;
static const uint8_t SCL = 33;

static const uint8_t SS    = 40;
static const uint8_t MOSI  = 35;
static const uint8_t SCK   = 36;
static const uint8_t MISO  = 37;

static const uint8_t A0 = 17;
static const uint8_t A1 = 2;
static const uint8_t A2 = 1;
static const uint8_t A3 = 18; // light sensor





static const uint8_t TX = 43;
static const uint8_t RX = 44;
#define TX1 TX
#define RX1 RX

static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;

static const uint8_t DAC1 = 17;
static const uint8_t DAC2 = 18;

#endif /* Pins_Arduino_h */
