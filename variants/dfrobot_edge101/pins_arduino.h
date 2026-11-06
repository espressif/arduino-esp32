#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x1A86
#define USB_PID 0x55D4

static const uint8_t LED_BUILTIN = 15;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

// UART0 (USB-serial via CH9102F)
static const uint8_t TX = 1;
static const uint8_t RX = 3;

// UART1 (optional 4G modem mini-PCIe slot)
static const uint8_t TX1 = 33;
static const uint8_t RX1 = 34;

// I2C (onboard PCF8563 RTC + Gravity connectors)
static const uint8_t SDA = 18;
static const uint8_t SCL = 23;

// SPI (onboard microSD + Gravity SPI header)
static const uint8_t SS = 5;
static const uint8_t MOSI = 12;
static const uint8_t MISO = 39;
static const uint8_t SCK = 14;

// Ethernet (IP101GRI PHY, RMII, 50 MHz REF_CLK input on GPIO0)
#define ETH_PHY_TYPE  ETH_PHY_IP101
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   4
#define ETH_PHY_MDIO  13
#define ETH_PHY_POWER 2
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN

// CAN (TJA1050, galvanically isolated)
#define CAN_TX 32
#define CAN_RX 35

// RS485 (TPT75176H, galvanically isolated)
#define RS485_TX 17
#define RS485_RX 36
#define RS485_DE 16

#endif /* Pins_Arduino_h */
