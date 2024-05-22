#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303a
#define USB_PID          0x814D  // for user apps (https://github.com/espressif/usb-pids/pull/77)
#define USB_MANUFACTURER "Crabik"
#define USB_PRODUCT      "Slot ESP32-S3"
#define USB_SERIAL       ""

static const uint8_t LED_BUILTIN = 21;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t S1 = 1;
static const uint8_t S2 = 12;
static const uint8_t S3 = 2;
static const uint8_t S4 = 11;
static const uint8_t S5 = 17;
static const uint8_t S6 = 18;
static const uint8_t S7 = 3;
static const uint8_t S8 = 4;
static const uint8_t S9 = 5;
static const uint8_t S10 = 6;
static const uint8_t S11 = 7;
static const uint8_t S12 = 8;
static const uint8_t S13 = 9;
static const uint8_t S14 = 10;
static const uint8_t S15 = 45;
static const uint8_t S16 = 46;
static const uint8_t S17 = 48;
static const uint8_t S18 = 47;
static const uint8_t S19 = 33;
static const uint8_t S20 = 34;

static const uint8_t TX = S12;
static const uint8_t RX = S11;
#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 13;
static const uint8_t SCL = 14;
static const uint8_t D = SDA;
static const uint8_t C = SCL;

static const uint8_t MOSI = 35;
static const uint8_t MISO = 37;
static const uint8_t SCK = 36;
static const uint8_t DO = MOSI;
static const uint8_t DI = MISO;
static const uint8_t CLK = SCK;
static const uint8_t CS1 = S5;
static const uint8_t CS2 = S6;
static const uint8_t SS = CS1;

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

static const uint8_t USB_DN = 19;
static const uint8_t USB_DP = 20;

static const uint8_t BOOT_BTN = 0;
static const uint8_t USER_LED = LED_BUILTIN;

static const uint8_t EN_TROYKA = 15;

static const uint8_t LIPO_ALERT = 16;

#endif /* Pins_Arduino_h */
