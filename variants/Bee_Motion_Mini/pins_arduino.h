#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>


#define EXTERNAL_NUM_INTERRUPTS 4
#define NUM_DIGITAL_PINS        4
#define NUM_ANALOG_INPUTS       2

#define analogInputToDigitalPin(p)  (((p)<29)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t BOOT_BTN = 9;
static const uint8_t PIR = 5;



#endif /* Pins_Arduino_h */

