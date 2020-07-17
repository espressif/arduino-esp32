#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

/*
 * These boards all the same:
 * TTGO LoRa32 V2.1.6
 * TTGO LoRa32 T3 V1.6 20180606
 * TTGO LoRa32 V2.1 release 1.6
 */

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t LED_BUILTIN =  25;
#define BUILTIN_LED  LED_BUILTIN	// backward compatibility

static const uint8_t KEY_BUILTIN =  0;

static const uint8_t TX =   1;
static const uint8_t RX =   3;

static const uint8_t SDA =  21;
static const uint8_t SCL =  22;

static const uint8_t SS =   18;
static const uint8_t MOSI = 27;
static const uint8_t MISO = 19;
static const uint8_t SCK =  5;
static const uint8_t A0 =   36;
static const uint8_t A3 =   39;
static const uint8_t A4 =   32;
static const uint8_t A5 =   33;
static const uint8_t A6 =   34;
static const uint8_t A7 =   35;
static const uint8_t A10 =  4;
static const uint8_t A11 =  0;
static const uint8_t A12 =  2;
static const uint8_t A13 =  15;
static const uint8_t A14 =  13;
static const uint8_t A15 =  12;
static const uint8_t A16 =  14;
static const uint8_t A18 =  25;
static const uint8_t A19 =  26;

static const uint8_t T0 =   4;
static const uint8_t T1 =   0;
static const uint8_t T2 =   2;
static const uint8_t T3 =   15;
static const uint8_t T4 =   13;
static const uint8_t T5 =   12;
static const uint8_t T6 =   14;
static const uint8_t T8 =   33;
static const uint8_t T9 =   32;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

// I2C SSD1306 OLED Display
static const uint8_t OLED_SDA = 21;
static const uint8_t OLED_SCL = 22;

// SPI SX1276 LoRa Radio
static const uint8_t LORA_CS   = 18;
static const uint8_t LORA_MOSI = 27;
static const uint8_t LORA_MISO = 19;
static const uint8_t LORA_SCK  = 5;
static const uint8_t LORA_RST  = 23;
static const uint8_t LORA_DIO0 = 26;
static const uint8_t LORA_DIO1 = 33;
static const uint8_t LORA_DIO2 = 32;

// SD card
static const uint8_t SD_CS   = 13;
static const uint8_t SD_MOSI = 15;
static const uint8_t SD_MISO = 2;
static const uint8_t SD_SCK  = 14;
static const uint8_t SD_DAT1 = 4;
static const uint8_t SD_DAT2 = 12;

// battery
static const uint8_t VBAT = 35;		// 100k + 100k divider

#endif /* Pins_Arduino_h */
