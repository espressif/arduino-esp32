#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Motor driver pins
#define MOTOR_A_IN1 25  //  PHASE/IN1
#define MOTOR_A_IN2 26  //  ENABLE/IN2

#define MOTOR_B_IN1 27  //  PHASE/IN1
#define MOTOR_B_IN2 32  //  ENABLE/IN2

#define MOTOR_C_IN1 33  //  PHASE/IN1
#define MOTOR_C_IN2 4   //  ENABLE/IN2

#define SLEEP_MOTOR_ABC 2  //  nSLEEP

#define LED_ROBOHEART 13           // Built in LED
#define BUILTIN_LED LED_ROBOHEART  // backward compatibility
#define LED_BUILTIN LED_ROBOHEART

#define BUTTON_ROBOHEART 0  // Button

// I2C IMU sensor
#define IMU_SDA 21
#define IMU_SCL 22

#define RXD1 16
#define TXD1 17

// GSM Vela connector board pins
#define GSM_PWRKEY 12
#define GSM_DTR 13
#define GSM_CTS 15
#define GSM_RTS 14
#define GSM_TX TXD1
#define GSM_RX RXD1
#define BATTERY_PIN 36  // Battery ADC pin

static const uint8_t TX = 35;
static const uint8_t RX = 34;

static const uint8_t TXD2 = 17;
static const uint8_t RXD2 = 16;

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 18;

static const uint8_t G23 = 23;
static const uint8_t G19 = 19;
static const uint8_t G18 = 18;
static const uint8_t G3 = 3;
static const uint8_t G16 = 16;
static const uint8_t G21 = 21;
static const uint8_t G2 = 2;
static const uint8_t G12 = 12;
static const uint8_t G15 = 15;
static const uint8_t G35 = 35;
static const uint8_t G36 = 36;
static const uint8_t G25 = 25;
static const uint8_t G26 = 26;
static const uint8_t G1 = 1;
static const uint8_t G17 = 17;
static const uint8_t G22 = 22;
static const uint8_t G5 = 5;
static const uint8_t G13 = 13;
static const uint8_t G0 = 0;
static const uint8_t G34 = 34;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

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

#endif /* Pins_Arduino_h */
