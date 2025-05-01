#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Huidu HD-WF2 - esp32-s3 HUB75 driver board
// https://www.hdwell.com/Product/index46.html
// https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/discussions/667

#define USB_VID 0x303a
#define USB_PID 0x1001

#define WF2_X1_R1_PIN 2
#define WF2_X1_R2_PIN 3
#define WF2_X1_G1_PIN 6
#define WF2_X1_G2_PIN 7
#define WF2_X1_B1_PIN 10
#define WF2_X1_B2_PIN 11
#define WF2_X1_E_PIN  21

#define WF2_X2_R1_PIN 4
#define WF2_X2_R2_PIN 5
#define WF2_X2_G1_PIN 8
#define WF2_X2_G2_PIN 9
#define WF2_X2_B1_PIN 12
#define WF2_X2_B2_PIN 13
#define WF2_X2_E_PIN  -1  // Currently unknown, so X2 port will not work (yet) with 1/32 scan panels

#define WF2_A_PIN   39
#define WF2_B_PIN   38
#define WF2_C_PIN   37
#define WF2_D_PIN   36
#define WF2_OE_PIN  35
#define WF2_CLK_PIN 34
#define WF2_LAT_PIN 33

#define WF2_BUTTON_TEST    17  // Test key button on PCB, 1=normal, 0=pressed
#define WF2_LED_RUN_PIN    40  // Status LED on PCB
#define WF2_BM8563_I2C_SDA 41  // RTC BM8563 I2C port
#define WF2_BM8563_I2C_SCL 42
#define WF2_USB_DN_PIN     19  // USB D-
#define WF2_USB_DP_PIN     20  // USB D+

#define WF2_PCB1_PIN 45  // open pad on PCB
#define WF2_PCB2_PIN 46  // open pad on PCB

#define LED_BUILTIN WF2_LED_RUN_PIN
#define BUILTIN_LED LED_BUILTIN  // backward compatibility

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = WF2_BM8563_I2C_SDA;
static const uint8_t SCL = WF2_BM8563_I2C_SCL;

static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

#endif /* Pins_Arduino_h */
