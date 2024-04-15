#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Renaming few signals
#define SPI_CLK SCK                 // IO14
#define SPI_MISO MISO               // IO12
#define SPI_MOSI MOSI               // IO13
#define SPI_CS0 SS                  // IO15, Default SPI CS: Extension Header, Pin_3
#define SD_SPI_CS1 SPI_CS1          // SPI Chip Select: MicroSD Card
#define LED_WIFI_LINK LED1_BUILDIN  // LED6 on the LogSens V1.1 Board
#define LED_WIFI_ACT LED2_BUILDIN   // LED7 on the LogSens V1.1 Board\

/* LED_BUILTIN is kept for compatibility reason; mapped to LED2 on the LogSens V1.1 Board */
static const uint8_t LED_BUILTIN = 33;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

/* UART0: Serial Port for Programming and Debugging on the LogSens V1.1 Board */
static const uint8_t TX = 1;
static const uint8_t RX = 3;

#ifdef BOARD_VARIANT_RS485
/* UART2: Serial Port connected to RS485 transceiver on the LogSens V1.1 Board */
static const uint8_t UART2_TX = 17;
static const uint8_t UART2_RX = 16;
static const uint8_t UART2_RTS = 4;
#endif /* BOARD_VARIANT_RS485 */

#ifdef BOARD_VARIANT_CAN
/* CAN Bus connected to CAN transceiver on the LogSens V1.1 Board */
static const uint8_t CAN_TX = 17;
static const uint8_t CAN_RX = 16;
static const uint8_t CAN_TXDE = 4;
#endif /* BOARD_VARIANT_CAN */

/* I2C Bus: Shared between RTC chip, Expansion Header (X3), Sensor Header (X7) on the LogSens V1.1 Board */
static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

/* SPI Bus: Shared between MicroSD Card (X6) and Expansion Header (X3) */
static const uint8_t SS = 15;  // SPI Chip Select - 0; Connected to Extension Header, Pin_3  on the LogSens V1.1 Board
static const uint8_t MOSI = 13;
static const uint8_t MISO = 12;
static const uint8_t SCK = 14;

static const uint8_t SS1 = 23;  // SPI Chip Select - 1; connected to MicroSD Card on the LogSens V1.1 Board

/* Software Controlled: IO, LEDs and Switches */
static const uint8_t BUZZER_CTRL = 19;     // Signal connected to MOSFET gate pin to control connector (X8)
static const uint8_t SD_CARD_DETECT = 35;  // MicroSD Card (X6): Card Detect Signal

static const uint8_t SW2_BUILDIN = 0;   // Tactile Switch-2 (SW2); ESP32 BOOT0 pin, Use it with care !!
static const uint8_t SW3_BUILDIN = 36;  // Tactile Switch-3 (SW3)
static const uint8_t SW4_BUILDIN = 34;  // Tactile Switch-4 (SW4)

static const uint8_t LED1_BUILDIN = 32;  // Connected to LogSens V1.1: LED6
static const uint8_t LED2_BUILDIN = 33;  // Connected to LogSens V1.1: LED7


/* Analog Input Channels accessible on the LogSens V1.1 Board */
//static const uint8_t A0 = 36;
//static const uint8_t A3 = 39;
static const uint8_t A4 = 32;
static const uint8_t A5 = 33;
//static const uint8_t A6 = 34;
//static const uint8_t A7 = 35;
//static const uint8_t A10 = 4;
//static const uint8_t A11 = 0;
static const uint8_t A12 = 2;
static const uint8_t A13 = 15;
//static const uint8_t A14 = 13;
//static const uint8_t A15 = 12;
//static const uint8_t A16 = 14;
static const uint8_t A17 = 27;
static const uint8_t A18 = 25;
static const uint8_t A19 = 26;

//static const uint8_t T0 = 4;
//static const uint8_t T1 = 0;
static const uint8_t T2 = 2;
static const uint8_t T3 = 15;
//static const uint8_t T4 = 13;
//static const uint8_t T5 = 12;
//static const uint8_t T6 = 14;
static const uint8_t T7 = 27;
static const uint8_t T8 = 33;
static const uint8_t T9 = 32;

/* DAC Channels accessible on the LogSens V1.1 Board */
static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
