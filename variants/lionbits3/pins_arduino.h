#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = 0;  //GPIO0,
#define BUILTIN_LED LED_BUILTIN        // backward compatibility
#define LED_BUILTIN LED_BUILTIN        // allow testing #ifdef LED_BUILTIN

static const uint8_t SWITCH_A = 46;  //GPIO46,
static const uint8_t SWITCH_B = 47;  //GPIO47,
//Wifi and Bluetooth LEDs
static const uint8_t WIFI_LED = 38;
static const uint8_t BT_LED = 37;

static const uint8_t TX = 1;
static const uint8_t RX = 3;
//-------------------------------------------------------------------
static const uint8_t U1RX = 9;   //IO,GPIO9
static const uint8_t U1TX = 10;  //IO,GPIO10
/* LionBits3 pin setup */
static const uint8_t D0 = 3;    //RX,GPIO3,MCPWM
static const uint8_t D1 = 1;    //TX,GPIO1,ADC1_CH0,MCPWM
static const uint8_t D2 = 9;    //IO,GPIO9,ADC1_CH8,TOUCH9,MCPWM
static const uint8_t D3 = 10;   //IO,GPIO10,ADC1_CH9,TOUCH10,MCPWM
static const uint8_t D4 = 11;   //IO,GPIO11,ADC2_CH0,TOUCH11,MCPWM
static const uint8_t D5 = 12;   //IO,GPIO12,ADC2_CH1,TOUCH12,MCPWM
static const uint8_t D6 = 13;   //IO,GPIO13,ADC2_CH2,TOUCH13,MCPWM
static const uint8_t D7 = 14;   //IO,GPIO14,ADC2_CH3,TOUCH14,MCPWM
static const uint8_t D8 = 15;   //IO,GPIO15,ADC2_CH4,MCPWM
static const uint8_t D9 = 16;   //IO,GPIO16,ADC2_CH5,MCPWM
static const uint8_t D10 = 17;  //IO,GPIO17,ADC2_CH6,MCPWM
static const uint8_t D11 = 18;  //IO,GPIO18,ADC2_CH7,MCPWM
static const uint8_t D12 = 8;   //IO,GPIO8,ADC1_CH7,MCPWM
static const uint8_t D13 = 39;  //IO,GPIO39,MCPWM
static const uint8_t D14 = 40;  //IO,GPIO40,MCPWM
static const uint8_t D15 = 41;  //IO,GPIO41,MCPWM
static const uint8_t D16 = 48;  //IO,GPIO48,MCPWM
static const uint8_t D17 = 21;  //IO,GPIO21,MCPWM

//Other pins.
static const uint8_t BUZZER = 21;
static const uint8_t LDR = 7;

static const uint8_t RGBLED = 48;

// Analog to Digital Converter (Support 5V) ADC2 pins not recommended while using Wifi
static const uint8_t A0 = 2;   //IO,GPIO2,ADC1_CH1,TOUCH2,MCPWM
static const uint8_t A1 = 1;   //IO,GPIO1,ADC1_CH0,TOUCH1,MCPWM
static const uint8_t A2 = 3;   //IO,GPIO3,ADC1_CH2,TOUCH3,MCPWM
static const uint8_t A3 = 4;   //IO,GPIO4,ADC1_CH3,TOUCH4,MCPWM
static const uint8_t A4 = 5;   //IO,GPIO5,ADC1_CH4,TOUCH5,MCPWM
static const uint8_t A5 = 6;   //IO,GPIO6,ADC1_CH5,TOUCH6,MCPWM
static const uint8_t A6 = 7;   //IO,GPIO7,ADC1_CH6,TOUCH7,MCPWM
static const uint8_t AD1 = 7;  //IO,GPIO7,ADC1_CH6,TOUCH7,MCPWM

// Inbuilt Display Unit 128*128 ST7735 Driver New

static const uint8_t SDA = 40;  //GPIO40;
static const uint8_t SCL = 41;  //GPIO41;

/* Hardware HSPI */
static const uint8_t MOSI = 35;  //GPIO35;
static const uint8_t MISO = 37;  //GPIO37;
static const uint8_t SCK = 36;   //GPIO36;
static const uint8_t SS = 34;    //GPIO34;
static const uint8_t SDO = 35;   //GPIO35;
static const uint8_t SDI = 37;   //GPIO37;
//----------------------------------

static const uint8_t TFT_RST = 38;   //GPIO38;
static const uint8_t TFT_SCLK = 35;  //GPIO35;
static const uint8_t TFT_CS = 42;    //GPIO42;
static const uint8_t TFT_DC = 37;    //GPIO37;
static const uint8_t TFT_MOSI = 36;  //GPIO36;

static const uint8_t LCD_A0 = 37;          //GPIO37,
static const uint8_t LCD_BACK_LIGHT = 45;  //GPIO45,
static const uint8_t DAC1 = 21;            //GPIO21,
//LCD additional pins

//Adafruit 128*128 ST7735 Driver New
static const uint8_t rst = 38;
static const uint8_t sclk = 35;
static const uint8_t cs = 42;
static const uint8_t dc = 37;
static const uint8_t mosi = 36;

#define VP 36  //GPIO36,
#define VN 39  //GPIO39,

#endif /* Pins_Arduino_h */
