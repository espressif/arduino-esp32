#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x2886
#define USB_PID 0x0056

static const uint8_t LED_BUILTIN = 21;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t TX1 = 42;
static const uint8_t RX1 = 41;

static const uint8_t SDA = 5;
static const uint8_t SCL = 6;

static const uint8_t SS = 44;
static const uint8_t MOSI = 9;
static const uint8_t MISO = 8;
static const uint8_t SCK = 7;

static const uint8_t MOSI1 = 11;
static const uint8_t MISO1 = 12;
static const uint8_t SCK1 = 13;

static const uint8_t I2S_SCK = 39;
static const uint8_t I2S_SD = 38;
static const uint8_t I2S_WS = 40;

static const uint8_t MTCK = 39;
static const uint8_t MTDO = 40;
static const uint8_t MTDI = 41;
static const uint8_t MTMS = 42;

static const uint8_t DVP_Y8 = 11;
static const uint8_t DVP_YP = 12;
static const uint8_t DVP_PCLK = 13;
static const uint8_t DVP_VSYNC = 38;
static const uint8_t CAM_SCL = 39;
static const uint8_t CAM_SDA = 40;
static const uint8_t XMCLK = 10;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A8 = 7;
static const uint8_t A9 = 8;
static const uint8_t A10 = 9;
static const uint8_t ADC_BAT = 10;

static const uint8_t D0 = 1;
static const uint8_t D1 = 2;
static const uint8_t D2 = 3;
static const uint8_t D3 = 4;
static const uint8_t D4 = 5;
static const uint8_t D5 = 6;
static const uint8_t D6 = 43;
static const uint8_t D7 = 44;
static const uint8_t D8 = 7;
static const uint8_t D9 = 8;
static const uint8_t D10 = 9;
static const uint8_t D11 = 38;
static const uint8_t D12 = 39;
static const uint8_t D13 = 40;
static const uint8_t D14 = 41;
static const uint8_t D15 = 42;
static const uint8_t D16 = 10;
static const uint8_t D17 = 13;
static const uint8_t D18 = 12;
static const uint8_t D19 = 11;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;

#endif /* Pins_Arduino_h */
