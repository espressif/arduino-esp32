#ifndef Pins_Arduino_h
#define Pins_Arduino_h


#define digitalPinToPort(pin)       (0)
#define digitalPinToBitMask(pin)    (1UL << (pin))
#define digitalPinToTimer(pin)      (0)
#define portOutputRegister(port)
#define portInputRegister(port)
#define portModeRegister(port)

#define NOT_A_PIN -1
#define NOT_A_PORT -1
#define NOT_AN_INTERRUPT -1
#define NOT_ON_TIMER 0

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       18

#define analogInputToDigitalPin(p)
#define digitalPinToInterrupt(p)
#define digitalPinHasPWM(p)

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS    = 2;
static const uint8_t MOSI  = 23;
static const uint8_t MISO  = 19;
static const uint8_t SCK   = 18;

#endif /* Pins_Arduino_h */