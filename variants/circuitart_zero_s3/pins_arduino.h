#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303A
#define USB_PID          0x80DB
#define USB_MANUFACTURER "CircuitART"
#define USB_PRODUCT      "ZeroS3"
#define USB_SERIAL       ""  // Empty string for MAC adddress

// User LED
#define LED_BUILTIN 46
#define BUILTIN_LED LED_BUILTIN  // backward compatibility

// Neopixel
#define PIN_NEOPIXEL 47
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite() and digitalWrite() for blinking
#define RGB_BUILTIN    (PIN_NEOPIXEL + SOC_GPIO_PIN_COUNT)
#define RGB_BRIGHTNESS 64
#define NEOPIXEL_NUM   1  // number of neopixels

static const uint8_t KEY_BUILTIN = 0;

static const uint8_t TFT_DC = 5;
static const uint8_t TFT_CS = 39;
static const uint8_t TFT_RST = 40;
static const uint8_t TFT_RESET = 40;

static const uint8_t SD_CS = 42;
static const uint8_t SD_CHIP_SELECT = 42;

static const uint8_t TX = 43;
static const uint8_t RX = 44;
static const uint8_t TX0 = 43;
static const uint8_t RX0 = 44;

static const uint8_t TX1 = 40;
static const uint8_t RX2 = 41;

static const uint8_t SDA = 33;
static const uint8_t SCL = 34;

static const uint8_t SS = 39;
static const uint8_t MOSI = 35;
static const uint8_t SCK = 36;
static const uint8_t MISO = 37;

static const uint8_t DAC1 = 17;
static const uint8_t DAC2 = 18;

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
static const uint8_t T15 = 15;

static const uint8_t D0 = 0;
static const uint8_t D1 = 1;
static const uint8_t D2 = 2;
static const uint8_t D3 = 3;
static const uint8_t D4 = 4;
static const uint8_t D5 = 5;
static const uint8_t D6 = 6;
static const uint8_t D7 = 7;
static const uint8_t D8 = 8;
static const uint8_t D9 = 9;
static const uint8_t D10 = 10;
static const uint8_t D11 = 11;
static const uint8_t D12 = 12;
static const uint8_t D13 = 13;
static const uint8_t D14 = 14;
static const uint8_t D15 = 15;
static const uint8_t D16 = 16;
static const uint8_t D17 = 17;
static const uint8_t D18 = 18;
static const uint8_t D33 = 33;
static const uint8_t D34 = 34;
static const uint8_t D35 = 35;
static const uint8_t D36 = 36;
static const uint8_t D37 = 37;
static const uint8_t D38 = 38;
static const uint8_t D39 = 39;
static const uint8_t D40 = 40;
static const uint8_t D41 = 41;

// Camera
#define TFT_CAM_POWER 21

#define PWDN_GPIO_NUM  -1  // connected through expander
#define RESET_GPIO_NUM -1  // connected through expander
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  SDA
#define SIOC_GPIO_NUM  SCL

#define Y9_GPIO_NUM    14  //16
#define Y8_GPIO_NUM    13  //14
#define Y7_GPIO_NUM    11  //13
#define Y6_GPIO_NUM    10
#define Y5_GPIO_NUM    9  //8
#define Y4_GPIO_NUM    8  //6
#define Y3_GPIO_NUM    7
#define Y2_GPIO_NUM    6  //9
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  48
#define PCLK_GPIO_NUM  16  //11

#endif /* Pins_Arduino_h */
