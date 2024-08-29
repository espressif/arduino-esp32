#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#ifndef digitalPinToInterrupt
#define digitalPinToInterrupt(p) (((p) < 48) ? (p) : -1)
#endif

#define USB_VID          0x303a
#define USB_PID          0x8227
#define USB_MANUFACTURER "LILYGO"
#define USB_PRODUCT      "T-Watch-Ultra"

#define DISP_WIDTH  (240)
#define DISP_HEIGHT (296)

#define DISP_D0  (39)
#define DISP_D1  (40)
#define DISP_D2  (45)
#define DISP_D3  (42)
#define DISP_SCK (41)
#define DISP_RST (6)
#define DISP_CS  (38)
#define DISP_TE  (37)

// touch screen
#define TP_INT (12)
#define TP_RST (46)
// Interrupt IO port
#define RTC_INT    (1)
#define PMU_INT    (7)
#define NFC_INT    (5)
#define SENSOR_INT (8)
#define NFC_RST    (4)

// PDM microphone
#define MIC_SCK (17)
#define MIC_DAT (18)

// MAX98357A
#define I2S_BCLK (9)
#define I2S_WCLK (10)
#define I2S_DOUT (11)

#define SD_CS (21)

// TX, RX pin connected to GPS
static const uint8_t TX = 43;
static const uint8_t RX = 44;

//BHI260,PCF85063,AXP2101,DRV2605L,PN532 share I2C Bus
static const uint8_t SDA = 2;
static const uint8_t SCL = 3;

// Default sd cs pin
static const uint8_t SS = SD_CS;
static const uint8_t MOSI = 34;
static const uint8_t MISO = 33;
static const uint8_t SCK = 35;

#define GPS_TX (TX)
#define GPS_RX (RX)

#define TP_SDA (SDA)
#define TP_SCL (SCL)

// LoRa and SD card share SPI bus
#define LORA_SCK  (SCK)   // share spi bus
#define LORA_MISO (MISO)  // share spi bus
#define LORA_MOSI (MOSI)  // share spi bus
#define LORA_CS   (36)
#define LORA_RST  (47)
#define LORA_BUSY (48)
#define LORA_IRQ  (14)

#endif /* Pins_Arduino_h */
