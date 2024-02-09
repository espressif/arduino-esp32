#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001
#define USB_MANUFACTURER 	"4D Systems Pty Ltd"
#define USB_PRODUCT 		"4D Systems gen4-ESP32 16MB Modules (ESP32-S3R8n16)"
//#define USB_CLASS 2

#define EXTERNAL_NUM_INTERRUPTS 46 
#define NUM_DIGITAL_PINS        48 
#define NUM_ANALOG_INPUTS       20 

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<49)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 17;
static const uint8_t SCL = 18;

static const uint8_t SS    = -1;	// Modified elsewhere
static const uint8_t MOSI  = -1;	// Modified elsewhere 
static const uint8_t MISO  = -1;	// Modified elsewhere
static const uint8_t SCK   = -1;	// Modified elsewhere

#endif /* Pins_Arduino_h */
