#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

//#define USB_VID          0x303A
//#define USB_PID          0x1001
//#define USB_MANUFACTURER "Sparkfun"
//#define USB_PRODUCT      "ESP32-S3 Thing Plus"
#define USB_SERIAL ""

#define LED_PIN     46  //Pin 46 on Thing Plus C S3 is connected to WS2812 LED
#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define NUM_LEDS    1
#define BRIGHTNESS  25
#define LED_BUILTIN LED_PIN
#define BUILTIN_LED LED_BUILTIN  // backward compatibility

static const uint8_t LED = LED_PIN;
static const uint8_t STAT_LED = 0;
static const uint8_t BTN = 0;
static const uint8_t Q_EN = 45;

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SS = 10;
static const uint8_t MISO = 13;  //POCI
static const uint8_t MOSI = 11;  //PICO
static const uint8_t SCK = 12;

static const uint8_t SCL = 9;
static const uint8_t SDA = 8;

static const uint8_t A0 = 10;
static const uint8_t A1 = 14;
static const uint8_t A2 = 15;
static const uint8_t A3 = 16;
static const uint8_t A4 = 17;
static const uint8_t A5 = 18;

static const uint8_t GPIO0 = 21;
static const uint8_t GPIO1 = 7;
static const uint8_t GPIO2 = 6;
static const uint8_t GPIO3 = 5;
static const uint8_t GPIO4 = 4;
static const uint8_t GPIO5 = 2;
static const uint8_t GPIO6 = 1;

static const uint8_t FREEBIE = 42;

static const uint8_t SDIO_DET = 48;
static const uint8_t SDIO0 = 39;
static const uint8_t SDIO1 = 40;
static const uint8_t SDIO2 = 47;
static const uint8_t SDIO3 = 33;
static const uint8_t SDIO_CLK = 38;
static const uint8_t SDIO_CMD = 34;

#endif /* Pins_Arduino_h */
