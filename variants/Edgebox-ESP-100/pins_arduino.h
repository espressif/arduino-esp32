#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

//Programming and Debugging Port
static const uint8_t TXD = 43;
static const uint8_t RXD = 44;
static const uint8_t RST = 0;

//I2C
static const uint8_t SDA = 20;
static const uint8_t SCL = 19;

//I2C INT fro RTC PCF8563
static const uint8_t I2C_INT = 9;

//SPI BUS for W5500 Ethernet Port Driver
static const uint8_t SS = 10;
static const uint8_t MOSI = 12;
static const uint8_t MISO = 11;
static const uint8_t SCK = 13;
static const uint8_t ETH_INT = 14;
static const uint8_t ETH_RST = 15;

//A7670G
static const uint8_t LTE_PWR_EN = 16;
static const uint8_t LTE_PWR_KEY = 21;
static const uint8_t LTE_TXD = 48;
static const uint8_t LTE_RXD = 47;

//RS485
static const uint8_t RS485_TXD = 17;
static const uint8_t RS485_RXD = 18;
static const uint8_t RS485_RTS = 8;

//CAN BUS
static const uint8_t CAN_TXD = 1;
static const uint8_t CAN_RXD = 2;

//BUZZER
static const uint8_t BUZZER = 45;

static const uint8_t DO0 = 40;
static const uint8_t DO1 = 39;
static const uint8_t DO2 = 38;
static const uint8_t DO3 = 37;
static const uint8_t DO4 = 36;
static const uint8_t DO5 = 35;

static const uint8_t DI0 = 4;
static const uint8_t DI1 = 5;
static const uint8_t DI2 = 6;
static const uint8_t DI3 = 7;

static const uint8_t AO0 = 42;
static const uint8_t AO1 = 41;

#endif /* Pins_Arduino_h */
