#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>


#define USB_VID            0x239A
#define USB_PID            0x8117
#define USB_MANUFACTURER   "Adafruit"
#define USB_PRODUCT        "Camera ESP32-S3"
#define USB_SERIAL         "" // Empty string for MAC adddress


#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS        48
#define NUM_ANALOG_INPUTS       20

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

static const uint8_t PIN_NEOPIXEL = 1;
static const uint8_t NEOPIXEL_PIN = 1;

//static const uint8_t TFT_BACKLIGHT = 41;
static const uint8_t TFT_DC        = 40;
static const uint8_t TFT_CS        = 39;
static const uint8_t TFT_RESET     = 38;
static const uint8_t TFT_RST       = 38;

static const uint8_t SD_CS          = 48;
static const uint8_t SD_CHIP_SELECT = 48;
static const uint8_t SPEAKER        = 41;

static const uint8_t SDA = 33;
static const uint8_t SCL = 34;

static const uint8_t SS    = 48;
static const uint8_t MOSI  = 35;
static const uint8_t SCK   = 36;
static const uint8_t MISO  = 37;

static const uint8_t A0 = 17;
static const uint8_t A1 = 18;
static const uint8_t BATT_MONITOR = 4;
static const uint8_t SHUTTER_BUTTON = 0;

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t DAC1 = 17;
static const uint8_t DAC2 = 18;

#define AWEXP_SPKR_SD 0
#define AWEXP_BUTTON_SEL 1
#define AWEXP_BACKLIGHT 2
#define AWEXP_CAM_PWDN 7
#define AWEXP_SD_DET 8
#define AWEXP_SD_PWR 9
#define AWEXP_CAM_RST 10
#define AWEXP_BUTTON_OK 11
#define AWEXP_BUTTON_RIGHT 12
#define AWEXP_BUTTON_UP 13
#define AWEXP_BUTTON_LEFT 14
#define AWEXP_BUTTON_DOWN 15

#define PWDN_GPIO_NUM     -1 // connected through expander
#define RESET_GPIO_NUM    -1 // connected through expander
#define XCLK_GPIO_NUM      8
#define SIOD_GPIO_NUM     SDA
#define SIOC_GPIO_NUM     SCL

#define Y9_GPIO_NUM        7
#define Y8_GPIO_NUM        9
#define Y7_GPIO_NUM       10
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       14
#define Y4_GPIO_NUM       16
#define Y3_GPIO_NUM       15
#define Y2_GPIO_NUM       13
#define VSYNC_GPIO_NUM     5
#define HREF_GPIO_NUM      6
#define PCLK_GPIO_NUM     11


#endif /* Pins_Arduino_h */
