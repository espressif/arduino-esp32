#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Default USB Settings
#define USB_VID          0x303a
#define USB_PID          0x80C5
#define USB_MANUFACTURER "WEMOS.CC"
#define USB_PRODUCT      "LOLIN-S2-PICO"
#define USB_SERIAL       "0"

// Default USB FirmwareMSC Settings
#define USB_FW_MSC_VENDOR_ID        "ESP32-S2"      //max 8 chars
#define USB_FW_MSC_PRODUCT_ID       "Firmware MSC"  //max 16 chars
#define USB_FW_MSC_PRODUCT_REVISION "1.23"          //max 4 chars
#define USB_FW_MSC_VOLUME_NAME      "S2-Firmware"   //max 11 chars
#define USB_FW_MSC_SERIAL_NUMBER    0x00000000

static const uint8_t LED_BUILTIN = 10;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 38;
static const uint8_t RX = 33;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS = 34;
static const uint8_t MOSI = 35;
static const uint8_t MISO = 37;
static const uint8_t SCK = 36;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 8;
static const uint8_t A8 = 9;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;
static const uint8_t A14 = 15;
static const uint8_t A15 = 16;
static const uint8_t A16 = 17;
static const uint8_t A17 = 18;
static const uint8_t A18 = 19;
static const uint8_t A19 = 20;

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

static const uint8_t DAC1 = 17;
static const uint8_t DAC2 = 18;

static const uint8_t S2_PICO_OLED_RESET = 18;

#endif /* Pins_Arduino_h */
