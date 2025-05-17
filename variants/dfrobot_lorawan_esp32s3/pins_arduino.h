#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define LED_BUILTIN 21

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

#define LORA_ANTPWR 42  //RXEN
#define LORA_RST    41  //RST
#define LORA_BUSY   40  //BUSY
#define LORA_DIO1   4   //DIO

static const uint8_t LORA_SS = 10;
static const uint8_t LORA_MOSI = 6;
static const uint8_t LORA_MISO = 5;
static const uint8_t LORA_SCK = 7;

static const uint8_t SS = 17;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

#define TFT_DC  14
#define TFT_CS  17
#define TFT_RST 15
#define TFT_BL  16  // Backlight pin

#endif /* Pins_Arduino_h */
