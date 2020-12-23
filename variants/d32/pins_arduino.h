#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include <d32_core.h>

static const uint8_t LED_BUILTIN = 5;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN
static const uint8_t _VBAT = 35; // battery voltage

#endif /* Pins_Arduino_h */
