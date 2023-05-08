#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>



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


static const uint8_t MOSI  = 15;
static const uint8_t MISO  = 16;
static const uint8_t SCK   = 17;



#define GDI_DISPLAY_FPC_INTERFACE
#ifdef  GDI_DISPLAY_FPC_INTERFACE

#define GDI_BLK       21
#define GDI_SPI_SCLK  SCK
#define GDI_SPI_MOSI  MOSI
#define GDI_SPI_MISO  MISO
#define GDI_DC        3
#define GDI_RES       38
#define GDI_CS        18
#define GDI_SDCS      0
#define GDI_FCS       7
#define GDI_TCS       12
#define GDI_SCL       SCL
#define GDI_SDA       SDA
#define GDI_INT       13
#define GDI_BUSY_TE   14

#endif

#endif /* Pins_Arduino_h */
