#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// ============================================================
// Maker ESP32 - Cytron
// ESP32-WROOM-32E-N8
// ============================================================

// ------------------------------------------------------------
// Built-in LED
// ------------------------------------------------------------
static const uint8_t LED_BUILTIN = 2;
#define BUILTIN_LED LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN

// ------------------------------------------------------------
// User Button
// ------------------------------------------------------------
static const uint8_t BUTTON = 4;
static const uint8_t KEY_BUILTIN = BUTTON;

// ------------------------------------------------------------
// UART
// ------------------------------------------------------------
static const uint8_t TX = 1;
static const uint8_t RX = 3;

// ------------------------------------------------------------
// I2C
// ------------------------------------------------------------
static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

// ------------------------------------------------------------
// SPI
// ------------------------------------------------------------
static const uint8_t SS   = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK  = 18;

// ------------------------------------------------------------
// Buzzer
// ------------------------------------------------------------
static const uint8_t BUZZER = 26;

// ------------------------------------------------------------
// Analog pins
// Standard ESP32 ADC mapping
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// Touch pins
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// DAC
// ------------------------------------------------------------
static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */