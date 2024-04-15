#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = 0;  // GPIO0, ADC2_CH1, TOUCH1, RTC_GPIO11, CLK_OUT1,EMAC_TX_CLK
#define BUILTIN_LED LED_BUILTIN        // backward compatibility
#define LED_BUILTIN LED_BUILTIN        // allow testing #ifdef LED_BUILTIN

static const uint8_t SWITCH_A = 2;  //  GPIO2, ADC2_CH2, TOUCH2, RTC_GPIO12, HSPIWP, HS2_DATA0,SD_DATA0
static const uint8_t SWITCH_B = 4;  //  GPIO4, ADC2_CH0, TOUCH0, RTC_GPIO10, HSPIHD, HS2_DATA1,SD_DATA1, EMAC_TX_ER

static const uint8_t TX = 1;
static const uint8_t RX = 3;

/* LionBit pin setup */
static const uint8_t D0 = 3;  //Rx GPIO3, U0RXD, CLK_OUT2
static const uint8_t D1 = 1;  //TX GPIO1, U0TXD, CLK_OUT3, EMAC_RXD2
//-------------------------------------------------------------------

//Please do not use while using QIO SPI mode ; Use only DIO flash mode
static const uint8_t D2 = 9;   //I/O   U1RX  GPIO9,  SD_DATA2, SPIHD, HS1_DATA2, U1RXD
static const uint8_t D3 = 10;  //I/O  U1TX  GPIO10, SD_DATA3, SPIWP, HS1_DATA3, U1TXD
//-------------------------------------------------------------------
static const uint8_t U1RX = 9;   //I/O   U1RX
static const uint8_t U1TX = 10;  //I/O  U1TX

//Second Segment - Sector -01 (Voltage (*5v or 3.3V) can be selected by using D4-7 Jumper
static const uint8_t D4 = 16;  //I/O   U2RX  GPIO16, HS1_DATA4, U2RXD, EMAC_CLK_OUT
static const uint8_t D5 = 17;  //I/O   U2TX  GPIO17, HS1_DATA5, U2TXD, EMAC_CLK_OUT_180
static const uint8_t D6 = 21;  //I/O   SDA GPIO21, VSPIHD, EMAC_TX_EN
static const uint8_t D7 = 22;  //I/O   SCl GPIO22, VSPIWP, U0RTS, EMAC_TXD1

//Second Segment - Sector -02 (Voltage (*5v or 3.3V) can be selected by using D8-11 Jumper
static const uint8_t D8 = 5;    //I/O  GPIO5, VSPICS0, HS1_DATA6, EMAC_RX_CLK
static const uint8_t D9 = 23;   //I/O GPIO23, VSPID, HS1_STROBE  **********************************************Don not use when display "ON or USE"*************************
static const uint8_t D10 = 19;  //I/O GPIO19, VSPIQ, U0CTS, EMAC_TXD0
static const uint8_t D11 = 18;  //I/O GPIO18, VSPICLK, HS1_DATA7 **********************************************Don not use when display "ON or USE"*************************


// Analog to Digital Converter (Support 5V) ADC2 pins not recommended while using Wifi
static const uint8_t A0 /*ADC2_CH3 */ = 12;  //MAX 5V,I/O GPIO12, ADC2_CH5, TOUCH5, RTC_GPIO15, MTDI, HSPIQ, HS2_DATA2,SD_DATA2, EMAC_TXD3
static const uint8_t A1 /*ADC1_CH0 */ = 14;  //MAX 5V,I/O GPIO14, ADC2_CH6, TOUCH6, RTC_GPIO16, MTMS, HSPICLK, HS2_CLK,SD_CLK, EMAC_TXD2
static const uint8_t A2 /*ADC2_CH6 */ = 34;  //MAX 5V,GPIO34, ADC1_CH6, RTC_GPIO4 ***********************/////////////////////Connected LDR/////////////////////////////
static const uint8_t A3 /*ADC1_CH7 */ = 35;  //MAX 5V,GPIO35, ADC1_CH7, RTC_GPIO5
static const uint8_t A4 /*ADC2_CH5 */ = 15;  //MAX 5V,GPIO15, ADC2_CH3, TOUCH3, MTDO, HSPICS0, RTC_GPIO13, HS2_CMD,SD_CMD, EMAC_RXD3
static const uint8_t A5 /*ADC2_CH4 */ = 13;  //MAX 5V,GPIO13, ADC2_CH4, TOUCH4, RTC_GPIO14, MTCK, HSPID, HS2_DATA3,SD_DATA3, EMAC_RX_ER
                                             //-------------------------------------------------------------------

//------------------Touch Sensors-------------------------------------------------
static const uint8_t VP = 36;  //  GPIO36, ADC1_CH0, RTC_GPIO0
static const uint8_t VN = 39;  //  GPIO39, ADC1_CH3, RTC_GPIO3

static const uint8_t T0 = 36;
static const uint8_t T1 = 39;

static const uint8_t DAC1 = 25;  // I/O GPIO25, DAC_1, ADC2_CH8, RTC_GPIO6, EMAC_RXD0
static const uint8_t DAC2 = 26;  // I/O GPIO26, DAC_2, ADC2_CH9, RTC_GPIO7, EMAC_RXD1

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

/* Hardware HSPI */
static const uint8_t MOSI = 13;  // 13;
static const uint8_t MISO = 12;  // 12;
static const uint8_t SCK = 14;   // 14;
static const uint8_t SS = 15;    // 15;

/* Software VSPI [Note : D9 and D11 Do not use when display "ON or USE"]*/
static const uint8_t VMOSI = 23;  //23 /*Do not use when display "ON or USE"*/
static const uint8_t VMISO = 19;  // 19
static const uint8_t VSCK = 18;   // 18 /*Do not use when display "ON or USE"*/
static const uint8_t VSS = 5;     // 5

// Inbuilt Display Unit 128*128 ST7735 Driver
static const uint8_t RST = 33;      // - RESET GPIO33, XTAL_32K_N (32.768 kHz crystal oscillator output),ADC1_CH5, TOUCH8, RTC_GPIO8
static const uint8_t CLK = 18;      // - (18) CLK  (D11) and  D9 pin will engaged when display "ON or USE"
static const uint8_t CS = 27;       // - CS    GPIO27, ADC2_CH7, TOUCH7, RTC_GPIO17, EMAC_RX_DV
static const uint8_t DC = 32;       //- DC/A0 GPIO32, XTAL_32K_P (32.768 kHz crystal oscillator input), ADC1_CH4,TOUCH9, RTC_GPIO9
static const uint8_t ST_MOSI = 23;  // - MOSI (D9) This D9 pin will engaged when display "ON or USE"

static const uint8_t MTDO = 15;  // A4 JTAG SIGNAL -> TDO
static const uint8_t MTDI = 12;  // A0 JTAG SIGNAL -> TDI
static const uint8_t MTCK = 13;  // A5 JTAG SIGNAL -> TCK
static const uint8_t MTMS = 14;  // A1 JTAG SIGNAL -> TMS

#endif /* Pins_Arduino_h */
