#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS        48
#define NUM_ANALOG_INPUTS       20

static const uint8_t LED_BUILTIN = 3;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

#define QMA7981_ADDR 0x12

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t BAT_LV   = 14;

static const uint8_t TFT_CLK  = 21;
static const uint8_t TFT_CS   = 44;
static const uint8_t TFT_DC   = 43;
static const uint8_t TFT_MOSI = 47;
static const uint8_t TFT_MISO = -1;
static const uint8_t TFT_RST  = -1;
static const uint8_t TFT_BL   = 48;

static const uint8_t SD_CMD    = 38;
static const uint8_t SD_CLK    = 39;
static const uint8_t SD_SDA    = 40;

static const uint8_t I2S_SCK    = 41;
static const uint8_t I2S_WS     = 42;
static const uint8_t I2S_SDO    = 2;

static const uint8_t ADC = 1;

static const uint8_t DVP_VSYNC  = 6;
static const uint8_t DVP_HREF   = 7;
static const uint8_t XMCLK      = 15;
static const uint8_t DVP_PCLK   = 13;
static const uint8_t DVP_Y9     = 16;
static const uint8_t DVP_Y8     = 17;
static const uint8_t DVP_Y7     = 18;
static const uint8_t DVP_Y6     = 12;
static const uint8_t DVP_Y2     = 11;
static const uint8_t DVP_Y5     = 10;
static const uint8_t DVP_Y3     = 9;
static const uint8_t DVP_Y4     = 8;

#endif /* Pins_Arduino_h */
