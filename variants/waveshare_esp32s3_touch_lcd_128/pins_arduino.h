#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x1A86
#define USB_PID 0x55D3
#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT "ESP32-S3 Touch LCD 1.28"
#define USB_SERIAL ""  // Empty string for MAC address

#define LCD_BACKLIGHT 2
#define LCD_DC 8
#define LCD_RST 14

#define TP_INT 5
#define TP_RST 13

#define IMU_INT1 4
#define IMU_INT2 3

static const uint8_t TX = 43;
static const uint8_t RX = 44;
#define TX1 TX
#define RX1 RX

static const uint8_t SCL = 7;
static const uint8_t SDA = 6;

static const uint8_t SS = 9;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 12;
static const uint8_t SCK = 10;

static const uint8_t A0 = 1;  // Connected through voltage divider to battery pin

#endif /* Pins_Arduino_h */
