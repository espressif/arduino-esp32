#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 22
#define NUM_DIGITAL_PINS        22
#define NUM_ANALOG_INPUTS       6

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):-1)
#define digitalPinHasPWM(p)         (p < EXTERNAL_NUM_INTERRUPTS)
// Default pins for I2S,
#ifndef PIN_I2S_MCK
	#define PIN_I2S_MCK 9
#endif
#ifndef PIN_I2S_SCK
  #define PIN_I2S_SCK 10
#endif

#ifndef PIN_I2S_FS
  #define PIN_I2S_FS 11
#endif

#ifndef PIN_I2S_SD
  #define PIN_I2S_SD 12   
#endif

#ifndef PIN_I2S_SD_OUT
  #define PIN_I2S_SD_OUT 13
#endif

#ifndef PIN_I2S_SD_IN
  #define PIN_I2S_SD_IN 14
#endif

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SDA1 = 8;
static const uint8_t SCL1 = 9;

static const uint8_t SS    = 7;
static const uint8_t MOSI  = 6;
static const uint8_t MISO  = 5;
static const uint8_t SCK   = 4;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;

#endif /* Pins_Arduino_h */
