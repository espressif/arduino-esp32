#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x303A
#define USB_PID          0x82F4
#define USB_MANUFACTURER "FoBE Studio"
#define USB_PRODUCT      "FoBE Quill ESP32-S3 Mesh"
#define USB_SERIAL       ""  // Empty string for MAC address

// User LED
#define LED_BUILTIN 11
#define BUILTIN_LED LED_BUILTIN  // backward compatibility

static const uint8_t TX = 9;
static const uint8_t RX = 8;
#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 14;
static const uint8_t SCL = 13;
#define WIRE1_PIN_DEFINED
static const uint8_t SDA1 = 34;
static const uint8_t SCL1 = 33;

static const uint8_t SS = 45;
static const uint8_t MOSI = 39;
static const uint8_t SCK = 40;
static const uint8_t MISO = 41;

static const uint8_t A0 = 2;
static const uint8_t A1 = 3;
static const uint8_t A2 = 4;
static const uint8_t A3 = 5;
static const uint8_t A4 = 6;
static const uint8_t A5 = 7;

static const uint8_t D0 = 8;
static const uint8_t D1 = 9;
static const uint8_t D2 = 11;
static const uint8_t D3 = 38;
static const uint8_t D4 = 37;
static const uint8_t D5 = 36;
static const uint8_t D6 = 35;
static const uint8_t D7 = 34;
static const uint8_t D8 = 33;
static const uint8_t D9 = 47;
static const uint8_t D10 = 48;
static const uint8_t D11 = 21;
static const uint8_t D12 = 18;
static const uint8_t D13 = 17;

/*
 * Screen
 */
#define PIN_SSD1312_SDA (14)
#define PIN_SSD1312_SCL (13)  // DC
#define PIN_OLED_EN (12)

/*
 * LoRa
 */
#define PIN_SX126X_NSS (45)
#define PIN_SX126X_DIO1 (42)
#define PIN_SX126X_BUSY (43)
#define PIN_SX126X_RESET (44)
#define PIN_SX126X_TXEN (-1)
#define PIN_SX126X_RXEN (46)
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8

/*
 * MFP Pins
 */
#define PIN_MFP1 (38)
#define PIN_MFP2 (37)
#define PIN_MFP3 (36)
#define PIN_MFP4 (35)
#define PIN_MFP_PWR (1)

#endif /* Pins_Arduino_h */
