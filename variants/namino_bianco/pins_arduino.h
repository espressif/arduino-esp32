//
// Copyright (c) 2023 Namino Team, bianco version: 1.0.1 @ 2023-10-06
//
//
#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

#define NAMINO_BIANCO_BOARD

/* Begin Pins on ESP32-S3-WROOM-1U-N4R8 */
static const uint8_t GPIO4 = 4;
static const uint8_t GPIO5 = 5;
static const uint8_t GPIO6 = 6;
static const uint8_t GPIO7 = 7;
static const uint8_t GPIO15 = 15;
static const uint8_t GPIO16 = 16;
static const uint8_t GPIO17 = 17;
static const uint8_t GPIO18 = 18;
static const uint8_t GPIO8 = 8;
static const uint8_t GPIO19 = 19;
static const uint8_t GPIO20 = 20;
static const uint8_t GPIO3 = 3;
static const uint8_t GPIO46 = 46;
static const uint8_t GPIO9 = 9;
static const uint8_t GPIO10 = 10;
static const uint8_t GPIO11 = 11;
static const uint8_t GPIO12 = 12;
static const uint8_t GPIO13 = 13;
static const uint8_t GPIO14 = 14;
static const uint8_t GPIO21 = 21;
static const uint8_t GPIO47 = 47;
static const uint8_t GPIO48 = 48;
static const uint8_t GPIO45 = 45;
static const uint8_t GPIO0 = 0;
static const uint8_t GPIO35 = 35;
static const uint8_t GPIO36 = 36;
static const uint8_t GPIO37 = 37;
static const uint8_t GPIO38 = 38;
static const uint8_t GPIO39 = 39;
static const uint8_t GPIO40 = 40;
static const uint8_t GPIO41 = 41;
static const uint8_t GPIO42 = 42;
static const uint8_t GPIO44 = 44;
static const uint8_t GPIO43 = 43;
static const uint8_t GPIO2 = 2;
static const uint8_t GPIO1 = 1;

static const uint8_t SS = GPIO48;
static const uint8_t MOSI = GPIO11;
static const uint8_t MISO = GPIO13;
static const uint8_t SCK = GPIO12;

/* End Pins on ESP32-S3-WROOM-1U-N4R8 */

/* Begin Analog Pins on ESP32-S3-WROOM-1U-N4R8 */
static const uint8_t ADC1_CH3 = GPIO4;
static const uint8_t ADC1_CH4 = GPIO5;
static const uint8_t ADC1_CH5 = GPIO6;
static const uint8_t ADC1_CH6 = GPIO7;
static const uint8_t ADC2_CH4 = GPIO15;
static const uint8_t ADC2_CH5 = GPIO16;
static const uint8_t ADC2_CH6 = GPIO17;
static const uint8_t ADC2_CH7 = GPIO18;
static const uint8_t ADC1_CH7 = GPIO8;
static const uint8_t ADC2_CH8 = GPIO19;
static const uint8_t ADC2_CH9 = GPIO20;
static const uint8_t ADC1_CH2 = GPIO3;
static const uint8_t ADC1_CH8 = GPIO9;
static const uint8_t ADC1_CH9 = GPIO10;
static const uint8_t ADC2_CH0 = GPIO11;
static const uint8_t ADC2_CH1 = GPIO12;
static const uint8_t ADC2_CH2 = GPIO13;
static const uint8_t ADC2_CH3 = GPIO14;
static const uint8_t ADC1_CH1 = GPIO2;
static const uint8_t ADC1_CH0 = GPIO1;
/* End Analog Pins on ESP32-S3-WROOM-1U-N4R8 */

/* Begin Touch Pins on ESP32-S3-WROOM-1U-N4R8 */
static const uint8_t TOUCH4 = GPIO4;
static const uint8_t TOUCH5 = GPIO5;
static const uint8_t TOUCH6 = GPIO6;
static const uint8_t TOUCH7 = GPIO7;
static const uint8_t TOUCH8 = GPIO8;
static const uint8_t TOUCH3 = GPIO3;
static const uint8_t TOUCH9 = GPIO9;
static const uint8_t TOUCH10 = GPIO10;
static const uint8_t TOUCH11 = GPIO11;
static const uint8_t TOUCH12 = GPIO12;
static const uint8_t TOUCH13 = GPIO13;
static const uint8_t TOUCH14 = GPIO14;
static const uint8_t TOUCH2 = GPIO2;
static const uint8_t TOUCH1 = GPIO1;
/* End Touch Pins on ESP32-S3-WROOM-1U-N4R8 */

