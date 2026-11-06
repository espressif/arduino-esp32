#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303A
#define USB_PID          0x81B5
#define USB_MANUFACTURER "LILYGO"
#define USB_PRODUCT      "T-Deck"

#define DISP_WIDTH  (240)
#define DISP_HEIGHT (320)

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 18;
static const uint8_t SCL = 8;

#define SD_CS                    (39)
#define LILYGO_TDECK_SD_SPI_FREQ (800000U)
static const uint8_t SS = SD_CS;
static const uint8_t MOSI = 41;
static const uint8_t MISO = 38;
static const uint8_t SCK = 40;

#define BOARD_POWERON (10)
#define BAT_ADC       (4)

#define TP_INT (16)
#define TP_RST (-1)
#define KB_INT (46)

#define TRACKBALL_RIGHT (2)
#define TRACKBALL_UP    (3)
#define TRACKBALL_LEFT  (1)
#define TRACKBALL_DOWN  (15)
#define TRACKBALL_CLICK (0)

#define I2S_WS    (5)
#define I2S_SCK   (7)
#define I2S_SDOUT (6)

#define MIC_I2S_MCLK (48)
#define MIC_I2S_WS   (21)
#define MIC_I2S_SCK  (47)
#define MIC_I2S_SDIN (14)

#define GPS_TX (TX)
#define GPS_RX (RX)

#define LORA_SCK  (SCK)
#define LORA_MISO (MISO)
#define LORA_MOSI (MOSI)
#define LORA_CS   (9)
#define LORA_BUSY (13)
#define LORA_RST  (17)
#define LORA_IRQ  (45)

#define DISP_MOSI (MOSI)
#define DISP_MISO (MISO)
#define DISP_SCK  (SCK)
#define DISP_RST  (-1)
#define DISP_CS   (12)
#define DISP_DC   (11)
#define DISP_BL   (42)

#define HAS_SD_CARD_SOCKET
#define HAS_TOUCHSCREEN
#define USING_INPUT_DEV_TOUCHPAD
#define USING_TDECK_KEYBOARD
#define USING_TDECK_TRACKBALL
#define USING_ES7210

#endif /* Pins_Arduino_h */
