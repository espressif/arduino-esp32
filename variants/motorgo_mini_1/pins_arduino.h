#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#include "soc/soc_caps.h"

#define USB_VID 0x303A
#define USB_PID 0x9001
#define USB_MANUFACTURER "MotorGo"
#define USB_PRODUCT "MotorGo Mini 1 (ESP32-S3)"
#define USB_SERIAL ""  // Empty string for MAC adddress

// Default USB FirmwareMSC Settings
#define USB_FW_MSC_VENDOR_ID "MotorGo"                     // max 8 chars
#define USB_FW_MSC_PRODUCT_ID "MotorGo Mini 1 (ESP32-S3)"  // max 16 chars
#define USB_FW_MSC_PRODUCT_REVISION "3.00"                 // max 4 chars
#define USB_FW_MSC_VOLUME_NAME "MotorGo"                   // max 11 chars

#define NUM_DIGITAL_PINS 6
#define NUM_ANALOG_INPUTS 1

#define analogInputToDigitalPin(p) \
  (((p) < 20) ? (analogChannelToDigitalPin(p)) : -1)
#define digitalPinToInterrupt(p) (((p) < 48) ? (p) : -1)
#define digitalPinHasPWM(p) (p < 46)

// A flag to indicate a GPIO pin is not set
#define MOTORGO_GPIO_NOT_SET 0xFF

// Built-in LED available to user
static const uint8_t LED_BUILTIN = 38;

// Status LED
static const uint8_t LED_STATUS = 8;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 38;
static const uint8_t SCL = 39;
static const uint8_t QWIIC_SDA = SDA;
static const uint8_t QWIIC_SCL = SCL;

static const uint8_t ENC_SDA = 35;
static const uint8_t ENC_SCL = 36;
// Encoder uses SSI, but we still need to define MOSI
// Pin 45 is not connected to anything, so we can use it
static const uint8_t ENC_MOSI = 45;

// ch0 Motor and Encoder pins
static const uint8_t CH0_ENC_CS = 37;
static const uint8_t CH0_UH = 18;
static const uint8_t CH0_UL = 15;
static const uint8_t CH0_VH = 17;
static const uint8_t CH0_VL = 5;
static const uint8_t CH0_WH = 16;
static const uint8_t CH0_WL = 6;
static const uint8_t CH0_CURRENT_U = 7;
static const uint8_t CH0_CURRENT_V = MOTORGO_GPIO_NOT_SET;
static const uint8_t CH0_CURRENT_W = 4;

// ch1 Motor and Encoder pins
static const uint8_t CH1_ENC_CS = 48;
static const uint8_t CH1_UH = 9;
static const uint8_t CH1_UL = 13;
static const uint8_t CH1_VH = 10;
static const uint8_t CH1_VL = 21;
static const uint8_t CH1_WH = 11;
static const uint8_t CH1_WL = 14;
static const uint8_t CH1_CURRENT_U = 8;
static const uint8_t CH1_CURRENT_V = MOTORGO_GPIO_NOT_SET;
static const uint8_t CH1_CURRENT_W = 12;

static const uint8_t CURRENT_SENSE_AMP_GAIN = 200;
static const uint8_t CURRENT_SENSE_RESISTANCE_mOHM = 3;

static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

// The MotorGo Mini 1 exposes 1 GPIO pin connected to an ADC
static const uint8_t A8 = 8;

#endif /* Pins_Arduino_h */