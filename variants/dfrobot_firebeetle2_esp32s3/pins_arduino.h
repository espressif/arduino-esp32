#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x3343
#define USB_PID 0x83CF
#define USB_MANUFACTURER   "DFRobot"
#define USB_PRODUCT        "FireBeetle 2 ESP32-S3"
#define USB_SERIAL         "" // Empty string for MAC adddress

#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS        48
#define NUM_ANALOG_INPUTS       20

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)


static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 1;
static const uint8_t SCL = 2;

static const uint8_t SS    = 10;
static const uint8_t MOSI  = 15;
static const uint8_t MISO  = 16;
static const uint8_t SCK   = 17;

static const uint8_t A0 = 4;
static const uint8_t A1 = 5;
static const uint8_t A2 = 6;
static const uint8_t A3 = 8;
static const uint8_t A4 = 10;
static const uint8_t A5 = 11;


static const uint8_t D2  = 3;
static const uint8_t D3  = 38;
static const uint8_t D5  = 7;
static const uint8_t D6  = 18;
static const uint8_t D7  = 9;
static const uint8_t D9  = 0;
static const uint8_t D10 = 14;
static const uint8_t D11 = 13;
static const uint8_t D12 = 12;
static const uint8_t D13 = 21;
static const uint8_t D14 = 47;

static const uint8_t LED_BUILTIN = D13;


static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;

#define GDI_DISPLAY_FPC_INTERFACE
#ifdef  GDI_DISPLAY_FPC_INTERFACE

#define GDI_BLK       21
#define GDI_SPI_SCLK  SCK
#define GDI_SPI_MOSI  MOSI
#define GDI_SPI_MISO  MISO
#define GDI_DC        3
#define GDI_RES       38
#define GDI_CS        18
#define GDI_SDCS      9
#define GDI_FCS       7
#define GDI_TCS       12
#define GDI_SCL       SCL
#define GDI_SDA       SDA
#define GDI_INT       13
#define GDI_BUSY_TE   14

#endif

#endif /* Pins_Arduino_h */
