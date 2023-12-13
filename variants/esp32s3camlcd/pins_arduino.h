#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 17;
static const uint8_t SCL = 18;

static const uint8_t SS    = 10;
static const uint8_t MOSI  = 11;
static const uint8_t MISO  = 13;
static const uint8_t SCK   = 12;

// Wire1 for Cam and TS
#define I2C_SDA    17
#define I2C_SCL    18

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     40
#define SIOD_GPIO_NUM     17
#define SIOC_GPIO_NUM     18
#define Y9_GPIO_NUM       39
#define Y8_GPIO_NUM       41
#define Y7_GPIO_NUM       42
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM        3
#define Y4_GPIO_NUM       14
#define Y3_GPIO_NUM       47
#define Y2_GPIO_NUM       13
#define VSYNC_GPIO_NUM    21
#define HREF_GPIO_NUM     38
#define PCLK_GPIO_NUM     11

#define TFT_FREQ    40000000
#define TFT_BITS           8
#define TFT_WIDTH        480
#define TFT_HEIGHT       320
#define TFT_WR             4
#define TFT_DC             2
#define TFT_D0            45
#define TFT_D1            16
#define TFT_D2            15
#define TFT_D3            10
#define TFT_D4             8
#define TFT_D5             7
#define TFT_D6             6
#define TFT_D7             5

#define SDMMC_CMD         20
#define SDMMC_CLK          9
#define SDMMC_DATA        19

#define MIC_CLK            0
#define MIC_DATA           1

#endif /* Pins_Arduino_h */
