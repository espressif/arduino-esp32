#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x820A

static const uint8_t LED_BUILTIN = 37;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t BUTTON_1 = 0;
static const uint8_t BAT_VOLT = 1;

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 18;
static const uint8_t SCL = 17;

#define WIRE1_PIN_DEFINED  //QWIIC
static const uint8_t SDA1 = 10;
static const uint8_t SCL1 = 21;

// SD Card SPI
static const uint8_t SS = 13;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 2;
static const uint8_t SCK = 14;

#define LORA_SCK  5  // LR1121 SCK
#define LORA_MISO 3  // LR1121 MISO
#define LORA_MOSI 6  // LR1121 MOSI
#define LORA_CS   7  // LR1121 CS
#define LORA_RST  8  // LR1121 RST

#define LORA_DIO9 36  // LR1121 DIO9
#define LORA_BUSY 34  // LR1121 BUSY
#define LORA_IRQ  LORA_DIO9

// P1
static const uint8_t PIN_42 = 45;
static const uint8_t PIN_46 = 46;
static const uint8_t PIN_45 = 45;
static const uint8_t PIN_41 = 41;
static const uint8_t PIN_40 = 40;
static const uint8_t PIN_39 = 39;
static const uint8_t PIN_43 = 43;
static const uint8_t PIN_44 = 44;
static const uint8_t PIN_38 = 38;

// P2
static const uint8_t PIN_37 = 37;
static const uint8_t PIN_36 = 36;
static const uint8_t PIN_0 = 0;
static const uint8_t PIN_35 = 35;
static const uint8_t PIN_34 = 34;
static const uint8_t PIN_33 = 33;
static const uint8_t PIN_47 = 47;
static const uint8_t PIN_48 = 48;
static const uint8_t PIN_12 = 12;
static const uint8_t PIN_8 = 8;
static const uint8_t PIN_15 = 15;
static const uint8_t PIN_16 = 16;

#endif /* Pins_Arduino_h */
