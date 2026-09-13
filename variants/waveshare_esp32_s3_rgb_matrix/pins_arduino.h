#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303A
#define USB_PID          0x1001
#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-RGB-Matrix"
#define USB_SERIAL       ""

// The board has no dedicated UART or general-purpose SPI connector.
// UART0 pins are shared with I2S and SD_MMC; SPI pins must be selected explicitly.
static const uint8_t TX = -1;
static const uint8_t RX = -1;

#define BOARD_I2C_SDA 47
#define BOARD_I2C_SCL 48
#define I2C_SDA_PIN   BOARD_I2C_SDA
#define I2C_SCL_PIN   BOARD_I2C_SCL

static const uint8_t SDA = BOARD_I2C_SDA;
static const uint8_t SCL = BOARD_I2C_SCL;

static const uint8_t SS = -1;
static const uint8_t MOSI = -1;
static const uint8_t MISO = -1;
static const uint8_t SCK = -1;

// HUB75 interface
#define WS_HUB75_R1_PIN  4
#define WS_HUB75_G1_PIN  5
#define WS_HUB75_B1_PIN  6
#define WS_HUB75_R2_PIN  7
#define WS_HUB75_G2_PIN  15
#define WS_HUB75_B2_PIN  16
#define WS_HUB75_A_PIN   18
#define WS_HUB75_B_PIN   8
#define WS_HUB75_C_PIN   3
#define WS_HUB75_D_PIN   42
#define WS_HUB75_E_PIN   9
#define WS_HUB75_LAT_PIN 40
#define WS_HUB75_OE_PIN  2
#define WS_HUB75_CLK_PIN 41

// SD_MMC (1-bit). GPIO14 is the card's DAT3/CS signal, not a card-detect input.
#define BOARD_HAS_SDMMC
#define BOARD_HAS_1BIT_SDMMC
#define BOARD_SD_CLK 1
#define BOARD_SD_CMD 44
#define BOARD_SD_D0  17
#define SDMMC_CLK    BOARD_SD_CLK
#define SDMMC_CMD    BOARD_SD_CMD
#define SDMMC_D0     BOARD_SD_D0
#define SDMMC_CS     14
#define BSP_SD_CLK   BOARD_SD_CLK
#define BSP_SD_CMD   BOARD_SD_CMD
#define BSP_SD_D0    BOARD_SD_D0

// I2S audio shared by the ES8311 codec and ES7210 ADC
#define BOARD_HAS_ES8311
#define BOARD_HAS_ES7210
#define BOARD_I2S_MCLK  12
#define BOARD_I2S_BCLK  43
#define BOARD_I2S_LRC   38
#define BOARD_I2S_DOUT  21
#define BOARD_I2S_DIN   39
#define I2S_MCLK        BOARD_I2S_MCLK
#define I2S_SCLK        BOARD_I2S_BCLK
#define I2S_BCLK        BOARD_I2S_BCLK
#define I2S_LRCK        BOARD_I2S_LRC
#define I2S_DOUT        BOARD_I2S_DOUT
#define I2S_DIN         BOARD_I2S_DIN
#define BOARD_PA_ENABLE 11
#define PA_POWER        BOARD_PA_ENABLE

// PCF85063 RTC, QMI8658 IMU, and SHTC3 share the default I2C bus.
#define RTC_SDA     47
#define RTC_SCL     48
#define RTC_ADDRESS 0x51
#define RTC_INT     10

#define QMI8658_SDA     47
#define QMI8658_SCL     48
#define QMI8658_ADDRESS 0x6B
#define QMI8658_INT1    13

#define SHTC3_SDA 47
#define SHTC3_SCL 48

// BOOT button, active low
#define BOARD_BUTTON_PIN 0
static const uint8_t BUTTON_BUILTIN = BOARD_BUTTON_PIN;

#define USB_DN 19
#define USB_DP 20

// GPIO45 and GPIO46 are strapping pins routed to the expansion header.
#define EXPANSION_GPIO_1 45
#define EXPANSION_GPIO_2 46

#endif /* Pins_Arduino_h */
