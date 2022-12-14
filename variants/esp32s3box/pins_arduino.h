#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS        48
#define NUM_ANALOG_INPUTS       20

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 41;
static const uint8_t SCL = 40;

static const uint8_t SS    = 10;
static const uint8_t MOSI  = 11;
static const uint8_t MISO  = 13;
static const uint8_t SCK   = 12;

static const uint8_t A8 = 9;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;

static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;

// Wire1 for ES7210 MIC ADC, ES8311 I2S DAC, ICM-42607-P IMU and TT21100 Touch Panel
#define I2C_SDA     8
#define I2C_SCL    18

#define ES7210_ADDR    0x40 //MIC ADC
#define ES8311_ADDR    0x18 //I2S DAC
#define ICM42607P_ADDR 0x68 //IMU
#define TT21100_ADDR   0x24 //Touch Panel

#define TFT_DC      4
#define TFT_CS      5
#define TFT_MOSI    6
#define TFT_CLK     7
#define TFT_MISO    0
#define TFT_BL     45
#define TFT_RST    48

#define I2S_LRCK   47
#define I2S_MCLK    2
#define I2S_SCLK   17
#define I2S_SDIN   16
#define I2S_DOUT   15

#define PA_PIN     46 //Audio Amp Power
#define MUTE_PIN    1 //MUTE Button
#define TS_IRQ      3 //Touch Screen IRQ

#endif /* Pins_Arduino_h */
