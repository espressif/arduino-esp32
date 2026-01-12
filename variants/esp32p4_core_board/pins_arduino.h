#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// BOOT_MODE 35
// BOOT_MODE2 36 pullup

// Some boards have too low voltage on this pin (board design bug)
// Use different pin with 3V and connect with 44
// and change this setup for the chosen pin (for example 38)
#define PIN_RGB_LED 44
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t TX = 37;
static const uint8_t RX = 38;

static const uint8_t SDA = 7;
static const uint8_t SCL = 8;

// Use GPIOs 36 or lower on the P4 DevKit to avoid LDO power issues with high numbered GPIOs.
static const uint8_t SS = 30;
static const uint8_t MOSI = 33;
static const uint8_t MISO = 32;
static const uint8_t SCK = 31;

static const uint8_t A0 = 16;
static const uint8_t A1 = 17;
static const uint8_t A2 = 18;
static const uint8_t A3 = 19;
static const uint8_t A4 = 20;
static const uint8_t A5 = 21;
static const uint8_t A6 = 22;
static const uint8_t A7 = 23;
static const uint8_t A8 = 49;
static const uint8_t A9 = 50;
static const uint8_t A10 = 51;
static const uint8_t A11 = 52;
static const uint8_t A12 = 53;
static const uint8_t A13 = 54;

static const uint8_t T0 = 2;
static const uint8_t T1 = 3;
static const uint8_t T2 = 4;
static const uint8_t T3 = 5;
static const uint8_t T4 = 6;
static const uint8_t T5 = 7;
static const uint8_t T6 = 8;
static const uint8_t T7 = 9;
static const uint8_t T8 = 10;
static const uint8_t T9 = 11;
static const uint8_t T10 = 12;
static const uint8_t T11 = 13;
static const uint8_t T12 = 14;
static const uint8_t T13 = 15;

//WIFI - ESP32C6
#define BOARD_HAS_SDIO_ESP_HOSTED
#define BOARD_SDIO_ESP_HOSTED_RESET 54 // chip_pu
#define BOARD_SDIO_ESP_HOSTED_BOOT  53 // io 9
#define BOARD_SDIO_ESP_HOSTED_CMD   52 // io 18
#define BOARD_SDIO_ESP_HOSTED_CLK   51 // io 19
#define BOARD_SDIO_ESP_HOSTED_D0    50 // io 20
#define BOARD_SDIO_ESP_HOSTED_D1    49 // io 21
#define BOARD_SDIO_ESP_HOSTED_D2    48 // io 22
#define BOARD_SDIO_ESP_HOSTED_D3    47 // io 23

#endif /* Pins_Arduino_h */
