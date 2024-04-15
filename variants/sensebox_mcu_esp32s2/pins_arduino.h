#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303A
#define USB_PID 0x81B8
#define USB_MANUFACTURER "senseBox"
#define USB_PRODUCT "MCU-S2 ESP32S2"
#define USB_SERIAL ""  // Empty string for MAC address

// Default USB FirmwareMSC Settings
#define USB_FW_MSC_VENDOR_ID "senseBox"         // max 8 chars
#define USB_FW_MSC_PRODUCT_ID "MCU-S2 ESP32S2"  // max 16 chars
#define USB_FW_MSC_PRODUCT_REVISION "1.00"      // max 4 chars
#define USB_FW_MSC_VOLUME_NAME "senseBox"       // max 11 chars
#define USB_FW_MSC_SERIAL_NUMBER 0x00000000

#define PIN_NEOPIXEL 1  // NeoPixel LED
#define NEOPIXEL_PIN 1  // NeoPixel LED
#define NEOPIXEL_NUM 1  // number of neopixels

// Default I2C QWIIC-Ports
static const uint8_t SDA = 39;
static const uint8_t SCL = 40;
#define PIN_QWIIC_SDA 39
#define PIN_QWIIC_SCL 40

// Secondary I2C MPU6050
#define WIRE1_PIN_DEFINED 1  // See Wire.cpp at bool TwoWire::initPins(int sdaPin, int sclPin)
static const uint8_t SCL1 = 42;
static const uint8_t SDA1 = 45;
#define PIN_I2C_SCL 42
#define PIN_I2C_SDA 45
#define PIN_I2C_INT 46

// SPI
static const uint8_t SS = 42;
static const uint8_t MOSI = 35;
static const uint8_t SCK = 36;
static const uint8_t MISO = 37;

// XBEE Pins
#define PIN_XBEE_ENABLE 41
#define PIN_XBEE_INT 33
#define PIN_XBEE_CS 34
#define PIN_XBEE_MOSI 35
#define PIN_XBEE_SCLK 36
#define PIN_XBEE_MISO 37
#define PIN_XBEE_RESET 38
#define PIN_XBEE_TXD 17
#define PIN_XBEE_RXD 18

// Alias XB1
#define PIN_XB1_ENABLE 41
#define PIN_XB1_INT 33
#define PIN_XB1_CS 34
#define PIN_XB1_MOSI 35
#define PIN_XB1_SCLK 36
#define PIN_XB1_MISO 37
#define PIN_XB1_RESET 38
#define PIN_XB1_TXD 17
#define PIN_XB1_RXD 18

// IO Pins
#define PIN_LED 1
#define PIN_IO2 2
#define PIN_IO3 3
#define PIN_IO4 4
#define PIN_IO5 5
#define PIN_IO6 6
#define PIN_IO7 7
#define IO_ENABLE 8

static const uint8_t A2 = PIN_IO2;
static const uint8_t A3 = PIN_IO3;
static const uint8_t A4 = PIN_IO4;
static const uint8_t A5 = PIN_IO5;
static const uint8_t A6 = PIN_IO6;
static const uint8_t A7 = PIN_IO7;

static const uint8_t D2 = PIN_IO2;
static const uint8_t D3 = PIN_IO3;
static const uint8_t D4 = PIN_IO4;
static const uint8_t D5 = PIN_IO5;
static const uint8_t D6 = PIN_IO6;
static const uint8_t D7 = PIN_IO7;

// UART Port
static const uint8_t TX = 43;
static const uint8_t RX = 44;
#define PIN_UART_TXD 43
#define PIN_UART_RXD 44
#define PIN_UART_ENABLE 26

// UART XBee
static const uint8_t TX1 = 17;
static const uint8_t RX1 = 18;

// PD-Sensor
#define PD_SENSE 14
#define PD_ENABLE 21
#define PIN_PD_ENABLE 21

// SD-Card
#define VSPI_MISO 13
#define VSPI_MOSI 11
#define VSPI_SCLK 12
#define VSPI_SS 10
#define SD_ENABLE 9

#define PIN_SD_ENABLE 9
#define PIN_SD_CS 10
#define PIN_SD_MOSI 11
#define PIN_SD_SCLK 12
#define PIN_SD_MISO 13

// USB
#define PIN_USB_DM 19
#define PIN_USB_DP 20

// Touch Pins
static const uint8_t T2 = PIN_IO2;
static const uint8_t T3 = PIN_IO3;
static const uint8_t T4 = PIN_IO4;
static const uint8_t T5 = PIN_IO5;
static const uint8_t T6 = PIN_IO6;
static const uint8_t T7 = PIN_IO7;


static const uint8_t DAC1 = 17;
static const uint8_t DAC2 = 18;


#endif /* Pins_Arduino_h */
