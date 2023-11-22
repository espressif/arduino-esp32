#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID            0x303A
#define USB_PID            0x81BB
#define USB_MANUFACTURER   "PowerFeather"
#define USB_PRODUCT        "ESP32-S3 PowerFeather"
#define USB_SERIAL         ""

#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS        23
#define NUM_ANALOG_INPUTS       13

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

static const uint8_t ALARM = GPIO_NUM_7;
static const uint8_t INT =   GPIO_NUM_5;

static const uint8_t LED =   GPIO_NUM_46;
static const uint8_t BTN =   GPIO_NUM_0;
static const uint8_t EN =    GPIO_NUM_13;

static const uint8_t TX =    GPIO_NUM_44;
static const uint8_t RX =    GPIO_NUM_42;
static const uint8_t TX0 =   GPIO_NUM_43;

static const uint8_t MISO =  GPIO_NUM_41;
static const uint8_t MOSI =  GPIO_NUM_40;
static const uint8_t SCK =   GPIO_NUM_39;

static const uint8_t SCL =   GPIO_NUM_36;
static const uint8_t SDA =   GPIO_NUM_35;

static const uint8_t A0 =    GPIO_NUM_10;
static const uint8_t A1 =    GPIO_NUM_9;
static const uint8_t A2 =    GPIO_NUM_8;
static const uint8_t A3 =    GPIO_NUM_3;
static const uint8_t A4 =    GPIO_NUM_2;
static const uint8_t A5 =    GPIO_NUM_1;

static const uint8_t D5 =    GPIO_NUM_15;
static const uint8_t D6 =    GPIO_NUM_16;
static const uint8_t D9 =    GPIO_NUM_17;
static const uint8_t D10 =   GPIO_NUM_18;
static const uint8_t D11 =   GPIO_NUM_45;
static const uint8_t D12 =   GPIO_NUM_12;
static const uint8_t D13 =   GPIO_NUM_11;

#endif /* Pins_Arduino_h */
