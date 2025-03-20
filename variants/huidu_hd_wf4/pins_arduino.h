#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Huidu HD-WF4 - esp32-s3 HUB75 driver board
// https://www.hdwell.com/Product/index46.html
// https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/discussions/667

#define USB_VID 0x303a
#define USB_PID 0x1001

#define WF4_A_PIN   39
#define WF4_B_PIN   38
#define WF4_C_PIN   37
#define WF4_D_PIN   36
#define WF4_E_PIN   21
#define WF4_OE_PIN  35
#define WF4_CLK_PIN 34
#define WF4_LAT_PIN 33

// X1 HUB75
#define WF4_X1_R1_PIN 2
#define WF4_X1_R2_PIN 3
#define WF4_X1_G1_PIN 6
#define WF4_X1_G2_PIN 7
#define WF4_X1_B1_PIN 10
#define WF4_X1_B2_PIN 11
#define WF4_X1_CS_PIN 45  // CS gpio must be set HIGH to enable X1 output

// X2 HUB75
#define WF4_X2_R1_PIN 4
#define WF4_X2_R2_PIN 5
#define WF4_X2_G1_PIN 8
#define WF4_X2_G2_PIN 9
#define WF4_X2_B1_PIN 12
#define WF4_X2_B2_PIN 13
#define WF4_X2_CS_PIN WF4_X1_CS_PIN  // CS gpio must be set HIGH to enable X2 output

// X3 HUB75
#define WF4_X3_R1_PIN 2
#define WF4_X3_R2_PIN 3
#define WF4_X3_G1_PIN 6
#define WF4_X3_G2_PIN 7
#define WF4_X3_B1_PIN 10
#define WF4_X3_B2_PIN 11
#define WF4_X3_CS_PIN 14  // CS gpio must be set HIGH to enable X3 output

// X4 HUB75
#define WF4_X4_R1_PIN 4
#define WF4_X4_R2_PIN 5
#define WF4_X4_G1_PIN 8
#define WF4_X4_G2_PIN 9
#define WF4_X4_B1_PIN 12
#define WF4_X4_B2_PIN 13
#define WF4_X4_CS_PIN WF4_X3_CS_PIN  // CS gpio must be set HIGH to enable X4 output

// P1 is a UART connector
#define WF4_P1_RX_PIN 44
#define WF4_P1_TX_PIN 43

// P2: PCB holes gpio/gnd
#define WF4_P2_DATA_PIN 0  // GPIO0 boot

// P5: temperature sensor connector
#define WF4_P5_DATA_PIN 16

// P7: VCC/GPIO holes on PCB
#define WF4_P7_DATA_PIN 1

// P11: IR connector
#define WF4_P11_DATA_PIN 15

// P12: two gpio's, Vcc, GND
#define WF4_P12_DATA1_PIN 47
#define WF4_P12_DATA2_PIN 18

// S1 Button
#define WF4_S1_DATA_PIN 17

// S2-S3 PCB holes
#define WF4_S2_DATA_PIN 48
#define WF4_S3_DATA_PIN 26
#define WF4_S4_DATA_PIN 46

#define WF4_BUTTON_TEST    WF4_S1_PIN  // Test key button on PCB, 1=normal, 0=pressed
#define WF4_LED_RUN_PIN    40          // Status LED on PCB
#define WF4_BM8563_I2C_SDA 41          // RTC BM8563 I2C port
#define WF4_BM8563_I2C_SCL 42
#define WF4_USB_DN_PIN     19  // USB-A D-
#define WF4_USB_DP_PIN     20  // USB-A D+

#define LED_BUILTIN WF4_LED_RUN_PIN
#define BUILTIN_LED LED_BUILTIN  // backward compatibility

static const uint8_t TX = WF4_P1_TX_PIN;
static const uint8_t RX = WF4_P1_RX_PIN;

static const uint8_t SDA = WF4_BM8563_I2C_SDA;
static const uint8_t SCL = WF4_BM8563_I2C_SCL;

// there is no dedicated SPI connector on board, but SPI could be accessed via PCB holes
static const uint8_t SS = WF4_S2_DATA_PIN;
static const uint8_t MOSI = WF4_S3_DATA_PIN;
static const uint8_t MISO = WF4_S4_DATA_PIN;
static const uint8_t SCK = WF4_P7_DATA_PIN;

// touch pins are mostly busy with HUB75 ports
static const uint8_t T1 = WF4_P7_DATA_PIN;

#endif /* Pins_Arduino_h */
