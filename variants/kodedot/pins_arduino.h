/*
  ────────────────────────────────────────────────────────────────────────
  KodeDot – ESP32-S3R8  Variant (rev. with OPI flash/PSRAM)
  Pin definition file for the Arduino-ESP32 core
  ────────────────────────────────────────────────────────────────────────
  * External 2 × 10 connector → simple aliases  PIN1 … PIN20
  * On-board QSPI LCD 410×502 @40 MHz  (SPI3_HOST)
  * micro-SD on SDMMC (1-bit)
  * Dual-I²C:  external (GPIO37/36)  +  internal-sensors (GPIO48/47)
  * USB VID/PID 0x303A:0x1001
*/

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include <stdbool.h>

/*──────────────── USB device descriptor ────────────────*/
#define USB_VID 0x303A  // Espressif Systems VID
#define USB_PID 0x1001  // Product ID: KodeDot-S3

/*──────────────── UART0 (Arduino Serial) ────────────────*/
static const uint8_t TX = 43;  // U0TXD – PIN16 on the 2×10 header
static const uint8_t RX = 44;  // U0RXD – PIN18 on the 2×10 header

/*──────────────── I²C buses ─────────────────────────────*/
/* External expansion bus → header pins 11/13 */
static const uint8_t SCL = 37;  // GPIO37 – PIN12
static const uint8_t SDA = 36;  // GPIO36 – PIN14

/* Internal sensor/touch bus (not on header) */
#define INT_I2C_SCL 47  // GPIO47
#define INT_I2C_SDA 48  // GPIO48

/*──────────────── SDMMC – micro-SD (1-bit) ───────────────*/
#define BOARD_HAS_SD_SDMMC
#define SDMMC_CMD 5   // SD_CMD
#define SDMMC_CLK 6   // SD_CLK
#define SDMMC_D0  7   // SD_D0
#define SD_CS     -1  // No CS pin in SDMMC mode

/*──────────────── QSPI LCD (SPI3_HOST) ─────────────────–
 * Controller: ST7789 / 4-line SPI (no D/C pin)
 * Resolution: 410×502 px, 16 bpp, RGB color-space
 * Clock:      40 MHz
 */
#define BOARD_HAS_SPI_LCD
#define LCD_MODEL  ST7789
#define LCD_WIDTH  410
#define LCD_HEIGHT 502

#define LCD_HOST SPI3_HOST
#define LCD_SCK  17  // GPIO17  • QSPI_CLK
#define LCD_MOSI 15  // GPIO15  • QSPI_IO0 (D0)
#define LCD_IO1  14  // GPIO14  • QSPI_IO1 (D1)
#define LCD_IO2  16  // GPIO16  • QSPI_IO2 (D2)
#define LCD_IO3  10  // GPIO10  • QSPI_IO3 (D3)
#define LCD_CS   9   // GPIO09
#define LCD_RST  8   // GPIO08
#define LCD_DC   -1  // not used in 4-line SPI

/*──────────────── Analog / Touch pads ────────────────*/
static const uint8_t A0 = 11;  // PIN4  – GPIO11 / TOUCH11 / ADC2_CH0
static const uint8_t A1 = 12;  // PIN6  – GPIO12 / TOUCH12 / ADC2_CH1
static const uint8_t A2 = 13;  // PIN8  – GPIO13 / TOUCH13 / ADC2_CH2
static const uint8_t A3 = 14;  // PIN10 – GPIO14 / TOUCH14 / ADC2_CH3
static const uint8_t T0 = A0, T1 = A1, T2 = A2, T3 = A3;

/*──────────────── On-board controls & indicator ─────────*/
#define BUTTON_TOP    0   // GPIO00 – BOOT    • active-LOW
#define BUTTON_BOTTOM -1  // via IO expander  • no direct GPIO
#define NEOPIXEL_PIN  4   // GPIO04 – WS2812
#define LED_BUILTIN   NEOPIXEL_PIN

/*──────────────── JTAG (also on connector) ──────────────*/
#define MTCK 39  // PIN11 – GPIO39
#define MTDO 40  // PIN13 – GPIO40
#define MTDI 41  // PIN15 – GPIO41
#define MTMS 42  // PIN17 – GPIO42

/*──────────────── 2×10 header: simple aliases ───────────
   NOTE: power pins (1 = 5 V, 2 = 3 V3, 19/20 = GND) are **not**
         exposed as GPIO numbers – they remain undefined here. */
#define PIN3  1   // GPIO01 / TOUCH1 / ADC1_CH0
#define PIN4  11  // GPIO11 / TOUCH11 / ADC2_CH0
#define PIN5  2   // GPIO02 / TOUCH2 / ADC1_CH1
#define PIN6  12  // GPIO12 / TOUCH12 / ADC2_CH1
#define PIN7  3   // GPIO03 / TOUCH3 / ADC1_CH2
#define PIN8  13  // GPIO13 / TOUCH13 / ADC2_CH2
#define PIN9  4   // GPIO04 / TOUCH4 / ADC1_CH3
#define PIN10 14  // GPIO14 / TOUCH14 / ADC2_CH3
#define PIN11 39  // MTCK
#define PIN12 37  // SCL  (external I²C)
#define PIN13 40  // MTDO
#define PIN14 36  // SDA  (external I²C)
#define PIN15 41  // MTDI
#define PIN16 43  // TX  (U0TXD)
#define PIN17 42  // MTMS
#define PIN18 44  // RX  (U0RXD)
/* PIN1, PIN2, PIN19, PIN20 are power/ground and deliberately
   left undefined – they are **not** usable as GPIO. */

/* ==== Default SPI pins for library compatibility ==== */
#define SCK  17  // LCD_SCK  • QSPI_CLK
#define MISO -1  // No MISO available in this design
#define MOSI 15  // LCD_MOSI • QSPI_IO0
#define SS   9   // LCD_CS

#endif /* Pins_Arduino_h */
