#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303a
#define USB_PID          0x821B
#define USB_MANUFACTURER "LILYGO"
#define USB_PRODUCT      "T-Watch-S3"

#define DISP_WIDTH  (240)
#define DISP_HEIGHT (240)

#define DISP_MOSI (13)
#define DISP_MISO (-1)
#define DISP_SCK  (18)
#define DISP_RST  (-1)
#define DISP_CS   (12)
#define DISP_DC   (38)
#define DISP_BL   (45)

// touch screen
#define TP_INT (16)
#define TP_SDA (39)
#define TP_SCL (40)

// Interrupt IO port
#define RTC_INT    (17)
#define PMU_INT    (21)
#define SENSOR_INT (14)

// PDM microphone
#define MIC_SCK (44)
#define MIC_DAT (47)

// MAX98357A
#define I2S_BCLK (48)
#define I2S_WCLK (15)
#define I2S_DOUT (46)

#define IR_SEND (2)

// TX, RX pin connected to GPS
static const uint8_t TX = 42;
static const uint8_t RX = 41;

// BMA423,PCF8563,AXP2101,DRV2605L share I2C Bus
static const uint8_t SDA = 10;
static const uint8_t SCL = 11;

// Default sd cs pin
static const uint8_t SS = 5;
static const uint8_t MOSI = 1;
static const uint8_t MISO = 4;
static const uint8_t SCK = 3;

// LoRa and SD card share SPI bus
#define LORA_SCK  (SCK)   // share spi bus
#define LORA_MISO (MISO)  // share spi bus
#define LORA_MOSI (MOSI)  // share spi bus
#define LORA_CS   (5)
#define LORA_RST  (8)
#define LORA_BUSY (7)
#define LORA_IRQ  (9)

#define GPS_TX (TX)
#define GPS_RX (RX)

// Peripheral definition exists
#define USING_PCM_AMPLIFIER
#define USING_PDM_MICROPHONE
#define USING_PMU_MANAGE
#define USING_INPUT_DEV_TOUCHPAD
#define USING_IR_REMOTE

#endif /* Pins_Arduino_h */
