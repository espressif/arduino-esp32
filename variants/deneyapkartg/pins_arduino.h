#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303A
#define USB_PID 0x814A
#define USB_MANUFACTURER "Turkish Technology Team Foundation (T3)"
#define USB_PRODUCT "DENEYAP KART G"
#define USB_SERIAL ""  // Empty string for MAC address

static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + 10;  //D3
#define BUILTIN_LED LED_BUILTIN                              // backward compatibility
#define LED_BUILTIN LED_BUILTIN                              // allow testing #ifdef LED_BUILTIN
#define RGB_BUILTIN LED_BUILTIN
#define RGBLED LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t GPKEY = 9;
#define KEY_BUILTIN GPKEY
#define BUILTIN_KEY GPKEY
#define BT GPKEY

static const uint8_t TX = 21;
static const uint8_t RX = 20;
#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 8;
static const uint8_t SCL = 2;

static const uint8_t SS = 7;
static const uint8_t MOSI = 6;
static const uint8_t MISO = 5;
static const uint8_t SCK = 4;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;

static const uint8_t D0 = 20;
static const uint8_t D1 = 21;
static const uint8_t D2 = 9;
static const uint8_t D3 = 10;
static const uint8_t D4 = 8;
static const uint8_t D5 = 7;
static const uint8_t D6 = 2;

static const uint8_t PWM0 = 0;
static const uint8_t PWM1 = 1;
static const uint8_t PWM2 = 3;

#endif /* Pins_Arduino_h */
