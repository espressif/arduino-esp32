#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x239A
#define USB_PID 0x800A
#define USB_MANUFACTURER "ATMegaZero"
#define USB_PRODUCT "ATMZ-ESP32S2"
#define USB_SERIAL ""

#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS        48
#define NUM_ANALOG_INPUTS       20

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

static const uint8_t NEOPIXEL = 40;
static const uint8_t PD5 = 0;

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS    = 38;
static const uint8_t MOSI  = 35;
static const uint8_t MISO  = 37;
static const uint8_t SCK   = 36;

static const uint8_t A0 = 17;
static const uint8_t A1 = 18;
static const uint8_t A2 = 13;
static const uint8_t A3 = 12;
static const uint8_t A4 = 6;
static const uint8_t A5 = 3;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;

static const uint8_t DAC1 = 17;
static const uint8_t DAC2 = 18;

#endif /* Pins_Arduino_h */
