#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x239A
#define USB_PID          0x817B
#define USB_MANUFACTURER "Adafruit"
#define USB_PRODUCT      "Metro ESP32-P4"
#define USB_SERIAL       ""  // Empty string for MAC address

// Metro ESP32-P4 Rev A: Arduino pin numbers are the raw GPIO numbers.
#define LED_BUILTIN    13
#define BUILTIN_LED    LED_BUILTIN
#define PIN_NEOPIXEL   23
#define NEOPIXEL_NUM   1
#define RGB_BUILTIN    (PIN_NEOPIXEL + SOC_GPIO_PIN_COUNT)
#define RGB_BRIGHTNESS 64
#define PIN_BUTTON1    35

// Metro header UART. The separate UART0 debug header uses GPIO37/38.
static const uint8_t TX = 1;
static const uint8_t RX = 0;
#define TX1 TX
#define RX1 RX

// Shared by the Metro I2C header, STEMMA QT, CSI and DSI connectors.
static const uint8_t SDA = 33;
static const uint8_t SCL = 32;

// DSI connector GPIOs. The LT8912B adapter uses the backlight line for HPD_N.
#define PIN_DSI_RESET      20
#define PIN_DSI_BACKLIGHT  19
#define PIN_CSI_IO0        21
#define PIN_CSI_IO1        22
#define PIN_USB_HOST_POWER 14

// SPI is on the ICSP header; SS defaults to Metro D10.
static const uint8_t SS = 10;
static const uint8_t MOSI = 16;
static const uint8_t MISO = 17;
static const uint8_t SCK = 15;

static const uint8_t A0 = 49;
static const uint8_t A1 = 50;
static const uint8_t A2 = 51;
static const uint8_t A3 = 52;
static const uint8_t A4 = 53;
static const uint8_t A5 = 54;
#define PIN_BATTERY   18
#define PIN_SD_DETECT 45
#define PIN_C6_BOOT   46
#define PIN_C6_ENABLE 47
#define PIN_C6_WAKE   48
#define PIN_C6_RX     26
#define PIN_C6_TX     27

#define BOARD_HAS_SDIO_ESP_HOSTED
#define BOARD_SDIO_ESP_HOSTED_CLK   34
#define BOARD_SDIO_ESP_HOSTED_CMD   36
#define BOARD_SDIO_ESP_HOSTED_D0    31
#define BOARD_SDIO_ESP_HOSTED_D1    30
#define BOARD_SDIO_ESP_HOSTED_D2    29
#define BOARD_SDIO_ESP_HOSTED_D3    28
#define BOARD_SDIO_ESP_HOSTED_RESET PIN_C6_ENABLE

// Four-bit microSD uses the dedicated slot 0 pins, GPIO39-44.
#define BOARD_HAS_SDMMC
#define BOARD_SDMMC_SLOT          0
#define BOARD_SDMMC_POWER_CHANNEL 4
#define BOARD_MAX_SDMMC_FREQ      SDMMC_FREQ_DEFAULT

// GPIO39-48 require the P4's on-chip 3.3 V LDO_VO4 supply.
#define BOARD_PERIMAN_IO_LDO_AUTO        1
#define BOARD_PERIMAN_IO_LDO0_CHANNEL    4
#define BOARD_PERIMAN_IO_LDO0_GPIO_MIN   39
#define BOARD_PERIMAN_IO_LDO0_GPIO_MAX   48
#define BOARD_PERIMAN_IO_LDO0_VOLTAGE_MV 3300

#endif
