#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#include "soc/soc_caps.h"

#define USB_VID          0x303A
#define USB_PID          0x8405
#define USB_MANUFACTURER "PCBCupid"
#define USB_PRODUCT      "Pcbcupid GLYPH S3 MD"
#define USB_SERIAL       ""

// Onboard user LED (plain LED on GPIO21, not a NeoPixel)
static const uint8_t LED_BUILTIN = 21;
#define BUILTIN_LED LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN

// Battery measurement
static const uint8_t BAT_MEASURE = 1;
#define BAT_VOLT_PIN BAT_MEASURE
#define MSR          BAT_MEASURE

// Serial
static const uint8_t TX = 43;
static const uint8_t RX = 44;

// I2C
static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

// SPI (FSPI)
static const uint8_t SS = 10;
static const uint8_t MOSI = 36;
static const uint8_t MISO = 37;
static const uint8_t SCK = 35;

// TB6612FNG dual H-bridge motor driver
static const uint8_t MOTOR_STBY = 42;  // active high
static const uint8_t MOTOR_A_PWM = 41;
static const uint8_t MOTOR_A_IN1 = 39;
static const uint8_t MOTOR_A_IN2 = 38;
static const uint8_t MOTOR_B_PWM = 40;
static const uint8_t MOTOR_B_IN1 = 47;
static const uint8_t MOTOR_B_IN2 = 48;

// Analog pins
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A6 = 6;
static const uint8_t A7 = 7;
static const uint8_t A8 = 8;
static const uint8_t A9 = 9;
static const uint8_t A10 = 10;

// Digital pins
static const uint8_t D1 = 1;
static const uint8_t D2 = 2;
static const uint8_t D4 = 4;
static const uint8_t D5 = 5;
static const uint8_t D6 = 6;
static const uint8_t D7 = 7;
static const uint8_t D8 = 8;
static const uint8_t D9 = 9;
static const uint8_t D10 = 10;
static const uint8_t D13 = 13;
static const uint8_t D14 = 14;
static const uint8_t D15 = 15;
static const uint8_t D16 = 16;
static const uint8_t D17 = 17;
static const uint8_t D18 = 18;
static const uint8_t D35 = 35;
static const uint8_t D36 = 36;
static const uint8_t D37 = 37;
static const uint8_t D43 = 43;
static const uint8_t D44 = 44;

#endif /* Pins_Arduino_h */
