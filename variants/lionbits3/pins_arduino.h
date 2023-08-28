#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS 40
#define NUM_ANALOG_INPUTS 16

#define analogInputToDigitalPin(p) (((p) < 20) ? (analogChannelToDigitalPin(p)) : -1)
#define digitalPinToInterrupt(p) (((p) < 40) ? (p) : -1)
#define digitalPinHasPWM(p) (p < 34)

static const uint8_t LED_BUILTIN = 0;   // GPIO0, 
static const uint8_t SWITCH_A = 46;     // GPIO46, 
static const uint8_t SWITCH_B = 47;     // GPIO47, 
//Wifi and Bluetooth LEDs
static const uint8_t WIFI_LED = 38;
static const uint8_t BT_LED = 37;


static const uint8_t TX = 1;
static const uint8_t RX = 3;
//-------------------------------------------------------------------
static const uint8_t U1RX = 9;  //IO,  U1RX,GPIO9
static const uint8_t U1TX = 10; //IO,  U1TX, GPIO10
/* LionBits3 pin setup */
static const uint8_t D0 = 3;    //RX,GPIO3,MCPWM 
static const uint8_t D1 = 1;    //TX,GPIO1, MCPWM
static const uint8_t D2 = 9;    //IO, U1RX,GPIO9,  TOUCH9, ADC1_CH8,MCPWM  
static const uint8_t D3 = 10;   //IO, U1TX,GPIO10, TOUCH10,ADC1_CH9,MCPWM  
static const uint8_t D4 = 11;   //IO, U2RX,GPIO11, TOUCH11,ADC2_CH0, MCPWM  
static const uint8_t D5 = 12;   //IO, U2TX, GPIO12, TOUCH12,ADC2_CH1, MCPWM 
static const uint8_t D6 = 13;   //IO, SDA,  GPIO13, TOUCH13,ADC2_CH2, MCPWM 
static const uint8_t D7 = 14;   //IO, SCl, GPIO14, TOUCH14,ADC2_CH3,  MCPWM  
static const uint8_t D8 = 15;   //IO, GPIO15,MCPWM  
static const uint8_t D9 = 16;   //IO,  GPIO16, MCPWM 
static const uint8_t D10 = 17;  //IO,  GPIO17,ADC2C_H6 MCPWM  
static const uint8_t D11 = 18;  //IO, GPIO18, ADC2C_H7MCPWM  
static const uint8_t D12 = 8;   //IO, GPIO8, MCPWM  
static const uint8_t D13 = 39;  //IO, GPIO39,  MCPWM   
static const uint8_t D14 = 40;  //IO,GPIO40,MCPWM  
static const uint8_t D15 = 41;  //IO, GPIO41, MCPWM   
static const uint8_t D16 = 48;  //IO, GPIO48,MCPWM   
static const uint8_t D17 = 21;  //IO,  GPIO21,  MCPWM  

//Other pins.
static const uint8_t BUZZER = 21;    
static const uint8_t LDR = 7;		 

static const uint8_t RGBLED = 48;

// Analog to Digital Converter (Support 5V) ADC2 pins not recommended while using Wifi
static const uint8_t A0  = 2;   //IO,  GPIO2,    TOUCH2, ADC1_CH1, */MAX 5V
static const uint8_t A1  = 1;   //IO,  GPIO1,    TOUCH1, ADC1_CH0, */MAX 5V  
static const uint8_t A2  = 3;   //IO,  GPIO3,    TOUCH3, ADC1_CH2, */MAX 5V 
static const uint8_t A3  = 4;   //IO,  GPIO4,    TOUCH4, ADC1_CH3, */MAX 5V 
static const uint8_t A4  = 5;   //IO,  GPIO5,    TOUCH5, ADC1_CH4, */MAX 5V    
static const uint8_t A5  = 6;   //IO,  GPIO6,    TOUCH6, ADC1_CH5, */MAX 5V   
static const uint8_t AD1 = 7;   //IO,  GPIO7,    TOUCH6, ADC1_CH5, */MAX 5V    

// Inbuilt Display Unit 128*128 ST7735 Driver New

static const uint8_t SDA = 40; // GPIO40;
static const uint8_t SCL = 41; // GPIO41;

/* Hardware HSPI */
static const uint8_t MOSI = 35; // GPIO35;
static const uint8_t MISO = 37; // GPIO37;
static const uint8_t SCK = 36;  // GPIO36;
static const uint8_t SS = 34;   // GPIO34;
static const uint8_t SDO  = 35; // GPIO35;
static const uint8_t SDI  = 37; // GPIO37;
//---------------------------------- 

static const uint8_t TFT_RST = 38;  //GPIO38;      
static const uint8_t TFT_SCLK = 35;  //GPIO35;     
static const uint8_t TFT_CS = 42;   //GPIO42;       
static const uint8_t TFT_DC = 37;   //GPIO37;  
static const uint8_t TFT_MOSI = 36;  //GPIO36; 

static const uint8_t LCD_A0 = 37;
static const uint8_t LCD_BACK_LIGHT = 45;
static const uint8_t DAC1 = 21; //  GPIO21, 
//LCD aditional pins

//Adafruit 128*128 ST7735 Driver New
static const uint8_t rst = 38; 
static const uint8_t sclk = 35; 
static const uint8_t cs = 42; 
static const uint8_t dc = 37; 
static const uint8_t mosi = 36; 

//Digital to Analog Converter 
#define DA1               (25)  //IO GPIO25, DAC_1, ADC2_CH8, RTC_GPIO6 
#define DA2               (26)  //IO GPIO26, DAC_2, ADC2_CH9, RTC_GPIO7 
#define VP 36  //  GPIO36, ADC1_CH0, RTC_GPIO0
#define VN 39  //  GPIO39, ADC1_CH3, RTC_GPIO3

#endif /* Pins_Arduino_h */