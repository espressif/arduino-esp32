#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       18

#define analogInputToDigitalPin(p)  (((p)<40)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

// SPI LoRa Radio
#define LORA_SCK 5      // GPIO5  - SX1276 SCK
#define LORA_MISO 19    // GPIO19 - SX1276 MISO
#define LORA_MOSI 27    // GPIO27 - SX1276 MOSI
#define LORA_CS 17      // GPIO17 - SX1276 CS
#define LORA_RST 18     // GPIO18 - SX1276 RST
#define LORA_IRQ 23     // GPIO23 - SX1276 IO0
#define LORA_IO0 LORA_IRQ  // alias
#define LORA_IO1 LORA_IRQ  // tied by diode to IO0
#define LORA_IO2 LORA_IRQ  // tied by diode to IO0

static const uint8_t LED_BUILTIN = 0; // ->2812 RGB !!!
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN

#define ANT_SELECT 16   // GPIO16 - External Antenna Switch

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 12;
static const uint8_t SCL = 13;
 
static const uint8_t SS    = 17;
static const uint8_t MOSI  = 22;
static const uint8_t MISO  = 37;
static const uint8_t SCK   = 13;

static const uint8_t A0 = 36;
static const uint8_t A1 = 37;
static const uint8_t A2 = 38;
static const uint8_t A3 = 39;
static const uint8_t A4 = 32;
static const uint8_t A5 = 33;
static const uint8_t A6 = 34;
static const uint8_t A7 = 35;
static const uint8_t A10 = 4;
static const uint8_t A11 = 0;
static const uint8_t A12 = 2;
static const uint8_t A13 = 15;
static const uint8_t A14 = 13;
static const uint8_t A15 = 12;
static const uint8_t A16 = 14;
static const uint8_t A17 = 27;
static const uint8_t A18 = 25;
static const uint8_t A19 = 26;

static const uint8_t T0 = 4;
static const uint8_t T1 = 0;
static const uint8_t T2 = 2;
static const uint8_t T3 = 15;
static const uint8_t T4 = 13;
static const uint8_t T5 = 12;
static const uint8_t T6 = 14;
static const uint8_t T7 = 27;
static const uint8_t T8 = 33;
static const uint8_t T9 = 32;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
