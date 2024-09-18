#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// BN: ESP32 Family Device
#define USB_VID 0x303a
#define USB_PID 0x8242

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-AMOLED-2.41"
#define USB_SERIAL       ""

// display QSPI SPI2
#define QSPI_CS       9
#define QSPI_SCK      10
#define QSPI_D0       11
#define QSPI_D1       12
#define QSPI_D2       13
#define QSPI_D3       14
#define AMOLED_RESET  21
#define AMOLED_TE     -1
#define AMOLED_PWR_EN -1

// Touch I2C
#define TP_SCL 48
#define TP_SDA 47
#define TP_RST -1
#define TP_INT -1

// Onboard RTC for PCF85063
#define RTC_SCL     48
#define RTC_SDA     47
#define RTC_ADDRESS 0x51
#define RTC_INT     -1

// Onboard  QMI8658 IMU
#define QMI8658_SDA     47
#define QMI8658_SCL     48
#define QMI8658_ADDRESS 0x6b
#define QMI8658_INT1    -1

// Partial voltage measurement method
#define BAT_ADC 17

// Def for I2C that shares the IMU I2C pins
static const uint8_t SDA = 47;
static const uint8_t SCL = 48;

// UART0 pins
static const uint8_t TX = 43;
static const uint8_t RX = 44;

//esp32s3-PSFlash   SPI1/SPI0
static const uint8_t SS = 34;    // FSPICS0
static const uint8_t MOSI = 35;  // FSPID
static const uint8_t MISO = 37;  // FSPIQ
static const uint8_t SCK = 36;   // FSPICLK
#endif                           /* Pins_Arduino_h */
