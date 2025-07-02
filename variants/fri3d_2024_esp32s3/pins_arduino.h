#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t LED_BUILTIN = 21;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 9;
static const uint8_t SCL = 18;

static const uint8_t SS = 14;
static const uint8_t MOSI = 6;
static const uint8_t MISO = 8;
static const uint8_t SCK = 7;

#define X_FRI3D_BADGE_2024   // General Define for use in sketches or lib files
#define X_WS2812_NUM_LEDS 5  // Number of RBG LEDs

#define PIN_I2C_SDA       SDA
#define PIN_I2C_SCL       SCL
#define PIN_WS2812        12
#define X_WS2812_NUM_LEDS 5

#define PIN_LED         21
#define PIN_IR_RECEIVER 11
#define PIN_BLASTER     10
#define PIN_BUZZER      46
#define PIN_BATTERY     13

#define PIN_SDCARD_CS SS

#define PIN_JOY_X 1
#define PIN_JOY_Y 3

#define PIN_A     39
#define PIN_B     40
#define PIN_X     38
#define PIN_Y     41
#define PIN_MENU  45
#define PIN_START 0

#define PIN_AUX 42  // Fri3d Badge 2024 Aux Pwr

#define CHANNEL_BUZZER 0

// Fri3d Badge 2024 Accelero Gyro
#define X_ACCELERO_GYRO 21

// I2S microphone on communicator addon
#define I2S_MIC_CHANNEL          I2S_CHANNEL_FMT_ONLY_LEFT
#define I2S_MIC_SERIAL_CLOCK     17  //serial clock SCLK: pin SCK
#define I2S_MIC_LEFT_RIGHT_CLOCK 47  //left/right clock LRCK: pin WS
#define I2S_MIC_SERIAL_DATA      15  //serial data DIN: pin SD

// Fri3d Badge 2024 LCD
// For using display with TFT_eSPI library
#define USER_SETUP_LOADED
#define SPI_FREQUENCY 80000000
#define ST7789_DRIVER
#define USE_HSPI_PORT

#define TFT_RGB_ORDER TFT_BGR  //# swap red and blue byte order
#define TFT_INVERSION_OFF
#define TFT_WIDTH  296  //;setting these will init the eSPI lib with correct dimensions
#define TFT_HEIGHT 240  //;setting these will init the eSPI lib with correct dimensions
#define TFT_MISO   MISO
#define TFT_MOSI   MOSI
#define TFT_SCLK   SCK
#define TFT_CS     5
#define TFT_DC     4
#define TFT_RST    48
#define LOAD_GLCD  1
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
#define SPI_FREQUENCY 80000000

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
