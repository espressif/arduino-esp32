#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x2D81
#define USB_PID          0x1901
#define USB_MANUFACTURER "OZOEDU"
#define USB_PRODUCT      "Ozobot circuit kit"
#define USB_SERIAL       ""  // Empty string for MAC address

#ifndef __cplusplus
#define constexpr const
#endif

#if defined(BOARD_HAS_PIN_REMAP) && !defined(BOARD_USES_HW_GPIO_NUMBERS)

// Arduino style definitions (API uses Dx)

static constexpr uint8_t D0 = 0;    // G0
static constexpr uint8_t D1 = 1;    // G1
static constexpr uint8_t D2 = 2;    // G2
static constexpr uint8_t D3 = 3;    // G3
static constexpr uint8_t D4 = 4;    // G4
static constexpr uint8_t D5 = 5;    // G5
static constexpr uint8_t D6 = 6;    // G6
static constexpr uint8_t D7 = 7;    // G7
static constexpr uint8_t D8 = 8;    // G8
static constexpr uint8_t D9 = 9;    // G9
static constexpr uint8_t D10 = 10;  // Motor L IN1
static constexpr uint8_t D11 = 11;  // Motor L IN2
static constexpr uint8_t D12 = 12;  // Motor R IN1
static constexpr uint8_t D13 = 13;  // Motor R IN2
static constexpr uint8_t D14 = 14;  // SPI CS
static constexpr uint8_t D15 = 15;  // SPI CLK
static constexpr uint8_t D16 = 16;  // SPI MOSI
static constexpr uint8_t D17 = 17;  // SPI MISO
static constexpr uint8_t D18 = 18;  // I2C SDA
static constexpr uint8_t D19 = 19;  // I2C SCK
static constexpr uint8_t D20 = 20;  // I2C Interrupt
static constexpr uint8_t D21 = 21;  // UART TX
static constexpr uint8_t D22 = 22;  // UART RX
static constexpr uint8_t D23 = 23;  // Boot pin

static constexpr uint8_t A0 = 24;  // also DTR
static constexpr uint8_t A1 = 25;
static constexpr uint8_t A2 = 26;
static constexpr uint8_t A3 = 27;
static constexpr uint8_t A4 = 28;  // also SDA
static constexpr uint8_t A5 = 29;  // also SCL
static constexpr uint8_t A6 = 30;
static constexpr uint8_t A7 = 31;
static constexpr uint8_t A8 = 32;
static constexpr uint8_t A9 = 33;

#else

// ESP32-style definitions (API uses GPIOx)

static constexpr uint8_t D0 = 35;   // G0
static constexpr uint8_t D1 = 36;   // G1
static constexpr uint8_t D2 = 37;   // G2
static constexpr uint8_t D3 = 38;   // G3
static constexpr uint8_t D4 = 45;   // G4
static constexpr uint8_t D5 = 46;   // G5
static constexpr uint8_t D6 = 39;   // G6
static constexpr uint8_t D7 = 40;   // G7
static constexpr uint8_t D8 = 41;   // G8
static constexpr uint8_t D9 = 42;   // G9
static constexpr uint8_t D10 = 18;  // Motor L IN1
static constexpr uint8_t D11 = 17;  // Motor L IN2
static constexpr uint8_t D12 = 21;  // Motor R IN1
static constexpr uint8_t D13 = 33;  // Motor R IN2
static constexpr uint8_t D14 = 34;  // SPI CS
static constexpr uint8_t D15 = 12;  // SPI CLK
static constexpr uint8_t D16 = 11;  // SPI MOSI
static constexpr uint8_t D17 = 13;  // SPI MISO
static constexpr uint8_t D18 = 47;  // I2C SDA
static constexpr uint8_t D19 = 48;  // I2C SCK
static constexpr uint8_t D20 = 14;  // I2C Interrupt
static constexpr uint8_t D21 = 43;  // UART TX
static constexpr uint8_t D22 = 44;  // UART RX
static constexpr uint8_t D23 = 0;   // Boot pin

static constexpr uint8_t A0 = 1;
static constexpr uint8_t A1 = 2;
static constexpr uint8_t A2 = 3;
static constexpr uint8_t A3 = 4;
static constexpr uint8_t A4 = 5;
static constexpr uint8_t A5 = 6;
static constexpr uint8_t A6 = 7;
static constexpr uint8_t A7 = 8;
static constexpr uint8_t A8 = 9;
static constexpr uint8_t A9 = 10;

#endif

// alternate pin functions

static constexpr uint8_t TX = D21;
static constexpr uint8_t RX = D22;

static constexpr uint8_t SS = D14;
static constexpr uint8_t MOSI = D16;
static constexpr uint8_t MISO = D17;
static constexpr uint8_t SCK = D15;

static constexpr uint8_t SDA = D18;
static constexpr uint8_t SCL = D19;

static constexpr uint8_t MOTOR_L_IN1 = D10;
static constexpr uint8_t MOTOR_L_IN2 = D11;

static constexpr uint8_t MOTOR_R_IN1 = D12;
static constexpr uint8_t MOTOR_R_IN2 = D13;

static constexpr uint8_t BUTTON = D23;

static constexpr uint8_t RGB_LED = D9;

#ifndef __cplusplus
#undef constexpr
#endif

#endif /* Pins_Arduino_h */
