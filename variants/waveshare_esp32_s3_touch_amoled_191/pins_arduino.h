#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// BN: ESP32 Family Device
#define USB_VID 0x303a
#define USB_PID 0x824B

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-AMOLED-1.91"
#define USB_SERIAL       ""

// display QSPI SPI2
#define QSPI_CS       6
#define QSPI_SCK      47
#define QSPI_D0       18
#define QSPI_D1       7
#define QSPI_D2       48
#define QSPI_D3       5
#define AMOLED_RESET  17
#define AMOLED_TE     -1
#define AMOLED_PWR_EN -1
// Touch I2C
#define TP_SCL 39
#define TP_SDA 40
#define TP_RST -1
#define TP_INT -1

// Partial voltage measurement method
#define BAT_ADC 1
// Onboard  QMI8658 IMU
#define QMI_INT1 45
#define QMI_INT1 46

//SD
#define SD_CS   9
#define SD_MISO 8
#define SD_MOSI 42
#define SD_CLK  47

//i2c

static const uint8_t SDA = 40;
static const uint8_t SCL = 39;

// UART0 pins
static const uint8_t TX = 43;
static const uint8_t RX = 44;

//esp32s3-PSFlash   SPI1/SPI0
static const uint8_t SS = 34;    // FSPICS0
static const uint8_t MOSI = 35;  // FSPID
static const uint8_t MISO = 37;  // FSPIQ
static const uint8_t SCK = 36;   // FSPICLK
#endif                           /* Pins_Arduino_h */
