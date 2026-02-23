#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

// Addressable RGB LED (NeoPixel)
#define RGB_LED 21

// User button
#define USER_BUTTON 45

// Battery management
#define BAT_SENSE  8   // Battery voltage (divider /2)
#define BAT_ENABLE 46  // Battery enable
#define BAT_STATUS 34  // Charge status indicator

// Battery voltage reading macro
#define getBatteryVoltage() ((analogRead(VBAT_SENSE) / 4095.0) * 3.3 * 2.0)

// I2C
static const uint8_t SDA = 10;
static const uint8_t SCL = 11;

// SPI
static const uint8_t MOSI = 12;
static const uint8_t MISO = 13;
static const uint8_t SCK = 14;
static const uint8_t SS = -1;  // No default CS

//Port 1 IO pins
static const uint8_t P1_IO0 = 4;
static const uint8_t P1_IO1 = 3;
static const uint8_t P1_IO2 = 2;

//Port 2 IO pins
static const uint8_t P2_IO0 = 7;
static const uint8_t P2_IO1 = 6;
static const uint8_t P2_IO2 = 5;

//Port 3 IO pins
static const uint8_t P3_IO0 = 9;
static const uint8_t P3_IO1 = 16;
static const uint8_t P3_IO2 = 15;

//Port 4 IO pins
static const uint8_t P4_IO0 = 1;
static const uint8_t P4_IO1 = 17;
static const uint8_t P4_IO2 = 18;

// Analog capable pins (ESP32-S3 ADC1: GPIO 1-10, ADC2: GPIO 11-20)
static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 8;
static const uint8_t A8 = 9;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;
static const uint8_t A14 = 15;
static const uint8_t A15 = 16;
static const uint8_t A16 = 17;
static const uint8_t A17 = 18;

// Touch capable pins (ESP32-S3: GPIO 1-14)
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

#endif /* Pins_Arduino_h */
