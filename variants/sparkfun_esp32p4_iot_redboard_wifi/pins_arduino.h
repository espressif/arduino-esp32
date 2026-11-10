#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// SparkFun IoT RedBoard - ESP32-P4 (WiFi 6 / BLE 5 via ESP32-C5-WROOM-1 co-processor)

// BOOT_MODE 35 (BOOT button SW2, 10k pullup)
// BOOT_MODE2 36 pullup

// WS2812 RGB LED
#define PIN_RGB_LED 12
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t TX = 37;
static const uint8_t RX = 38;

// Qwiic, MIPI camera/display FPCs and MAX17048 fuel gauge
static const uint8_t SDA = 26;
static const uint8_t SCL = 27;

static const uint8_t SS = 7;
static const uint8_t MOSI = 8;
static const uint8_t MISO = 10;
static const uint8_t SCK = 9;

// RedBoard (Uno R3 footprint) header
static const uint8_t D0 = 38;
static const uint8_t D1 = 37;
static const uint8_t D2 = 11;
static const uint8_t D3 = 0;
static const uint8_t D4 = 1;
static const uint8_t D5 = 2;
static const uint8_t D6 = 3;
static const uint8_t D7 = 4;
static const uint8_t D8 = 5;
static const uint8_t D9 = 6;
static const uint8_t D10 = 7;
static const uint8_t D11 = 8;
static const uint8_t D12 = 10;
static const uint8_t D13 = 9;

static const uint8_t A0 = 54;
static const uint8_t A1 = 53;
static const uint8_t A2 = 52;
static const uint8_t A3 = 51;
static const uint8_t A4 = 50;
static const uint8_t A5 = 49;

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

// MIPI camera connector (J5) GPIO, 10k pullup
#define CAM_IO 13

// MAX17048 fuel gauge alert (active low) and LM66200 power source status
#define FUEL_GAUGE_ALERT 46
#define BAT_STATUS       48

//SDMMC
#define BOARD_HAS_SDMMC
#define BOARD_SDMMC_SLOT           0
#define BOARD_SDMMC_POWER_CHANNEL  4
#define BOARD_SDMMC_POWER_PIN      45
#define BOARD_SDMMC_POWER_ON_LEVEL LOW
#define SD_CARD_DETECT             47

// On-chip GP LDO: periman enables VO4 when a GPIO in the range is used (see esp32-hal-ldo.c).
#define BOARD_PERIMAN_IO_LDO_AUTO        1
#define BOARD_PERIMAN_IO_LDO0_CHANNEL    4   // LDO_VO4 on ESP32-P4 feeds VDD_IO_5
#define BOARD_PERIMAN_IO_LDO0_GPIO_MIN   39  // GPIO 39-48 on VO4
#define BOARD_PERIMAN_IO_LDO0_GPIO_MAX   48
#define BOARD_PERIMAN_IO_LDO0_VOLTAGE_MV 3300

//WIFI - ESP32C5
// The C5 EN pin is driven through an NPN inverter (Q4) from GPIO20: HIGH holds the C5 in reset.
// The prebuilt ESP-Hosted library drives its reset pin as an active-high enable, so it is pointed
// at GPIO21 (not connected) and variant.cpp resets the C5 from GPIO20 with the correct polarity.
#define C5_RESET_PIN 20
#define BOARD_HAS_SDIO_ESP_HOSTED
#define BOARD_SDIO_ESP_HOSTED_CLK   16
#define BOARD_SDIO_ESP_HOSTED_CMD   17
#define BOARD_SDIO_ESP_HOSTED_D0    15
#define BOARD_SDIO_ESP_HOSTED_D1    14
#define BOARD_SDIO_ESP_HOSTED_D2    19
#define BOARD_SDIO_ESP_HOSTED_D3    18
#define BOARD_SDIO_ESP_HOSTED_RESET 21

#endif /* Pins_Arduino_h */
