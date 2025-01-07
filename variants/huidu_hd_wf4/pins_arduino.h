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

// X1
#define WF4_X1_R1_PIN 2
#define WF4_X1_R2_PIN 3
#define WF4_X1_G1_PIN 6
#define WF4_X1_G2_PIN 7
#define WF4_X1_B1_PIN 10
#define WF4_X1_B2_PIN 11
#define WF4_X1_CS_PIN 45  // CS gpio must be set HIGH to enable X1 output

// X2
#define WF4_X2_R1_PIN 4
#define WF4_X2_R2_PIN 5
#define WF4_X2_G1_PIN 8
#define WF4_X2_G2_PIN 9
#define WF4_X2_B1_PIN 12
#define WF4_X2_B2_PIN 13
#define WF4_X2_CS_PIN WF4_X1_CS_PIN  // CS gpio must be set HIGH to enable X2 output

// X3
#define WF4_X3_R1_PIN 2
#define WF4_X3_R2_PIN 3
#define WF4_X3_G1_PIN 6
#define WF4_X3_G2_PIN 7
#define WF4_X3_B1_PIN 10
#define WF4_X3_B2_PIN 11
#define WF4_X3_CS_PIN 14  // CS gpio must be set HIGH to enable X3 output

// X4
#define WF4_X4_R1_PIN 4
#define WF4_X4_R2_PIN 5
#define WF4_X4_G1_PIN 8
#define WF4_X4_G2_PIN 9
#define WF4_X4_B1_PIN 12
#define WF4_X4_B2_PIN 13
#define WF4_X4_CS_PIN WF4_X3_CS_PIN  // CS gpio must be set HIGH to enable X4 output

//#define WF4_P1_PIN      UART
#define WF4_P2_DATA_PIN 0   // GPIO0 boot
#define WF4_P5_DATA_PIN 16  // temperature
//#define WF4_P7_PIN      VCC/GND
#define WF4_P11_DATA_PIN  15  // IR
#define WF4_P12_DATA1_PIN 47
#define WF4_P12_DATA2_PIN 18
#define WF4_S1_DATA_PIN   17  // Button
#define WF4_S2_DATA_PIN   48
#define WF4_S3_DATA_PIN   26
#define WF4_S4_DATA_PIN   46

#define WF4_BUTTON_TEST    WF4_S1_PIN  // Test key button on PCB, 1=normal, 0=pressed
#define WF4_LED_RUN_PIN    40          // Status LED on PCB
#define WF4_BM8563_I2C_SDA 41          // RTC BM8563 I2C port
#define WF4_BM8563_I2C_SCL 42
#define WF4_USB_DM_PIN     19
#define WF4_USB_DP_PIN     20

#define LED_BUILTIN WF4_LED_RUN_PIN
#define BUILTIN_LED LED_BUILTIN  // backward compatibility

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = WF4_BM8563_I2C_SDA;
static const uint8_t SCL = WF4_BM8563_I2C_SCL;

static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

static const uint8_t T1 = WF4_X1_R1_PIN;
static const uint8_t T2 = WF4_X1_R2_PIN;
static const uint8_t T3 = WF4_X1_G1_PIN;
static const uint8_t T4 = WF4_X1_G2_PIN;
static const uint8_t T5 = WF4_X1_B1_PIN;
static const uint8_t T6 = WF4_X1_B2_PIN;
static const uint8_t T7 = WF4_A_PIN;
static const uint8_t T8 = WF4_B_PIN;
static const uint8_t T9 = WF4_C_PIN;
static const uint8_t T10 = WF4_D_PIN;
static const uint8_t T11 = WF4_E_PIN;
static const uint8_t T12 = WF4_OE_PIN;
static const uint8_t T13 = WF4_CLK_PIN;
static const uint8_t T14 = WF4_LAT_PIN;

#endif /* Pins_Arduino_h */
