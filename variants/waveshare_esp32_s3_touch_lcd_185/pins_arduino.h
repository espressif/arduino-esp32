#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// BN: ESP32 Family Device
#define USB_VID 0x303a
#define USB_PID 0x8290

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-LCD-1.85"
#define USB_SERIAL       ""

// display QSPI SPI2
#define LCD_SPI_DATA0       46
#define LCD_SPI_DATA1       45
#define LCD_SPI_DATA2       42
#define LCD_SPI_DATA3       41
#define LCD_SPI_SCK         40
#define LCD_SPI_CS          21
#define LCD_TE_IO           18

#define LCD_Backlight_IO    5
#define LCD_RESET_IO        -1

// Touch I2C
#define TP_SDA_IO           1
#define TP_SCL_IO           3
#define TP_INT_IO           4
#define TP_RST_IO           -1

// Onboard PCF85063 / QMI8658 / I2C Port
#define I2C_SCL_IO          10
#define I2C_SDA_IO          11

// Onboard RTC for PCF85063
#define PCF85063_ADDRESS    0x51
#define PCF85063_SCL_IO     I2C_SCL_IO
#define PCF85063_SDA_IO     I2C_SDA_IO
#define PCF85063__INT       9              

// Onboard  QMI8658 IMU
#define QMI8658_L_SLAVE_ADDRESS    0x6B
#define QMI8658_H_SLAVE_ADDRESS    0x6A

#define QMI8658_ADDRESS     QMI8658_L_SLAVE_ADDRESS
#define QMI8658_SCL_IO      I2C_SCL_IO
#define QMI8658_SDA_IO      I2C_SDA_IO
#define QMI8658_INT1_IO     -1                          // Using extended IO5
#define QMI8658_INT2_IO     -1                          // Using extended IO4

// Partial voltage measurement method
#define BAT_ADC_IO          8

// Def for I2C that shares the IMU I2C pins
static const uint8_t SDA = I2C_SDA_IO;
static const uint8_t SCL = I2C_SCL_IO;

// UART0 pins
static const uint8_t TX = 43;
static const uint8_t RX = 44;

//esp32s3-PSFlash   SPI1/SPI0
static const uint8_t SS = 34;    // FSPICS0
static const uint8_t MOSI = 35;  // FSPID
static const uint8_t MISO = 37;  // FSPIQ
static const uint8_t SCK = 36;   // FSPICLK
#endif                           /* Pins_Arduino_h */
