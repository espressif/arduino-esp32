#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303A
#define USB_PID          0x82D1
#define USB_MANUFACTURER "senseBox"
#define USB_PRODUCT      "Eye"
#define USB_SERIAL       ""  // Empty string for MAC address

// Default USB FirmwareMSC Settings
#define USB_FW_MSC_VENDOR_ID        "senseBox"      // max 8 chars
#define USB_FW_MSC_PRODUCT_ID       "Eye"           // max 16 chars
#define USB_FW_MSC_PRODUCT_REVISION "1.4"           // max 4 chars
#define USB_FW_MSC_VOLUME_NAME      "senseBox_Eye"  // max 11 chars
#define USB_FW_MSC_SERIAL_NUMBER    0x00000000

#define PIN_RGB_LED 45  // RGB LED
#define RGBLED_PIN  45  // RGB LED
#define PIN_LED     45
#define RGBLED_NUM  1  // number of RGB LEDs

static const uint8_t LED_BUILTIN = PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN  // allow #ifdef LED_BUILTIN

// =============================================
// I2C (QWIIC)
// =============================================
#define PIN_WIRE_SDA 2
#define PIN_WIRE_SCL 1

static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

// =============================================
// SPI
// =============================================
#define PIN_SPI_MOSI 38
#define PIN_SPI_MISO 40
#define PIN_SPI_SCK  39
#define PIN_SPI_SS   41

static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK = PIN_SPI_SCK;
static const uint8_t SS = PIN_SPI_SS;

// IO Pins
#define PIN_IO14 14
static const uint8_t A14 = PIN_IO14;  // Analog
static const uint8_t D14 = PIN_IO14;  // Digital
static const uint8_t T14 = PIN_IO14;  // Touch

#define PIN_IO48 48
static const uint8_t A48 = PIN_IO48;  // Analog
static const uint8_t D48 = PIN_IO48;  // Digital
static const uint8_t T48 = PIN_IO48;  // Touch

// Button
#define PIN_BUTTON 47
#define BUTTON     47

// =============================================
// SD Card
// =============================================
// SD shares the SPI bus — CS, detect switch, enable are SD-specific
#define PIN_SD_CS     41
#define PIN_SD_SW     42
#define PIN_SD_ENABLE 3
#define SD_CS         PIN_SD_CS
#define SD_SW         PIN_SD_SW
#define SD_ENABLE     PIN_SD_ENABLE

// USB
#define PIN_USB_DM 19  // DM = D Minus
#define PIN_USB_DN 19  // DN = D Negative (same as DM)
#define PIN_USB_DP 20
#define USB_DN     PIN_USB_DN
#define USB_DP     PIN_USB_DP

// =============================================
// Camera
// =============================================
#define PWDN_GPIO_NUM  46
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5
#define Y9_GPIO_NUM    16
#define Y8_GPIO_NUM    17
#define Y7_GPIO_NUM    18
#define Y6_GPIO_NUM    12
#define Y5_GPIO_NUM    10
#define Y4_GPIO_NUM    8
#define Y3_GPIO_NUM    9
#define Y2_GPIO_NUM    11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

// Schematics
#define CAM_PWDN  46
#define CAM_RESET -1
#define CAM_XMCLK 15
#define CAM_SDA   4
#define CAM_SCL   5
#define CAM_Y9    16
#define CAM_Y8    17
#define CAM_Y7    18
#define CAM_Y6    12
#define CAM_Y5    10
#define CAM_Y4    8
#define CAM_Y3    9
#define CAM_Y2    11
#define CAM_VSYNC 6
#define CAM_HREF  7
#define CAM_PCLK  13

// =============================================
// LoRa
// =============================================
#define PIN_LORA_CS  43
#define PIN_LORA_INT 44
#define LORA_CS      PIN_LORA_CS
#define LORA_INT     PIN_LORA_INT

#endif /* Pins_Arduino_h */
