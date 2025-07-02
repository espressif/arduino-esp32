#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x2341
#define USB_PID 0x0070

#ifndef __cplusplus
#define constexpr const
#endif

// primary pin names

#if defined(BOARD_HAS_PIN_REMAP) && !defined(BOARD_USES_HW_GPIO_NUMBERS)

// Arduino style definitions (API uses Dx)

static constexpr uint8_t D0 = 0;  // also RX
static constexpr uint8_t D1 = 1;  // also TX
static constexpr uint8_t D2 = 2;
static constexpr uint8_t D3 = 3;  // also CTS
static constexpr uint8_t D4 = 4;  // also DSR
static constexpr uint8_t D5 = 5;
static constexpr uint8_t D6 = 6;
static constexpr uint8_t D7 = 7;
static constexpr uint8_t D8 = 8;
static constexpr uint8_t D9 = 9;
static constexpr uint8_t D10 = 10;  // also SS
static constexpr uint8_t D11 = 11;  // also MOSI
static constexpr uint8_t D12 = 12;  // also MISO
static constexpr uint8_t D13 = 13;  // also SCK, LED_BUILTIN
static constexpr uint8_t LED_RED = 14;
static constexpr uint8_t LED_GREEN = 15;
static constexpr uint8_t LED_BLUE = 16;  // also RTS

static constexpr uint8_t A0 = 17;  // also DTR
static constexpr uint8_t A1 = 18;
static constexpr uint8_t A2 = 19;
static constexpr uint8_t A3 = 20;
static constexpr uint8_t A4 = 21;  // also SDA
static constexpr uint8_t A5 = 22;  // also SCL
static constexpr uint8_t A6 = 23;
static constexpr uint8_t A7 = 24;

#else

// ESP32-style definitions (API uses GPIOx)

static constexpr uint8_t D0 = 44;  // also RX
static constexpr uint8_t D1 = 43;  // also TX
static constexpr uint8_t D2 = 5;
static constexpr uint8_t D3 = 6;  // also CTS
static constexpr uint8_t D4 = 7;  // also DSR
static constexpr uint8_t D5 = 8;
static constexpr uint8_t D6 = 9;
static constexpr uint8_t D7 = 10;
static constexpr uint8_t D8 = 17;
static constexpr uint8_t D9 = 18;
static constexpr uint8_t D10 = 21;  // also SS
static constexpr uint8_t D11 = 38;  // also MOSI
static constexpr uint8_t D12 = 47;  // also MISO
static constexpr uint8_t D13 = 48;  // also SCK, LED_BUILTIN
static constexpr uint8_t LED_RED = 46;
static constexpr uint8_t LED_GREEN = 0;
static constexpr uint8_t LED_BLUE = 45;  // also RTS

static constexpr uint8_t A0 = 1;  // also DTR
static constexpr uint8_t A1 = 2;
static constexpr uint8_t A2 = 3;
static constexpr uint8_t A3 = 4;
static constexpr uint8_t A4 = 11;  // also SDA
static constexpr uint8_t A5 = 12;  // also SCL
static constexpr uint8_t A6 = 13;
static constexpr uint8_t A7 = 14;

#endif

// Aliases

static constexpr uint8_t LEDR = LED_RED;
static constexpr uint8_t LEDG = LED_GREEN;
static constexpr uint8_t LEDB = LED_BLUE;

// alternate pin functions

static constexpr uint8_t LED_BUILTIN = D13;

static constexpr uint8_t TX = D1;
static constexpr uint8_t RX = D0;
static constexpr uint8_t RTS = LED_BLUE;
static constexpr uint8_t CTS = D3;
static constexpr uint8_t DTR = A0;
static constexpr uint8_t DSR = D4;

static constexpr uint8_t SS = D10;
static constexpr uint8_t MOSI = D11;
static constexpr uint8_t MISO = D12;
static constexpr uint8_t SCK = D13;

static constexpr uint8_t SDA = A4;
static constexpr uint8_t SCL = A5;

#define PIN_I2S_SCK    D7
#define PIN_I2S_FS     D8
#define PIN_I2S_SD     D9
#define PIN_I2S_SD_OUT D9  // same as bidir
#define PIN_I2S_SD_IN  D10

#ifndef __cplusplus
#undef constexpr
#endif

#endif /* Pins_Arduino_h */
