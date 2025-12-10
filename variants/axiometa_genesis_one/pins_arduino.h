#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

// Battery monitoring (voltage divider /2)
#define VBAT_SENSE 10

// Battery voltage reading macro
#define getBatteryVoltage() ((analogRead(VBAT_SENSE) / 4095.0) * 3.3 * 2.0)

// Fixed communication pins (shared across all ports)
static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 47;
static const uint8_t SCL = 48;

static const uint8_t SS = 1;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 12;
static const uint8_t SCK = 13;

// Port 1 IO pins
static const uint8_t P1_IO1 = 1;
static const uint8_t P1_IO2 = 14;
static const uint8_t P1_IO3 = 41;

// Port 2 IO pins
static const uint8_t P2_IO1 = 2;
static const uint8_t P2_IO2 = 15;
static const uint8_t P2_IO3 = 42;

// Port 3 IO pins
static const uint8_t P3_IO1 = 3;
static const uint8_t P3_IO2 = 16;
static const uint8_t P3_IO3 = 45;

// Port 4 IO pins
static const uint8_t P4_IO1 = 4;
static const uint8_t P4_IO2 = 17;
static const uint8_t P4_IO3 = 46;

// Port 5 IO pins
static const uint8_t P5_IO1 = 5;
static const uint8_t P5_IO2 = 18;
static const uint8_t P5_IO3 = 21;

// Port 6 IO pins
static const uint8_t P6_IO1 = 6;
static const uint8_t P6_IO2 = 40;
static const uint8_t P6_IO3 = 38;

// Port 7 IO pins
static const uint8_t P7_IO1 = 7;
static const uint8_t P7_IO2 = 9;
static const uint8_t P7_IO3 = 39;

// Port 8 IO pins
static const uint8_t P8_IO1 = 48;
static const uint8_t P8_IO2 = 43;
static const uint8_t P8_IO3 = 44;

// Analog capable pins (ESP32-S3 specific)
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
static const uint8_t A18 = 19;
static const uint8_t A19 = 20;

// Touch capable pins (ESP32-S3 specific)
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
