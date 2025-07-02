#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = 16;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 18;

static const uint8_t A0 = 36;
static const uint8_t A3 = 39;
static const uint8_t A4 = 32;
static const uint8_t A5 = 33;
static const uint8_t A6 = 34;
static const uint8_t A7 = 35;
static const uint8_t A10 = 4;
static const uint8_t A11 = 0;
static const uint8_t A12 = 2;
static const uint8_t A13 = 15;
static const uint8_t A14 = 13;
static const uint8_t A15 = 12;
static const uint8_t A16 = 14;
static const uint8_t A17 = 27;
static const uint8_t A18 = 25;
static const uint8_t A19 = 26;

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

static const uint8_t LDR_PIN = 36;

static const uint8_t SW1 = 16;
static const uint8_t SW2 = 14;

static const uint8_t BT_LED = 17;
static const uint8_t WIFI_LED = 2;
static const uint8_t NTP_LED = 15;
static const uint8_t IOT_LED = 12;

static const uint8_t BUZZER = 13;

static const uint8_t INPUT1 = 32;
static const uint8_t INPUT2 = 33;
static const uint8_t INPUT3 = 34;
static const uint8_t INPUT4 = 35;

static const uint8_t OUTPUT1 = 26;
static const uint8_t OUTPUT2 = 27;

static const uint8_t SDA0 = 21;
static const uint8_t SCL0 = 22;

#define WIRE1_PIN_DEFINED 1  // See Wire.cpp at bool TwoWire::initPins(int sdaPin, int sclPin)
static const uint8_t SDA1 = 4;
static const uint8_t SCL1 = 5;

static const uint8_t KB_GPIO18 = 18;
static const uint8_t KB_GPIO19 = 19;
static const uint8_t KB_GPIO23 = 23;

#endif /* Pins_Arduino_h */
