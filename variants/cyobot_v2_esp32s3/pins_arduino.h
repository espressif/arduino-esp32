#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_MANUFACTURER "CYOBot"
#define USB_PRODUCT      "CYOBrain ESP32S3"
#define USB_SERIAL       ""  // Empty string for MAC address

static const uint8_t BUTTON0 = 4;
static const uint8_t BUTTON1 = 38;
static const uint8_t LED = 24;

static const uint8_t BAT_MEAS = 6;
static const uint8_t CHAR_DET = 23;

static const uint8_t NEO_BASE = 7;
static const uint8_t NEO_BRAIN = 15;

static const uint8_t I2S0_MCLK = 16;
static const uint8_t I2S0_DSDIN = 8;
static const uint8_t I2S0_SCLK = 9;
static const uint8_t I2S0_LRCK = 45;

static const uint8_t SDA = 17;
static const uint8_t SCL = 18;

static const uint8_t SS = 5;
static const uint8_t MOSI = 2;
static const uint8_t MISO = 42;
static const uint8_t SCK = 41;

static const uint8_t ENCODER1_A = 39;
static const uint8_t ENCODER1_B = 40;
static const uint8_t ENCODER2_B = 19;
static const uint8_t ENCODER2_A = 20;

static const uint8_t UART1_RXD = 3;
static const uint8_t UART1_TXD = 1;

static const uint8_t GPIO46 = 46;
static const uint8_t ESP_IO0 = 0;

static const uint8_t SD_OUT = 10;
static const uint8_t SD_SPI_MOSI = 11;
static const uint8_t SD_SPI_CLK = 12;
static const uint8_t SD_SPI_MISO = 13;
static const uint8_t SD_SPI_CS = 14;

static const uint8_t PA_CTRL = 25;

#endif /* Pins_Arduino_h */