static const uint8_t TX = GPIO17;
static const uint8_t RX = GPIO18;

static const uint8_t SDA = GPIO1;
static const uint8_t SCL = GPIO2;
static const uint8_t NM_I2C_SDA = SDA;
static const uint8_t NM_I2C_SCL = SCL;

static const uint8_t A0 = ADC1_CH2;
static const uint8_t A1 = ADC1_CH3;
static const uint8_t A2 = ADC1_CH4;
static const uint8_t A3 = ADC1_CH5;
static const uint8_t A4 = ADC1_CH6;
static const uint8_t A5 = ADC2_CH4;
static const uint8_t A6 = 0;
static const uint8_t A7 = 0;
static const uint8_t A8 = 0;
static const uint8_t A9 = 0;
static const uint8_t A10 = 0;
static const uint8_t A11 = 0;
static const uint8_t A12 = 0;
static const uint8_t A13 = 0;
static const uint8_t A14 = 0;
static const uint8_t A15 = 0;

static const uint8_t DAC1 = 0;
static const uint8_t DAC2 = 0;

/* Begin Arduino naming */
static const uint8_t RESET_ARDUINO = 0;
static const uint8_t PC0 = GPIO3;
static const uint8_t PC1 = GPIO4;
static const uint8_t PC2 = GPIO5;
static const uint8_t PC3 = GPIO6;
static const uint8_t PC4 = GPIO7;
static const uint8_t PC5 = GPIO15;
static const uint8_t PB5 = GPIO35;
static const uint8_t PB4 = GPIO36;
static const uint8_t PB3 = GPIO37;
static const uint8_t PB2 = GPIO38;
static const uint8_t PB1 = GPIO39;
static const uint8_t PB0 = GPIO40;
static const uint8_t PD7 = GPIO41;
static const uint8_t PD6 = GPIO42;
static const uint8_t PD5 = GPIO21;
static const uint8_t PD4 = GPIO16;
static const uint8_t PD3 = GPIO14;
static const uint8_t PD2 = GPIO47;
static const uint8_t PD1 = GPIO17;
static const uint8_t PD0 = GPIO18;
/* End Arduino naming */

/* Begin alternate naming */
static const uint8_t PB_SCL = SCL;
static const uint8_t PB_SDA = SDA;
static const uint8_t PB_35 = PB5;
static const uint8_t PB_36 = PB4;
static const uint8_t PB_37 = PB3;
static const uint8_t PB_38 = PB2;
static const uint8_t PB_39 = PB1;
static const uint8_t PB_40 = PB0;

static const uint8_t PD_io41 = PD7;
static const uint8_t PD_42 = PD6;
static const uint8_t PD_21 = PD5;
static const uint8_t PD_16 = PD4;
static const uint8_t PD_14 = PD3;
static const uint8_t PD_47 = PD2;
static const uint8_t PD_17 = TX;
static const uint8_t PD_18 = RX;

static const uint8_t PC_io3 = PC0;
static const uint8_t PC_4 = PC1;
static const uint8_t PC_5 = PC2;
static const uint8_t PC_6 = PC3;
static const uint8_t PC_7 = PC4;
static const uint8_t PC_15 = PC5;

static const uint8_t M1_3_AN = GPIO3;
static const uint8_t M1_RST = 0;
static const uint8_t M1_48_CS = SS;
static const uint8_t M1_12_SCK = SCK;
static const uint8_t M1_13_MISO = MISO;
static const uint8_t M1_11_MOSI = MOSI;
static const uint8_t M2_47_PWM = GPIO47;
static const uint8_t M2_14_INT = GPIO14;
static const uint8_t M2_18_RX = GPIO18;
static const uint8_t M2_17_TX = GPIO17;
static const uint8_t M2_2_SCL = SCL;
static const uint8_t M2_1_SDA = SDA;

static const uint8_t J3_SCL = SCL;
static const uint8_t J3_SDA = SDA;

/* End alternate naming */

#endif /* Pins_Arduino_h */
