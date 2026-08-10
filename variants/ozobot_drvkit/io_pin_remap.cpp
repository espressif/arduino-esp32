#if defined(BOARD_HAS_PIN_REMAP) && !defined(ARDUINO_CORE_BUILD)
// -DARDUINO_CORE_BUILD must be set for core files only, to avoid extra
// remapping steps that would create all sorts of issues in the core.
// Removing -DBOARD_HAS_PIN_REMAP at least does correctly restore the
// use of GPIO numbers in the API.
#error This build system is not supported. Please rebuild without BOARD_HAS_PIN_REMAP.
#endif

#if !defined(BOARD_HAS_PIN_REMAP)
// This board uses pin mapping but the build system has disabled it
#warning The build system forces the Arduino API to use GPIO numbers on a board that has custom pin mapping.
#elif defined(BOARD_USES_HW_GPIO_NUMBERS)
// The user has chosen to disable pin mapping.
#warning The Arduino API will use GPIO numbers for this build.
#endif

#include "Arduino.h"

// NOTE: This must match with the remapped pin sequence in pins_arduino.h
static const int8_t TO_GPIO_NUMBER[] = {
  35,  // [ 0] G0
  36,  // [ 1] G1
  37,  // [ 2] G2
  38,  // [ 3] G3
  45,  // [ 4] G4
  46,  // [ 5] G5
  39,  // [ 6] G6
  40,  // [ 7] G7
  41,  // [ 8] G8
  42,  // [ 9] G9
  18,  // [10] Motor L IN1
  17,  // [11] Motor L IN2
  21,  // [12] Motor R IN1
  33,  // [13] Motor R IN2
  34,  // [14] SPI CS
  12,  // [15] SPI CLK
  11,  // [16] SPI MOSI
  13,  // [17] SPI MISO
  47,  // [18] I2C SDA
  48,  // [19] I2C SCK
  14,  // [20] I2C Interrupt
  43,  // [21] UART TX
  44,  // [22] UART RX
  0,   // [23] Boot pin
  1,   // [24] ADC Channel 0
  2,   // [25] ADC Channel 1
  3,   // [26] ADC Channel 2
  4,   // [27] ADC Channel 3
  5,   // [28] ADC Channel 4
  6,   // [29] ADC Channel 5
  7,   // [30] ADC Channel 6
  8,   // [31] ADC Channel 7
  9,   // [32] ADC Channel 8
  10,  // [33] ADC Channel 9
};

static const unsigned REMAP_TABLE_ENTRIES = sizeof(TO_GPIO_NUMBER) / sizeof(TO_GPIO_NUMBER[0]);

#if defined(BOARD_HAS_PIN_REMAP) && !defined(BOARD_USES_HW_GPIO_NUMBERS)

int8_t digitalPinToGPIONumber(int8_t digitalPin) {
  if ((digitalPin < 0) || (digitalPin >= REMAP_TABLE_ENTRIES)) {
    return -1;
  }
  return TO_GPIO_NUMBER[digitalPin];
}

int8_t gpioNumberToDigitalPin(int8_t gpioNumber) {
  if (gpioNumber < 0) {
    return -1;
  }

  // slow linear table lookup
  for (int8_t digitalPin = 0; digitalPin < REMAP_TABLE_ENTRIES; ++digitalPin) {
    if (TO_GPIO_NUMBER[digitalPin] == gpioNumber) {
      return digitalPin;
    }
  }

  // not found
  return -1;
}

#endif
