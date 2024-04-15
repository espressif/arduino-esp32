#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"


static const uint8_t LED_BUILTIN = 15;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t SDA = 19;
static const uint8_t SCL = 20;

static const uint8_t SS = 5;
static const uint8_t MOSI = 22;
static const uint8_t MISO = 21;
static const uint8_t SCK = 23;

static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;

static const uint8_t D2 = 8;
static const uint8_t D3 = 14;
static const uint8_t D6 = 1;
static const uint8_t D7 = 18;
static const uint8_t D9 = 9;
static const uint8_t D11 = 7;
static const uint8_t D12 = 6;
static const uint8_t D13 = 15;

#define GDI_DISPLAY_FPC_INTERFACE
#ifdef GDI_DISPLAY_FPC_INTERFACE

#define GDI_BLK D13
#define GDI_SPI_SCLK SCK
#define GDI_SPI_MOSI MOSI
#define GDI_SPI_MISO MISO
#define GDI_DC D2
#define GDI_RES D3
#define GDI_CS D6  //LCD_CS
#define GDI_SDCS D7
#define GDI_FCS -1
#define GDI_TCS D12
#define GDI_SCL SCL
#define GDI_SDA SDA
#define GDI_INT D11
#define GDI_BUSY_TE -1

#endif

#endif /* Pins_Arduino_h */
