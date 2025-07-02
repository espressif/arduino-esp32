#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include <../d32/d32_core.h>

static const uint8_t LED_BUILTIN = 5;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t _VBAT = 35;  // battery voltage

#define TF_CS   4   // TF (Micro SD Card) CS pin
#define TS_CS   12  // Touch Screen CS pin
#define TFT_CS  14  // TFT CS pin
#define TFT_LED 32  // TFT backlight control pin
#define TFT_RST 33  // TFT reset pin
#define TFT_DC  27  // TFT DC pin

#define SS TF_CS

#endif /* Pins_Arduino_h */
