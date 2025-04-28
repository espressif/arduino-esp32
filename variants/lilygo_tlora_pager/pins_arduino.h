#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#ifndef digitalPinToInterrupt
#define digitalPinToInterrupt(p) (((p) < 48) ? (p) : -1)
#endif

#define USB_VID          0x303a
#define USB_PID          0x82D4
#define USB_MANUFACTURER "LILYGO"
#define USB_PRODUCT      "T-LoRa-Pager"

// ST7796
#define DISP_WIDTH  (222)
#define DISP_HEIGHT (480)
#define SD_CS       (21)

static const uint8_t TX = 43;
static const uint8_t RX = 44;

// BHI260,PCF85063,BQ25896,DRV2605L,ES8311 share I2C Bus
static const uint8_t SDA = 3;
static const uint8_t SCL = 2;

// Default sd cs pin
static const uint8_t SS = SD_CS;
static const uint8_t MOSI = 34;
static const uint8_t MISO = 33;
static const uint8_t SCK = 35;

#define KB_INT       (6)
#define KB_BACKLIGHT (46)

// Rotary
#define ROTARY_A (40)
#define ROTARY_B (41)
#define ROTARY_C (7)

// Interrupt IO port
#define RTC_INT    (1)
#define NFC_INT    (5)
#define SENSOR_INT (8)
#define NFC_CS     (39)

// ES8311
#define I2S_WS    (18)
#define I2S_SCK   (11)
#define I2S_MCLK  (10)
#define I2S_SDOUT (45)
#define I2S_SDIN  (17)

// GPS
#define GPS_TX  (12)
#define GPS_RX  (4)
#define GPS_PPS (13)

// LoRa, SD, ST25R3916 card share SPI bus
#define LORA_SCK  (SCK)   // share spi bus
#define LORA_MISO (MISO)  // share spi bus
#define LORA_MOSI (MOSI)  // share spi bus
#define LORA_CS   (36)
#define LORA_RST  (47)
#define LORA_BUSY (48)
#define LORA_IRQ  (14)

// SPI interface display
#define DISP_MOSI (MOSI)
#define DISP_MISO (MISO)
#define DISP_SCK  (SCK)
#define DISP_RST  (-1)
#define DISP_CS   (38)
#define DISP_DC   (37)
#define DISP_BL   (42)

// External expansion chip IO definition
#define EXPANDS_DRV_EN    (0)
#define EXPANDS_AMP_EN    (1)
#define EXPANDS_KB_RST    (2)
#define EXPANDS_LORA_EN   (3)
#define EXPANDS_GPS_EN    (4)
#define EXPANDS_NFC_EN    (5)
#define EXPANDS_GPS_RST   (7)
#define EXPANDS_KB_EN     (8)
#define EXPANDS_GPIO_EN   (9)
#define EXPANDS_SD_DET    (10)
#define EXPANDS_SD_PULLEN (11)
#define EXPANDS_SD_EN     (12)

// Peripheral definition exists
#define USING_AUDIO_CODEC
#define USING_XL9555_EXPANDS
#define USING_PPM_MANAGE
#define USING_BQ_GAUGE
#define USING_INPUT_DEV_ROTARY
#define USING_INPUT_DEV_KEYBOARD
#define USING_ST25R3916
#define USING_BHI260_SENSOR
#define HAS_SD_CARD_SOCKET

#endif /* Pins_Arduino_h */
