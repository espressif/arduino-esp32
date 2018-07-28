#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include <../d32/d32_core.h>

static const uint8_t LED_BUILTIN = 5;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
static const uint8_t _VBAT = 35; // battery voltage


static const uint8_t TF_CS = 4;    // TF (Micro SD Card) CS pin
static const uint8_t TS_CS = 12;   // Touch Screen CS pin
static const uint8_t TFT_CS = 14;  // TFT CS pin
static const uint8_t TFT_LED = 32; // TFT backlight control pin
static const uint8_t TFT_RST = 33; // TFT reset pin
static const uint8_t TFT_DC = 27;  // TFT DC pin

#endif /* Pins_Arduino_h */
