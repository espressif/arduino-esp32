#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// UART0 is connected to the board's serial interface.
static const uint8_t TX = 37;
static const uint8_t RX = 38;

// The board I2C bus is shared by ES8311, GT911, LCD backlight, and camera SCCB.
static const uint8_t SDA = 7;
static const uint8_t SCL = 8;
#define WIRE1_PIN_DEFINED 1
static const uint8_t SDA1 = SDA;
static const uint8_t SCL1 = SCL;
#define BOARD_I2C_PORT         1
#define BOARD_I2C_FREQUENCY_HZ 400000

// Default Arduino SPI pins are available on the expansion header.
static const uint8_t SS = 26;
static const uint8_t MOSI = 32;
static const uint8_t MISO = 33;
static const uint8_t SCK = 36;

// Keep the standard ESP32-P4 analog and touch aliases available.
static const uint8_t A0 = 16;
static const uint8_t A1 = 17;
static const uint8_t A2 = 18;
static const uint8_t A3 = 19;
static const uint8_t A4 = 20;
static const uint8_t A5 = 21;
static const uint8_t A6 = 22;
static const uint8_t A7 = 23;
static const uint8_t A8 = 49;
static const uint8_t A9 = 50;
static const uint8_t A10 = 51;
static const uint8_t A11 = 52;
static const uint8_t A12 = 53;
static const uint8_t A13 = 54;

static const uint8_t T0 = 2;
static const uint8_t T1 = 3;
static const uint8_t T2 = 4;
static const uint8_t T3 = 5;
static const uint8_t T4 = 6;
static const uint8_t T5 = 7;
static const uint8_t T6 = 8;
static const uint8_t T7 = 9;
static const uint8_t T8 = 10;
static const uint8_t T9 = 11;
static const uint8_t T10 = 12;
static const uint8_t T11 = 13;
static const uint8_t T12 = 14;
static const uint8_t T13 = 15;

// The LCD has no reset GPIO and its backlight is controlled by I2C.
#define LCD_INTERFACE_MIPI
#define LCD_RST                    (-1)
#define LCD_RST_IO                 LCD_RST
#define LCD_RST_ACTIVE_HIGH        0
#define LCD_BL                     (-1)
#define LCD_BL_IO                  LCD_BL
#define LCD_BL_ON_LEVEL            1
#define LCD_BL_OFF_LEVEL           0
#define LCD_BACKLIGHT_I2C_ADDRESS  0x45
#define LCD_BACKLIGHT_I2C_REGISTER 0x96

// GT911 reset and interrupt signals are not connected on this board.
#define TOUCH_RST (-1)
#define TOUCH_INT (-1)
#define CTP_RST   TOUCH_RST
#define CTP_INT   TOUCH_INT
#define I2C_SDA   SDA
#define I2C_SCL   SCL

// ES8311 audio codec.
#define I2S_DOUT  9
#define I2S_LRCLK 10
#define I2S_DIN   11
#define I2S_BCLK  12
#define I2S_MCLK  13
#define PA_POWER  53

// The microSD card uses SDMMC slot 0 in 4-bit mode.
#define BOARD_HAS_SDMMC
#define BOARD_SDMMC_SLOT           0
#define BOARD_SDMMC_CLK            43
#define BOARD_SDMMC_CMD            44
#define BOARD_SDMMC_D0             39
#define BOARD_SDMMC_D1             40
#define BOARD_SDMMC_D2             41
#define BOARD_SDMMC_D3             42
#define BOARD_SDMMC_POWER_CHANNEL  4
#define BOARD_SDMMC_POWER_PIN      45
#define BOARD_SDMMC_POWER_ON_LEVEL LOW

// ESP32-C5 wireless co-processor connected through ESP-Hosted SDIO slot 1.
#define BOARD_HAS_SDIO_ESP_HOSTED
#define BOARD_SDIO_ESP_HOSTED_CLK   18
#define BOARD_SDIO_ESP_HOSTED_CMD   19
#define BOARD_SDIO_ESP_HOSTED_D0    14
#define BOARD_SDIO_ESP_HOSTED_D1    15
#define BOARD_SDIO_ESP_HOSTED_D2    16
#define BOARD_SDIO_ESP_HOSTED_D3    17
#define BOARD_SDIO_ESP_HOSTED_RESET 54
#define BOARD_SDIO_ESP_HOSTED_WAKE  6

// IP101 RMII PHY with an external 50 MHz reference clock.
#define BOARD_HAS_ETHERNET
#define BOARD_ETH_PHY_ADDR    1
#define BOARD_ETH_PHY_RST     51
#define BOARD_ETH_MDC         31
#define BOARD_ETH_MDIO        52
#define BOARD_ETH_RMII_CLK    50
#define BOARD_ETH_RMII_TX_EN  49
#define BOARD_ETH_RMII_TXD0   34
#define BOARD_ETH_RMII_TXD1   35
#define BOARD_ETH_RMII_CRS_DV 28
#define BOARD_ETH_RMII_RXD0   29
#define BOARD_ETH_RMII_RXD1   30

// Defaults used by Arduino-ESP32's ETH.begin().
#ifndef ETH_PHY_TYPE
#define ETH_PHY_TYPE ETH_PHY_IP101
#endif
#ifndef ETH_PHY_ADDR
#define ETH_PHY_ADDR BOARD_ETH_PHY_ADDR
#endif
#ifndef ETH_PHY_MDC
#define ETH_PHY_MDC BOARD_ETH_MDC
#endif
#ifndef ETH_PHY_MDIO
#define ETH_PHY_MDIO BOARD_ETH_MDIO
#endif
#ifndef ETH_PHY_POWER
#define ETH_PHY_POWER BOARD_ETH_PHY_RST
#endif
#ifndef ETH_CLK_MODE
#define ETH_CLK_MODE EMAC_CLK_EXT_IN
#endif

// On-chip GP LDO: periman/hosted enable VO4 for GPIO 39-48 (VDD_IO_5), e.g. hosted D2/D3 = 48/47.
#define BOARD_PERIMAN_IO_LDO_AUTO        1
#define BOARD_PERIMAN_IO_LDO0_CHANNEL    4
#define BOARD_PERIMAN_IO_LDO0_GPIO_MIN   39
#define BOARD_PERIMAN_IO_LDO0_GPIO_MAX   48
#define BOARD_PERIMAN_IO_LDO0_VOLTAGE_MV 3300

#endif /* Pins_Arduino_h */
