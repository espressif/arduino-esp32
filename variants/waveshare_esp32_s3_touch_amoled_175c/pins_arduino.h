#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-AMOLED-1.75C"
#define USB_SERIAL       ""

// 1.75" 466x466 AMOLED, CO5300 driver over QSPI
#define LCD_CS     12
#define LCD_SCLK   38
#define LCD_SDIO0  4
#define LCD_SDIO1  5
#define LCD_SDIO2  6
#define LCD_SDIO3  7
#define LCD_RESET  1
#define LCD_TE     13
#define LCD_WIDTH  466
#define LCD_HEIGHT 466

// CST9217 touch, on the shared I2C bus
#define TP_INT   11
#define TP_RESET 2

// The shared I2C bus: CST9217 touch (0x5A), AXP2101 PMU (0x34), ES8311 speaker
// codec (0x18), ES7210 mic ADC (0x40) and QMI8658 IMU (0x6B)
#define IIC_SDA 15
#define IIC_SCL 14

// I2S to the ES8311 (speaker) and ES7210 (microphones)
#define I2S_MCLK 16
#define I2S_BCLK 9
#define I2S_LRCK 45
#define I2S_DOUT 8   // to the ES8311
#define I2S_DIN  10  // from the ES7210
#define PA_CTRL  46  // speaker amplifier enable

#define QMI8658_ADDRESS 0x6B
#define QMI8658_INT1    21

#define BUTTON_BOOT 0

// The PWR key reads high while held (on the 1.75 it is on the PMU instead)
#define BUTTON_PWR 3

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = IIC_SDA;
static const uint8_t SCL = IIC_SCL;

// On the test pads on the back of the board (17, 18, 41, 42)
static const uint8_t SS = 42;
static const uint8_t MOSI = 18;
static const uint8_t MISO = 17;
static const uint8_t SCK = 41;

#endif /* Pins_Arduino_h */
