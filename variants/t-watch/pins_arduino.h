#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        20
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

#define TP_SDA              14
#define TP_SCL              15
#define TP_INT              38

#define RTC_INT             37
#define APX20X_INT          35
#define BMA42X_INT1         39
#define BMA42X_INT2         4

static const uint8_t KEY_BUILTIN = 36;

static const uint8_t TX = 33;
static const uint8_t RX = 34;

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS    = 13;
static const uint8_t MOSI  = 15;
static const uint8_t MISO  = 2;
static const uint8_t SCK   = 14;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
