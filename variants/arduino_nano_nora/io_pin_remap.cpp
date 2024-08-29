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
  44,  // [ 0] D0, RX
  43,  // [ 1] D1, TX
  5,   // [ 2] D2
  6,   // [ 3] D3, CTS
  7,   // [ 4] D4, DSR
  8,   // [ 5] D5
  9,   // [ 6] D6
  10,  // [ 7] D7
  17,  // [ 8] D8
  18,  // [ 9] D9
  21,  // [10] D10, SS
  38,  // [11] D11, MOSI
  47,  // [12] D12, MISO
  48,  // [13] D13, SCK, LED_BUILTIN
  46,  // [14] LED_RED
  0,   // [15] LED_GREEN
  45,  // [16] LED_BLUE, RTS
  1,   // [17] A0, DTR
  2,   // [18] A1
  3,   // [19] A2
  4,   // [20] A3
  11,  // [21] A4, SDA
  12,  // [22] A5, SCL
  13,  // [23] A6
  14,  // [24] A7
};

#if defined(BOARD_HAS_PIN_REMAP) && !defined(BOARD_USES_HW_GPIO_NUMBERS)

int8_t digitalPinToGPIONumber(int8_t digitalPin) {
  if ((digitalPin < 0) || (digitalPin >= NUM_DIGITAL_PINS)) {
    return -1;
  }
  return TO_GPIO_NUMBER[digitalPin];
}

int8_t gpioNumberToDigitalPin(int8_t gpioNumber) {
  if (gpioNumber < 0) {
    return -1;
  }

  // slow linear table lookup
  for (int8_t digitalPin = 0; digitalPin < NUM_DIGITAL_PINS; ++digitalPin) {
    if (TO_GPIO_NUMBER[digitalPin] == gpioNumber) {
      return digitalPin;
    }
  }

  // not found
  return -1;
}

#endif
