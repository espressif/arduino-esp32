#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303a
#define USB_PID          0x1001
#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Tiny"
#define USB_SERIAL       ""  // Empty string for MAC address

#define RGB_BUILTIN    38
#define RGB_BRIGHTNESS 64

#define MTDO 45
#define MTDI 47
#define MTMS 48
#define MTCK 44

// UART0 pins
static const uint8_t TX = 49;
static const uint8_t RX = 50;

static const uint8_t SDA = -1;
static const uint8_t SCL = -1;

// Mapping based on the ESP32S3 data sheet - alternate for SPI2
static const uint8_t SS = 32;    // FSPICS0
static const uint8_t MOSI = 35;  // FSPID
static const uint8_t MISO = 34;  // FSPIQ
static const uint8_t SCK = 33;   // FSPICLK

//static const uint8_t XTAL_32K_N = 22;
//static const uint8_t XTAL_32K_P = 21;

//static const uint8_t SPIWP = 31;
//static const uint8_t SPIHD = 30;

// Mapping based on the ESP32S3 data sheet - alternate for OUTPUT
static const uint8_t GPIO1 = 1;
static const uint8_t GPIO2 = 2;
static const uint8_t GPIO3 = 3;
static const uint8_t GPIO4 = 4;
static const uint8_t GPIO5 = 5;
static const uint8_t GPIO6 = 6;
static const uint8_t GPIO7 = 7;
static const uint8_t GPIO8 = 8;
static const uint8_t GPIO9 = 9;
static const uint8_t GPIO10 = 10;
static const uint8_t GPIO11 = 11;
static const uint8_t GPIO12 = 12;
static const uint8_t GPIO13 = 13;
static const uint8_t GPIO14 = 14;
static const uint8_t GPIO15 = 15;
static const uint8_t GPIO16 = 16;
static const uint8_t GPIO17 = 17;
static const uint8_t GPIO18 = 18;

static const uint8_t GPIO21 = 21;

static const uint8_t GPIO33 = 33;
static const uint8_t GPIO34 = 34;
static const uint8_t GPIO35 = 35;
static const uint8_t GPIO36 = 36;
static const uint8_t GPIO37 = 37;
static const uint8_t GPIO38 = 38;
static const uint8_t GPIO39 = 39;
static const uint8_t GPIO40 = 40;
static const uint8_t GPIO41 = 41;
static const uint8_t GPIO42 = 42;

static const uint8_t GPIO45 = 45;

static const uint8_t GPIO47 = 47;
static const uint8_t GPIO48 = 48;

#endif /* Pins_Arduino_h */
