#ifndef Pins_Arduino_h
#define Pins_Arduino_h

/**
 * Variant: WT32-SC01 PLUS
 * Vendor: Wireless-Tag
 * Url: http://www.wireless-tag.com/portfolio/wt32-eth01/
 */

#include <stdint.h>

#define USB_VID          0x303A
#define USB_PID          0x80D0
#define USB_MANUFACTURER "PANLEE"
#define USB_PRODUCT      "SC01PLUS"
#define USB_SERIAL       ""
//GENERAL I/O
static const uint8_t BOOT_0 = 0;
static const uint8_t IO1 = 10;
static const uint8_t IO2 = 11;
static const uint8_t IO3 = 12;
static const uint8_t IO4 = 13;
static const uint8_t IO5 = 14;
static const uint8_t IO6 = 21;
//RS485
static const uint8_t TX = 42;
static const uint8_t RX = 1;
static const uint8_t RTS = 2;
//TOUCHSCREEN
static const uint8_t BL_PWM = 45;    //BACKLIGHT PWM
static const uint8_t LCD_RESET = 4;  //LCD RESET, MULTIPLEXED WITH TOUCH RESET
static const uint8_t LCD_RS = 0;     //COMMAND/DATA
static const uint8_t LCD_WR = 47;    //WRITE CLOCK
static const uint8_t LCD_TE = 48;    //FRAME SYNC
static const uint8_t LCD_DB0 = 9;
static const uint8_t LCD_DB1 = 46;
static const uint8_t LCD_DB2 = 3;
static const uint8_t LCD_DB3 = 8;
static const uint8_t LCD_DB4 = 18;
static const uint8_t LCD_DB5 = 17;
static const uint8_t LCD_DB6 = 16;
static const uint8_t LCD_DB7 = 15;

//SPEAKER
static const uint8_t LRCK = 35;
static const uint8_t BCLK = 36;
static const uint8_t DOUT = 37;

//TOUCHSCREEN DIGITIZER
static const uint8_t TP_INT = 7;
static const uint8_t SDA = 6;
static const uint8_t SCL = 5;
static const uint8_t RST = 4;
//MICRO SD CARD
static const uint8_t SD_CS = 41;
static const uint8_t SD_DI = 40;  //MOSI
static const uint8_t SD_DO = 38;  //MISO
static const uint8_t SD_CLK = 39;
static const uint8_t SS = 41;
static const uint8_t MOSI = 40;
static const uint8_t MISO = 38;
static const uint8_t SCK = 39;

#endif /* Pins_Arduino_h */
