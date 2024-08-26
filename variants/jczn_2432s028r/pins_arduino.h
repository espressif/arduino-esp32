
#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t D35 = 35;
static const uint8_t D22 = 22;
static const uint8_t D27 = 27;
static const uint8_t D21 = 21;

static const uint8_t A6 = 34;
static const uint8_t A17 = 27;

static const uint8_t T7 = 27;

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 18;

#define CYD_TP_IRQ     36
#define CYD_TP_MOSI    32
#define CYD_TP_MISO    39
#define CYD_TP_CLK     25
#define CYD_TP_CS      33
#define CYD_TP_DIN     CYD_TP_MOSI
#define CYD_TP_OUT     CYD_TP_MOSI
#define CYD_TP_SPI_BUS VSPI

#define CYD_TFT_DC      2
#define CYD_TFT_MISO    12
#define CYD_TFT_MOSI    13
#define CYD_TFT_SCK     14
#define CYD_TFT_CS      15
#define CYD_TFT_RS      CYD_TFT_DC
#define CYD_TFT_SDO     CYD_TFT_MISO
#define CYD_TFT_SDI     CYD_TFT_MOSI
#define CYD_TFT_SPI_BUS HSPI

#define CYD_TFT_WIDTH     320
#define CYD_TFT_HEIGHT    240
#define CYD_SCREEN_WIDTH  CYD_TFT_WIDTH
#define CYD_SCREEN_HEIGHT CYD_TFT_HEIGHT

#define CYD_TFT_BL          21
#define CYD_TFT_BL_ENABLE() ((pinMode(CYD_TFT_BL, OUTPUT)))
#define CYD_TFT_BL_OFF()    (digitalWrite(CYD_TFT_BL, 0))
#define CYD_TFT_BL_ON()     (digitalWrite(CYD_TFT_BL, 1))

#define CYD_LED_RED   4
#define CYD_LED_GREEN 16
#define CYD_LED_BLUE  17

#define CYD_AUDIO_OUT 26

#define CYD_USER_BUTTON 0

#define CYD_SD_SS      5
#define CYD_SD_MOSI    23
#define CYD_SD_MISO    19
#define CYD_SD_SCK     18
#define CYD_SD_SPI_BUS VSPI

#define CYD_LDR 34

#define CYD_LED_RED_OFF()   (digitalWrite(CYD_LED_RED, 1))
#define CYD_LED_RED_ON()    (digitalWrite(CYD_LED_RED, 0))
#define CYD_LED_GREEN_OFF() (digitalWrite(CYD_LED_GREEN, 1))
#define CYD_LED_GREEN_ON()  (digitalWrite(CYD_LED_GREEN, 0))
#define CYD_LED_BLUE_OFF()  (digitalWrite(CYD_LED_BLUE, 1))
#define CYD_LED_BLUE_ON()   (digitalWrite(CYD_LED_BLUE, 0))
#define CYD_LED_RGB_OFF() \
  CYD_LED_RED_OFF();      \
  CYD_LED_GREEN_OFF();    \
  CYD_LED_BLUE_OFF()
#define CYD_LED_RGB_ON() \
  CYD_LED_RED_ON();      \
  CYD_LED_GREEN_ON();    \
  CYD_LED_BLUE_ON()
#define CYD_LED_WHITE_OFF() CYD_LED_RGB_OFF()
#define CYD_LED_WHITE_ON()  CYD_LED_RGB_ON()

#endif /* Pins_Arduino_h */
