#ifndef Pins_Arduino_h
#define Pins_Arduino_h

/* variant: Espoir
 * vendor: Connaxio
 * url: https://www.connaxio.com/electronics/espoir/
 */

#include <stdint.h>

/* USB UART */
static const uint8_t TX = 1;
static const uint8_t RX = 3;

/* mikroBUS UART */
static const uint8_t TX1 = 10;
static const uint8_t RX1 = 9;

/* mikroBUS I2C */
static const uint8_t SDA = 23;
static const uint8_t SCL = 18;

/* mikroBUS SPI */
static const uint8_t SS = 15;
static const uint8_t MOSI = 13;
static const uint8_t MISO = 12;
static const uint8_t SCK = 14;

/* Default analog pins */
static const uint8_t A0 = 36;
static const uint8_t A1 = 37;
static const uint8_t A2 = 38;
static const uint8_t A3 = 39;
static const uint8_t A6 = 34;

/* Alternative analog pins */
static const uint8_t A10 = 4;
static const uint8_t A12 = 2;
static const uint8_t A13 = 15;
static const uint8_t A14 = 13;
static const uint8_t A15 = 12;
static const uint8_t A16 = 14;

/* Touch pins */
static const uint8_t T0 = 4;
static const uint8_t T2 = 2;
static const uint8_t T3 = 15;
static const uint8_t T4 = 13;
static const uint8_t T5 = 12;
static const uint8_t T6 = 14;

/* Other pin names */
static const uint8_t AN = 36;
static const uint8_t RST = 5;
static const uint8_t PWM = 2;
static const uint8_t INT = 4;
static const uint8_t CS = 15;
static const uint8_t SDO = 13;
static const uint8_t SDI = 12;

/* Ethernet interface */
static const uint8_t ETH_INT = 35;
#define ETH_PHY_ADDR 0
#define ETH_PHY_POWER -1
#define ETH_PHY_MDC 32
#define ETH_PHY_MDIO 33
#define ETH_PHY_TYPE ETH_PHY_KSZ8081
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN

/* USB interface */
#define USB_VID 0x10C4  // Silabs's VID
#define USB_PID 0x8D9A  // Espoir's PID, requires Silab USB PHY
#define USB_MANUFACTURER "Connaxio"
#define USB_PRODUCT "Espoir"
#define USB_SERIAL ""

#endif /* Pins_Arduino_h */
