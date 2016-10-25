#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#define EXTERNAL_NUM_INTERRUPTS 11
#define NUM_DIGITAL_PINS        12
#define NUM_ANALOG_INPUTS       5

#define analogInputToDigitalPin(p)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t SDA = 2;
static const uint8_t SCL = 14;

static const uint8_t SS    = 15;
static const uint8_t MOSI  = 13;
static const uint8_t MISO  = 12;
static const uint8_t SCK   = 14;

#endif /* Pins_Arduino_h */
