#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Serial is the USB-Serial/JTAG. UART0 (Serial0) is on the expansion header, J3 pins 3/4:
// SOC_TX0/SOC_RX0 are what the core reads for it (the P4 defaults, GPIO37/38, are the codec's I2S pins here).
#define SOC_TX0 (gpio_num_t)8
#define SOC_RX0 (gpio_num_t)9
// UART1 has no default pins: GPIO10 is the ESP32-C5's download strap on J3. Pass pins to Serial1.begin().
#define TX1 (gpio_num_t)(-1)
#define RX1 (gpio_num_t)(-1)
static const uint8_t TX = 8;
static const uint8_t RX = 9;

// Internal I2C bus, shared by every on-board device (DVT board)
static const uint8_t SDA = 4;
static const uint8_t SCL = 5;
#define KODE_I2C_IMU    0x6A  // QMI8658A accelerometer + gyroscope
#define KODE_I2C_MAG    0x2C  // QMC5883P magnetometer, its own chip on this bus
#define KODE_I2C_PMIC   0x6B  // BQ25896
#define KODE_I2C_GAUGE  0x64  // CW2217
#define KODE_I2C_LED    0x30  // KTD2026 RGB LED (no LED on a GPIO, so no LED_BUILTIN)
#define KODE_I2C_IOEXP  0x20  // TCA6408A: buttons P0-P4, IMU INT1 P5, touch INT P6, display power P7
#define KODE_I2C_CODEC  0x18  // ES8311
#define KODE_I2C_HAPTIC 0x5A  // AW86233 (antenna PCB), address fixed in silicon
#define KODE_I2C_TOUCH  0x15  // CST820
#define KODE_IOEXP_INT  6     // TCA6408A INT, active low

// SPI: NFC reader (ST25R3916B, antenna PCB), mode 1
static const uint8_t SS = 28;
static const uint8_t MOSI = 31;
static const uint8_t MISO = 32;
static const uint8_t SCK = 30;
#define NFC_IRQ 29

// Buttons: only the top one is a GPIO, the other five are on the expander
#define BUTTON_TOP 35

// ADC1 pins on the expansion header
static const uint8_t A0 = 16;
static const uint8_t A1 = 17;
static const uint8_t A2 = 18;
static const uint8_t A3 = 19;
static const uint8_t A4 = 20;
static const uint8_t A5 = 21;

// Expansion header J3 (2x10), in connector order. Pins 1, 2, 19, 20 are power; 10 and 11 are GND
#define EXP1  8
#define EXP2  9
#define EXP3  10
#define EXP4  11
#define EXP5  12
#define EXP6  13
#define EXP7  14
#define EXP8  21
#define EXP9  19
#define EXP10 20
#define EXP11 17
#define EXP12 18
#define EXP13 15
#define EXP14 16
// The same pins by J3 pin number
#define PIN3  8
#define PIN4  9
#define PIN5  10
#define PIN6  11
#define PIN7  12
#define PIN8  13
#define PIN9  14
#define PIN12 21
#define PIN13 19
#define PIN14 20
#define PIN15 17
#define PIN16 18
#define PIN17 15
#define PIN18 16

// SDMMC slot 0, 4-bit, on its IOMUX pins. The card's supply is GPIO22 through Q1, active low,
// and SD_MMC.begin() switches it on; the IO domain is the P4's internal LDO channel 4.
// Card-detect is on the ESP32-C5 (its IO6), out of a sketch's reach.
#define BOARD_HAS_SDMMC
#define BOARD_SDMMC_SLOT           0
#define BOARD_SDMMC_POWER_CHANNEL  4
#define BOARD_SDMMC_POWER_PIN      22
#define BOARD_SDMMC_POWER_ON_LEVEL LOW
#define SDMMC_CLK                  43
#define SDMMC_CMD                  44
#define SDMMC_D0                   39
#define SDMMC_D1                   40
#define SDMMC_D2                   41
#define SDMMC_D3                   42

// GPIO39-48 (SDMMC, RFID, the ESP-Hosted D1) are VDD_IO_5, fed by the P4's LDO VO4, which boots at about
// 1.2 V: the core raises it to 3.3 V while one of them is in use, and hands it to SD_MMC while a card is up.
#define BOARD_PERIMAN_IO_LDO_AUTO        1
#define BOARD_PERIMAN_IO_LDO0_CHANNEL    4
#define BOARD_PERIMAN_IO_LDO0_GPIO_MIN   39
#define BOARD_PERIMAN_IO_LDO0_GPIO_MAX   48
#define BOARD_PERIMAN_IO_LDO0_VOLTAGE_MV 3300

// IR: both lines are the ESP32-C5's (its IO24 TX, IO23 RX), so the P4 has no IR pins

// 125 kHz RFID front end (discrete, antenna PCB). RFID_PULL idles high through R11, and high
// shunts the envelope detector: hold it LOW to read. RF_IN is on ADC2 because GPIO23 is C5_EN.
#define RFID_PULL    45
#define RFID_CARRIER 46  // the coil's zero-crossing sense, input only
#define RFID_OUT     47  // carrier drive
#define RFID_RF_IN   54  // demodulated envelope, ADC2_CH5

// Display (CO5300, MIPI-DSI) and touch share the reset line; panel power is expander P7
#define DISPLAY_RST 7

// ESP32-C5 radio: EN high = running, SDIO on GPIO48-53
#define C5_EN 23
// The same link for the core's esp-hosted. Without these, WiFi.begin() drives the P4 dev board's
// SDIO pins (GPIO14-19, on J3) and its reset (GPIO54, RFID_RF_IN here). The prebuilt libraries
// still expect an ESP32-C6 slave, so WiFi/BLE are not expected to work.
#define BOARD_HAS_SDIO_ESP_HOSTED
#define BOARD_SDIO_ESP_HOSTED_CLK   50
#define BOARD_SDIO_ESP_HOSTED_CMD   51
#define BOARD_SDIO_ESP_HOSTED_D0    49
#define BOARD_SDIO_ESP_HOSTED_D1    48
#define BOARD_SDIO_ESP_HOSTED_D2    53
#define BOARD_SDIO_ESP_HOSTED_D3    52
#define BOARD_SDIO_ESP_HOSTED_RESET C5_EN

#endif /* Pins_Arduino_h */
