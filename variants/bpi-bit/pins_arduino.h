#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS 40
#define NUM_ANALOG_INPUTS 16

#define analogInputToDigitalPin(p) (((p) < 20) ? (esp32_adc2gpio[(p)]) : -1)
#define digitalPinToInterrupt(p) (((p) < 40) ? (p) : -1)
#define digitalPinHasPWM(p) (p < 34)

static const uint8_t BUZZER = 25;

static const uint8_t BUTTON_A = 35;
static const uint8_t BUTTON_B = 27;

static const uint8_t RGB_LED = 4;
static const uint8_t RGB_LED_POWER = 2;

static const uint8_t LIGHT_SENSOR1 = 36;
static const uint8_t LIGHT_SENSOR2 = 39;

static const uint8_t TEMPERATURE_SENSOR = 34;

static const uint8_t MPU9250_AD0 = 0;

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 23;

static const uint8_t P0 = 25;
static const uint8_t P1 = 32;
static const uint8_t P2 = 33;
static const uint8_t P3 = 13;
static const uint8_t P4 = 15;
static const uint8_t P5 = 35;
static const uint8_t P6 = 12;
static const uint8_t P7 = 14;
static const uint8_t P8 = 16;
static const uint8_t P9 = 17;
static const uint8_t P10 = 26;
static const uint8_t P11 = 27;
static const uint8_t P12 = 2;
static const uint8_t P13 = 18;
static const uint8_t P14 = 19;
static const uint8_t P15 = 23;
static const uint8_t P16 = 5;
static const uint8_t P19 = 22;
static const uint8_t P20 = 21;

static const uint8_t DAC1 = 26;
static const uint8_t DAC2 = 25;

#endif /* Pins_Arduino_h */