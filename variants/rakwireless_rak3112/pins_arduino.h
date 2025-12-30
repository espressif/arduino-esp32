#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Reference: RAK3112 Module Datasheet
// https://docs.rakwireless.com/product-categories/wisduo/rak3112-module/datasheet/

// Note:GPIO33,GPIO34,GPIO35.GPIO36,GPIO37 is not available in the 8MB and 16MB Octal PSRAM version

#define USB_VID 0x303a
#define USB_PID 0x1001

#define LED_GREEN 46
#define LED_BLUE  45

static const uint8_t LED_BUILTIN = LED_GREEN;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t BAT_VOLT = 21;
#define BAT_VOLT_PIN BAT_VOLT

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 9;
static const uint8_t SCL = 40;

#define WIRE1_PIN_DEFINED
static const uint8_t SDA1 = 17;
static const uint8_t SCL1 = 18;

static const uint8_t SS = 12;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 10;
static const uint8_t SCK = 13;

#define LORA_ANT_SWITCH 4  // Antenna switch power control pin

#define LORA_SCK  5  // SX1262 SCK
#define LORA_MISO 3  // SX1262 MISO
#define LORA_MOSI 6  // SX1262 MOSI
#define LORA_CS   7  // SX1262 CS
#define LORA_RST  8  // SX1262 RST

#define LORA_DIO1 47  //SX1262 DIO1
#define LORA_BUSY 48
#define LORA_IRQ  LORA_DIO1

// For WisBlock modules, see: https://docs.rakwireless.com/Product-Categories/WisBlock/
#define WB_IO1  21
#define WB_IO2  14
#define WB_IO3  41
#define WB_IO4  42
#define WB_IO5  38
#define WB_IO6  39
#define WB_A0   1
#define WB_A1   2
#define WB_CS   12
#define WB_LED1 46
#define WB_LED2 45

#endif /* Pins_Arduino_h */
