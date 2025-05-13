/*  Satellite 1 CORE ‒ rev 5.1  (R2025-03-18)
 *  ESP32-S3-WROOM-1-N16R8  · 16 MB Octal-SPI flash + 8 MB Octal-SPI PSRAM
 *  40-pin header is a one-for-one Raspberry-Pi-Zero footprint (BCM numbering).
 */
#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

/* ───────── USB descriptors (used only when USB-Mode = TinyUSB) ───────── */
#define USB_VID          0x303A                 // Espressif Systems
#define USB_PID          0x80F2                 // provisional, unused PID
#define USB_MANUFACTURER "FutureProofHomes"
#define USB_PRODUCT      "Satellite1 CORE rev5.1"

/* ───────── Pi-header aliases (primary UART / I²C / I²S) ─────────────── */
static const uint8_t TX  = 17;   // BCM14  (header pin 8)
static const uint8_t RX  = 16;   // BCM15  (header pin 10)

static const uint8_t SDA = 38;   // BCM2   (pin 3)
static const uint8_t SCL = 39;   // BCM3   (pin 5)

/* Wire0 definition expected by core */
#define PIN_WIRE_SDA SDA
#define PIN_WIRE_SCL SCL
#define SDA2 SDA     // back-compat
#define SCL2 SCL

/* I²S */
static const uint8_t I2S_BCLK  = 36;   // BCM18 (pin 12)
static const uint8_t I2S_LRCLK = 37;   // BCM19 (pin 35)
static const uint8_t I2S_DIN   = 35;   // BCM20 (pin 38) ← ADC
static const uint8_t I2S_DOUT  = 34;   // BCM21 (pin 40) → DAC

/* Optional default SPI (remappable via GPIO-matrix) */
static const uint8_t SS   = 5;   // BCM8  (CE0)
static const uint8_t MOSI = 23;  // BCM10
static const uint8_t MISO = 22;  // BCM9
static const uint8_t SCK  = 18;  // BCM11

/* ───────── On-board user interface ───────── */
#define LED_BUILTIN      45         // red status LED (active-HIGH)
#define BUILTIN_LED      LED_BUILTIN

#define BUTTON_BUILTIN   0          // SW2, active-LOW

/* ───────── PSRAM / flash configuration ───────── */
#define BOARD_HAS_PSRAM   1
#define DEFAULT_PSRAM_OCT 1         // 8-bit OPI @ 120 MHz

/* ───────── ADC & touch aliases ───────── */
static const uint8_t A0 = 1;   static const uint8_t T4  = 4;
static const uint8_t A1 = 2;   static const uint8_t T5  = 5;
static const uint8_t A2 = 3;   static const uint8_t T6  = 6;
static const uint8_t A3 = 4;   static const uint8_t T7  = 7;
static const uint8_t A4 = 6;   static const uint8_t T8  = 8;
static const uint8_t A5 = 7;

#endif /* Pins_Arduino_h */
