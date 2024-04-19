#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x239A
#define USB_PID          0x8117
#define USB_MANUFACTURER "Adafruit"
#define USB_PRODUCT      "Camera ESP32-S3"
#define USB_SERIAL       ""  // Empty string for MAC address

static const uint8_t PIN_NEOPIXEL = 1;
static const uint8_t NEOPIXEL_PIN = 1;

//By making LED_BUILTIN have the same value of RGB_BUILTIN
//NeoPixel LED can also be used as LED_BUILTIN with digitalMode() + digitalWrite()
static const uint8_t LED_BUILTIN = PIN_NEOPIXEL + SOC_GPIO_PIN_COUNT;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite() and digitalWrite() for blinking
#define RGB_BUILTIN    (PIN_NEOPIXEL + SOC_GPIO_PIN_COUNT)
#define RGB_BRIGHTNESS 64

static const uint8_t TFT_BACKLIGHT = 45;
static const uint8_t TFT_DC = 40;
static const uint8_t TFT_CS = 39;
static const uint8_t TFT_RESET = 38;
static const uint8_t TFT_RST = 38;

static const uint8_t SD_CS = 48;
static const uint8_t SD_CHIP_SELECT = 48;
static const uint8_t SPEAKER = 46;

static const uint8_t SCL = 33;
static const uint8_t SDA = 34;

static const uint8_t SS = 48;
static const uint8_t MOSI = 35;
static const uint8_t SCK = 36;
static const uint8_t MISO = 37;

static const uint8_t A0 = 17;
static const uint8_t A1 = 18;
static const uint8_t BATT_MONITOR = 4;
static const uint8_t SHUTTER_BUTTON = 0;

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t DAC1 = 17;
static const uint8_t DAC2 = 18;

#define AWEXP_SPKR_SD      0
#define AWEXP_BUTTON_SEL   1
#define AWEXP_SD_DET       8
#define AWEXP_SD_PWR       9
#define AWEXP_BUTTON_OK    11
#define AWEXP_BUTTON_RIGHT 12
#define AWEXP_BUTTON_UP    13
#define AWEXP_BUTTON_LEFT  14
#define AWEXP_BUTTON_DOWN  15

#define RESET_GPIO_NUM 47
#define PWDN_GPIO_NUM  21
#define XCLK_GPIO_NUM  8
#define SIOD_GPIO_NUM  SDA
#define SIOC_GPIO_NUM  SCL

#define Y9_GPIO_NUM    7
#define Y8_GPIO_NUM    9
#define Y7_GPIO_NUM    10
#define Y6_GPIO_NUM    12
#define Y5_GPIO_NUM    14
#define Y4_GPIO_NUM    16
#define Y3_GPIO_NUM    15
#define Y2_GPIO_NUM    13
#define VSYNC_GPIO_NUM 5
#define HREF_GPIO_NUM  6
#define PCLK_GPIO_NUM  11

#endif /* Pins_Arduino_h */
