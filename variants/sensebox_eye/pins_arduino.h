#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303A
#define USB_PID          0x82D1
#define USB_MANUFACTURER "senseBox"
#define USB_PRODUCT      "Eye ESP32S3"
#define USB_SERIAL       ""  // Empty string for MAC address

// Default USB FirmwareMSC Settings
#define USB_FW_MSC_VENDOR_ID        "senseBox"     // max 8 chars
#define USB_FW_MSC_PRODUCT_ID       "Eye ESP32S3"  // max 16 chars
#define USB_FW_MSC_PRODUCT_REVISION "1.00"         // max 4 chars
#define USB_FW_MSC_VOLUME_NAME      "senseBox"     // max 11 chars
#define USB_FW_MSC_SERIAL_NUMBER    0x00000000

#define PIN_RGB_LED 45  // RGB LED
#define RGBLED_PIN  45  // RGB LED
#define PIN_LED     45
#define RGBLED_NUM  1  // number of RGB LEDs

// Default I2C QWIIC-Ports
static const uint8_t SDA = 2;
static const uint8_t SCL = 1;
#define PIN_QWIIC_SDA 2
#define PIN_QWIIC_SCL 1

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

// UART Port
static const uint8_t TX = 43;
static const uint8_t RX = 44;
#define PIN_UART_TXD    43
#define PIN_UART_RXD    44
#define PIN_UART_ENABLE 26

// SD-Card
#define MISO      40
#define MOSI      38
#define SCK       39
#define SS        41
#define SD_ENABLE 3

#define PIN_SD_MISO   40
#define PIN_SD_MOSI   38
#define PIN_SD_SCLK   39
#define PIN_SD_CS     41
#define PIN_SD_ENABLE 3

// USB
#define PIN_USB_DM 19
#define PIN_USB_DP 20

// Camera
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

// LoRa
#define LORA_TX 43
#define LORA_RX 44

#endif /* Pins_Arduino_h */
