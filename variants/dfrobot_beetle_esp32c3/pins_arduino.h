#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID            0x3343
#define USB_PID            0x8364
#define USB_MANUFACTURER   "DFRobot"
#define USB_PRODUCT        "Beetle ESP32-C3"
#define USB_SERIAL         "" // Empty string for MAC adddress

static const uint8_t LED_BUILTIN = 10;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS    = 7;
static const uint8_t MOSI  = 6;
static const uint8_t MISO  = 5;
static const uint8_t SCK   = 4;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;

#define GDI_DISPLAY_FPC_INTERFACE
#ifdef  GDI_DISPLAY_FPC_INTERFACE

#define GDI_BLK   LED_BUILTIN
#define GDI_SPI_SCLK  SCK
#define GDI_SPI_MOSI  MOSI
#define GDI_SPI_MISO  MISO
#define GDI_DC    A1
#define GDI_RES   A2
#define GDI_CS    SS
#define GDI_SDCS  A0
#define GDI_TCS   A3
#define GDI_SCL   SCL
#define GDI_SDA   SDA

#endif

#endif /* Pins_Arduino_h */
