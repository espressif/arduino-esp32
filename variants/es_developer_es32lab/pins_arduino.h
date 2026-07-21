#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define ES32LAB_PIN_CONSTANTS_DEFINED

static const uint8_t P00 = 0;
static const uint8_t P01 = 1;
static const uint8_t P02 = 2;
static const uint8_t P03 = 3;
static const uint8_t P04 = 4;
static const uint8_t P05 = 5;

static const uint8_t P12 = 12;
static const uint8_t P13 = 13;
static const uint8_t P14 = 14;
static const uint8_t P15 = 15;
static const uint8_t P16 = 16;
static const uint8_t P17 = 17;
static const uint8_t P18 = 18;
static const uint8_t P19 = 19;

static const uint8_t P21 = 21;
static const uint8_t P22 = 22;
static const uint8_t P23 = 23;

static const uint8_t P25 = 25;
static const uint8_t P26 = 26;
static const uint8_t P27 = 27;

static const uint8_t P32 = 32;
static const uint8_t P33 = 33;
static const uint8_t P34 = 34;
static const uint8_t P35 = 35;
static const uint8_t P36 = 36;
static const uint8_t P39 = 39;

static const uint8_t P17_G = 17;
static const uint8_t P16_Y = 16;
static const uint8_t P13_R = 13;
static const uint8_t P12_B = 12;

// Serial
static const uint8_t TX = 1;
static const uint8_t RX = 3;
static const uint8_t TX0 = 1;
static const uint8_t RX0 = 3;

// I2C
static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

// I2S
static const uint8_t LCK = 26;
static const uint8_t DIN = 25;
static const uint8_t BCK = 27;

// SPI
static const uint8_t SS = 5;
static const uint8_t CS_SD = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 18;

// TFT
static const uint8_t CS_TFT = 15;
static const uint8_t DC_TFT = 2;
static const uint8_t A0_TFT = 2;
static const uint8_t DA_TFT = 23;
static const uint8_t CL_TFT = 18;

// Onboard peripherals
static const uint8_t P_KEYBOARD = 33;
static const uint8_t P_LED_GREEN = 17;
static const uint8_t P_LED_YELLOW = 16;
static const uint8_t P_LED_RED = 13;
static const uint8_t P_LED_BLUE = 12;
static const uint8_t P_POT1 = 36;
static const uint8_t P_POT2 = 39;
static const uint8_t P_VOLT = 34;
static const uint8_t P_BUZZER = 25;
static const uint8_t P_DAC1 = 25;
static const uint8_t P_DAC2 = 26;

static const uint8_t LED_BUILTIN = 17;
#define BUILTIN_LED LED_BUILTIN

#endif /* Pins_Arduino_h */