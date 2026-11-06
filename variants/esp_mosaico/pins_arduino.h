#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303a
#define USB_PID          0x1001
#define USB_MANUFACTURER "Espressif"
#define USB_PRODUCT      "ESP-Mosaico V1.2"
#define USB_SERIAL       ""

#define MOSAICO_BOARD_VERSION 12

static const uint8_t TX = 58;
static const uint8_t RX = 59;

// Mainboard peripherals use Wire; expansion modules use Wire1.
static const uint8_t SDA = 56;
static const uint8_t SCL = 3;
#define WIRE1_PIN_DEFINED
static const uint8_t SDA1 = 0;
static const uint8_t SCL1 = 1;

// No uncommitted SPI bus: specify pins explicitly for external peripherals.
static const int8_t SS = -1;
static const int8_t MOSI = -1;
static const int8_t MISO = -1;
static const int8_t SCK = -1;

// Exposed ADC-capable pins; A3, A4 and A6 are shared with onboard audio.
static const uint8_t A0 = 46;
static const uint8_t A1 = 47;
static const uint8_t A2 = 48;
static const uint8_t A3 = 49;
static const uint8_t A4 = 52;
static const uint8_t A5 = 53;
static const uint8_t A6 = 54;
static const uint8_t A7 = 55;

// CO5300 QSPI on SPI2_HOST. V1.0 used SCK=44 and RESET=42 instead.
#define MOSAICO_LCD_CS     50
#define MOSAICO_LCD_SCK    42
#define MOSAICO_LCD_D0     36
#define MOSAICO_LCD_D1     51
#define MOSAICO_LCD_D2     35
#define MOSAICO_LCD_D3     9
#define MOSAICO_LCD_RESET  44
#define MOSAICO_LCD_TE     43
#define MOSAICO_LCD_WIDTH  480
#define MOSAICO_LCD_HEIGHT 480

// CST9220 touch on Wire; no separate reset pin.
#define MOSAICO_TOUCH_INT     6
#define MOSAICO_TOUCH_ADDRESS 0x5A

// ES8311 on Wire; DIN/DOUT are named from the MCU's perspective.
#define MOSAICO_I2S_MCLK      54
#define MOSAICO_I2S_BCLK      37
#define MOSAICO_I2S_WS        49
#define MOSAICO_I2S_DOUT      52
#define MOSAICO_I2S_DIN       40
#define MOSAICO_PA_ENABLE     45
#define MOSAICO_CODEC_ADDRESS 0x19

// Active-low peripheral supply; GPIO57 must only be asserted open-drain.
#define MOSAICO_PERIPHERAL_POWER 60
#define MOSAICO_POWER_ON_LEVEL   0
#define MOSAICO_POWER_OFF_LEVEL  1
#define MOSAICO_SHUTDOWN         57
#define MOSAICO_BUTTON           7
#define MOSAICO_BOOT             61
#define MOSAICO_BUTTON_ACTIVE    0
#define MOSAICO_MOTOR            8

// V1.2 has no programmable LED and no separate codec power control pin.
// Do not define LED_BUILTIN/RGB_BUILTIN: GPIO3 is SCL, GPIO60 is power.

// NAND flash on SPI3_HOST, separate from the application NOR flash.
#define MOSAICO_NAND_SCK  20
#define MOSAICO_NAND_MOSI 21
#define MOSAICO_NAND_MISO 22
#define MOSAICO_NAND_CS   23
#define MOSAICO_NAND_HOLD 24
#define MOSAICO_NAND_WP   25

#define MOSAICO_SENSOR_INT      2
#define MOSAICO_IMU_ADDRESS     0x69
#define MOSAICO_MAG_ADDRESS_0   0x11
#define MOSAICO_MAG_ADDRESS_1   0x12
#define MOSAICO_BATTERY_ADDRESS 0x55

#endif /* Pins_Arduino_h */
