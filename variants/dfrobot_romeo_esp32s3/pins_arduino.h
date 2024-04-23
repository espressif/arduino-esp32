#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 1;
static const uint8_t SCL = 2;

static const uint8_t MOSI = 15;
static const uint8_t MISO = 16;
static const uint8_t SCK = 17;
static const uint8_t SS = 18;

#define GDI_DISPLAY_FPC_INTERFACE
#ifdef GDI_DISPLAY_FPC_INTERFACE

#define GDI_BLK      21
#define GDI_SPI_SCLK SCK
#define GDI_SPI_MOSI MOSI
#define GDI_SPI_MISO MISO
#define GDI_DC       3
#define GDI_RES      38
#define GDI_CS       18
#define GDI_SDCS     0
#define GDI_FCS      7
#define GDI_TCS      12
#define GDI_SCL      SCL
#define GDI_SDA      SDA
#define GDI_INT      13
#define GDI_BUSY_TE  14

#endif /* GDI_DISPLAY_FPC_INTERFACE */

// CAM
#define CAM_DVP_INTERFACE
#ifdef CAM_DVP_INTERFACE

#define CAM_D5    4
#define CAM_PCLK  5
#define CAM_VSYNC 6
#define CAM_D6    7
#define CAM_D7    8
#define CAM_D8    46
#define CAM_D9    48
#define CAM_XMCLK 45
#define CAM_D2    39
#define CAM_D3    40
#define CAM_D4    41
#define CAM_HREF  42
#define CAM_SCL   SCL
#define CAM_SDA   SDA

#endif /* CAM_DVP_INTERFACE */

// Motor
#define MOTOR_INTERFACE
#ifdef MOTOR_INTERFACE

#define M1_EN 12
#define M1_PH 13
#define M2_EN 14
#define M2_PH 21
#define M3_EN 9
#define M3_PH 10
#define M4_EN 47
#define M4_PH 11

#endif

#endif /* Pins_Arduino_h */
