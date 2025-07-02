#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = -1;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

#define MTDO 15
#define MTDI 12
#define MTMS 14
#define MTCK 13

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 22;  //23;
static const uint8_t SCL = 21;  //19;

#define WIRE1_PIN_DEFINED 1  // See Wire.cpp at bool TwoWire::initPins(int sdaPin, int sclPin)
static const uint8_t SDA1 = 12;
static const uint8_t SCL1 = 13;

static const uint8_t SS = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 18;

static const uint8_t A0 = 12;
static const uint8_t A1 = 13;
static const uint8_t A2 = 15;
static const uint8_t A3 = 4;
static const uint8_t A6 = 38;
static const uint8_t A7 = 37;

static const uint8_t A8 = 32;
static const uint8_t A9 = 33;
static const uint8_t A10 = 25;
static const uint8_t A11 = 26;
static const uint8_t A12 = 27;
static const uint8_t A13 = 14;

static const uint8_t D0 = 3;
static const uint8_t D1 = 1;
static const uint8_t D2 = 16;
static const uint8_t D3 = 17;
static const uint8_t D4 = 32;  //ADC1_CH4
static const uint8_t D5 = 33;  //ADC1_CH5
static const uint8_t D6 = 25;  //ADC2_CH8 DAC_1
static const uint8_t D7 = 26;  //ADC2_CH9 DAC_2
static const uint8_t D8 = 27;  //ADC2_CH7
static const uint8_t D9 = 14;  //ADC2_CH6
static const uint8_t D10 = 5;
static const uint8_t D11 = 23;
static const uint8_t D12 = 19;
static const uint8_t D13 = 18;
static const uint8_t D14 = 12;
static const uint8_t D15 = 13;
static const uint8_t D16 = 15;
static const uint8_t D17 = 4;
static const uint8_t D18 = 22;
static const uint8_t D19 = 21;
static const uint8_t D20 = 38;
static const uint8_t D21 = 37;

static const uint8_t T0 = 4;
static const uint8_t T1 = 0;
static const uint8_t T2 = 2;
static const uint8_t T3 = 15;
static const uint8_t T4 = 13;
static const uint8_t T5 = 12;
static const uint8_t T6 = 14;
static const uint8_t T7 = 27;
static const uint8_t T8 = 33;
static const uint8_t T9 = 32;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
