#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 45;
static const uint8_t SCL = 46;

static const uint8_t SS = 34;
static const uint8_t MOSI = 35;
static const uint8_t MISO = 37;
static const uint8_t SCK = 36;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;

static const uint8_t T3 = 3;

// SDCARD Slot
#define BOARD_HAS_SDMMC
#define SDMMC_D2             33  // SDMMC Data2
#define SDMMC_D3             34  // SDMMC Data3 / SPI CS
#define SDMMC_CMD            35  // SDMMC CMD   / SPI MOSI
#define SDMMC_CLK            36  // SDMMC CLK   / SPI SCK
#define SDMMC_D0             37  // SDMMC Data0 / SPI MISO
#define SDMMC_D1             38  // SDMMC Data1
#define BOARD_MAX_SDMMC_FREQ SDMMC_FREQ_DEFAULT

// 240x240 LCD
#define BOARD_HAS_SPI_LCD
#define LCD_MODEL  ST7789
#define LCD_WIDTH  240
#define LCD_HEIGHT 240  // *RAM height is actually 320!
#define LCD_MISO   -1   // LCD Does not use MISO.
#define LCD_DC     4    // Used to switch data and command status.
#define LCD_CS     5    // used to enable LCD, low level to enable.
#define LCD_CLK    6    // LCD SPI Clock.
#define LCD_MOSI   7    // LCD SPI MOSI.
#define LCD_RST    8    // used to reset LCD, low level to reset.
#define LCD_BL     9    // LCD backlight control.

// Buttons
#define BUTTON_OK   0   // OK button, low level when pressed.
#define BUTTON_UP   10  // UP button, low level when pressed.
#define BUTTON_DOWN 11  // Down button, low level when pressed.
#define BUTTON_MENU 14  // Menu button, low level when pressed.

// LEDs
#define LED_GREEN  15  // the light is lit when set high level.
#define LED_YELLOW 16  // the light is lit when set high level.

// Board Controls
#define DEV_VBUS_EN 12  // High level to enable DEV_VBUS power supply.
#define BOOST_EN    13  // High level to enable Battery Boost circuit.
#define LIMIT_EN    17  // Enable USB_HOST current limiting IC, high level enable.
#define USB_HOST_EN \
  18  // Used to switch the USB interface. When high level, the USB_HOST interface is enabled. When low level, the USB_DEV interface is enabled.

// Board Sensors
#define OVER_CURRENT 21  // Current overrun signal, high level means overrun.
#define HOST_VOLTS   1   // USB_DEV voltage monitoring, ADC1 channel 0. actual_v = value_v * 3.7
#define BAT_VOLTS    2   // Battery voltage monitoring, ADC1 channel 1. actual_v = value_v * 2

// USB Port
#define USB_DN 19  // USB D-
#define USB_DP 20  // USB D+

// Bottom header
#define MTCK 39
#define MTDO 40
#define MTDI 41
#define MTMS 42
// #define FREE_6        3 // Idle, can be customized.
// #define FREE_4       26 // Idle, can be customized.
// #define FREE_1       45 // Idle, can be customized.
// #define FREE_2       46 // Idle, can be customized.
// #define FREE_5       47 // Idle, can be customized.
// #define FREE_3       48 // Idle, can be customized.

typedef enum {
  USB_HOST_POWER_OFF,
  USB_HOST_POWER_VBUS,
  USB_HOST_POWER_BAT
} UsbHostPower_t;
void usbHostPower(UsbHostPower_t mode);
void usbHostEnable(bool enable);

#endif /* Pins_Arduino_h */
