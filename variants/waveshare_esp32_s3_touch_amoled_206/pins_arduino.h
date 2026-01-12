#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"


// SD
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 17


// BN: ESP32 Family Device
#define USB_VID 0x303a
#define USB_PID 0x1001

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-AMOLED-2.06"
#define USB_SERIAL       ""

// display AMOLED SPI2
#define AMOLED_CS       12
#define AMOLED_SCK      11
#define AMOLED_D0       4
#define AMOLED_D1       5
#define AMOLED_D2       6
#define AMOLED_D3       7
#define AMOLED_RESET  8
#define AMOLED_TE     13
#define AMOLED_PWR_EN 21
#define AMOLED_WIDTH  410
#define AMOLED_HEIGHT 502

// Touch I2C
#define TP_SCL 14
#define TP_SDA 15
#define TP_RST 9
#define TP_INT 38

// Onboard RTC for PCF85063
#define RTC_SCL     14
#define RTC_SDA     15
#define RTC_ADDRESS 0x51
#define RTC_INT     39

// Onboard  QMI8658 IMU
#define QMI8658_SDA     14
#define QMI8658_SCL     15
#define QMI8658_ADDRESS 0x6b
#define QMI8658_INT1    21

// Partial voltage measurement method
#define BAT_ADC      33
#define BAT_VOLT_PIN BAT_ADC

// Def for I2C that shares the IMU I2C pins
static const uint8_t SDA = 15;
static const uint8_t SCL = 14;

// UART0 pins
static const uint8_t TX = 43;
static const uint8_t RX = 44;

//esp32s3-PSFlash   SPI1/SPI0
#define SS  32    // FSPICS0
#define MOSI  35 // FSPID
#define MISO  34  // FSPIQ
#define SCK 33   // FSPICLK
#endif                           /* Pins_Arduino_h */
