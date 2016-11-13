#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       18

#define analogInputToDigitalPin(p)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t SDA = 22;
static const uint8_t SCL = 21;

static const uint8_t SS    = 5;
static const uint8_t MOSI  = 23;
static const uint8_t MISO  = 19;
static const uint8_t SCK   = 18;

#endif /* Pins_Arduino_h */
